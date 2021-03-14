//
//  USB.cpp
//  EMUUSBAudio
//
//  Created by wouter on 11/07/16.
//  Copyright © 2016 com.emu. All rights reserved.
//


#include "USB.h"

#ifdef HAVE_OLD_USB_INTERFACE
#include <IOKit/usb/USB.h>


void LowLatencyIsocFrame::set(IOReturn status, UInt16 reqCount, UInt16 actCount, AbsoluteTime t) {
    frStatus = status;
    frReqCount = reqCount;
    frActCount = actCount;
    frTimeStamp = t;
}



bool LowLatencyIsocFrame::isDone() {
    return -1 != frStatus && kUSBLowLatencyIsochTransferKey != frStatus;
}

AbsoluteTime LowLatencyIsocFrame::getTime() {
    return frTimeStamp;
}

uint32_t LowLatencyIsocFrame::getCompleteCount() {
    return frActCount;
}

void LowLatencyIsocFrame::resetTime() {
    frTimeStamp = 0xFFFFFFFFFFFFFFFFull;
}



void LowLatencyCompletion::set(void * t, LowLatencyCompletionAction a , void *p) {
    target = t;
    action = a;
    parameter = p;
}
    
    

void Completion::set(void *t, IOUSBCompletionAction a, void *p ) {
    target = t;
    action = a;
    parameter = p;
}


#else
/********************* 10.11 DEFINITIONS **********************/

// SEE ALSO IOUSBHostInterface for the new functionality compared to 10.9


#include <IOKit/usb/StandardUSB.h>
#include <IOKit/usb/USB.h>
#include <IOKit/usb/IOUSBHostIOSource.h>


void LowLatencyIsocFrame::set(IOReturn s, uint32_t requestc, uint32_t actualcc, AbsoluteTime t) {
    status=s;
    requestCount=requestc;
    completeCount=actualcc;
    timeStamp=t;
}

bool LowLatencyIsocFrame::isDone() {
    return -1 != status && kIOReturnInvalid != status;
}

AbsoluteTime LowLatencyIsocFrame::getTime() {
    return timeStamp;
}


uint32_t LowLatencyIsocFrame::getCompleteCount() {
    return completeCount;
}

void LowLatencyIsocFrame::resetTime() {
    timeStamp = 0xFFFFFFFFFFFFFFFFull;
}





void LowLatencyCompletion::set(void * t, LowLatencyCompletionAction a , void *p) {
    owner = t;
    action = a;
    parameter = p;
    
}



void Completion::set(void *t, IOUSBHostCompletionAction a, void *p ) {
    owner = t;
    action = a;
    parameter = p;
}



bool FindInterfaceRequest::matches(const InterfaceDescriptor *descriptor) {
    return
    descriptor->bInterfaceClass == bInterfaceClass &&
    descriptor->bInterfaceSubClass == bInterfaceSubClass &&
    descriptor->bInterfaceProtocol == bInterfaceProtocol &&
    descriptor->bAlternateSetting == bAlternateSetting;
}




#endif // 10.11+
