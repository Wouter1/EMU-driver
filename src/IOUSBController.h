//
//  IOUSBController.hpp
//  EMUUSBAudio
//
//  Created by wouter on 05/06/16.
//  Copyright © 2016 com.emu. All rights reserved.
//


#ifndef __EMUUSBAudio__IOUSBController__
#define __EMUUSBAudio__IOUSBController__

#include "osxversion.h"

#ifdef HAVE_OLD_USB_INTERFACE
#include <IOKit/usb/IOUSBController.h>
#else

//ADAPTER for 10.11 and higher

class IOUSBController {
private:
//    AppleUSBHostController *controller;
public:
    
};
#endif

#endif /* __EMUUSBAudio__IOUSBController__ */
