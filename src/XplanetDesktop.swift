import AppKit
import Darwin
import ImageIO
import CryptoKit
import zlib

func note(_ s: String) {
    try? FileHandle.standardError.write(contentsOf: Data(("xplanet-desktop: " + s + "\n").utf8))
}

struct Failure: Error, CustomStringConvertible {
    let description: String
    init(_ description: String) { self.description = description }
}

struct Frame: @unchecked Sendable {
    let image: CGImage
    var hash: String?
    private static func validPNG(_ data: Data) -> Bool {
        let signature: [UInt8] = [137,80,78,71,13,10,26,10]
        guard data.starts(with: signature), data.count >= 33 else { return false }
        return data.withUnsafeBytes { raw in
            let bytes = raw.bindMemory(to: UInt8.self)
            guard let base = bytes.baseAddress else { return false }
            func u32(_ i: Int) -> UInt32 {
                UInt32(bytes[i]) << 24 | UInt32(bytes[i+1]) << 16
                    | UInt32(bytes[i+2]) << 8 | UInt32(bytes[i+3])
            }
            var offset = 8
            var imageData = false
            while offset <= bytes.count - 12 {
                let length = Int(u32(offset))
                guard length <= bytes.count - offset - 12 else { return false }
                let type = u32(offset + 4)
                if offset == 8 && (type != 0x49484452 || length != 13) { return false } // IHDR
                let crc = crc32(0, base.advanced(by: offset + 4), UInt32(length + 4))
                guard crc == u32(offset + 8 + length) else { return false }
                if type == 0x49444154 { imageData = true } // IDAT
                offset += length + 12
                if type == 0x49454e44 { // IEND
                    return imageData && length == 0 && offset == bytes.count
                }
            }
            return false
        }
    }
    init(_ data: Data, width: Int, height: Int, recordHash: Bool) throws {
        guard Self.validPNG(data),
              let source = CGImageSourceCreateWithData(data as CFData, nil),
              CGImageSourceGetStatus(source) == .statusComplete,
              let props = CGImageSourceCopyPropertiesAtIndex(source, 0, nil) as? [CFString: Any],
              let w = props[kCGImagePropertyPixelWidth] as? Int, let h = props[kCGImagePropertyPixelHeight] as? Int,
              w == width, h == height,
              let img = CGImageSourceCreateImageAtIndex(source, 0, [kCGImageSourceShouldCacheImmediately: true] as CFDictionary),
              CGImageSourceGetStatusAtIndex(source, 0) == .statusComplete else {
            throw Failure("incomplete or invalid PNG")
        }
        image = img
        if recordHash { hash = SHA256.hash(data: data).map { String(format: "%02x", $0) }.joined() }
    }
}

struct Canvas: Equatable, Sendable {
    let width: Int
    let height: Int
    var argument: String { "\(width)x\(height)" }

    init(width: Int, height: Int) throws {
        guard width > 0, height > 0, width <= 32768, height <= 32768,
              width <= 24_000_000 / height else { throw Failure("unsupported display pixel dimensions") }
        self.width = width
        self.height = height
    }

    init(argument: String) throws {
        let pieces = argument.split(separator: "x", omittingEmptySubsequences: false)
        guard pieces.count == 2, let width = Int(pieces[0]), let height = Int(pieces[1]) else {
            throw Failure("expected a WIDTHxHEIGHT canvas")
        }
        try self.init(width: width, height: height)
    }
}

final class Cancellation: @unchecked Sendable {
    private let lock = NSLock()
    private var requested = false
    var isRequested: Bool {
        lock.lock()
        defer { lock.unlock() }
        return requested
    }
    func request() {
        lock.lock()
        requested = true
        lock.unlock()
    }
}

struct Configuration: Sendable {
    static let usage = """
    Usage: xplanet-desktop /renderer /scene-directory cadence timeout attempt-count -- [renderer options]
      cadence and timeout are in seconds; attempt-count 0 runs until stopped.
      Use the desktop-layer opt-in in the launcher. No system wallpaper is set.
    """
    let renderer: URL
    let scene: URL
    let renderArgs: [String]
    let cadence: TimeInterval
    let timeout: TimeInterval
    let attempts: Int

    init(_ args: [String]) throws {
        guard args.count >= 6, args[5] == "--", args[0].hasPrefix("/"), args[1].hasPrefix("/"),
              let cadence = Double(args[2]), cadence.isFinite, cadence >= 0.5, cadence <= 86400,
              let timeout = Double(args[3]), timeout.isFinite, timeout >= 0.1, timeout <= 3600,
              let attempts = Int(args[4]), attempts >= 0 else {
            throw Failure(Self.usage)
        }
        var directory: ObjCBool = false
        guard FileManager.default.isExecutableFile(atPath: args[0]),
              FileManager.default.fileExists(atPath: args[1], isDirectory: &directory), directory.boolValue else {
            throw Failure("renderer must be executable and scene directory must exist")
        }
        renderer = URL(fileURLWithPath: args[0])
        scene = URL(fileURLWithPath: args[1], isDirectory: true)
        renderArgs = Array(args.dropFirst(6))
        self.cadence = cadence
        self.timeout = timeout
        self.attempts = attempts
    }
    func render(canvas: Canvas, cancellation: Cancellation, recordHash: Bool) throws -> Frame {
        if cancellation.isRequested { throw Failure("interrupted") }
        let p = Process()
        let pipe = Pipe()
        let input = pipe.fileHandleForReading
        let fd = input.fileDescriptor
        guard fcntl(fd, F_SETFL, O_NONBLOCK) == 0 else { throw Failure("cannot configure frame pipe") }
        p.executableURL = renderer
        p.currentDirectoryURL = scene
        p.arguments = ["--frame-pipe", canvas.argument] + renderArgs
        p.standardOutput = pipe
        let done = DispatchSemaphore(value: 0)
        p.terminationHandler = { _ in done.signal() }
        try p.run()
        try? pipe.fileHandleForWriting.close()
        var reaped = false
        defer {
            try? input.close()
            if !reaped {
                if p.isRunning { p.terminate() }
                if done.wait(timeout: .now() + .seconds(1)) == .timedOut {
                    if p.isRunning { kill(p.processIdentifier, SIGKILL) }
                    done.wait()
                }
            }
        }
        let deadline = DispatchTime.now() + timeout
        var result = Data()
        var buffer = [UInt8](repeating: 0, count: 64 * 1024)
        while true {
            if cancellation.isRequested { throw Failure("interrupted") }
            let now = DispatchTime.now()
            guard now < deadline else { throw Failure("renderer timed out") }
            let remaining = (deadline.uptimeNanoseconds - now.uptimeNanoseconds) / 1_000_000
            var event = pollfd(fd: fd, events: Int16(POLLIN | POLLHUP), revents: 0)
            let available = poll(&event, 1, Int32(max(1, min(remaining, 250))))
            if available == 0 || (available < 0 && errno == EINTR) { continue }
            guard available > 0, event.revents & Int16(POLLERR | POLLNVAL) == 0 else {
                throw Failure("frame pipe interrupted")
            }
            let count = buffer.withUnsafeMutableBytes { Darwin.read(fd, $0.baseAddress, $0.count) }
            if count == 0 { break }
            if count < 0 && (errno == EINTR || errno == EAGAIN) { continue }
            guard count > 0 else { throw Failure("cannot read frame pipe") }
            guard result.count <= 64 * 1024 * 1024 - count else { throw Failure("frame pipe exceeded byte limit") }
            result.append(contentsOf: buffer.prefix(count))
        }
        while true {
            if cancellation.isRequested { throw Failure("interrupted") }
            let now = DispatchTime.now()
            guard now < deadline else { throw Failure("renderer timed out") }
            if done.wait(timeout: min(deadline, now + .milliseconds(250))) == .success {
                reaped = true
                break
            }
        }
        guard p.terminationReason == .exit, p.terminationStatus == 0 else {
            throw Failure("renderer terminated (\(p.terminationStatus))")
        }
        guard !result.isEmpty else { throw Failure("no fresh frame") }
        return try Frame(result, width: canvas.width, height: canvas.height, recordHash: recordHash)
    }
}

@MainActor
final class Layer: NSObject, NSApplicationDelegate {
    private let configuration: Configuration
    private let worker = DispatchQueue(label: "org.xplanet.desktop.render", qos: .utility)
    private let cancellation = Cancellation()
    private let view = NSImageView()
    private var timer: Timer?
    private var window: NSWindow?
    private var observers: [(NotificationCenter, NSObjectProtocol)] = []
    private var signals: [DispatchSourceSignal] = []
    private var attempts = 0
    private var successfulFrames = 0
    private var inFlight = false
    private var refreshPending = false
    private var lastFailure: String?
    private var finished = false

    init(_ configuration: Configuration) { self.configuration = configuration }

    var exitStatus: Int32 {
        configuration.attempts > 0 && successfulFrames == 0 ? 1 : 0
    }

    private func mainDisplay() -> NSScreen? {
        let id = CGMainDisplayID()
        return NSScreen.screens.first {
            let number = $0.deviceDescription[NSDeviceDescriptionKey("NSScreenNumber")] as? NSNumber
            return number?.uint32Value == id
        }
    }

    func applicationDidFinishLaunching(_ notification: Notification) {
        guard mainDisplay() != nil else { note("no screen in GUI session"); exit(1) }
        for sig in [SIGTERM, SIGINT, SIGHUP] {
            signal(sig, SIG_IGN)
            let source = DispatchSource.makeSignalSource(signal: sig, queue: .main)
            source.setEventHandler { [weak self] in self?.stop() }
            source.resume()
            signals.append(source)
        }
        observe(NotificationCenter.default, NSApplication.didChangeScreenParametersNotification)
        observe(NSWorkspace.shared.notificationCenter, NSWorkspace.didWakeNotification)
        let refreshTimer = Timer(timeInterval: configuration.cadence, repeats: true) { [weak self] _ in
            MainActor.assumeIsolated { self?.refresh() }
        }
        RunLoop.main.add(refreshTimer, forMode: .common)
        timer = refreshTimer
        refresh()
    }

    private func observe(_ center: NotificationCenter, _ name: Notification.Name) {
        let observer = center.addObserver(forName: name, object: nil, queue: .main) { [weak self] _ in
            MainActor.assumeIsolated {
                guard let self, !self.finished else { return }
                self.show()
                if self.inFlight { self.refreshPending = true }
                else { self.refresh() }
            }
        }
        observers.append((center, observer))
    }

    private func show() {
        guard view.image != nil, let screen = mainDisplay() else { return }
        if window == nil {
            let desktop = NSWindow(contentRect: screen.frame, styleMask: .borderless,
                                   backing: .buffered, defer: false)
            desktop.level = NSWindow.Level(rawValue: Int(CGWindowLevelForKey(.desktopWindow)) + 1)
            desktop.collectionBehavior = [.canJoinAllSpaces, .stationary, .ignoresCycle]
            desktop.ignoresMouseEvents = true
            desktop.isExcludedFromWindowsMenu = true
            desktop.isReleasedWhenClosed = false
            view.imageScaling = .scaleAxesIndependently
            desktop.contentView = view
            window = desktop
        }
        window?.setFrame(screen.frame, display: true)
        window?.orderBack(nil)
    }

    private func stop() {
        guard !finished else { return }
        finished = true
        timer?.invalidate()
        for (center, observer) in observers { center.removeObserver(observer) }
        observers.removeAll()
        cancellation.request()
        window?.orderOut(nil)
        if !inFlight { NSApp.stop(nil); wakeApp() }
    }

    // Wake NSApplication.run after stopping even if the user is idle.
    private func wakeApp() {
        if let event = NSEvent.otherEvent(with: .applicationDefined, location: .zero,
                                          modifierFlags: [], timestamp: 0, windowNumber: 0,
                                          context: nil, subtype: 0, data1: 0, data2: 0) {
            NSApp.postEvent(event, atStart: true)
        }
    }

    private func refresh() {
        guard !finished, !inFlight, configuration.attempts == 0 || attempts < configuration.attempts else { return }
        guard mainDisplay() != nil else { return }
        let displayID = CGMainDisplayID()
        let canvas: Canvas
        do {
            canvas = try Canvas(width: CGDisplayPixelsWide(displayID), height: CGDisplayPixelsHigh(displayID))
        } catch {
            if lastFailure != "\(error)" { note("\(error)"); lastFailure = "\(error)" }
            if configuration.attempts > 0 { stop() }
            return
        }
        inFlight = true
        attempts += 1
        let attempt = attempts
        let configuration = self.configuration
        let cancellation = self.cancellation
        worker.async {
            let result = Result { try configuration.render(canvas: canvas, cancellation: cancellation,
                                                           recordHash: configuration.attempts > 0) }
            DispatchQueue.main.async {
                self.inFlight = false
                if self.finished { NSApp.stop(nil); self.wakeApp(); return }
                switch result {
                case .success(let frame):
                    self.successfulFrames += 1
                    self.lastFailure = nil
                    self.view.image = NSImage(cgImage: frame.image, size: .zero)
                    self.show()
                    if let hash = frame.hash { note("presented \(attempt) \(hash)") }
                case .failure(let error):
                    let failure = String(describing: error)
                    if self.lastFailure != failure || configuration.attempts > 0 {
                        note("rejected \(attempt): \(failure)")
                    }
                    self.lastFailure = failure
                }
                if configuration.attempts == attempt {
                    DispatchQueue.main.asyncAfter(deadline: .now() + .milliseconds(500)) { self.stop() }
                } else if self.refreshPending {
                    self.refreshPending = false
                    self.refresh()
                }
            }
        }
    }
}

do {
    var arguments = Array(CommandLine.arguments.dropFirst())
    if arguments == ["--help"] {
        print(Configuration.usage)
    } else if arguments.first == "--check-frame" {
        guard arguments.count > 2 else { throw Failure(Configuration.usage) }
        // Offline transport/decoder check; doesn't open a window or contact the wallpaper agent.
        let canvas = try Canvas(argument: arguments[1])
        arguments.removeFirst(2)
        let frame = try Configuration(arguments).render(canvas: canvas, cancellation: Cancellation(), recordHash: true)
        print(frame.hash ?? "")
    } else {
        try MainActor.assumeIsolated {
            let layer = try Layer(Configuration(arguments))
            let app = NSApplication.shared
            app.setActivationPolicy(.prohibited)
            app.delegate = layer
            withExtendedLifetime(layer) { app.run() }
            exit(layer.exitStatus)
        }
    }
} catch {
    note(String(describing: error))
    exit(1)
}
