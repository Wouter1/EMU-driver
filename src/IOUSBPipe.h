//
//  IOUSBPipe.h
//  EMUUSBAudio
//
//  Created by Wouter Pasman on 05/06/16.
// This is a stub for IOUSBInterface, to make the old code run on
// the new IOUSBHostInterface.
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

class IOUSBPipe: public IOUSBHostPipe {  
   
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
     @param completion describes action to take when buffer has been filled. MUST NOT BE NULL.
     @param updateFrequency describes how often (in milliseconds) should the frame list be processed
     */
    IOReturn Read(IOMemoryDescriptor *	buffer,
                          UInt64 frameStart, UInt32 numFrames, LowLatencyIsocFrame *frameList,
                  LowLatencyCompletion *	completion, UInt32 updateFrequency = 0);
    
    
    IOReturn Read(IOMemoryDescriptor* buffer, Completion* completion = 0, UInt64 *bytesRead = 0);

    
    /*!
     @function Write
     AVAILABLE ONLY IN VERSION 1.9.2 AND ABOVE
     Write to an isochronous endpoint
     @param buffer place to get the data to transfer
     @param frameStart USB frame number of the frame to start transfer. For SuperSpeed Isoc devices, if the frameStart is kAppleUSBSSIsocContinuousFrame, then just continue after the last transfer which was called.
     @param numFrames Number of frames to transfer
     @param frameList Pointer to list of frames indicating bytes to transfer and result for each frame
     @param completion describes action to take when buffer has been emptied. MUST NOT BE NULL.
     @param updateFrequency describes how often (in milliseconds) should the frame list be processed
     */
    IOReturn Write(IOMemoryDescriptor *	buffer,
                   UInt64 frameStart, UInt32 numFrames, LowLatencyIsocFrame *frameList,
                   LowLatencyCompletion * completion, UInt32 updateFrequency = 0);
    /*!
     @function GetEndpointDescriptor
     returns the endpoint descriptor for the pipe.
     */
    const EndpointDescriptor *	GetEndpointDescriptor();

    
    IOReturn SetPipePolicy(UInt16 maxPacketSize, UInt8 maxInterval) ;
    
    /*!
     @function ClearPipeStall
     AVAILABLE ONLY IN VERSION 1.9 AND ABOVE
     This method causes all outstanding I/O on a pipe to complete with return code kIOUSBTransactionReturned. It also clears both the halted bit and the
     toggle bit on the endpoint in the controller. The driver may need to reset the data toggle within the device to avoid losing any data. If the
     device correctly handles the ClearFeature(ENDPOINT_HALT) device request, then this API will handle that by sending the correct request to the
     device.
     @param withDeviceRequest if true, a ClearFeature(ENDPOINT_HALT) is sent to the appropriate endpoint on the device after the transactions on the
     controllers endpoint are returned and the toggle bit on the controllers endpoint is cleared. if this parameter is false, then this is equivalent
     to the pre-1.9 API. This means that the endpoint on the controller is cleared, but no DeviceRequest is sent to the device's endpoint.
     */
    IOReturn ClearPipeStall(bool withDeviceRequest);
    
    
};
#endif







#endif /* defined(__EMUUSBAudio__IOUSBPipe__) */
