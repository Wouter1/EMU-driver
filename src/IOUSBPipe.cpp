//
//  IOUSBPipe.cpp
//  EMUUSBAudio
//
//  Created by Wouter Pasman on 05/06/16.
//  Copyright (c) 2016 com.emu. All rights reserved.
//

#include "IOUSBPipe.h"

#ifndef HAVE_OLD_USB_INTERFACE
// new interface. Stub the old pipe

IOReturn IOUSBPipe::Read(IOMemoryDescriptor *	buffer,
              UInt64 frameStart, UInt32 numFrames, LowLatencyIsocFrame *frameList,
              LowLatencyCompletion *	completion, UInt32 updateFrequency) {
    return io(buffer, frameList, numFrames, frameStart, completion);
}

IOReturn IOUSBPipe::Read(IOMemoryDescriptor* buffer, Completion* completion, UInt64 *bytesRead) {
    // arg2 must be dataBufferLength. Strange, isn't that just getLength???
    return io(buffer, (int)buffer->getLength(), completion);
}

IOReturn IOUSBPipe::Write(IOMemoryDescriptor *	buffer,
               UInt64 frameStart, UInt32 numFrames, LowLatencyIsocFrame *frameList,
               LowLatencyCompletion * completion, UInt32 updateFrequency) {
    // io is replacing both read and write! I guess buffer->getDirection determines if it's read or write,
    // but I could not find this documented...
    return io(buffer, frameList, numFrames, frameStart, completion);
}

const EndpointDescriptor *	IOUSBPipe::GetEndpointDescriptor() {
    return getEndpointDescriptor();
}

IOReturn IOUSBPipe::SetPipePolicy(UInt16 maxPacketSize, UInt8 maxInterval) {
    
    const EndpointDescriptor *currentDescriptor = getEndpointDescriptor();
    
    EndpointDescriptor endpointDescriptor=*currentDescriptor;
    endpointDescriptor.wMaxPacketSize=0;
    SuperSpeedEndpointCompanionDescriptor ssDesc;
    // no idea what it is, let's hope all 0 will do the job...
    bzero(&ssDesc, sizeof(SuperSpeedEndpointCompanionDescriptor));
    
    return adjustPipe(&endpointDescriptor, &ssDesc);
}

IOReturn IOUSBPipe::ClearPipeStall(bool withDeviceRequest) {
    return clearStall(withDeviceRequest);
}



#endif