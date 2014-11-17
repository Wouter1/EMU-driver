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
	USBDevice.cpp
	
=============================================================================*/

#include "USBDevice.h"
#include <IOKit/IOCFPlugIn.h>
#include <unistd.h>

#if DEBUG
//#define VERBOSE 1
#endif

// _________________________________________________________________________________________
//	USBDevice::USBDevice
//
USBDevice::USBDevice(io_service_t ioDeviceObj) :
	mIOService(ioDeviceObj),
	mPluginIntf(NULL),
	mLocationID(0),
	mIsOpen(false),
	mConfigDescPtr(NULL),
	mLangID(0),
	mNumLangIDs(0),
	mLangIDs(NULL)
{
	IOObjectRetain(mIOService);
	mDeviceDesc.bLength = 0;		// signals that we don't have it
}

// _________________________________________________________________________________________
//	USBDevice::~USBDevice
//
USBDevice::~USBDevice()
{
	if (mIsOpen)
		verify_noerr((*mPluginIntf)->USBDeviceClose(mPluginIntf));
	if (mPluginIntf != NULL)
		(*mPluginIntf)->Release(mPluginIntf);
	IOObjectRelease(mIOService);
	
	delete[] mLangIDs;
}

// _________________________________________________________________________________________
//	USBDevice::GetPluginInterface
//
IOUSBDeviceInterface **	USBDevice::GetPluginInterface()
{
	if (mPluginIntf == NULL) {
		// Get plugin interface to device
		IOCFPlugInInterface 	**ioPlugin;
		IOReturn	 			kr;
		SInt32 					score;

		kr = IOCreatePlugInInterfaceForService(
			mIOService, kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID, 
			&ioPlugin, &score);

		if (kr) {
			usleep(100 * 1000);	// wait 100 ms
	
			require_noerr(IOCreatePlugInInterfaceForService(
				mIOService, kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID, 
				&ioPlugin, &score), errexit);
		}
		kr = (*ioPlugin)->QueryInterface(ioPlugin, CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID), (LPVOID *)&mPluginIntf);
		(*ioPlugin)->Release(ioPlugin);
		ioPlugin = NULL;
		if (kr) {
			debug_string("QueryInterface failed");
			mPluginIntf = NULL;
		} else {
			verify_noerr((*mPluginIntf)->GetLocationID(mPluginIntf, &mLocationID));
#if VERBOSE
			printf("device @ 0x%08lX\n", mLocationID);
#endif
		}
	}
errexit:
	return mPluginIntf;
}


// _________________________________________________________________________________________
//	USBDevice::GetDescriptor
//
const IOUSBDeviceDescriptor *	USBDevice::GetDeviceDescriptor()
{
	// already loaded?
	if (mDeviceDesc.bLength != 0)
		return &mDeviceDesc;
	
	{	// this is for 2771050
		IOUSBDeviceInterface **deviceIntf = GetPluginInterface();
		if (deviceIntf == NULL)
			goto errexit;
	
		UInt8 devClass;
		require_noerr((*deviceIntf)->GetDeviceClass(deviceIntf, &devClass), errexit);
		if (devClass == kUSBHubClass)
			goto errexit;
	}
	
	if (LoadDescriptor(kUSBDeviceDesc, 0, 0, &mDeviceDesc, sizeof(mDeviceDesc)) < 0) {
#if DEBUG
		printf("device @ 0x%08lX - can't get descriptor\n", mLocationID);
#endif
		goto errexit;
	}
	
	{
		Byte buf[256];
		int len = LoadDescriptor(kUSBStringDesc, 0, 0, buf, sizeof(buf));
		if (len > 2) {
			len = buf[0];	// use actual descriptor length, not number of bytes returned
			mNumLangIDs = (len - 2) / 2;
			mLangIDs = new UInt16[mNumLangIDs];
			for (UInt16 i = 0; i < mNumLangIDs; ++i)
				mLangIDs[i] = USBToHostWord(*(UInt16 *)(buf + 2 + 2*i));
			mLangID = mLangIDs[0];
		}
	}
	
	return &mDeviceDesc;
errexit:
	return NULL;
}

// _________________________________________________________________________________________
//	USBDevice::OpenAndConfigure
//
bool		USBDevice::OpenAndConfigure(UInt8 configIndex)
{
	require(Usable(), errexit);

	// Get a pointer to the configuration descriptor
	require_noerr((*mPluginIntf)->GetConfigurationDescriptorPtr(mPluginIntf, configIndex, &mConfigDescPtr), errexit);

	// Open the device
	if (!mIsOpen) {
		require_noerr((*mPluginIntf)->USBDeviceOpen(mPluginIntf), errexit);
		mIsOpen = true;
	}

	// Get the existing config value
	UInt8  oldConfigVal;
	require_noerr((*mPluginIntf)->GetConfiguration(mPluginIntf,&oldConfigVal), errexit);
	
	// if the new value and old value are different, set the new configuration	
	if (oldConfigVal != mConfigDescPtr->bConfigurationValue) {
		require_noerr((*mPluginIntf)->SetConfiguration(mPluginIntf, mConfigDescPtr->bConfigurationValue), errexit);
	}
	mInputJackCount = mOutputJackCount = 0;
	return true;
errexit:
	return false;
}

// _________________________________________________________________________________________
//	USBDevice::GetCompositeConfiguration
//
bool		USBDevice::GetCompositeConfiguration()
{
	if (!Usable())	// don't use require; it would generate a redundant debug message
		goto errexit;

	if (mConfigDescPtr == NULL) {
		// for now we're assuming configuration index 0
		require_noerr((*mPluginIntf)->GetConfigurationDescriptorPtr(mPluginIntf, 0, &mConfigDescPtr), errexit);
	}
	return true;

errexit:
	return false;
}


// _________________________________________________________________________________________
//	USBDevice::GetString
//
//	Load a descriptor string from a device.  Caller owns the string reference.
CFStringRef	USBDevice::GetString(		UInt8					stringIndex)
{
	if (stringIndex == 0)
		return NULL;
	
	struct StringDesc {
		UInt8 			bLength;
		UInt8 			bDescriptorType;
		UniChar			chars[127];	// most that can fit w/ 8-bit length
	};
	StringDesc desc;
	
	int len = LoadDescriptor(kUSBStringDesc, stringIndex, mLangID, &desc, sizeof(desc));
	if (len <= 2)
		return NULL;
	
	int nChars = (len-2) / 2;
	for (int i = 0; i < nChars; ++i)
		desc.chars[i] = USBToHostWord(desc.chars[i]);
		
	return CFStringCreateWithCharacters(NULL, desc.chars, nChars);
}

// _________________________________________________________________________________________
//	USBDevice::LoadDescriptor
//
//	Load a descriptor from a device, return its size or -1 if an error occurred.
int		USBDevice::LoadDescriptor(			UInt8					descType,
											UInt8					descIndex,
											UInt16					wIndex,
											void *					buf,
											UInt16					len)
{
	IOUSBDeviceInterface **deviceIntf = GetPluginInterface();
	if (deviceIntf == NULL)
		return -1;

	IOUSBDevRequest req;
	req.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBStandard, kUSBDevice);
	req.bRequest = kUSBRqGetDescriptor;
	req.wValue = (descType << 8) | descIndex;
	req.wIndex = wIndex;
	req.pData = buf;
	
	if (descType == kUSBStringDesc)
		req.wLength = 2;
	else
		req.wLength = len;
	
	IOReturn err;
	err = (*deviceIntf)->DeviceRequest(deviceIntf, &req);
#if DEBUG
	// filter out errors that are apparently normal, before a DebugAssert fires
	if (err == kIOUSBPipeStalled && descType == kUSBStringDesc && descIndex == 0)
		return -1;
#endif
	if (descType != kUSBStringDesc) {
		if (err) {
			check_noerr(err);	// debugging reporting
			return -1;
		}
	} else {
		if (err != kIOReturnSuccess && err != kIOReturnOverrun) {	// overrun is normal for strings
			check_noerr(err);	// debugging reporting
			return -1;
		}
		int stringLen = ((Byte *)buf)[0];
		if (stringLen == 0)
			return 0;	// zero-length string
		// now make request for full length
		req.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBStandard, kUSBDevice);
		req.bRequest = kUSBRqGetDescriptor;
		req.wValue = (descType << 8) | descIndex;
		req.wIndex = wIndex;
		req.wLength = stringLen;
		req.pData = buf;
		verify_noerr(err = (*deviceIntf)->DeviceRequest(deviceIntf, &req));
		if (err)
			return -1;
	}
	
	return req.wLenDone;
}


// _________________________________________________________________________________________
//	USBDevice::FindInterface
//
USBInterface *	USBDevice::FindInterface(	
											UInt8					desiredInterfaceNumber,
											UInt8					desiredAlternateSetting)
{
	IOUSBDeviceInterface **deviceIntf = GetPluginInterface();
	if (deviceIntf == NULL)
		return NULL;
	
	// Create the interface iterator
	IOUSBFindInterfaceRequest 	intfRequest;
	io_service_t				ioInterfaceObj;
	io_iterator_t				intfIter = 0;
	intfRequest.bInterfaceClass		= kIOUSBFindInterfaceDontCare;
	intfRequest.bInterfaceSubClass	= kIOUSBFindInterfaceDontCare;
	intfRequest.bInterfaceProtocol	= kIOUSBFindInterfaceDontCare;
	intfRequest.bAlternateSetting	= desiredAlternateSetting;
	
	require_noerr((*deviceIntf)->CreateInterfaceIterator(deviceIntf, &intfRequest, &intfIter), errexit);

	while ((ioInterfaceObj = IOIteratorNext(intfIter)) != 0) {
		USBInterface *interface = new USBInterface(this, ioInterfaceObj);
		IOUSBInterfaceInterface **intfIntf = interface->GetPluginInterface();
		if (intfIntf != NULL) {
			UInt8 intfNum;
			require_noerr((*intfIntf)->GetInterfaceNumber(intfIntf, &intfNum), nextInterface);
			if (intfNum == desiredInterfaceNumber) {
			printf("Found interface %d\n", intfNum);
				IOObjectRelease(ioInterfaceObj);
				IOObjectRelease(intfIter);
				return interface;
			}
		}
nextInterface:
		IOObjectRelease(ioInterfaceObj);
		delete interface;
	}
	IOObjectRelease(intfIter);

errexit:
	return NULL;
}

// _________________________________________________________________________________________
//	USBDevice::FindInterface
//
USBInterface *	USBDevice::FindInterface(	IOUSBFindInterfaceRequest &intfRequest)
{
	IOUSBDeviceInterface **deviceIntf = GetPluginInterface();
	if (deviceIntf == NULL)
		return NULL;
	
	// Create the interface iterator
	io_service_t				ioInterfaceObj;
	io_iterator_t				intfIter = 0;
	
	require_noerr((*deviceIntf)->CreateInterfaceIterator(deviceIntf, &intfRequest, &intfIter), errexit);

	if ((ioInterfaceObj = IOIteratorNext(intfIter)) != 0) {
		USBInterface *interface = new USBInterface(this, ioInterfaceObj);
		IOObjectRelease(ioInterfaceObj);
		IOObjectRelease(intfIter);
		return interface;
	}
	IOObjectRelease(intfIter);

errexit:
	return NULL;
}

// _________________________________________________________________________________________
//	USBDevice::GetMIDIPortCount
//
UInt32 USBDevice::GetTotalMIDIPortCount() {
	MIDIitemDescHeaderPtr	item = (MIDIitemDescHeaderPtr) ((UInt8*) mConfigDescPtr + mConfigDescPtr->bLength);
	SInt32	bytesLeft = (SInt32) (mConfigDescPtr->wTotalLength - mConfigDescPtr->bLength);
	while (bytesLeft > 0) {
		switch (item->bDescriptorSubtype) {
			case kMIDI_IN_JACK:
				mInputJackCount += (kEMBEDDED == item->bJackType);
				break;
			case kMIDI_OUT_JACK:
				mOutputJackCount += (kEMBEDDED == item->bJackType);
				break;
			default:	// ELEMENT
				break;
		}
		bytesLeft -= item->bLength;
	}
	return mInputJackCount + mOutputJackCount;
}

// _________________________________________________________________________________________
//	USBInterface::USBInterface
//
USBInterface::USBInterface(					USBDevice *				device,
											io_service_t			ioInterfaceObj) :
	mDevice(NULL),
	mIOService(ioInterfaceObj),
	mPluginIntf(NULL),
	mCreatedDevice(false),
	mIsOpen(false),
	mInterfaceDescPtr(NULL)
{
	const IOUSBConfigurationDescriptor *config;
	
	IOObjectRetain(mIOService);
	
	IOUSBInterfaceInterface **intfIntf = GetPluginInterface();
	if (intfIntf == NULL) {
		usleep(100 * 1000);	// wait 100 ms
		intfIntf = GetPluginInterface();
	}
	require(intfIntf != NULL, errexit);
	
	if (device == NULL) {
		io_service_t ioDeviceObj;
		require_noerr((*intfIntf)->GetDevice(intfIntf, &ioDeviceObj), errexit);
		device = new USBDevice(ioDeviceObj);
		device->GetCompositeConfiguration();
		mCreatedDevice = true;
		//const IOUSBDeviceDescriptor *devDesc = device->GetDeviceDescriptor();
	}
	mDevice = device;
	
	config = device->GetConfigDescriptor();
	if (config != NULL && intfIntf != NULL) {
		Byte *p = (Byte *)config, *pend = p + USBToHostWord(config->wTotalLength);
		UInt8 interfaceNumber, alternateSetting;
		require_noerr((*intfIntf)->GetInterfaceNumber(intfIntf, &interfaceNumber), errexit);
		require_noerr((*intfIntf)->GetAlternateSetting(intfIntf, &alternateSetting), errexit);
		
		while (p < pend) {
			IOUSBInterfaceDescriptor *intfDesc = (IOUSBInterfaceDescriptor *)p;
			if (intfDesc->bDescriptorType == kUSBInterfaceDesc
			 && intfDesc->bInterfaceNumber == interfaceNumber
			 && intfDesc->bAlternateSetting == alternateSetting) {
				mInterfaceDescPtr = intfDesc;
				break;
			}
			p += intfDesc->bLength;
		}
	}
errexit: ;
}

// _________________________________________________________________________________________
//	USBInterface::~USBInterface
//
USBInterface::~USBInterface()
{
	if (mIsOpen)
		verify_noerr((*mPluginIntf)->USBInterfaceClose(mPluginIntf));
	if (mPluginIntf != NULL)
		(*mPluginIntf)->Release(mPluginIntf);
	IOObjectRelease(mIOService);
	
	if (mCreatedDevice)
		delete mDevice;
}

bool	USBInterface::Open()
{
	if (mIsOpen) return true;
	
	IOUSBInterfaceInterface **intfIntf = GetPluginInterface();
	if (intfIntf == NULL) return false;
	
	require_noerr((*intfIntf)->USBInterfaceOpen(intfIntf), errexit);
	return true;
	
errexit:
	return false;
}

// _________________________________________________________________________________________
//	USBInterface::GetPluginInterface
//
IOUSBInterfaceInterface **	USBInterface::GetPluginInterface()
{
	if (mPluginIntf == NULL) {
		// Get plugin interface to interface
		IOCFPlugInInterface 	**ioPlugin;
		IOReturn	 			kr;
		SInt32 					score;

		require_noerr(IOCreatePlugInInterfaceForService(
			mIOService, kIOUSBInterfaceUserClientTypeID, kIOCFPlugInInterfaceID, 
			&ioPlugin, &score), errexit);
		
		kr = (*ioPlugin)->QueryInterface(ioPlugin, CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID), (LPVOID *)&mPluginIntf);
		(*ioPlugin)->Release(ioPlugin);
		ioPlugin = NULL;
		if (kr) {
			debug_string("QueryInterface failed");
			mPluginIntf = NULL;
		}
	}
errexit:
	return mPluginIntf;
}

// _________________________________________________________________________________________
//	USBInterface::GetPipe
//
IOReturn	USBInterface::GetPipe(int pipeIndex, USBPipe &pipe)
{
	pipe.mPipeIndex = pipeIndex;
	return (*mPluginIntf)->GetPipeProperties(mPluginIntf, pipeIndex, &pipe.mDirection, &pipe.mPipeNum, &pipe.mTransferType, &pipe.mMaxPacketSize, &pipe.mInterval);
}

