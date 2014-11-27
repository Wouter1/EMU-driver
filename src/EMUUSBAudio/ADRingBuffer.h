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
#include "Queue.h"


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

    /*!
     Copy input frames from given USB port framelist into the mInput and inform HAL about
     timestamps when we recycle the ring buffer. Also updates mInput. bufferOffset.
     
     The idea is that this function can be called any number of times while we are waiting
     for the framelist read to finish.
     What is unclear is how this is done - there seems no memory of what was already done
     in previous calls to this routine and duplicate work may be done.
     
     You must lock IO ( IOLockLock(mLock)) before calling this. We can not do this ourselves
     because readHandler (who will need to call us) has to lock before this point and
     locking multiple times ourselves will deadlock.
     
     This function always uses mInput.currentFrameList as the framelist to handle.
     
     @return kIOReturnSuccess if all frames were read properly, or kIOReturnStillOpen if there
     were still un-handled frames in the frame list.
     
     @discussion
     routine called by the readHandler.
     Expects that all RECORD_NUM_USB_FRAMES_PER_LIST frames are read completely.
     If not, an error may be logged. Not clear what will happen then higher up,
     the mInput values will be updated properly in any case but of course with
     less data than might be expected.
     
     This should be a low latency callback. I THINK that we should not do all the copy
     work here. But the time stamping that is done here is crucial. It is currently
     halfway the code, it should be right at the start.
     
     Called exclusively from readHandler.
     This code directly uses readBuffer to access the bytes.
     
     This function does NOT check for buffer overrun.
     
     This function modifies FrameIndex, lastInputSize, LastInputFrames, and runningInputCount. It may
     also alter bufferOffset but that will result in a warning in the logs.
     
     @param doTimeStamp true if function should also execute makeTimeStampForWrap if a wrap occurs.
     If false, the timestamp will be stored in frameListWrapTimestamp, and will be executed when
     this function is called on the same frame again but then with doTimeStamp=true (which happens at read completion)
     */
	IOReturn GatherInputSamples(Boolean doTimeStamp);
    
    /*! get the framesize queue */
    Queue * getFrameSizeQueue();

    
    
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

    /*! The value we expect for firstSampleFrame in next call to convertInputSamples.
     The reading of our input buffer should be continuous, not jump around. */
	UInt32								nextExpectedFrame;

    /*! = maxFrameSize * numUSBFramesPerList; total byte size for buffering frameLists for USB reading. eg 582*64 = 37248.
     */
	UInt32								readUSBFrameListSize;

    /*!  direct ptr to USB data buffer = mInput. usbBufferDescriptor. These are
     the buffers for each of the USB readFrameLists. Not clear why this is allocated as one big slot. */
	void *								readBuffer;

    /*! number of initial frames that are dropped. See kNumberOfStartingFramesToDrop */
	UInt32								mDropStartingFrames;

private:
    /*! as takeTimeStamp but takes nanoseconds instead of AbsoluteTime */
    void takeTimeStampNs(UInt64 timeStampNs, Boolean increment);

    Queue frameSizeQueue;


};


#endif /* defined(__EMUUSBAudio__ADRingBuffer__) */
