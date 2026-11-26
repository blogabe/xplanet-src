#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "config.h"

#include "body.h"
#include "findFile.h"
#include "keywords.h"
#include "Options.h"
#include "parse.h"
#include "parseColor.h"
#include "PlanetProperties.h"
#include "readOriginFile.h"
#include "Satellite.h"
#include "SceneValidation.h"
#include "xpDefines.h"
#include "xpUtil.h"
#include "libephemeris/EphemerisHigh.h"
#include "libimage/Image.h"

extern void readConfigFile(std::string, PlanetProperties *[]);
extern "C" int opterr;

namespace SceneValidation
{
namespace
{
struct Diagnostic
{
    std::string severity, code, message, path, field;
    std::size_t line;
};

struct Report
{
    std::vector<Diagnostic> diagnostics;
    std::string path;
    std::size_t line = 0;
    const char *stage = "arguments";
    bool invalid = false;
};

Report *report = nullptr;
struct ParseFailure {};

void add(const char *severity, const char *code, const std::string &message,
         const std::string &path = {}, const std::string &field = {}, std::size_t line = 0)
{
    const auto end = message.find_last_not_of("\r\n");
    report->diagnostics.push_back({severity, code,
        end == std::string::npos ? std::string() : message.substr(0, end + 1),
        path, field, line});
    if (std::string(severity) == "error") report->invalid = true;
}

// Paths and diagnostics can contain non-UTF-8 bytes even on UTF-8 systems.
// Preserve valid UTF-8 and replace off-encoding bytes to keep stdout valid JSON.
void jsonString(const std::string &value)
{
    const char hex[] = "0123456789abcdef";
    std::cout.put('"');
    for (std::size_t i = 0; i < value.size(); ++i)
    {
        const unsigned char c = value[i];
        if (c == '"' || c == '\\')
            std::cout << '\\' << static_cast<char>(c);
        else if (c < 0x20)
            std::cout << "\\u00" << hex[c >> 4] << hex[c & 15];
        else if (c < 0x80)
            std::cout.put(static_cast<char>(c));
        else
        {
            const std::size_t n = c >= 0xc2 && c <= 0xdf ? 2 :
                                  c >= 0xe0 && c <= 0xef ? 3 :
                                  c >= 0xf0 && c <= 0xf4 ? 4 : 0;
            bool valid = n != 0 && n <= value.size() - i;
            for (std::size_t j = 1; valid && j < n; ++j)
            {
                const unsigned char next = value[i + j];
                valid = next >= 0x80 && next <= 0xbf;
                if (j == 1 && ((c == 0xe0 && next < 0xa0) || (c == 0xed && next >= 0xa0)
                               || (c == 0xf0 && next < 0x90) || (c == 0xf4 && next >= 0x90)))
                    valid = false;
            }
            if (valid)
            {
                std::cout.write(value.data() + i, n);
                i += n - 1;
            }
            else
                std::cout << "\\ufffd";
        }
    }
    std::cout.put('"');
}

void print(const Report &result)
{
    std::cout << "{\"schema_version\":1,\"valid\":" << (result.invalid ? "false" : "true")
              << ",\"diagnostics\":[";
    bool first = true;
    for (const auto &item : result.diagnostics)
    {
        if (!first) std::cout.put(',');
        first = false;
        std::cout << "{\"severity\":";
        jsonString(item.severity);
        std::cout << ",\"code\":";
        jsonString(item.code);
        std::cout << ",\"message\":";
        jsonString(item.message);
        if (!item.path.empty())
        {
            std::cout << ",\"path\":";
            jsonString(item.path);
        }
        if (item.line != 0) std::cout << ",\"line\":" << item.line;
        if (!item.field.empty())
        {
            std::cout << ",\"field\":";
            jsonString(item.field);
        }
        std::cout.put('}');
    }
    std::cout << "]}\n";
}

class Resources
{
public:
    bool check(std::string &path, const char *dir, const std::string &field,
               const char *missingSeverity = "error")
    {
        if (path.empty()) return false;
        if (findFile(path, dir, false)) return true;
        if (missingSeverity != nullptr && missing_.emplace(dir, path, missingSeverity).second)
            add(missingSeverity, "missing_file", "Cannot find a readable file in the scene search path.",
                path, field);
        return false;
    }

    void file(std::string path, const char *dir, const std::string &field)
    {
        check(path, dir, field);
    }

    void image(std::string path, const std::string &field, const char *missingSeverity = "error")
    {
        if (!check(path, "images", field, missingSeverity) || !images_.insert(path).second)
            return;
        Image image;
        if (!image.Read(path.c_str()))
            add("error", "invalid_image", "The renderer could not decode this image.", path, field);
    }

private:
    std::set<std::tuple<std::string, std::string, std::string>> missing_;
    std::set<std::string> images_;
};

bool containsOrbit(const std::vector<Satellite> &orbits, int id)
{
    return std::any_of(orbits.begin(), orbits.end(),
                       [id](const Satellite &orbit) { return orbit.getID() == id; });
}

// The renderer's row processing can produce orbit trails and open output
// files. Reuse its lexical parser here, without executing any row actions.
void validateSatelliteMarkers(Resources &resources, std::string path,
                              const std::vector<Satellite> &orbits)
{
    if (!resources.check(path, "satellites", "earth.satellite_file")) return;
    std::ifstream file(path.c_str());
    char line[MAX_LINE_LENGTH];
    std::size_t number = 0;
    while (nextAnnotationRecord(file, line, sizeof(line), path, number))
    {
        const Location location(path, number);
        int i = 0;
        while (!isEndOfLine(line[i]) && isDelimiter(line[i])) ++i;
        if (isEndOfLine(line[i])) continue;

        bool sawId = false;
        while (static_cast<std::size_t>(i) < std::strlen(line) && !isEndOfLine(line[i]))
        {
            char *value = nullptr;
            const int kind = parse(i, line, value);
            const std::unique_ptr<char[]> token(value);
            if (kind == ENDOFLINE) break;
            if (kind == LATLON)
            {
                int id;
                if (!parseSatelliteCatalogId(value, id)) continue; // Ignore extra options, like rendering.
                sawId = true;
                if (!containsOrbit(orbits, id))
                    add("error", "unknown_satellite", "No configured orbit records contain catalog ID "
                        + std::to_string(id) + ".", path, "earth.satellite_file", number);
            }
            else if (kind == IMAGE && value && std::string(value) != "none")
                resources.image(value, "earth.satellite_file.image");
#ifdef HAVE_LIBFREETYPE
            else if (kind == FONT && value)
                resources.file(value, "fonts", "earth.satellite_file.font");
#endif
        }
        if (!sawId)
            add("error", "invalid_satellite", "Satellite marker row does not specify a valid catalog ID.",
                path, "earth.satellite_file", number);
    }
}

void validateConfig(Resources &resources, const Options &options,
                    PlanetProperties *properties[], std::vector<Satellite> &orbits)
{
    std::set<std::string> satelliteFiles;
    for (int i = SUN; i < RANDOM_BODY; ++i)
    {
        const auto &p = *properties[i];
        const std::string name = std::string(body_string[i]) + ".";
        // A missing built-in map is a legitimate solid-color planet. A
        // configured map must be readable even for bodies outside this view.
        if (p.DayMapExplicit() && p.DayMap().empty())
            add("error", "missing_file", "Set a readable day map or use map=none.", {}, name + "map");
        else if (p.DayMap() != "none" && (p.DayMapExplicit() || i == options.Target()))
            resources.image(p.DayMap(), name + "map", p.DayMapExplicit() ? "error" : nullptr);

        if (p.Shade() < 1) resources.image(p.NightMap(), name + "night_map", "warning");
        resources.image(p.BumpMap(), name + "bump_map", "warning");
        resources.image(p.CloudMap(), name + "cloud_map", "warning");
        resources.image(p.SpecularMap(), name + "specular_map", "warning");
        resources.file(p.RayleighFile(), "scattering", name + "rayleigh_file");

#ifdef HAVE_LIBFREETYPE
        resources.file(p.MarkerFont(), "fonts", name + "marker_font");
#endif
        for (const auto &path : p.ArcFiles()) resources.file(path, "arcs", name + "arc_file");
        for (const auto &path : p.MarkerFiles()) resources.file(path, "markers", name + "marker_file");
        if (i != EARTH) continue;
        for (const auto &path : p.SatelliteFiles())
        {
            if (!satelliteFiles.insert(path).second) continue;
            const Location location(path);
            readSatelliteOrbitFile(path, orbits); // The shared reader supplies diagnostics.
        }
    }
    for (const auto &path : satelliteFiles) validateSatelliteMarkers(resources, path, orbits);
}

void checkOrbit(const std::vector<Satellite> &orbits, body object, int id, const char *field)
{
    if (object != NORAD) return;
    if (!containsOrbit(orbits, id))
        add("error", "unknown_satellite", "No configured orbit records contain catalog ID "
            + std::to_string(id) + ".", {}, field);
}

void validateOptions(Resources &resources, const Options &options,
                     const std::vector<Satellite> &orbits)
{
    for (const auto &path : options.ArcFiles()) resources.file(path, "arcs", "-arc_file");
    for (const auto &path : options.MarkerFiles()) resources.file(path, "markers", "-marker_file");
    if (!options.Background().empty())
    {
        unsigned char rgb[3];
        std::string error;
        parseColor(options.Background(), rgb, error);
        if (!error.empty()) resources.image(options.Background(), "-background");
    }
#ifdef HAVE_LIBFREETYPE
    if (options.Font() != defaultFont)
        resources.file(options.Font(), "fonts", "-font");
#endif
    if (options.getStarMap() != defaultStarMap)
        resources.file(options.getStarMap(), "stars", "-starmap");

    std::string jplFile = options.JPLFile();
    if (resources.check(jplFile, "ephemeris", "-jpl_file"))
    {
        const Location location(jplFile);
        try { EphemerisHigh ephemeris(jplFile); }
        catch (const ParseFailure &) {}
    }

    std::string originFile = options.OriginFile();
    if (resources.check(originFile, "origin", "-origin_file"))
    {
        const Location location(originFile);
        std::vector<LBRPoint> records;
        try { readOriginFile(originFile, records, options.InterpolateOriginFile()); }
        catch (const ParseFailure &) {}
    }

    std::string dynamicOrigin = options.DynamicOrigin();
    if (resources.check(dynamicOrigin, "origin", "-dynamic_origin"))
    {
        const Location location(dynamicOrigin);
        LBRPoint last;
        try { readDynamicOrigin(dynamicOrigin, last); }
        catch (const ParseFailure &) {}
    }

    resources.file(options.RayleighFile(), "scattering", "-create_scattering_tables");
    for (const auto &path : options.SpiceFiles())
    {
        resources.file(path, "spice", "-spice_file");
        resources.file(path + ".krn", "spice", "-spice_file");
    }
    checkOrbit(orbits, options.Origin(), options.OriginID(), "-origin");
    checkOrbit(orbits, options.Target(), options.TargetID(), "-target");
    checkOrbit(orbits, options.PathRelativeTo(), options.PathRelativeToID(), "-path_relative_to");

    if (!options.PrevCommand().empty() || !options.PostCommand().empty())
        add("warning", "skipped_hook", "Pre/post-render commands were not run.");
}
} // namespace

bool active() { return report != nullptr; }

Location::Location(const std::string &path, std::size_t line) : enabled_(active())
{
    if (!enabled_) return;
    oldPath_ = report->path;
    oldLine_ = report->line;
    report->path = path;
    report->line = line;
}

Location::~Location()
{
    if (!enabled_) return;
    report->path.swap(oldPath_);
    report->line = oldLine_;
}

void parserWarning(const std::string &message)
{
    add("warning", report->stage, message, report->path, {}, report->line);
}

void requiredResourceError(const std::string &message)
{
    add("error", report->stage, message, report->path, {}, report->line);
}

void satelliteOrbitSource(const std::string &marker, const std::string &path,
                          const char *format)
{
    if (active())
        add("info", "satellite_orbit_source", std::string("Using ") + format
            + " orbital elements for " + marker + ".", path, "earth.satellite_file");
}

[[noreturn]] void parserError(const std::string &message)
{
    requiredResourceError(message);
    throw ParseFailure{};
}

int run(Options &options, int argc, char **argv)
{
    Report result;
    report = &result;
    opterr = 0;
    // Keep argv[0] for getopt diagnostics, but remove the mode selector.
    argv[1] = argv[0];
    try
    {
        options.parseArgs(argc - 1, argv + 1);
        result.stage = "config";
        bool configParsed = true;
        std::unique_ptr<PlanetProperties> owners[RANDOM_BODY];
        PlanetProperties *properties[RANDOM_BODY];
        for (int i = SUN; i < RANDOM_BODY; ++i)
        {
            owners[i].reset(new PlanetProperties(static_cast<body>(i)));
            properties[i] = owners[i].get();
        }
        try { readConfigFile(options.ConfigFile(), properties); }
        catch (const ParseFailure &) { configParsed = false; }

        result.stage = "resource";
        Resources resources;
        std::vector<Satellite> orbits;
        if (configParsed) validateConfig(resources, options, properties, orbits);
        validateOptions(resources, options, orbits);
    }
    catch (const ParseFailure &) {}
    report = nullptr;
    print(result);
    return result.invalid ? EXIT_FAILURE : EXIT_SUCCESS;
}
} // namespace SceneValidation
