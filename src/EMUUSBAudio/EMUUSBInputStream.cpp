//
//  EMUUSBInputStream.cpp
//  EMUUSBAudio
//
//  Created by Wouter Pasman on 28/11/14.
//

#include "EMUUSBInputStream.h"

#include "EMUUSBInputStream.h"
#include "EMUUSBLogging.h"
#include "EMUUSBAudioCommon.h"


IOReturn EMUUSBInputStream::init(RingBufferDefault<UInt8> *inputRing, FrameSizeQueue *frameRing) {
    ReturnIf(inputRing == 0, kIOReturnBadArgument);
    ReturnIf(frameRing == 0, kIOReturnBadArgument);
    
    IOReturn res = StreamInfo::init();
    ReturnIf(res != kIOReturnSuccess, res);
    
    usbRing = inputRing;
    frameSizeQueue = frameRing;
    frameIndex = 0;
    
	startingEngine = TRUE;
    
	mLock = IOLockAlloc();
    ReturnIf(!mLock, kIOReturnNoMemory);
    
    initialized = true;
    return kIOReturnSuccess;
}


IOReturn EMUUSBInputStream::start(UInt64 startFrameNr) {
    ReturnIf(!initialized, kIOReturnNotReady);
#ifdef HAVE_OLD_USB_INTERFACE
    ReturnIf(startFrameNr < streamInterface->GetDevice()->GetBus()->GetFrameNumber() + 10,
             kIOReturnTimeout);
#else
    ReturnIf(startFrameNr < streamInterface->getFrameNumber() + 10, kIOReturnTimeout);
#endif
    ReturnIfFail(StreamInfo::reset());
    ReturnIfFail(StreamInfo::start(startFrameNr));
    
    shouldStop = 0;
    nextCompleteFrameList = 0;
    previousFrameList = 3; //  different from currentFrameList.
    currentReadList = nextCompleteFrameList;
    mDropStartingFrames = kNumberOfStartingFramesToDrop;
    
    // we start reading on all framelists. USB will figure it out and take the next one in order
    // when it has data. We restart each framelist in readCompleted when we get data.
    // this restarting may take some time.
	for (UInt32 frameListNum = nextCompleteFrameList; frameListNum < numUSBFrameListsToQueue; ++frameListNum) {
		readFrameList(frameListNum);         // FIXME handle failed call
	}
    started = true;
    return kIOReturnSuccess;
}


IOReturn EMUUSBInputStream::stop() {
    debugIOLogC("+EMUUSBInputStream::stop");
    ReturnIf(!started, kIOReturnNotOpen);
    started = false;
    if (shouldStop == 0) {
        shouldStop=1;
    }
    return kIOReturnSuccess;
}

bool EMUUSBInputStream::isRunning() {
    return !startingEngine && !shouldStop;
}


IOReturn EMUUSBInputStream::free() {
    ReturnIf(!initialized, kIOReturnNotReady);
    ReturnIf(started, kIOReturnStillOpen);
    initialized=false;
    
    if (NULL != mLock) {
		IOLockFree(mLock);
		mLock = NULL;
	}
    return kIOReturnSuccess;
}



IOReturn EMUUSBInputStream::update() {
    ReturnIf(!started, kIOReturnNotOpen);
    
    GatherInputSamples();
    return kIOReturnSuccess;
}

void EMUUSBInputStream::GatherInputSamples() {
    
    debugIOLogRD("+GatherInputSamples %d", bufferOffset / multFactor);
    
    IOLockLock(mLock);
    while (gatherFromReadList() == kIOReturnSuccess);
    IOLockUnlock(mLock);
}

IOReturn       EMUUSBInputStream::gatherFromReadList() {
    if (shouldStop) return kIOServiceTerminate;
    
    IOReturn result = kIOReturnStillOpen;
    
    LowLatencyIsocFrame * pFrames = &usbIsocFrames[currentReadList * numUSBFramesPerList];
	if (bufferSize <= bufferOffset) {
        // sanity checking to prevent going beyond the end of the allocated dest buffer
		bufferOffset = 0;
        debugIOLogR("BUG EMUUSBAudioEngine::GatherInputSamples wrong offset");
    }
    while(frameIndex < RECORD_NUM_USB_FRAMES_PER_LIST && pFrames[frameIndex].isDone())
    {
        UInt16 size = pFrames[frameIndex].getCompleteCount();
        UInt8 *source = (UInt8*) readBuffer + (currentReadList * readUSBFrameListSize) + maxFrameSize * frameIndex;
        
        if (size%6 == 4 ) {
            size = size-4; // workaround for MICROSOFT_USB_EHCI_BUG_WORKAROUND
            source = source + 4;
        }
        
        if (mDropStartingFrames <= 0)
        {
            UInt64 wrapTimeNs;
            absolutetime_to_nanoseconds(pFrames[frameIndex].getTime(),&wrapTimeNs);
            // FIXME. We should not call from inside locked area.
            // -0.5/1ms: we need start instead of end of frame. Frame size depends on
            // usb microinterval but we don't (yet) have access to that here.
            usbRing-> push(source, size ,wrapTimeNs- (sampleRate>96000? 500000: 1000000), 1000000000l/(sampleRate * multFactor) );
            frameSizeQueue-> push(size , wrapTimeNs);
            
            // if (frameIndex == 1) {
            //   debugIOLogC("latency %d",usbRing->available());
            // }
        }
        else if(size && mDropStartingFrames > 0)
        {
            mDropStartingFrames--; // only skip frames of nonzero length.
        }
        
        frameIndex++;
    }
    
    if (frameIndex == RECORD_NUM_USB_FRAMES_PER_LIST) {
        // succes reading the entire frame! Continue with the next
        currentReadList = (currentReadList + 1) % numUSBFrameListsToQueue;
        frameIndex = 0;
        result = kIOReturnSuccess;
    }
    
    //Don't restart reading here, that can be done only from readCompleted callback.
    //debugIOLogRD("-GatherInputSamples received %d first open frame=%d",totalreceived, frameIndex);
    
    return result;
}




IOReturn EMUUSBInputStream::readFrameList (UInt32 frameListNum) {
    
    debugIOLogR("+ read frameList %d ", frameListNum);
    
    if (shouldStop) {
        debugIOLog("*** Read should have stopped. Who is calling this? Canceling call")
        return kIOReturnAborted;
    }
    
	IOReturn	result = kIOReturnError;
	if (pipe) {
		UInt32		firstFrame = frameListNum * numUSBFramesPerList;
        usbCompletion[frameListNum].set((void*) this, (LowLatencyCompletionAction)readCompletedStatic, (void*) (UInt64)frameListNum);
        
		for (int i = 0; i < numUSBFramesPerList; ++i) {
            usbIsocFrames[firstFrame+i].set(-1, maxFrameSize, 0, 0);
		}
        
        
        /* The updatefrequency (last arg of Read) is not so well documented. But in IOUSBInterfaceInterface192:
         Specifies how often, in milliseconds, the frame list data should be updated. Valid range is 0 - 8.
         If 0, it means that the framelist should be updated at the end of the transfer.
         
         It appears that this number also has impact on the timing details (latency) in the frame list.
         If you set this to 0, there happens an additional 8ms for a full framelist once in a
         few minutes in the timings.
         If you set this to 1, this jump is 8x more often, about once 30 seconds, but is only 1ms.
         We must keep these jumps small, to avoid timestamp errors and buffer overruns.
         */
        UInt64  frameNr = getNextFrameNr();
        
        
        result = pipe->Read(bufferDescriptors[frameListNum], frameNr, numUSBFramesPerList,
                            &usbIsocFrames[firstFrame], &usbCompletion[frameListNum],1);
        
        debugIOLogR("READ framelist %d in framenr %lld at %lld",frameListNum, frameNr,mach_absolute_time());
        
        if (result != kIOReturnSuccess) {
            // FIXME #17 if this goes wrong, why continue?
            doLog("USB pipe READ error %x",result);
        }
	}
	return result;
}




void EMUUSBInputStream::readCompletedStatic (void * object, void * frameListNrPtr,
                                             IOReturn result, LowLatencyIsocFrame * pFrames) {
    ((EMUUSBInputStream *)object)->readCompleted(frameListNrPtr, result, pFrames);
}




void EMUUSBInputStream::readCompleted ( void * frameListNrPtr,
                                       IOReturn result, LowLatencyIsocFrame * pFrames) {
    
    // HACK we have numUSBFramesPerList frames, which one to check?? Print frame 0 info.
    debugIOLogR("+ readCompleted framelist %p  result %x ", frameListNrPtr, result);
    
    
    startingEngine = FALSE; // HACK this is old code...
    
    /*An "underrun error" occurs when the UART transmitter has completed sending a character and the transmit buffer is empty. In asynchronous modes this is treated as an indication that no data remains to be transmitted, rather than an error, since additional stop bits can be appended. This error indication is commonly found in USARTs, since an underrun is more serious in synchronous systems.
     */
    
	if (kIOReturnAborted != result) {
        GatherInputSamples(); // also check if there is more in the buffer.
	}
	
    // Data collection from the USB read is complete.
    // Now start the read on the next block.
	if (!shouldStop) {
        
		// (orig doc) keep incrementing until limit of numUSBFrameLists - 1 is reached.
        // also, we can wonder if we want to do it this way. Why not just check what comes in instead
        nextCompleteFrameList =(nextCompleteFrameList + 1) % RECORD_NUM_USB_FRAME_LISTS;
        
        // now we have already numUSBFrameListsToQueue-1 other framelist queues running.
        // We set our current list to the next one that is not yet running
        // CHECK can we just use ALL framelists instead of queueing only part of them?
        
        UInt32 frameListToRead = nextCompleteFrameList - 1 + numUSBFrameListsToQueue;
        frameListToRead -= numUSBFrameLists * (frameListToRead >= numUSBFrameLists);// crop the number of framesToRead
        // seems something equal to frameListToRead = (frameListnr + usbstream->numUSBFrameListsToQueue) % usbstream->numUSBFrameLists;
        readFrameList(frameListToRead); // restart reading (but for different framelist).
        
	} else  {
		debugIOLogR("++EMUUSBAudioEngine::readCompleted() - stopped: %d", shouldStop);
		shouldStop++;
	}
    
    if (shouldStop > RECORD_NUM_USB_FRAME_LISTS) {
        debugIOLogC("EMUUSBInputStream::readCompleted all input streams stopped");
        started = false;
        notifyClosed();
    }
    
    
	debugIOLogR("- readCompleted currentFrameList=%p",frameListNrPtr);
	return;
}




