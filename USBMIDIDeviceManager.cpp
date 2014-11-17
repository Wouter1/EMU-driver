/*	Copyright: 	© Copyright 2005 Apple Computer, Inc. All rights reserved.

	Disclaimer:	IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc.
			("Apple") in consideration of your agreement to the following terms, and your
			use, installation, modification or redistribution of this Apple software
			constitutes acceptance of these terms.  If you do not agree with these terms,
			please do not use, install, modify or redistribute this Apple software.

			In consideration of your agreement to abide by the following terms, and subject
			to these terms, Apple grants you a personal, non-exclusive license, under AppleÕs
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
	USBMIDIDeviceManager.cpp
	
=============================================================================*/

#include "USBMIDIDeviceManager.h"
#include "USBMIDIDevice.h"

#if DEBUG
	//#define VERBOSE 1
#endif

// _________________________________________________________________________________________
//	USBMIDIDeviceManager::USBMIDIDeviceManager
//
USBMIDIDeviceManager::USBMIDIDeviceManager(				USBMIDIDriverBase *		driver,
														MIDIDeviceListRef		devList) :
	USBDeviceManager(CFRunLoopGetCurrent()),
	mDriver(driver)
{
	// this could be moved to a utility function
	int nDevices = MIDIDeviceListGetNumberOfDevices(devList);
	
	if (driver->mVersion >= 2) {
		// mark everything previously present as offline
		for (int iDevice = 0; iDevice < nDevices; ++iDevice) {
			MIDIDeviceRef midiDevice = MIDIDeviceListGetDevice(devList, iDevice);
			MIDIObjectSetIntegerProperty(midiDevice, kMIDIPropertyOffline, true);
		}
	}
	
	ScanServices();
}

// _________________________________________________________________________________________
//	USBMIDIDeviceManager::~USBMIDIDeviceManager
//
USBMIDIDeviceManager::~USBMIDIDeviceManager()
{
	for (USBMIDIDeviceList::iterator it = mUSBMIDIDeviceList.begin(); 
	it != mUSBMIDIDeviceList.end(); ++it) {
		USBMIDIDevice *device = *it;
		delete device;
	}
}

// _________________________________________________________________________________________
//	USBMIDIDeviceManager::MatchDevice
//
//	Called to determine whether this driver wishes to use this device.
bool	USBMIDIDeviceManager::MatchDevice(			USBDevice *device)
{
	return mDriver->MatchDevice(device);
}

// _________________________________________________________________________________________
//	USBMIDIDeviceManager::UseDevice
//
//	Called after a device has been matched, configured, and opened.  We update the
//	current MIDISetup if necessary, then ask the driver to create a USBMIDIDevice subclass,
//	which should set it up for I/O.
OSStatus	USBMIDIDeviceManager::UseDevice(USBDevice *usbDevice)
{
	return UseDeviceAndInterface(usbDevice, NULL);
}


OSStatus	USBMIDIDeviceManager::UseDeviceAndInterface(USBDevice *		usbDevice,
														USBInterface *	usbInterface)
{
	// Match the device that was just located with what is in the current state
	printf("UseDevuceAndInterface");
	MIDIDeviceRef midiDevice = NULL;
	IOUSBDeviceInterface **devIntf = usbDevice->GetPluginInterface();
	const IOUSBDeviceDescriptor *devDesc = usbDevice->GetDeviceDescriptor();
	bool deviceInSetup = false;
	UInt32 vendorProduct = ((UInt32)USBToHostWord(devDesc->idVendor) << 16) | 
							USBToHostWord(devDesc->idProduct);
	CFStringRef serialNumber = usbDevice->GetString(devDesc->iSerialNumber);
	OSStatus err;
	UInt32 locationID;
	require_noerr(err = (*devIntf)->GetLocationID(devIntf, &locationID), errexit);
	{
		// See if it's already in the setup
		MIDIDeviceListRef curDevices = MIDIGetDriverDeviceList(mDriver->Self());
		int nDevices = MIDIDeviceListGetNumberOfDevices(curDevices), firstPass, lastPass;
		if (serialNumber == NULL) {
			firstPass = 2;
			lastPass = 3;
		} else {
			firstPass = 1;
			lastPass = 1;
		}
		
		for (int pass = firstPass; pass <= lastPass && !deviceInSetup; ++pass) {
			// pass 1: match by serial number if present (skipped if not)
			// pass 2: match by locationID
			// pass 3: match by order found
			for (int iDevice = 0; iDevice < nDevices; ++iDevice) {
				SInt32 prevLocation, prevVendorProduct, isOffline;
				midiDevice = MIDIDeviceListGetDevice(curDevices, iDevice);
				err = MIDIObjectGetIntegerProperty(midiDevice, kUSBVendorProductProperty, 
													&prevVendorProduct);
				if (!err && UInt32(prevVendorProduct) == vendorProduct) {
					switch (pass) {
					case 1:
						{
							CFStringRef prevSerial;
							err = MIDIObjectGetStringProperty(midiDevice, kSerialNumberProperty,
																&prevSerial);
							if (!err) {
								if (CFEqual(prevSerial, serialNumber))
									deviceInSetup = true;
								CFRelease(prevSerial);
							}
						}
						break;
					case 2:
						err = MIDIObjectGetIntegerProperty(midiDevice, kUSBLocationProperty, 
															&prevLocation);
						if (!err && UInt32(prevLocation) == locationID)
							deviceInSetup = true;
						break;
					case 3:
						err = MIDIObjectGetIntegerProperty(midiDevice, kMIDIPropertyOffline,
															&isOffline);
						if (!err && isOffline)
							deviceInSetup = true;
						break;
					}
				}
				if (deviceInSetup) break;
			}
		}
		MIDIDeviceListDispose(curDevices);
	}

	if (!deviceInSetup) {
		#if VERBOSE
			printf("creating new device\n");
		#endif
		
		midiDevice = mDriver->CreateDevice(usbDevice, usbInterface);
		require_noerr(err = MIDISetupAddDevice(midiDevice), errexit);
	} else {
		#if VERBOSE
			printf("old device found\n");
		#endif
		mDriver->PreExistingDeviceFound(midiDevice, usbDevice, usbInterface);
	}
	
	// set device properties unconditionally
	MIDIObjectSetIntegerProperty(midiDevice, kUSBVendorProductProperty, vendorProduct);
	MIDIObjectSetIntegerProperty(midiDevice, kUSBLocationProperty, locationID);
	if (serialNumber != NULL)
		MIDIObjectSetStringProperty(midiDevice, kSerialNumberProperty, serialNumber);
	
	// Create a USBMIDIDevice (or subclass), starting it for I/O
	{
		USBMIDIDevice *ioDev = mDriver->CreateUSBMIDIDevice(usbDevice, usbInterface, midiDevice);
		if (ioDev == NULL) goto errexit;
		if (!ioDev->Initialize())
			delete ioDev;
		else {
			if (mUSBMIDIDeviceList.size() == 0)
				mUSBMIDIDeviceList.reserve(4);
			mUSBMIDIDeviceList.push_back(ioDev);
			MIDIObjectSetIntegerProperty(midiDevice, kMIDIPropertyOffline, false);
		}
	}
errexit:
	if (serialNumber != NULL)
		CFRelease(serialNumber);
	return err;
}

static bool
GetNumericProperty (io_service_t ioDeviceObj, CFStringRef propName, SInt32& value)
{
	// Get the property:
	CFTypeRef prop  = IORegistryEntryCreateCFProperty (ioDeviceObj,
											propName,
											kCFAllocatorDefault, 0);
	if (prop == NULL)
		return false;
	
	// Make sure it's a number, and get the value:
	bool success = false;
	if (CFGetTypeID (prop) == CFNumberGetTypeID ())
		success = CFNumberGetValue ((CFNumberRef) prop, kCFNumberSInt32Type, &value);

	CFRelease (prop);
	return success;
}

// _________________________________________________________________________________________
//	USBMIDIDeviceManager::ServiceTerminated
//
//	Called when the device is unplugged.
void	USBMIDIDeviceManager::ServiceTerminated(			io_service_t	terminatedService)
{
	SInt32 terminatedLocation;
	
	if ( ! GetNumericProperty (terminatedService,
						CFSTR (kUSBDevicePropertyLocationID),
						terminatedLocation))
		return;
	
	for (USBMIDIDeviceList::iterator it = mUSBMIDIDeviceList.begin(); 
	it != mUSBMIDIDeviceList.end(); ++it) {
		USBMIDIDevice *device = *it;
		if (device->GetUSBDevice()->GetLocationID() == (UInt32)terminatedLocation) {
			#if VERBOSE
				printf("shutting down removed device 0x%X\n", (int)terminatedService);
			#endif
			MIDIObjectSetIntegerProperty(device->mMIDIDevice, kMIDIPropertyOffline, true);
			delete device;
			mUSBMIDIDeviceList.erase(it);
			break;
		}
	}
}

bool	USBMIDIDeviceManager::UsingDevice(					USBDevice *		usbDevice)
{
	for (USBMIDIDeviceList::iterator it = mUSBMIDIDeviceList.begin(); 
	it != mUSBMIDIDeviceList.end(); ++it) {
		USBMIDIDevice *device = *it;
		if (device->GetUSBDevice()->GetLocationID() == usbDevice->GetLocationID())
			return true;
	}
	return false;
}

