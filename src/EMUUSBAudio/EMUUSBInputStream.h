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


/*! The USB Input stream handler. It pushes the data from the USB stream into the 
 input ring provided with the init call.
 
 Life cycle:
 
 ( init ( start stop )* free )*
 
 See discussion in EMUUSBAudioEngine for global explanation of input and output.

 */
struct EMUUSBInputStream: public StreamInfo {
public:
    /*! initializes the input stream. Must be called before use.
     @param inputRing the initialized USBInputRing.
     @param frameQueue a initialized FrameSizeQueue.
     */
    virtual IOReturn                init(RingBufferDefault<UInt8> *inputRing, FrameSizeQueue *frameQueue);
    

    /*! starts the input stream. Must be called to start
     @return kIOReturnSuccess if all ok. */
    virtual IOReturn                start(UInt64 startFrameNr);
    
    /*! @return true iff the input stream is running */
    virtual bool isRunning();
    
    /*! stops the input stream.
     Stop takes some time (have to wait for callbacks from all streams). 
     A callback notifyClosed is done when close is complete.*/
    virtual IOReturn                stop();

    
    /*! frees the stream. Only to be called after stop() FINISHED (which is notified
     through the notifyClosed() call */
    virtual IOReturn free();
    

    
    /*!
     Called when this inputstream has closed all input streams.
     */
    virtual void                notifyClosed() =0  ;
    
    /*! This can be called externally to grab all available data from the streams.
     This is to ensure low latency, because the normal USB completion callback
     comes only after all has read. */
    IOReturn                        update();
    

    
    // BELOW should become private. Right now it's still shared with EMUUSBAudioEngine.
    
    /*! = maxFrameSize * numUSBFramesPerList; total byte size for buffering frameLists for USB reading. eg 582*64 = 37248.
     */
	UInt32					readUSBFrameListSize;
    
    /*!  direct ptr to USB data buffer = mInput. usbBufferDescriptor. These are
     the buffers for each of the USB readFrameLists. Not clear why this is allocated as one big slot. */
	void *					readBuffer;
    
    
    
    
private:
    
    
    
    UInt32					lastInputFrames; 

    
    /*! Used in GatherInputSamples to keep track of which framelist we were converting. */
    UInt64                  previousFrameList;
    
    /*! used in GatherInputSamples to keep track of which frame in the list we were left at (first non-converted frame index) */
    UInt32                  frameIndex;
    
    
    /*! number of initial frames that are dropped. See kNumberOfStartingFramesToDrop */
	UInt32					mDropStartingFrames;

    
    /*! if !=0 then we are busy stopping. Counts up till we reach RECORD_NUM_FRAMELISTS.
     If stop complete, notifyStop is called and stopped=true */
    volatile UInt32			shouldStop;

    
    /*! HACK for Yosemite #18 explicit counting of USB frame numbers */
    UInt64                  nextFrameNr;

    /*!
     Copy input frames from given USB port framelist into the ring buffer (which will 
     give warnings on overrun). Also updates mInput. bufferOffset.
     
     THis function can be called any number of times while we are waiting
     for the framelist read to finish. This can be called both from readHandler and from
     convertInputSamples. Thread safe - it will lock the caller till it can execute.
     
     This function always uses mInput.currentFrameList as the framelist to handle.
     
     This code directly uses readBuffer to access the bytes.
     
     This function modifies FrameIndex, lastInputSize, LastInputFrames, and runningInputCount. It may
     also alter bufferOffset but that will result in a warning in the logs.
     
     */
	void                GatherInputSamples();
    
    /*! the current list that we are reading from. Used in gatherFromReadList */
    
    UInt32 currentReadList;
    /*! Gather input data from USB list <currentReadList>. NOT threadsafe.
     @return kIOReturnStillOpen if the read ended halfway the list, or kIOReturnSuccess if all data in the list has
     been read.
     */
    IOReturn                gatherFromReadList();
    
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
    IOReturn                readFrameList (UInt32 frameListNum);
    
    
    /*! static version of readCompleted, with first argument being 'this' */
    static void             readCompleted (void * object, void * frameListIndex, IOReturn result, IOUSBLowLatencyIsocFrame * pFrames);
    
    /*!readHandler is the callback from USB completion. This implements IOUSBLowLatencyIsocCompletionAction and the callback function for USB frameread.
     
     @param frameListIndex the frameList number that completed and triggered this call.
     @param result  this handler will do special actions if set values different from kIOReturnSuccess.
     @param pFrames the frames that need checking. Expects that all RECORD_NUM_USB_FRAMES_PER_LIST frames are available completely.
     */
    void                    readCompleted (void * frameListIndex, IOReturn result,
                                   IOUSBLowLatencyIsocFrame * pFrames);
    
    /*! get the first framenumber on which transfer can start. */
    UInt64                  getStartTransferFrameNr();

        
    FrameSizeQueue *        frameSizeQueue;
    
    /*! the input ring. Received from the Engine */
    RingBufferDefault<UInt8> *  usbRing;
    
    /* lock to ensure update and readHandler are never run together */
    IOLock*                     mLock;
    
    /*! set to true after succesful init() */
    bool                    initialized;
    
    /*! set to true after succesful start() */
    bool                    started;

    
    /*!  this is TRUE until we receive the first USB packet.  Can we replace this wit started? */
	Boolean					startingEngine;
    

    
    /*! the framelist that we are expecting to complete from next.
     Basically runs from 0 to numUSBFrameListsToQueue-1 and then
     restarts at 0. Updated after readHandler handled the block. */
    volatile UInt32			nextCompleteFrameList;
    
	/*! orig doc: we need to drop the first 2 frames because the device can't flush the first frames. 
     However doing so de-syncs the pipes. */
    static const long		kNumberOfStartingFramesToDrop = 0;

};


#endif /* defined(__EMUUSBAudio__EMUUSBInputStream__) */
 