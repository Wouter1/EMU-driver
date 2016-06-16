//
//  IOUSBInterface.h
//  EMUUSBAudio
//
//  Created by Wouter Pasman on 05/06/16.
//  Copyright (c) 2016 com.emu. All rights reserved.
//

#ifndef __EMUUSBAudio__IOUSBInterface__
#define __EMUUSBAudio__IOUSBInterface__

#include "osxversion.h"


#ifdef HAVE_OLD_USB_INTERFACE
/******************** 10.9 *********************/

#include <IOKit/usb/IOUSBInterface.h>

class IOUSBInterface1: public IOUSBInterface {
public:
    inline UInt8 getInterfaceNumber() {
        return GetInterfaceNumber();
    }

    /*! @result the index of the string descriptor describing the interface
     */
    inline UInt8  getInterfaceStringIndex() {
        return GetInterfaceStringIndex();
    };
    
    /*!
     @function GetDevice
     returns the device the interface is part of.
     @result Pointer to the IOUSBDevice object which is the parent of this IOUSBInterface object.
     */
    inline IOUSBDevice *getDevice1() {
        return OSDynamicCast(IOUSBDevice, GetDevice());
    }


};

#else

/******************** 10.11 and higher *********************/
#include <IOKit/usb/IOUSBHostInterface.h>
#include <IOUSBDevice.h>

class IOUSBInterface1: public IOUSBHostInterface {
public:
    /*!
     @function GetDevice
     returns the device the interface is part of.
     @result Pointer to the IOUSBDevice object which is the parent of this IOUSBInterface object.
     */
    inline IOUSBDevice *getDevice1() {
        return OSDynamicCast(IOUSBDevice ,getDevice());
    }
    
    /*!
     * @brief Return the current frame number of the USB bus
     *
     * @description This method will return the current frame number of the USB bus.  This is most useful for
     * scheduling future isochronous requests.
     *
     * @param theTime If not NULL, this will be updated with the current system time
     *
     * @result The current frame number
     */
    inline uint64_t getFrameNumber(AbsoluteTime* theTime = NULL) const {
        return getFrameNumber();
    }
    
    inline UInt8 getInterfaceNumber() {
        return getInterfaceDescriptor()->bInterfaceNumber;
    }
    
    /*! @result the index of the string descriptor describing the interface
     */
    inline UInt8  getInterfaceStringIndex() {
        return getInterfaceDescriptor()->iInterface;
    };
    
    IOUSBHostPipe* FindNextPipe(IOUSBHostPipe* current, IOUSBFindEndpointRequest* request) {
        
    // Replacement: getInterfaceDescriptor and StandardUSB::getNextAssociatedDescriptorWithType to find an endpoint descriptor,
    // then use copyPipe to retrieve the pipe object
    }
    
};
#endif

#endif /* defined(__EMUUSBAudio__IOUSBInterface__) */
