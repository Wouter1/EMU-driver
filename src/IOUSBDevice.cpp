//
//  IOUSBDevice.cpp
//  EMUUSBAudio
//
//  Created by Wouter Pasman on 05/06/16.
//  Copyright (c) 2016 com.emu. All rights reserved.
//

#include "IOUSBDevice.h"

#ifdef HAVE_OLD_USB_INTERFACE

#include "USBAudioObject.h"

#include <IOKit/usb/IOUSBDevice.h>

bool IOUSBDevice1::isHighSpeed() {
    // we never should get superspeed or higher, as this is USB2 device.
    return GetSpeed() == kUSBDeviceSpeedHigh;
}

IOReturn IOUSBDevice1::devRequestSampleRate(UInt32 inSampleRate, UInt16 endpointAddress) {
    IOUSBDevRequest		devReq;
    UInt32				theSampleRate = OSSwapHostToLittleInt32 (inSampleRate);
    
    devReq.bmRequestType = USBmakebmRequestType (kUSBOut, kUSBClass, kUSBEndpoint);
    devReq.bRequest = SET_CUR;
    devReq.wValue = (SAMPLING_FREQ_CONTROL << 8) | 0;
    devReq.wIndex = endpointAddress;
    // 3 + (usbAudioDevice->isHighHubSpeed()?1:0);// USB 2.0 device has maxPacket size of 4
    devReq.wLength = 4;
    devReq.pData = &theSampleRate;
    return DeviceRequest (&devReq);
}

UInt64 getFrameNumber() {
    return GetBus()->GetFrameNumber();
}
    


#else


/********* 10.11 *************/
#include <IOUSBController.h>

// which one do we really need?
//#include <IOKit/usb/IOUSBHostHIDDevice.h>

#include "USB.h"
#include <IOUSBInterface.h>
#include <sys/utfconv.h>
#include "USBAudioObject.h"



UInt8 IOUSBDevice1::GetProductStringIndex(void ) {
    return getDeviceDescriptor()->iProduct;
};

 UInt16 IOUSBDevice1::GetVendorID(void) {
    return getDeviceDescriptor()->idVendor;
}

UInt16 IOUSBDevice1::GetProductID(void) {
    return getDeviceDescriptor()->idProduct;
}

const ConfigurationDescriptor * IOUSBDevice1::GetFullConfigurationDescriptor(UInt8 configIndex) {
    return getConfigurationDescriptor(configIndex);
}

IOReturn IOUSBDevice1::GetStringDescriptor(UInt8 index, char* buf, int maxLen, UInt16 lang) {
    size_t utf8len = 0;
    const StringDescriptor* stringDescriptor = getStringDescriptor(index, lang);
    if (!stringDescriptor || stringDescriptor->bLength <= kDescriptorSize) {
        return kIOReturnError;
    }
    
    utf8_encodestr(reinterpret_cast<const u_int16_t*>(stringDescriptor->bString),
                   stringDescriptor->bLength - kDescriptorSize,
                   reinterpret_cast<u_int8_t*>(buf), &utf8len, maxLen,
                   '/', UTF_LITTLE_ENDIAN);
    
    return kIOReturnSuccess;
}

IOReturn IOUSBDevice1::ResetDevice() {
    return kIOReturnSuccess; //Replacement: none...
}

UInt8 IOUSBDevice1::GetManufacturerStringIndex() {
    return getDeviceDescriptor()->iManufacturer;
}

bool IOUSBDevice1::isHighSpeed() {
    // we never should get superspeed or higher, as this is USB2 device.
    return getSpeed() == kUSBHostConnectionSpeedHigh;
}


IOReturn IOUSBDevice1::devRequestSampleRate(UInt32 inSampleRate, UInt16 endpointAddress) {
    DeviceRequest		devReq;
    UInt32				theSampleRate = OSSwapHostToLittleInt32 (inSampleRate);
    
    devReq.bmRequestType = USBmakebmRequestType (kUSBOut, kUSBClass, kUSBEndpoint);
    devReq.bRequest = SET_CUR;
    devReq.wValue = (SAMPLING_FREQ_CONTROL << 8) | 0;
    devReq.wIndex = endpointAddress;
    // 3 + (usbAudioDevice->isHighHubSpeed()?1:0);// USB 2.0 device has maxPacket size of 4
    devReq.wLength = 4;
    uint32_t bytesTransferred;
    
    return deviceRequest(this, devReq, &theSampleRate, bytesTransferred);
}

IOUSBInterface1* IOUSBDevice1::FindNextInterface(IOUSBInterface1*        current,
                                   FindInterfaceRequest* request) {
    OSIterator* iterator = getChildIterator(gIOServicePlane);
    if (iterator==NULL) {
        debugIOLog("ERR can't create an interface iterator to find the device!");
        return NULL;
    }
    
    OSObject* candidate = NULL;
    IOUSBInterface1 *found=NULL;
    bool searchCurrent = (current!=NULL);
    
    while( (candidate = iterator->getNextObject()) != NULL)
    {
        IOUSBInterface1* interfaceCandidate = OSDynamicCast(IOUSBInterface1, candidate);
        if (interfaceCandidate==NULL) continue;
        
        if (searchCurrent) {
            if (candidate == current)  {
                searchCurrent=false; // found current!
            }
        } else {
            // next matching one is the one we want
            if (request->matches(interfaceCandidate->getInterfaceDescriptor())) {
                found=interfaceCandidate;
                break;
            }
        }
        
    }
    OSSafeReleaseNULL(iterator);
    return found;
}


#endif

