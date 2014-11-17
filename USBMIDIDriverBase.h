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
	USBMIDIDriverBase.h
	
=============================================================================*/

#ifndef __USBMIDIDriverBase_h__
#define __USBMIDIDriverBase_h__

#include "USBDeviceManager.h"
#include "USBMIDIDevice.h"
#include "MIDIDriverClass.h"
#include "MIDIDriverUtils.h"
#include <vector>
#include <list>
#include <pthread.h>

// some Apple-defined properties useful for USB drivers to attach to their devices
#define kUSBLocationProperty		CFSTR("USBLocationID")
#define kUSBVendorProductProperty	CFSTR("USBVendorProduct")
#define kSerialNumberProperty		CFSTR("SerialNumber")

// _________________________________________________________________________________________
// USBMIDIDriverBase
//
// MIDIDriver subclass, behavior common to a class driver or a vendor-specific driver.
class USBMIDIDriverBase : public MIDIDriver {
public:
	USBMIDIDriverBase(CFUUIDRef factoryID);
	
	// overrides of MIDIDriver methods
	virtual OSStatus		Send(					const MIDIPacketList *pktlist,
													void *			endptRef1,
													void *			endptRef2);
	
	// our abstract methods - required overrides
	virtual bool			MatchDevice(			USBDevice *		inUSBDevice) = 0;

	virtual MIDIDeviceRef	CreateDevice(			USBDevice *		inUSBDevice,
													USBInterface *	inUSBInterface) = 0;

	virtual void			PreExistingDeviceFound(	MIDIDeviceRef	inMIDIDevice,
													USBDevice *		inUSBDevice,
													USBInterface *	inUSBInterface) { }

	virtual USBInterface *	CreateInterface(		USBMIDIDevice *	inDevice) = 0;
	
	virtual void			StartInterface(			USBMIDIDevice *	usbmDev) = 0;
								// pipes are opened, do any extra initialization (send config msgs etc)
							
	virtual void			StopInterface(			USBMIDIDevice *	usbmDev) = 0;
								// pipes are about to be closed, do any preliminary cleanup
							
	virtual void			HandleInput(			USBMIDIDevice *	usbmDev,
													MIDITimeStamp 	when,
													Byte *			readBuf,
													ByteCount		readBufSize) = 0;
								// a USB message arrived, parse it into a MIDIPacketList and
								// call MIDIReceived

	virtual ByteCount		PrepareOutput(			USBMIDIDevice *	usbmDev,
													WriteQueue &	writeQueue,
													Byte *			destBuf) = 0;
								// dequeue from WriteQueue into a single USB message, return
								// length of the message.  Called with the queue mutex locked.

	virtual USBMIDIDevice *	CreateUSBMIDIDevice(	USBDevice *		inUSBDevice,
													USBInterface *	inUSBInterface,
													MIDIDeviceRef	inMIDIDevice);
								// may override to create a subclass
	
	// Utilities to implement the USB MIDI class spec methods of encoding MIDI in USB packets
	static void				USBMIDIHandleInput(		USBMIDIDevice *	usbmDev, 
													MIDITimeStamp	when,
													Byte *			readBuf,
													ByteCount		bufSize);

	static ByteCount		USBMIDIPrepareOutput(	USBMIDIDevice *	usbmDev,
													WriteQueue &	writeQueue, 
													Byte *			destBuf,
													ByteCount 		bufSize);

};


#endif // __USBMIDIDriverBase_h__
