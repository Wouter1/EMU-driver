//
//  IOUSBDevice.h
//  EMUUSBAudio
//
//  Created by Wouter Pasman on 05/06/16.
//  Copyright (c) 2016 com.emu. All rights reserved.
//

#ifndef __EMUUSBAudio__IOUSBDevice__
#define __EMUUSBAudio__IOUSBDevice__

#include "osxversion.h"




#ifdef HAVE_OLD_USB_INTERFACE
#include <IOKit/usb/IOUSBDevice.h>
class IOUSBDevice1 : public IOUSBDevice {
public:
    inline bool isHighSpeed() {
        // we never should get superspeed or higher, as this is USB2 device.
        return GetSpeed() == kUSBDeviceSpeedHigh;
    }
};

#else


/********* 10.11 *************/
#include <IOUSBController.h>

// which one do we really need?
//#include <IOKit/usb/IOUSBHostHIDDevice.h>

// I suppose we need this, because here many changes wrt 10.9 are explained.
#include <IOKit/usb/IOUSBHostDevice.h>
#include <sys/utfconv.h>


class IOUSBDevice1: public IOUSBHostDevice {
public:
    inline UInt8 GetProductStringIndex(void ) {
        return getDeviceDescriptor()->iProduct;
    };
    
    /*!
     @function GetVendorID
     returns the Vendor ID of the device
     */
    inline  UInt16 GetVendorID(void) {
        return getDeviceDescriptor()->idVendor;
    }
    
    /*!
     @function GetProductID
     returns the Product ID of the device
     */
    inline UInt16 GetProductID(void) {
        return getDeviceDescriptor()->idProduct;
    }
    
    /*!
     @function GetFullConfigurationDescriptor
     return a pointer to all the descriptors for the requested configuration.
     @param configIndex The configuration index (not the configuration value)
     @result Pointer to the descriptors, which are cached in the IOUSBDevice object.
     */
    inline const ConfigurationDescriptor * GetFullConfigurationDescriptor(UInt8 configIndex) {
        return getConfigurationDescriptor(configIndex);
    }
    
    /*!
     @function GetStringDescriptor
     Get a string descriptor as ASCII, in the specified language (default is US English)
     @param index Index of the string descriptor to get.
     @param buf Pointer to place to store ASCII string
     @param maxLen Size of buffer pointed to by buf
     @param lang Language to get string in (default is US English)
     */
    IOReturn GetStringDescriptor(UInt8 index, char* buf, int maxLen, UInt16 lang = 0x409) {
        size_t utf8len = 0;
        const StringDescriptor* stringDescriptor = getStringDescriptor(index, lang);
        if (stringDescriptor != NULL && stringDescriptor->bLength > StandardUSB::kDescriptorSize)
        {
            utf8_encodestr(reinterpret_cast<const u_int16_t*>(stringDescriptor->bString),
                    stringDescriptor->bLength - kDescriptorSize,
                    reinterpret_cast<u_int8_t*>(buf), &utf8len, maxLen,
                    '/', UTF_LITTLE_ENDIAN);
            return kIOReturnSuccess;
        }
        return kIOReturnError;
    }
    
    /*!
     @function ResetDevice
     Reset the device, returning it to the addressed, unconfigured state.
     This is useful if a device has got badly confused. Note that the AppleUSBComposite driver will automatically reconfigure the device if it is a composite device.
     */
    inline IOReturn ResetDevice() {
        return kIOReturnSuccess; //Replacement: none...
    }
    
    /*!
     @function GetManufacturerStringIndex
     returns the index of string descriptor describing manufacturer
     */
    inline UInt8 GetManufacturerStringIndex() {
        return getDeviceDescriptor()->iManufacturer;
    }
    
    inline bool isHighSpeed() {
        // we never should get superspeed or higher, as this is USB2 device.
        return getSpeed() == kUSBHostConnectionSpeedHigh;
    }
    


    
    
};
#endif

#endif /* defined(__EMUUSBAudio__IOUSBDevice__) */
