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
#include "RingBufferDefault.h"
#include "kern/locks.h"

#define FRAMESIZE_QUEUE_SIZE				    128

typedef RingBufferDefault<UInt32> FrameSizeQueue;

struct ADRingBuffer: public StreamInfo {
public:
    /*! initializes the ring buffer. Must be called before use. */
    void                init();
    
    /*! starts the ring buffer IO. Must be called to start */
    void                start();
    
    /*! notify that the Ring Buffer wrapped around at given time.
     @param time the smoothed-out time stamp when the wrap occured
     @param increment true if the wrap should increment the wrap counter.
     */
    virtual void        notifyWrap(AbsoluteTime *time, bool increment) = 0;
    
    /*!
     Called when the Ring Buffer has closed all input streams.
     */
    virtual void        notifyClosed() =0  ;
    
    /*! make a time stamp for a frame that has given frametime .
     We ignore the exact pos of the sample in the frame because measurements showed no relation between
     this position and the time of the frame.
     
     @param frametime the timestamp for the USB frame that wrapped the buffer.
     I guess that the timestamp is for completion of the frame.
     */
    virtual void        makeTimeStampFromWrap(AbsoluteTime frametime);
    
    /*!
     Copy input frames from given USB port framelist into the mInput and inform HAL about
     timestamps when we recycle the ring buffer. Also updates mInput. bufferOffset.
     
     THis function can be called any number of times while we are waiting
     for the framelist read to finish. This can be called both from readHandler and from
     convertInputSamples.
     
     You must lock IO ( IOLockLock(mLock)) before calling this. We can not do this ourselves
     because readHandler (who will need to call us) has to lock before this point and
     locking multiple times ourselves will deadlock.
     
     This function always uses mInput.currentFrameList as the framelist to handle.
     
     @return kIOReturnSuccess if all frames were read properly, or kIOReturnStillOpen if there
     were still un-handled frames in the frame list.
     
     This code directly uses readBuffer to access the bytes.
     
     This function does NOT check for buffer overrun.
     
     This function modifies FrameIndex, lastInputSize, LastInputFrames, and runningInputCount. It may
     also alter bufferOffset but that will result in a warning in the logs.
     
     @param doTimeStamp true if function should also execute makeTimeStampForWrap if a wrap occurs.
     If false, the timestamp will be stored in frameListWrapTimestamp, and will be executed when
     this function is called on the same frame again but then with doTimeStamp=true (which happens at read completion)
     */
	IOReturn            GatherInputSamples(Boolean doTimeStamp);
    
    /*! get the framesize queue */
    FrameSizeQueue *             getFrameSizeQueue();
    
    /*!
     @abstract initializes the read of a frameList (typ. 64 frames) from USB.
     @discussion queues all numUSBFramesPerList frames in given frameListNum for reading.
     The callback when the read is complete is readHandler. There used to be multiple callbacks
     every mPollInterval
     Also it is requested to update the info every 1 ms.
     @param frameListNum the frame list to use, in range [0-numUSBFrameLists> which is usually 0-8.
     orig docu said "frameListNum must be in the valid range 0 - 126".
     This makes no sense to me. Maybe this is a hardware requirement.
     */
    IOReturn            readFrameList (UInt32 frameListNum);
    
    /*!readHandler is the callback from USB completion. Updates mInput.usbFrameToQueueAt.
     
     @discussion Wouter: This implements IOUSBLowLatencyIsocCompletionAction and the callback function for USB frameread.
     Warning: This routine locks the IO. Probably because the code is not thread safe.
     Note: The original code also calls it explicitly from convertInputSamples but that hangs the system (I think because of the lock).
     
     
     @param object the parent audiohandler
     @param frameListIndex the frameList number that completed and triggered this call.
     @param result  this handler will do special actions if set values different from kIOReturnSuccess. This probably indicates the USB read status.
     @param pFrames the frames that need checking. Expects that all RECORD_NUM_USB_FRAMES_PER_LIST frames are available completely.
     
     */
    static void         readCompleted (void * object, void * frameListIndex, IOReturn result, IOUSBLowLatencyIsocFrame * pFrames);
    
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
    
    volatile UInt32						shouldStop;
    
    Boolean								terminatingDriver;
    
    /*!  this is TRUE until we receive the first USB packet. */
	Boolean								startingEngine;
    
    
private:
    /*! as takeTimeStamp but takes nanoseconds instead of AbsoluteTime */
    void takeTimeStampNs(UInt64 timeStampNs, Boolean increment);
    
    FrameSizeQueue frameSizeQueue;
    
    
};


#endif /* defined(__EMUUSBAudio__ADRingBuffer__) */
