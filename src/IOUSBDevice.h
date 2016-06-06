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
#else


// ADAPTER
#include <IOUSBController.h>
#include <IOKit/usb/IOUSBHostHIDDevice.h>

class IOUSBDevice {
private:
    IOUSBHostHIDDevice *device;
public:
};
#endif

#endif /* defined(__EMUUSBAudio__IOUSBDevice__) */
