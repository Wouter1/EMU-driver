/*
   This file is part of the EMU CA0189 USB Audio Driver.

   Copyright (C) 2008 EMU Systems/Creative Technology Ltd. 

   This driver is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This driver is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library.   If not, a copy of the GNU Lesser General Public 
   License can be found at <http://www.gnu.org/licenses/>.
*/
/*=============================================================================
	SampleUSBMIDI.cpp
	
=============================================================================*/

#include "EMUUSBMIDI.h"

// ^^^^^^^^^^^^^^^^^^ THINGS YOU MUST CUSTOMIZE ^^^^^^^^^^^^^^^^^^^^^^

// MAKE A NEW UUID FOR EVERY NEW DRIVER!
#define kFactoryUUID CFUUIDGetConstantUUIDWithBytes(NULL, 0xC4, 0xA7, 0xFF, 0xE6, 0xBB, 0xAA, 0x4F, 0x55, 0x90, 0x0B, 0x7B, 0x9B, 0x4B, 0x3F, 0x4B, 0xFA)
//C4A7FFE6-BBAA-4F55-900B-7B9B4B3F4BFA

#define kTheInterfaceToUse	3

#define kMyVendorID			1054//0
#define kMyProductID		16132//0
#define kMyNumPorts			2		// might vary this by model

// __________________________________________________________________________________________________


// Implementation of the factory function for this type.
extern "C" void *NewEMUUSBMIDIDriver(CFAllocatorRef allocator, CFUUIDRef typeID);
extern "C" void *NewEMUUSBMIDIDriver(CFAllocatorRef allocator, CFUUIDRef typeID) 
{
	// If correct type is being requested, allocate an
	// instance of TestType and return the IUnknown interface.
	if (CFEqual(typeID, kMIDIDriverTypeID)) {
		EMUUSBMIDIDevice *result = new EMUUSBMIDIDevice;
		return result->Self();
	} else {
		// If the requested type is incorrect, return NULL.
		return NULL;
	}
}

// __________________________________________________________________________________________________

EMUUSBMIDIDevice::EMUUSBMIDIDevice() :
	USBVendorMIDIDriver(kFactoryUUID)
{
debugIOLog("in constructor");
}

EMUUSBMIDIDevice::~EMUUSBMIDIDevice()
{
}

// __________________________________________________________________________________________________

bool		EMUUSBMIDIDevice::MatchDevice(	USBDevice *		inUSBDevice)
{
	const IOUSBDeviceDescriptor * devDesc = inUSBDevice->GetDeviceDescriptor();
	debugIOLog("starting to match");
	if (USBToHostWord(devDesc->idVendor) == kMyVendorID) {
		UInt16 devProduct = USBToHostWord(devDesc->idProduct);
		if (devProduct == kMyProductID) {
			debugIOLog("Found it");
			return true;
		}
	}
	return false;
}

MIDIDeviceRef	EMUUSBMIDIDevice::CreateDevice(	USBDevice *		inUSBDevice,
													USBInterface *	inUSBInterface)
{
	MIDIDeviceRef dev = NULL;
	MIDIEntityRef ent;
	//UInt16 devProduct = USBToHostWord(inUSBDevice->GetDeviceDescriptor()->idProduct);
	// get the product and manufacturer names from the box
	// get the number of ports the device supports
	const IOUSBDeviceDescriptor*	desc = inUSBDevice->GetDeviceDescriptor();
	if (desc) {
		CFStringRef manufacturerName = inUSBDevice->GetString(desc->iManufacturer);
		CFStringRef	productName = inUSBDevice->GetString(desc->iProduct);
		MIDIDeviceCreate(Self(),
			productName,
			manufacturerName,	// manufacturer name
			productName,
			&dev);
		const IOUSBInterfaceDescriptor* interfaceDesc = NULL;
		if (inUSBInterface) {
			interfaceDesc = inUSBInterface->GetInterfaceDescriptor();
		}else {
			USBInterface *intf = inUSBDevice->FindInterface(kTheInterfaceToUse, 0);
			debugIOLog("FindInterface in CreateDevice, %x", intf);
			interfaceDesc = intf->GetInterfaceDescriptor();
		}
		if (interfaceDesc) {
			// need to find out the number of EMBEDDED MIDI endpoints
			int portLimit = inUSBDevice->GetTotalMIDIPortCount() / 2;
			debugIOLog("portLimit is %d", portLimit);
			if (portLimit > 1) {
				for (int port = 1; port <= portLimit; ++port) {
					char portname[64];
					sprintf(portname, "Port %d", port);

					CFStringRef str = CFStringCreateWithCString(NULL, portname, 0);
					MIDIDeviceAddEntity(dev, str, false, 1, 1, &ent);
					CFRelease(str);
				}
			} else {
				MIDIDeviceAddEntity(dev, productName, false, 1, 1, &ent);
			}
		}
		CFRelease(manufacturerName);
		CFRelease(productName);
	}

	return dev;
}

USBInterface *	EMUUSBMIDIDevice::CreateInterface(USBMIDIDevice *device)
{
	USBInterface *intf = device->mUSBDevice->FindInterface(kTheInterfaceToUse, 0);
	debugIOLog("Finding interface to use %d interface %x", kTheInterfaceToUse, intf);
	return intf;
}

void		EMUUSBMIDIDevice::StartInterface(USBMIDIDevice *usbmDev)
{
debugIOLog("StartInterface called");
}

void		EMUUSBMIDIDevice::StopInterface(USBMIDIDevice *usbmDev)
{
}

void		EMUUSBMIDIDevice::HandleInput(USBMIDIDevice *usbmDev, MIDITimeStamp when, Byte *readBuf, ByteCount readBufSize)
{
debugIOLog("HandleInput");
	USBMIDIHandleInput(usbmDev, when, readBuf, readBufSize);
}

ByteCount	EMUUSBMIDIDevice::PrepareOutput(USBMIDIDevice *usbmDev, WriteQueue &writeQueue, 
				Byte *destBuf)
{
debugIOLog("PrepareOutput");
	int n = USBMIDIPrepareOutput(usbmDev, writeQueue, destBuf, usbmDev->mOutPipe.mMaxPacketSize);
	if (n < usbmDev->mOutPipe.mMaxPacketSize) {
		memset(destBuf + n, 0, usbmDev->mOutPipe.mMaxPacketSize - n);
	}
	return usbmDev->mOutPipe.mMaxPacketSize;
}
