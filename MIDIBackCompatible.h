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
	MIDIBackCompatible.h
	
=============================================================================*/

#ifndef __MIDIBackCompatible_h__
#define __MIDIBackCompatible_h__

#ifndef MIDI_WEAK_SYM
	#define MIDI_WEAK_SYM extern
#endif

MIDI_WEAK_SYM void	InitMIDIWeakLinks();

// We have to use dyld calls to manually link to CoreMIDIServer's new
// calls that are present in v2 but not v1.  These macros make that
// underlying manual linkage transparent to the client.
#define MIDIDeviceListDispose                    (*g_MIDIDeviceListDispose)
#define MIDIDeviceRemoveEntity                   (*g_MIDIDeviceRemoveEntity)
#define MIDIDriverEnableMonitoring               (*g_MIDIDriverEnableMonitoring)
#define MIDIFlushOutput                          (*g_MIDIFlushOutput)
#define MIDIGetDriverDeviceList                  (*g_MIDIGetDriverDeviceList)
#define MIDIGetExternalDevice                    (*g_MIDIGetExternalDevice)
#define MIDIGetNumberOfExternalDevices           (*g_MIDIGetNumberOfExternalDevices)
#define MIDIGetSerialPortDrivers                 (*g_MIDIGetSerialPortDrivers)
#define MIDIGetSerialPortOwner                   (*g_MIDIGetSerialPortOwner)
#define MIDIObjectGetProperties                  (*g_MIDIObjectGetProperties)
#define MIDIRestart                              (*g_MIDIRestart)
#define MIDISetSerialPortOwner                   (*g_MIDISetSerialPortOwner)
#define MIDISetupAddDevice                       (*g_MIDISetupAddDevice)
#define MIDISetupAddExternalDevice               (*g_MIDISetupAddExternalDevice)
#define MIDISetupRemoveDevice                    (*g_MIDISetupRemoveDevice)
#define MIDISetupRemoveExternalDevice            (*g_MIDISetupRemoveExternalDevice)
#define kMIDIPropertyConnectionUniqueID          (*g_kMIDIPropertyConnectionUniqueID)
#define kMIDIPropertyDriverOwner                 (*g_kMIDIPropertyDriverOwner)
#define kMIDIPropertyFactoryPatchNameFile        (*g_kMIDIPropertyFactoryPatchNameFile)
#define kMIDIPropertyIsEmbeddedEntity            (*g_kMIDIPropertyIsEmbeddedEntity)
#define kMIDIPropertyOffline                     (*g_kMIDIPropertyOffline)
#define kMIDIPropertyUserPatchNameFile           (*g_kMIDIPropertyUserPatchNameFile)

MIDI_WEAK_SYM OSStatus             (*g_MIDIDeviceListDispose)(MIDIDeviceListRef devList);
MIDI_WEAK_SYM OSStatus             (*g_MIDIDeviceRemoveEntity)(MIDIDeviceRef device, MIDIEntityRef entity);
MIDI_WEAK_SYM OSStatus             (*g_MIDIDriverEnableMonitoring)(MIDIDriverRef driver, Boolean enabled);
MIDI_WEAK_SYM OSStatus             (*g_MIDIFlushOutput)(MIDIEndpointRef dest);
MIDI_WEAK_SYM MIDIDeviceListRef    (*g_MIDIGetDriverDeviceList)(MIDIDriverRef driver);
MIDI_WEAK_SYM MIDIDeviceRef        (*g_MIDIGetExternalDevice)(ItemCount deviceIndex0);
MIDI_WEAK_SYM ItemCount            (*g_MIDIGetNumberOfExternalDevices)();
MIDI_WEAK_SYM OSStatus             (*g_MIDIGetSerialPortDrivers)(CFArrayRef *outDriverNames);
MIDI_WEAK_SYM OSStatus             (*g_MIDIGetSerialPortOwner)(CFStringRef portName, CFStringRef *outDriverName);
MIDI_WEAK_SYM OSStatus             (*g_MIDIObjectGetProperties)(MIDIObjectRef obj, CFPropertyListRef *outProperties, Boolean deep);
MIDI_WEAK_SYM OSStatus             (*g_MIDIRestart)();
MIDI_WEAK_SYM OSStatus             (*g_MIDISetSerialPortOwner)(CFStringRef portName, CFStringRef driverName);
MIDI_WEAK_SYM OSStatus             (*g_MIDISetupAddDevice)(MIDIDeviceRef device);
MIDI_WEAK_SYM OSStatus             (*g_MIDISetupAddExternalDevice)(MIDIDeviceRef device);
MIDI_WEAK_SYM OSStatus             (*g_MIDISetupRemoveDevice)(MIDIDeviceRef device);
MIDI_WEAK_SYM OSStatus             (*g_MIDISetupRemoveExternalDevice)(MIDIDeviceRef device);
MIDI_WEAK_SYM const CFStringRef    *g_kMIDIPropertyConnectionUniqueID;
MIDI_WEAK_SYM const CFStringRef    *g_kMIDIPropertyDriverOwner;
MIDI_WEAK_SYM const CFStringRef    *g_kMIDIPropertyFactoryPatchNameFile;
MIDI_WEAK_SYM const CFStringRef    *g_kMIDIPropertyIsEmbeddedEntity;
MIDI_WEAK_SYM const CFStringRef    *g_kMIDIPropertyOffline;
MIDI_WEAK_SYM const CFStringRef    *g_kMIDIPropertyUserPatchNameFile;


#endif // __BackCompatible_h__
