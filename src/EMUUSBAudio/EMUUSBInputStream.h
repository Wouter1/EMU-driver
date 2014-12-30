//
//  EMUUSBInputStream.h
//  EMUUSBAudio
//
//  Created by Wouter Pasman on 28/11/14.
//  Copyright (c) 2014 Wouter Pasman. All rights reserved.
//

#ifndef __EMUUSBAudio__EMUUSBInputStream__
#define __EMUUSBAudio__EMUUSBInputStream__

#include "LowPassFilter.h"
#include "StreamInfo.h"
#include <IOKit/IOLib.h>
#include "RingBufferDefault.h"
#include "kern/locks.h"


/*! Ring to store recent frame sizes, to sync write to read speed */
typedef RingBufferDefault<UInt32> FrameSizeQueue;

/*! The USB Input stream handler. It pushes the data from the USB stream into the 
 input ring provided with the init call.
 
 Life cycle:
 
 ( init ( start stop )* free )*
 */
struct EMUUSBInputStream: public StreamInfo {
public:
    /*! initializes the ring buffer. Must be called before use.
     @param inputRing the initialized USBInputRing.
     @param frameQueue a initialized FrameSizeQueue.
     */
    virtual IOReturn                init(RingBufferDefault<UInt8> *inputRing, FrameSizeQueue *frameQueue);
    

    /*! starts the input stream. Must be called to start
     @return kIOReturnSuccess if all ok. */
    virtual IOReturn                start();
    
    /*! stops the input stream. Must be called to stop.
     Stop takes some time (have to wait for callbacks from all streams). 
     A callback notifyClosed is done when close is complete.*/
    virtual IOReturn                stop();

    /*! frees the stream. Only to be called after stop() FINISHED (which is notified
     through the notifyClosed() call */
    virtual IOReturn free();
    /*!
     Called when the Ring Buffer has closed all input streams.
     */
    virtual void                    notifyClosed() =0  ;
    
    /*! This can be called externally to grab all available data from the streams.
     This is to ensure low latency, because the normal USB completion callback
     comes only after all has read. */
    IOReturn                        update();
    

    // BELOW should become private. Right now it's still shared with EMUUSBAudioEngine.
    
    
    /*! Used in GatherInputSamples to keep track of which framelist we were converting. */
    UInt64                  previousFrameList;
    
    /*! used in GatherInputSamples to keep track of which frame in the list we were left at (first non-converted frame index) */
    UInt32                  frameIndex;
    
    
	UInt32					lastInputFrames;
    
    /*! counter used to steer the DAStream (playback). FIXME not here? */
	UInt32					runningInputCount;
    
    
    
    /*! = maxFrameSize * numUSBFramesPerList; total byte size for buffering frameLists for USB reading. eg 582*64 = 37248.
     */
	UInt32					readUSBFrameListSize;
    
    /*!  direct ptr to USB data buffer = mInput. usbBufferDescriptor. These are
     the buffers for each of the USB readFrameLists. Not clear why this is allocated as one big slot. */
	void *								readBuffer;
    
    /*! number of initial frames that are dropped. See kNumberOfStartingFramesToDrop */
	UInt32					mDropStartingFrames;
    
    /*! if !=0 then we are busy stopping. Counts up till we reach RECORD_NUM_FRAMELISTS.
     If stop complete, notifyStop is called and stopped=true */
    volatile UInt32			shouldStop;
    
    
    /*!  this is TRUE until we receive the first USB packet. */
	Boolean					startingEngine;
    
    /*! HACK for Yosemite #18 explicit counting of USB frame numbers */
    UInt64                  nextFrameNr;
    
    
private:
    
    /*!
     Copy input frames from given USB port framelist into the ring buffer (which will 
     give warnings on overrun). Also updates mInput. bufferOffset.
     
     THis function can be called any number of times while we are waiting
     for the framelist read to finish. This can be called both from readHandler and from
     convertInputSamples.
     
     Caller must lock IO (IOLockLock(mLock)) before calling this. We can not do this ourselves
     because readHandler (who will need to call us) has to lock before this point and
     locking multiple times ourselves will deadlock.
     
     This function always uses mInput.currentFrameList as the framelist to handle.
     
     @return kIOReturnSuccess if all frames were read properly, or kIOReturnStillOpen if there
     were still un-handled frames in the frame list.
     
     This code directly uses readBuffer to access the bytes.
     
     This function modifies FrameIndex, lastInputSize, LastInputFrames, and runningInputCount. It may
     also alter bufferOffset but that will result in a warning in the logs.
     
     */
	IOReturn                    GatherInputSamples();
    
    /*!
     @abstract initializes the read of a frameList (typ. 64 frames) from USB.
     @discussion queues all numUSBFramesPerList frames in given frameListNum for reading.
     The callback when the read is complete is readCompleted.
     Also it is requested to update the info in the framelists every 1 ms to
     make it possible to achieve low latency (by reading the framelist before completion).
     @param frameListNum the frame list to use, in range [0-numUSBFrameLists> which is usually 0-8.
     orig docu said "frameListNum must be in the valid range 0 - 126".
     This makes no sense to me. Maybe this is a hardware requirement.
     */
    IOReturn                    readFrameList (UInt32 frameListNum);
    
    
    /*! static version of readCompleted, with first argument being 'this' */
    static void         readCompleted (void * object, void * frameListIndex, IOReturn result, IOUSBLowLatencyIsocFrame * pFrames);
    
    /*!readHandler is the callback from USB completion. This implements IOUSBLowLatencyIsocCompletionAction and the callback function for USB frameread.
     
     @param frameListIndex the frameList number that completed and triggered this call.
     @param result  this handler will do special actions if set values different from kIOReturnSuccess.
     @param pFrames the frames that need checking. Expects that all RECORD_NUM_USB_FRAMES_PER_LIST frames are available completely.
     */
    void            readCompleted (void * frameListIndex, IOReturn result,
                                   IOUSBLowLatencyIsocFrame * pFrames);
    
    /*! get the first framenumber on which transfer can start. */
    UInt64 getStartTransferFrameNr();

        
    FrameSizeQueue *            frameSizeQueue;
    
    /*! the input ring. Received from the Engine */
    RingBufferDefault<UInt8> *  usbRing;
    
    /* lock to ensure update and readHandler are never run together */
    IOLock*                     mLock;
    
    /*! set to true after succesful init() */
    bool initialized;
    
    /*! set to true after succesful start() */
    bool started;

};


#endif /* defined(__EMUUSBAudio__EMUUSBInputStream__) */
