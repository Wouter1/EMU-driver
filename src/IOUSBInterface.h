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
#include <IOKit/usb/IOUSBInterface.h>
#else

//ADAPTER for 10.11 and higher
#include <IOKit/usb/IOUSBHostInterface.h>
#include <IOUSBDevice.h>
class IOUSBInterface {
private:
    IOUSBHostInterface *interface;
    
public:
    /*!
     @function GetDevice
     returns the device the interface is part of.
     @result Pointer to the IOUSBDevice object which is the parent of this IOUSBInterface object.
     */
    IOUSBDevice *GetDevice();
    
    /*!
     * @brief Return the current frame number of the USB bus
     *
     * @description This method will return the current frame number of the USB bus.  This is most useful for
     * scheduling future isochronous requests.
     *
     * @param theTime If not NULL, this will be updated with the current system time
     *
     * @return The current frame number
     */
    uint64_t getFrameNumber(AbsoluteTime* theTime = NULL) const;

    void        close(IOService* forClient, IOOptionBits options = 0);


};
#endif

#endif /* defined(__EMUUSBAudio__IOUSBInterface__) */
