/*
 File:  DesktopPicture.m

 Description:  This file contains the SetDesktopPicture function, which tells the Finder to set a given picture as the desktop picture.

 Author:  MCF

 Copyright:    Copyright 2002 Apple Computer, Inc. All rights reserved.

 Disclaimer: IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc.
 ("Apple") in consideration of your agreement to the following terms, and your
 use, installation, modification or redistribution of this Apple software
 constitutes acceptance of these terms.  If you do not agree with these terms,
 please do not use, install, modify or redistribute this Apple software.

 In consideration of your agreement to abide by the following terms, and subject
 to these terms, Apple grants you a personal, non-exclusive license, under Apple's
 copyrights in this original Apple software (the "Apple Software"), to use,
 reproduce, modify and redistribute the Apple Software, with or without
 modifications, in source and/or binary forms; provided that if you redistribute
 the Apple Software in its entirety and without modifications, you must retain
 this notice and the following text and disclaimers in all such redistributions of
 the Apple Software.  Neither the name, trademarks, service marks or logos of
 Apple Computer, Inc. may be used to endorse or promote products derived from the
 Apple Software without specific prior written permission from Apple.  Except as
 expressly stated in this notice, no other rights or licenses, express or implied,
 are granted by Apple herein, including but not limited to any patent rights that
 may be infringed by your derivative works or by other works in which the Apple
 Software may be incorporated.

 The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO
 WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
 WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR IN
 COMBINATION WITH YOUR PRODUCTS.

 IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR
 CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
                        GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION
 OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT
 (INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN
 ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 Version History:
 
 1.0 - 03/2002 Initial Release

 */


#import <Cocoa/Cocoa.h>
#include <Carbon/Carbon.h>

// Keep the picture available until Finder has processed the request, including
// any first-use macOS automation authorization.
static OSStatus SetDesktopPicture(NSString *picturePath)
{
    AEDesc pictureDesc = {typeNull, nil};
    AppleEvent event = {typeNull, nil};
    AppleEvent reply = {typeNull, nil};
    AliasHandle aliasHandle = nil;
    FSRef pictureRef;
    OSStatus status = paramErr;

    if (picturePath != nil)
        status = FSPathMakeRef([picturePath fileSystemRepresentation],
                               &pictureRef, NULL);

    if (status == noErr)
        status = FSNewAlias(nil, &pictureRef, &aliasHandle);
    if (status == noErr && aliasHandle == nil)
        status = paramErr;

    if (status == noErr)
    {
        char handleState = HGetState((Handle) aliasHandle);
        HLock((Handle) aliasHandle);
        status = AECreateDesc(typeAlias, *aliasHandle,
                              GetHandleSize((Handle) aliasHandle), &pictureDesc);
        HSetState((Handle) aliasHandle, handleState);
    }
    if (aliasHandle != nil)
        DisposeHandle((Handle) aliasHandle);

    if (status == noErr)
    {
        OSType signature = 'MACS';
        AEBuildError buildError;
        status = AEBuildAppleEvent(kAECoreSuite, kAESetData, typeApplSignature,
                                  &signature, sizeof(signature),
                                  kAutoGenerateReturnID, kAnyTransactionID,
                                  &event, &buildError,
                                  "'----':'obj '{want:type(prop),form:prop,seld:type('dpic'),from:'null'()},data:(@)",
                                  &pictureDesc);
    }
    if (status == noErr)
    {
        // AppleEvent timeouts are in 1/60-second ticks. A timeout is not proof
        // that Finder abandoned the request; the caller retains the image.
        status = AESend(&event, &reply, kAEWaitReply, kAENormalPriority,
                        120 * 60, nil, nil);
        if (status == noErr)
        {
            SInt32 finderError = noErr;
            DescType actualType;
            Size actualSize;
            OSErr replyStatus = AEGetParamPtr(&reply, keyErrorNumber, typeSInt32,
                                             &actualType, &finderError,
                                             sizeof(finderError), &actualSize);
            if (replyStatus == noErr)
                status = (actualType == typeSInt32 &&
                          actualSize == sizeof(finderError)) ? finderError : paramErr;
            else if (replyStatus != errAEDescNotFound)
                status = replyStatus;
        }
    }

    AEDisposeDesc(&reply);
    AEDisposeDesc(&event);
    AEDisposeDesc(&pictureDesc);
    return status;
}

/*
  This function written by Hari Nair (hari@alumni.caltech.edu) and
  distributed with Xplanet (http://xplanet.sourceforge.net)
 */
OSStatus SetDesktopPictureFromCharString(const char *file)
{
     NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

     NSString *filePath = [NSString stringWithUTF8String: file];

     OSStatus err = SetDesktopPicture(filePath);
     
     [pool release];

     return err;
}
