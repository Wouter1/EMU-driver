//
//  ADRingBuffer.h
//  EMUUSBAudio
//
//  Created by Wouter Pasman on 06/11/14.
//  Copyright (c) 2014 com.emu. All rights reserved.
//

#ifndef __EMUUSBAudio__ADRingBuffer__
#define __EMUUSBAudio__ADRingBuffer__
#include "LowPassFilter.h"
#include "StreamInfo.h"
#include <IOKit/IOLib.h> 


class ADRingBuffer: public StreamInfo {
public:

    /*! notify that the Ring Buffer wrapped around at given time.
     @param timeNs the smoothed-out time stamp when the wrap occured 
     @param increment true if the wrap should increment the wrap counter. 
     */
    //virtual void notifyWrapTimeNs(AbsoluteTime timeNs, Boolean increment);

    //void makeTimeStampFromWrap(AbsoluteTime wt);

// should become private. Right now it's still shared with EMUUSBAudioEngine.
        
    /* lock to ensure convertInputSamples and readHandler are never run together */
    IOLock*					mLock;
    
    /*! our low pass filter to smooth out wrap times */
    LowPassFilter           lpfilter;
    
    /*! Used in GatherInputSamples to keep track of which framelist we were converting. */
    UInt64                  previousFrameList;
    
    /*! used in GatherInputSamples to keep track of which frame in the list we were left at (first non-converted frame index) */
    UInt32                  frameIndex;
    
    /*! wrap timestamp for this framelist. 0 if no wrap occured */
    UInt64                  frameListWrapTimeStamp;
    
    UInt32					lastInputSize;
	UInt32					lastInputFrames;
    /*! counter used to steer the DAStream (playback) */
	UInt32					runningInputCount;

    

};


#endif /* defined(__EMUUSBAudio__ADRingBuffer__) */
