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


void EMUUSBInputStream::init(RingBufferDefault<UInt8> *inputRing) {
    usbRing = inputRing;
}


void EMUUSBInputStream::start() {
    shouldStop = 0;
    
    currentFrameList = 0;
    
    frameSizeQueue.init(FRAMESIZE_QUEUE_SIZE);
    
    // we start reading on all framelists. USB will figure it out and take the next one in order
    // when it has data. We restart each framelist immediately in readCompleted when we get data.
	for (UInt32 frameListNum = currentFrameList; frameListNum < numUSBFrameListsToQueue; ++frameListNum) {
		readFrameList(frameListNum);
	}
}

void EMUUSBInputStream::stop() {
    if (shouldStop==0) {
        shouldStop=1;
    }
}

FrameSizeQueue * EMUUSBInputStream::getFrameSizeQueue() {
    return &frameSizeQueue;
}


IOReturn EMUUSBInputStream::GatherInputSamples(Boolean doTimeStamp) {
	UInt32			numBytesToCopy = 0; // number of bytes to move the dest ptr by each time
	UInt8*			buffStart = (UInt8*) bufferPtr;
    UInt32         totalreceived=0;
	UInt8*			dest = buffStart + bufferOffset;
    UInt32 readHeadPos = nextExpectedFrame * multFactor;     // for OVERRUN checks
    
    debugIOLogRD("+GatherInputSamples %d", bufferOffset / multFactor);
    // check if we moved to the next frame. This is done when primary USB interrupt occurs.
    
    // handle outstanding wraps so that we have it available for this round
    if (doTimeStamp && frameListWrapTimeStamp!=0) {
        usbRing->notifyWrap(frameListWrapTimeStamp) ; // HACK!!!! ring should call this itself.
        frameListWrapTimeStamp = 0;
    }
    
    if (previousFrameList != currentFrameList) {
        debugIOLogRD("GatherInputSamples going from framelist %d to %d. lastindex=%d",previousFrameList , currentFrameList,frameIndex);
        if (frameIndex != RECORD_NUM_USB_FRAMES_PER_LIST) {
            doLog("***** Previous framelist was not handled completely, only %d",frameIndex);
        }
        if (frameListWrapTimeStamp != 0) {
            doLog("wrap is still open!!"); // we can't erase, we would loose sync point....
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
          //&& (doTimeStamp || frameIndex < 33) // for testing partial processing
          )
    {
        UInt8*			source = (UInt8*) readBuffer + (currentFrameList * readUSBFrameListSize)
        + maxFrameSize * frameIndex;
        if (mDropStartingFrames <= 0)
        {
            UInt32	byteCount = pFrames[frameIndex].frActCount;
            if (byteCount != lastInputSize) {
                debugIOLogR("Happy Leap Sample!  new size = %d, current size = %d",byteCount,lastInputSize);
                lastInputSize = byteCount;
                lastInputFrames = byteCount / multFactor;
            }
            totalreceived += byteCount;
            
            if (bufferOffset <  readHeadPos && bufferOffset + byteCount > readHeadPos) {
                // this is not a water tight test but will catch most cases as
                // the write head step sizes are small. Only when read head is at 0 we may miss.
                doLog("****** OVERRUN: write head is overtaking read head!");
            }
            
            runningInputCount += lastInputFrames;
            // save the # of frames to the framesize queue so that we
            // can generate an appropriately sized output packet
            // HACK disabled for now, we don't have the DA running yet so this only
            // causes queue overflows right now.
            //            if (frameSizeQueue.push(&lastInputFrames) != kIOReturnSuccess) {
            //                doLog("framesizequeue overflow");
            //            }
            
            
            SInt32	numBytesToEnd = bufferSize - bufferOffset; // assumes that bufferOffset <= bufferSize
            if (byteCount < numBytesToEnd) { // no wrap
                memcpy(dest, source, byteCount);
                bufferOffset += byteCount;
                numBytesToCopy = byteCount;// number of bytes the dest ptr will be moved by
            } else { // wrapping around - end up at bufferStart or bufferStart + an offset
                UInt32	overflow = byteCount - numBytesToEnd;
                memcpy(dest, source, numBytesToEnd); // copy data into the remaining portion of the dest buffer
                dest = (UInt8*) buffStart;	// wrap around to the start
                bufferOffset = overflow; // remember the location the dest ptr will be set to
                if (overflow)	// copy the remaining bytes into the front of the dest buffer
                    memcpy(dest, source + numBytesToEnd, overflow);
                frameListWrapTimeStamp = pFrames[frameIndex].frTimeStamp;
                
                numBytesToCopy = overflow;
            }
        }
        else if(pFrames[frameIndex].frActCount && mDropStartingFrames > 0) // first frames might have zero length
        {
            mDropStartingFrames--;
        }
        
        //		debugIOLogC("GatherInputSamples frameIndex is %d, numBytesToCopy %d", frameIndex, numBytesToCopy);
        frameIndex++;
        //source += maxFrameSize; // each frame's frReqCount is set to maxFrameSize
        dest += numBytesToCopy;
    }
    
    // now check, why did we leave loop? Was all normal or failure? log failures.
    if (frameIndex != RECORD_NUM_USB_FRAMES_PER_LIST) {
        return kIOReturnStillOpen; // caller has to decide what to do if this happens.
    } else {
        // reached end of list reached.
        if (doTimeStamp && frameListWrapTimeStamp!=0) {
            usbRing->notifyWrap(frameListWrapTimeStamp) ; // HACK!!!! ring should call this itself.
            frameListWrapTimeStamp=0;
        }
        //Don't restart reading here, that can be done only from readCompleted callback.
    }
    debugIOLogRD("-GatherInputSamples received %d first open frame=%d",totalreceived, frameIndex);
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
        
        /*The updatefrequency is not so well documented. But in IOUSBInterfaceInterface192 I read:
         Specifies how often, in milliseconds, the frame list data should be updated. Valid range is 0 - 8. If 0, it means that the framelist should be updated at the end of the transfer.
         It appears that this number also has impact on the timing details in the frame list.
         If you set this to 0, there happens an additional 8ms for a full framelist once in a
         few minutes in the timings.
         If you set this to 1, this jump is 8x more often, about once 30 seconds, but is only 1ms.
         We must keep these jumps small, to avoid timestamp errors and buffer overruns.
         */
		result = pipe->Read(bufferDescriptors[frameListNum], kAppleUSBSSIsocContinuousFrame,
                            numUSBFramesPerList, &usbIsocFrames[firstFrame], &usbCompletion[frameListNum],1);
        if (result!=kIOReturnSuccess) {
            doLog("USB pipe READ error %x",result);
        }
		if (frameQueuedForList) {
			frameQueuedForList[frameListNum] = usbFrameToQueueAt;
        }
        
		usbFrameToQueueAt += numUSBTimeFrames;
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
    debugIOLogR("+ readCompleted framelist %d  result %x frametime %lld ",
                currentFrameList, result);
    
    startingEngine = FALSE; // HACK if we turn off the timer to start the  thing...
    
    
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
        if (GatherInputSamples(true) != kIOReturnSuccess) {
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
        if (shouldStop == RECORD_NUM_USB_FRAME_LISTS) {
            stopped();
		}
		shouldStop++;
	}
	IOLockUnlock(mLock);
    
	debugIOLogR("- readCompleted currentFrameList=%d",currentFrameList);
	return;
}


void EMUUSBInputStream::stopped() {
    frameSizeQueue.free();
    debugIOLogR("All stopped");
    notifyClosed();
}