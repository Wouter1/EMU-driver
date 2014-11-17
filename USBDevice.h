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
	USBDevice.h
	
=============================================================================*/

#ifndef __USBDevice_h__
#define __USBDevice_h__

#include <CoreServices/CoreServices.h>	// we need Debugging.h, CF, etc.
#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/usb/USB.h>

class USBInterface;
class USBPipe;
enum {
	kMS_DESC_UNDEF,
	kMS_HEADER		= 0x01,
	kMIDI_IN_JACK	= 0x02,
	kMIDI_OUT_JACK	= 0x03,
	kELEMENT		= 0x04
};

enum {
	kJACK_TYPE_UNDEF,
	kEMBEDDED,
	kEXTERNAL
};

typedef struct MSInterfaceDescriptor {
	UInt8	bLength;				// size of this descriptor 7 bytes
	UInt8	bDescriptorType;		// CS_INTERFACE descriptor type
	UInt8	bDescriptorSubtype;		// MS_HEADER descriptor subtype
	UInt16	bcdMSC;					// MIDIStreaming subclass specification
	UInt16	wTotalLength;			// total length
} MSInterfaceDescriptor, *MSInterfaceDescriptorPtr;

typedef struct MIDIitemDescHeader {
	UInt8	bLength;				// length of this descriptor 6 bytes
	UInt8	bDescriptorType;		// CS_INTERFACE descriptor type
	UInt8	bDescriptorSubtype;		// Element or MIDI_IN_JACK or MIDI_OUT_JACK descriptor subtype
	UInt8	bJackType;				// EMBEDDED OR EXTERNAL
} MIDIitemDescHeader, *MIDIitemDescHeaderPtr;

typedef struct MIDIJackDescHeader {
	UInt8	bLength;				// length of this descriptor 6 bytes
	UInt8	bDescriptorType;		// CS_INTERFACE descriptor type
	UInt8	bDescriptorSubtype;		// MIDI_IN_JACK or MIDI_OUT_JACK descriptor subtype
	UInt8	bJackType;				// EMBEDDED OR EXTERNAL
	UInt8	bJackID;				// unique constant id
} MIDIJackDescHeader, *MIDIJackDescHeaderPtr;


// *** note that all descriptors are in bus-endianism and should be accessed using the
// *** endian macros in IOKit/usb/USB.h (USBToHostWord etc.)

// _________________________________________________________________________________________
//	USBDevice
//
//	Wraps everything we know about a device
class USBDevice {
public:
							USBDevice(		io_service_t			ioDeviceObj);
								// retains its own reference

							~USBDevice();
	
	io_service_t			GetIOService() const { return mIOService; }
	
	IOUSBDeviceInterface **	GetPluginInterface();
								// lazily initializes
	UInt32 GetTotalMIDIPortCount();
	UInt32 GetMIDIOutPortCount() { return mOutputJackCount;}
	UInt32 GetMIDIInPortCount() {return mInputJackCount;}
	const IOUSBDeviceDescriptor *
							GetDeviceDescriptor();
								// lazily initializes - if this succeeds, GetPluginInterface
								// will also have succeeded
	
	bool					Usable() { return GetDeviceDescriptor() != NULL; }
	
	bool					OpenAndConfigure(UInt8					configIndex);
								// return true for success

	bool					GetCompositeConfiguration();
								// use this to initialize the config descriptor
								// in a composite device or other situation where
								// another driver has already opened the device
								// return true for success

	const IOUSBConfigurationDescriptor *
							GetConfigDescriptor() { return mConfigDescPtr; }
								// not valid until device opened
	
	CFStringRef				GetString(		UInt8					stringIndex);
	
	int						LoadDescriptor(	UInt8					descType,
											UInt8					descIndex,
											UInt16					wIndex,
											void *					buf,
											UInt16					len);
								// returns descriptor length

	USBInterface *			FindInterface(	UInt8					interfaceNumber,
											UInt8					alternateSetting);
								// returns newly-allocated object

	USBInterface *			FindInterface(	IOUSBFindInterfaceRequest &intfRequest);
								// returns newly-allocated object

	UInt32					GetLocationID() { return mLocationID; }

private:
	io_service_t					mIOService;
	IOUSBDeviceInterface **			mPluginIntf;
	UInt32							mLocationID;	// redundant with mDeviceDesc, but 
													// useful for debugging
	IOUSBDeviceDescriptor			mDeviceDesc;
	bool							mIsOpen;
	IOUSBConfigurationDescriptor *	mConfigDescPtr;
	UInt32							mInputJackCount;
	UInt32							mOutputJackCount;
	
	UInt16							mLangID;
	UInt16							mNumLangIDs;
	UInt16 *						mLangIDs;
};

// _________________________________________________________________________________________
//	USBInterface
//
class USBInterface {
public:
							USBInterface(	USBDevice *				device,
											io_service_t			ioInterfaceObj);
								// retains its own reference to the io_service_t
							~USBInterface();

	IOUSBInterfaceInterface ** GetPluginInterface();
								// lazily initializes
	
	IOUSBInterfaceDescriptor *
							GetInterfaceDescriptor() { return mInterfaceDescPtr; }
	
	bool					Usable() { return GetInterfaceDescriptor() != NULL; }
	
	bool					Open();
								// return true for success
	
	USBDevice *				GetDevice() { return mDevice; }
	IOReturn				GetPipe(int pipeIndex, USBPipe &pipe);
	
	void					DisownDevice() { mCreatedDevice = false; }

private:
	USBDevice *						mDevice;
	io_service_t					mIOService;
	IOUSBInterfaceInterface **		mPluginIntf;
	bool							mCreatedDevice;		// if true, we own mDevice and must dispose it
	bool							mIsOpen;
	IOUSBInterfaceDescriptorPtr		mInterfaceDescPtr;	// points into device's configuration desc.
};

// _________________________________________________________________________________________
//	USBPipe
//
class USBPipe {
public:
	USBPipe() : mPipeIndex(UInt8(kUSBNoPipeIdx)), mMaxPacketSize(0) { }

	UInt8			mPipeIndex;		// used for I/O

	// information returned from GetPipeProperties
	UInt8	   		mPipeNum, mDirection, mTransferType, mInterval;
	UInt16			mMaxPacketSize;
};

#endif // __USBDevice_h__
