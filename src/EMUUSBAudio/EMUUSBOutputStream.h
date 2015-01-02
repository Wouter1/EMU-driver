//
//  EMUUSBOutputStream.h
//  EMUUSBAudio
//
//  Created by Wouter Pasman on 31/12/14.
//  Copyright (c) 2014 com.emu. All rights reserved.
//

#ifndef __EMUUSBAudio__EMUUSBOutputStream__
#define __EMUUSBAudio__EMUUSBOutputStream__

#include "StreamInfo.h"


/*! The USB Output stream handler. It pushes the data into the USB pipe.
 
 Life cycle:
 
 ( init ( start stop )* free )*
 */
class EMUUSBOutputStream: public StreamInfo {

public:
    /*! an array of size [frameListnum] holding usbFrameToQueueAt for each frame when it was requested for read or write. read does not use this anymore. */
    //UInt64 *					frameQueuedForList;

    /*! the next USB MBus Frame number that can be used for read/write. Initially this is at frameOffset from the current frame number. Must be incremented with steps of size numUSBTimeFrames. Currently we just set it to kAppleUSBSSIsocContinuousFrame after
     the initial startup cycle. */
    //UInt64						usbFrameToQueueAt;

    /*! the framelist that was written last.
     Basically runs from 0 to numUSBFrameListsToQueue-1 and then
     restarts at 0. Updated after readHandler handled the block. */
    volatile UInt32						currentFrameList;

};

#endif /* defined(__EMUUSBAudio__EMUUSBOutputStream__) */
