//
//  EMUUSBInputStream.cpp
//  EMUUSBAudio
//
//  Created by Wouter Pasman on 28/11/14.
//  Copyright (c) 2014 com.emu. All rights reserved.
//

#include "EMUUSBInputStream.h"

#include "EMUUSBInputStream.h"
#include "EMUUSBLogging.h"
#include "EMUUSBAudioCommon.h"


IOReturn EMUUSBInputStream::init(RingBufferDefault<UInt8> *inputRing, FrameSizeQueue *frameRing) {
    ReturnIf(inputRing == 0, kIOReturnBadArgument);
    ReturnIf(frameRing == 0, kIOReturnBadArgument);
    ReturnIf(!pipe, kIOReturnNotOpen);
    
    usbRing = inputRing;
    frameSizeQueue = frameRing;
    UInt16 pollInterval  = 1 << (pipe->GetEndpointDescriptor()->bInterval - 1);
    frameNumberIncreasePerRead  = (NUMBER_FRAMES / 8) * pollInterval; // 1 per frame
	
    mLock = NULL;
	mLock = IOLockAlloc();
    
    ReturnIf(!mLock, kIOReturnNoMemory);
    
    initialized = true;
    return kIOReturnSuccess;
}


IOReturn EMUUSBInputStream::start() {
    ReturnIf(!initialized, kIOReturnNotReady);
    
    shouldStop = 0;
    currentFrameList = 0;
    
    usbFrameToQueueAt = 0;
    
    // we start reading on all framelists. USB will figure it out and take the next one in order
    // when it has data. We restart each framelist immediately in readCompleted when we get data.
	for (UInt32 frameListNum = currentFrameList; frameListNum < numUSBFrameListsToQueue; ++frameListNum) {
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
    
    Boolean haveLock=IOLockTryLock(mLock);
    
    if (haveLock) {
        GatherInputSamples();
        IOLockUnlock(mLock);
    }
    return kIOReturnSuccess;
}

IOReturn EMUUSBInputStream::GatherInputSamples() {
    UInt16 size;
    UInt8 *source;
    
    debugIOLogRD("+GatherInputSamples %d", bufferOffset / multFactor);

    // check if we moved to the next frame. This is done when callback happened.
    if (previousFrameList != currentFrameList) {
        debugIOLogRD("GatherInputSamples going from framelist %d to %d. lastindex=%d",previousFrameList , currentFrameList,frameIndex);
        if (frameIndex != RECORD_NUM_USB_FRAMES_PER_LIST) {
            doLog("***** Previous framelist was not handled completely, only %d",frameIndex);
        }
        previousFrameList = currentFrameList;
        frameIndex=0;
    }
    
    IOUSBLowLatencyIsocFrame * pFrames = &usbIsocFrames[currentFrameList * numUSBFramesPerList];
	if (bufferSize <= bufferOffset) {
        // sanity checking to prevent going beyond the end of the allocated dest buffer
		bufferOffset = 0;
        debugIOLogR("BUG EMUUSBAudioEngine::GatherInputSamples wrong offset");
    }
    
    while(frameIndex < RECORD_NUM_USB_FRAMES_PER_LIST
          && -1 != pFrames[frameIndex].frStatus // no transport at all
          && kUSBLowLatencyIsochTransferKey != pFrames[frameIndex].frStatus // partially transported
          )
    {
        size = pFrames[frameIndex].frActCount;
        source = (UInt8*) readBuffer + (currentFrameList * readUSBFrameListSize) + maxFrameSize * frameIndex;

        if (size%6 == 4 ) {
            size = size-4; // workaround for MICROSOFT_USB_EHCI_BUG_WORKAROUND
            source = source + 4;
        }

        if (mDropStartingFrames <= 0)
        {
            // FIXME. We should not call from inside locked area.
            usbRing-> push(source, size , pFrames[frameIndex].frTimeStamp );
            frameSizeQueue-> push(size , pFrames[frameIndex].frTimeStamp);
        }
        else if(size && mDropStartingFrames > 0)
        {
            mDropStartingFrames--; // only skip frames of nonzero length.
        }
        
        frameIndex++;
    }
    
    //Don't restart reading here, that can be done only from readCompleted callback.
    debugIOLogRD("-GatherInputSamples received %d first open frame=%d",totalreceived, frameIndex);

    if (frameIndex != RECORD_NUM_USB_FRAMES_PER_LIST) {
        return kIOReturnStillOpen; // caller has to decide what to do if this happens.
    }
    return kIOReturnSuccess;
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
		usbCompletion[frameListNum].target = (void*) this;
		usbCompletion[frameListNum].action = readCompleted;
		usbCompletion[frameListNum].parameter = (void*) (UInt64)frameListNum; // remember the frameListNum
        
		for (int i = 0; i < numUSBFramesPerList; ++i) {
			usbIsocFrames[firstFrame+i].frStatus = -1; // used to check if this frame was already received
			usbIsocFrames[firstFrame+i].frActCount = 0; // actual #bytes transferred
			usbIsocFrames[firstFrame+i].frReqCount = maxFrameSize; // #bytes to read.
			*(UInt64 *)(&(usbIsocFrames[firstFrame + i].frTimeStamp)) = 	0ul; //time when frame was procesed
		}

        /* kAppleUSBSSIsocContinuousFrame seems to give error e00002ef on some computers
           therefore we are doing this usbFrameToQueueAt stuff */
        
        if (usbFrameToQueueAt == 0) {
            usbFrameToQueueAt=streamInterface->GetDevice()->GetBus()->GetFrameNumber() + frameOffset;
        } else {
            usbFrameToQueueAt += frameNumberIncreasePerRead;
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

        result = pipe->Read(bufferDescriptors[frameListNum], usbFrameToQueueAt, numUSBFramesPerList,
                            &usbIsocFrames[firstFrame], &usbCompletion[frameListNum],1);
        
        if (result != kIOReturnSuccess) {
            // FIXME #17 if this goes wrong, why continue?
            doLog("USB pipe READ error %x",result);
        }
	}
	return result;
}




void EMUUSBInputStream::readCompleted (void * object, void * frameListNrPtr,
                                       IOReturn result, IOUSBLowLatencyIsocFrame * pFrames) {
    ((EMUUSBInputStream *)object)->readCompleted(frameListNrPtr, result, pFrames);
}




void EMUUSBInputStream::readCompleted ( void * frameListNrPtr,
                                       IOReturn result, IOUSBLowLatencyIsocFrame * pFrames) {
    
    // HACK we have numUSBFramesPerList frames, which one to check?? Print frame 0 info.
    debugIOLogR("+ readCompleted framelist %d  result %x ", currentFrameList, result);
    
    startingEngine = FALSE; // HACK this is old code...
    
    
    /* HACK we MUST have the lock now as we must restart reading the list.
     This means we may have to wait for GatherInputSamples to complete from a call from convertInputSamples.
     Should return quick. And our call to GatherInputSamples will be very fast. Would be nice
     if we can fix this better.
     
     CHECK it might be long if this thread is scheduled out. Not sure if that would matter
     as we have a few more readlists running.
     
     Maybe we can  attach a state to framelists so that we can restart from gatherInputSamples.
     */
    IOLockLock(mLock);
    
    /*An "underrun error" occurs when the UART transmitter has completed sending a character and the transmit buffer is empty. In asynchronous modes this is treated as an indication that no data remains to be transmitted, rather than an error, since additional stop bits can be appended. This error indication is commonly found in USARTs, since an underrun is more serious in synchronous systems.
     */
    
	if (kIOReturnAborted != result) {
        if (GatherInputSamples() != kIOReturnSuccess) {
            debugIOLog("***** EMUUSBAudioEngine::readCompleted failed to read all packets from USB stream!");
        }
	}
	
    // Data collection from the USB read is complete.
    // Now start the read on the next block.
	if (!shouldStop) {
        UInt32	frameListToRead;
		// (orig doc) keep incrementing until limit of numUSBFrameLists - 1 is reached.
        // also, we can wonder if we want to do it this way. Why not just check what comes in instead
        currentFrameList =(currentFrameList + 1) % RECORD_NUM_USB_FRAME_LISTS;
        
        // now we have already numUSBFrameListsToQueue-1 other framelist queues running.
        // We set our current list to the next one that is not yet running
        
        frameListToRead = currentFrameList - 1 + numUSBFrameListsToQueue;
        frameListToRead -= numUSBFrameLists * (frameListToRead >= numUSBFrameLists);// crop the number of framesToRead
        // seems something equal to frameListToRead = (frameListnr + usbstream->numUSBFrameListsToQueue) % usbstream->numUSBFrameLists;
        readFrameList(frameListToRead); // restart reading (but for different framelist).
        
	} else  {
		debugIOLogR("++EMUUSBAudioEngine::readCompleted() - stopped: %d", shouldStop);
		shouldStop++;
	}
	IOLockUnlock(mLock);
    
    if (shouldStop > RECORD_NUM_USB_FRAME_LISTS) {
        debugIOLogC("EMUUSBInputStream::readCompleted all input streams stopped");
        started = false;
        notifyClosed();
    }

// HACK attempt to read feedback from device.
//    if (counter++==100) {
//
//        CheckForAssociatedEndpoint( interfaceNumber, alternateSettingID);
//    }
    
	debugIOLogR("- readCompleted currentFrameList=%d",currentFrameList);
	return;
}



