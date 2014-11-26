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


struct ADRingBuffer: public StreamInfo {
public:
    /*! initializes the ring buffer. Must be called before use. */
    void init();
    
    /*! starts the ring buffer IO. Must be called to start */
    void start();

    /*! notify that the Ring Buffer wrapped around at given time.
     @param time the smoothed-out time stamp when the wrap occured 
     @param increment true if the wrap should increment the wrap counter. 
     */
    virtual void notifyWrap(AbsoluteTime *time, bool increment) = 0;

    /*! make a time stamp for a frame that has given frametime .
     we ignore the exact pos of the sample in the frame because measurements showed no relation between
     this position and the time of the frame.
     @param frametime the timestamp for the USB frame that wrapped the buffer. I guess that the timestamp is for completion of the frame.
     */
    virtual void makeTimeStampFromWrap(AbsoluteTime frametime);

    
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

    /*! good wraps since start of audio input */
    UInt16 goodWraps;

    /*! last received frame timestamp */
    AbsoluteTime previousfrTimestampNs;

private:
    /*! as takeTimeStamp but takes nanoseconds instead of AbsoluteTime */
    void takeTimeStampNs(UInt64 timeStampNs, Boolean increment);


};


#endif /* defined(__EMUUSBAudio__ADRingBuffer__) */
