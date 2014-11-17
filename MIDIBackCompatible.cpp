/*	Copyright: 	© Copyright 2005 Apple Computer, Inc. All rights reserved.

	Disclaimer:	IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc.
			("Apple") in consideration of your agreement to the following terms, and your
			use, installation, modification or redistribution of this Apple software
			constitutes acceptance of these terms.  If you do not agree with these terms,
			please do not use, install, modify or redistribute this Apple software.

			In consideration of your agreement to abide by the following terms, and subject
			to these terms, Apple grants you a personal, non-exclusive license, under Apple’s
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
*/
/*=============================================================================
	MIDIBackCompatible.cpp
	
=============================================================================*/

#define MIDI_WEAK_SYM			// NOT "extern" - gets our global function pointers defined
#include "MIDIDriverClass.h"

#if MIDI_WEAK_LINK_TO_V2_CALLS

#include <mach-o/dyld.h>

static void *LinkSymbol(const char *symname, const char *fwhint)
{
	return NSAddressOfSymbol(NSLookupAndBindSymbolWithHint(symname, fwhint));
}

#define LINK(typ, name) \
	g_##name = (typ)LinkSymbol("_"#name, frameworkName);

// caution: this will fail (exiting program) if symbols are unresolved
void	InitMIDIWeakLinks()
{
	static bool didit = false;
	static const char *frameworkName = "CoreMIDIServer";
	
	if (didit) return;
	
	LINK(OSStatus(*)(MIDIDeviceListRef), MIDIDeviceListDispose);
	LINK(OSStatus(*)(MIDIDeviceRef, MIDIEntityRef), MIDIDeviceRemoveEntity);
	LINK(OSStatus(*)(MIDIDriverRef, Boolean), MIDIDriverEnableMonitoring);
	LINK(OSStatus(*)(MIDIEndpointRef), MIDIFlushOutput);
	LINK(MIDIDeviceListRef(*)(MIDIDriverRef), MIDIGetDriverDeviceList);
	LINK(MIDIDeviceRef(*)(ItemCount), MIDIGetExternalDevice);
	LINK(ItemCount(*)(), MIDIGetNumberOfExternalDevices);
	LINK(OSStatus(*)(CFArrayRef *), MIDIGetSerialPortDrivers);
	LINK(OSStatus(*)(CFStringRef, CFStringRef *), MIDIGetSerialPortOwner);
	LINK(OSStatus(*)(MIDIObjectRef, CFPropertyListRef *, Boolean), MIDIObjectGetProperties);
	LINK(OSStatus(*)(), MIDIRestart);
	LINK(OSStatus(*)(CFStringRef, CFStringRef), MIDISetSerialPortOwner);
	LINK(OSStatus(*)(MIDIDeviceRef), MIDISetupAddDevice);
	LINK(OSStatus(*)(MIDIDeviceRef), MIDISetupAddExternalDevice);
	LINK(OSStatus(*)(MIDIDeviceRef), MIDISetupRemoveDevice);
	LINK(OSStatus(*)(MIDIDeviceRef), MIDISetupRemoveExternalDevice);
	LINK(const CFStringRef *, kMIDIPropertyConnectionUniqueID);
	LINK(const CFStringRef *, kMIDIPropertyDriverOwner);
	LINK(const CFStringRef *, kMIDIPropertyFactoryPatchNameFile);
	LINK(const CFStringRef *, kMIDIPropertyIsEmbeddedEntity);
	LINK(const CFStringRef *, kMIDIPropertyOffline);
	LINK(const CFStringRef *, kMIDIPropertyUserPatchNameFile);
	
	didit = true;
}

#endif // MIDI_WEAK_LINK_TO_V2_CALLS
