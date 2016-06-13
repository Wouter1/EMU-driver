//
//  IOUSBPipe.h
//  EMUUSBAudio
//
//  Created by Wouter Pasman on 05/06/16.
//  Copyright (c) 2016 com.emu. All rights reserved.
//

#ifndef __EMUUSBAudio__IOUSBPipe__
#define __EMUUSBAudio__IOUSBPipe__

#include "osxversion.h"

#include "USB.h"

#ifdef HAVE_OLD_USB_INTERFACE
#include <IOKit/usb/IOUSBPipe.h>

#else

//ADAPTER for OSX. Only used when on 10.11 or higher

#include <IOKit/usb/IOUSBHostPipe.h>

class IOUSBPipe {
    
private:
    IOUSBHostPipe *pipe;
    
public:
    /*!
     @function Read
     AVAILABLE ONLY IN VERSION 1.9.2 AND ABOVE
     Read from an isochronous endpoint and process the LowLatencyIsocFrame fields at
     hardware interrupt time
     @param buffer place to put the transferred data
     @param frameStart USB frame number of the frame to start transfer. For SuperSpeed Isoc devices, if the frameStart is kAppleUSBSSIsocContinuousFrame, then just continue after the last transfer which was called.
     @param numFrames Number of frames to transfer
     @param frameList Bytes to transfer, result, and time stamp for each frame
     @param completion describes action to take when buffer has been filled
     @param updateFrequency describes how often (in milliseconds) should the frame list be processed
     */
    virtual IOReturn Read(IOMemoryDescriptor *	buffer,
                          UInt64 frameStart, UInt32 numFrames, LowLatencyIsocFrame *frameList,
                          LowLatencyCompletion *	completion = 0, UInt32 updateFrequency = 0);
    
    
    
    /*!
     @function Write
     AVAILABLE ONLY IN VERSION 1.9.2 AND ABOVE
     Write to an isochronous endpoint
     @param buffer place to get the data to transfer
     @param frameStart USB frame number of the frame to start transfer. For SuperSpeed Isoc devices, if the frameStart is kAppleUSBSSIsocContinuousFrame, then just continue after the last transfer which was called.
     @param numFrames Number of frames to transfer
     @param frameList Pointer to list of frames indicating bytes to transfer and result for each frame
     @param completion describes action to take when buffer has been emptied
     @param updateFrequency describes how often (in milliseconds) should the frame list be processed
     */
    IOReturn Write(IOMemoryDescriptor *	buffer,
                   UInt64 frameStart, UInt32 numFrames, LowLatencyIsocFrame *frameList,
                   LowLatencyCompletion * completion = 0, UInt32 updateFrequency = 0);
    
    /*!
     @function GetEndpointDescriptor
     returns the endpoint descriptor for the pipe.
     */
    const EndpointDescriptor *	GetEndpointDescriptor();
    
    void release();
    
    
    
};
#endif







#endif /* defined(__EMUUSBAudio__IOUSBPipe__) */
