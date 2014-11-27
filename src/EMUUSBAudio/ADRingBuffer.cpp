//
//  ADRingBuffer.cpp
//  EMUUSBAudio
//
//  Created by Wouter Pasman on 06/11/14.
//  Copyright (c) 2014 com.emu. All rights reserved.
//

#include "ADRingBuffer.h"
#include "EMUUSBLogging.h"

void ADRingBuffer::init() {

}


void ADRingBuffer::start() {
    previousfrTimestampNs = 0;
    goodWraps = 0;
}


void ADRingBuffer::makeTimeStampFromWrap(AbsoluteTime wt) {
    UInt64 wrapTimeNs;
    
    absolutetime_to_nanoseconds(wt,&wrapTimeNs);
    
    if (goodWraps >= 5) {
        // regular operation after initial wraps.
        takeTimeStampNs(lpfilter.filter(wrapTimeNs,FALSE),TRUE);
    } else {
        // setting up the timer. Find good wraps.
        if (goodWraps == 0) {
            goodWraps++;
        } else {
            // check if previous wrap had correct spacing deltaT.
            SInt64 deltaT = wrapTimeNs - previousfrTimestampNs - EXPECTED_WRAP_TIME;
            UInt64 errorT = abs( deltaT );
            
            if (errorT < EXPECTED_WRAP_TIME/1000) {
                goodWraps ++;
                if (goodWraps == 5) {
                    takeTimeStampNs(lpfilter.filter(wrapTimeNs,TRUE),FALSE);
                    doLog("USB timer started");
                }
            } else {
                goodWraps = 0;
                doLog("USB hick (%lld). timer re-syncing.",errorT);
            }
        }
        previousfrTimestampNs = wrapTimeNs;
    }
}

void ADRingBuffer::takeTimeStampNs(UInt64 timeStampNs, Boolean increment) {
    AbsoluteTime t;
    
    nanoseconds_to_absolutetime(timeStampNs, &t);
    notifyWrap(&t,increment);
}

Queue * ADRingBuffer::getFrameSizeQueue() {
    return &frameSizeQueue;
}


IOReturn ADRingBuffer::GatherInputSamples(Boolean doTimeStamp) {
	UInt32			numBytesToCopy = 0; // number of bytes to move the dest ptr by each time
	UInt8*			buffStart = (UInt8*) bufferPtr;
    UInt32         totalreceived=0;
	UInt8*			dest = buffStart + bufferOffset;
    UInt32 readHeadPos = nextExpectedFrame * multFactor;     // for OVERRUN checks
    
    debugIOLogR("+GatherInputSamples %d", bufferOffset / multFactor);
    // check if we moved to the next frame. This is done when primary USB interrupt occurs.
    
    // handle outstanding wraps so that we have it available for this round
    if (doTimeStamp && frameListWrapTimeStamp!=0) {
        makeTimeStampFromWrap(frameListWrapTimeStamp);
        frameListWrapTimeStamp = 0;
    }
    
    if (previousFrameList != currentFrameList) {
        debugIOLogR("GatherInputSamples going from framelist %d to %d. lastindex=%d",previousFrameList , currentFrameList,frameIndex);
        if (frameIndex != RECORD_NUM_USB_FRAMES_PER_LIST) {
            doLog("***** Previous framelist was not handled completely, only %d",frameIndex);
        }
        if (frameListWrapTimeStamp != 0) {
            doLog("wrap         is still open!!"); // we can't erase, we would loose sync point....
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
          //&& (doTimeStamp || frameIndex < 33) // HACK for testing partial processing
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
            // save the # of frames to the framesize queue so that we generate an appropriately sized output packet
            frameSizeQueue.push(lastInputFrames);

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
            makeTimeStampFromWrap(frameListWrapTimeStamp);
            frameListWrapTimeStamp=0;
        }
    }
    debugIOLogR("-GatherInputSamples received %d first open frame=%d",totalreceived, frameIndex);
    return kIOReturnSuccess;
}


IOReturn ADRingBuffer::readFrameList (UInt32 frameListNum) {
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



void ADRingBuffer::readCompleted (void * object, void * frameListNrPtr,
                                       IOReturn result, IOUSBLowLatencyIsocFrame * pFrames) {
    
	ADRingBuffer *	ringbuf = (ADRingBuffer *)object;
    
    // HACK we have numUSBFramesPerList frames, which one to check?? Print frame 0 info.
    debugIOLogR("+ readCompleted framelist %d currentFrameList %d result %x frametime %lld systime %lld",
                frameListnr, ringbuf->currentFrameList, result, myFrames->frTimeStamp,systemTime);
    
    ringbuf->startingEngine = FALSE; // HACK if we turn off the timer to start the  thing...
    
    
    IOLockLock(ringbuf->mLock);
    /* HACK we MUST have the lock now as we must restart reading the list.
     This means we may have to wait for GatherInputSamples to complete from a call from convertInputSamples.
     should be short. And our call to GatherInputSamples will be very fast. Would be nice
     if we can fix this better.
     */
    /*An "underrun error" occurs when the UART transmitter has completed sending a character and the transmit buffer is empty. In asynchronous modes this is treated as an indication that no data remains to be transmitted, rather than an error, since additional stop bits can be appended. This error indication is commonly found in USARTs, since an underrun is more serious in synchronous systems.
     */
    
	if (kIOReturnAborted != result) {
        if (ringbuf->GatherInputSamples(true) != kIOReturnSuccess) {
            debugIOLog("***** EMUUSBAudioEngine::readCompleted failed to read all packets from USB stream!");
        }
	}
	
    // Wouter:  data collection from the USB read is complete.
    // Now start the read on the next block.
	if (!ringbuf->shouldStop) {
        UInt32	frameListToRead;
		// (orig doc) keep incrementing until limit of numUSBFrameLists - 1 is reached.
        // also, we can wonder if we want to do it this way. Why not just check what comes in instead
        ringbuf->currentFrameList =(ringbuf->currentFrameList + 1) % RECORD_NUM_USB_FRAME_LISTS;
        
        // now we have already numUSBFrameListsToQueue-1 other framelist queues running.
        // We set our current list to the next one that is not yet running
        
        frameListToRead = ringbuf->currentFrameList - 1 + ringbuf->numUSBFrameListsToQueue;
        frameListToRead -= ringbuf->numUSBFrameLists * (frameListToRead >= ringbuf->numUSBFrameLists);// crop the number of framesToRead
        // seems something equal to frameListToRead = (frameListnr + ringbuf->numUSBFrameListsToQueue) % ringbuf->numUSBFrameLists;
        ringbuf->readFrameList(frameListToRead); // restart reading (but for different framelist).
        
	} else  {// stop issued FIX THIS CODE
		debugIOLogR("++EMUUSBAudioEngine::readCompleted() - stopping: %d", ringbuf->shouldStop);
		++ringbuf->shouldStop;
		if (ringbuf->shouldStop == (ringbuf->numUSBFrameListsToQueue + 1) && TRUE == ringbuf->terminatingDriver) {
            ringbuf->notifyClosed();
		}
	}
	IOLockUnlock(ringbuf->mLock);
    
	debugIOLogR("- readCompleted currentFrameList=%d",ringbuf->currentFrameList);
	return;
}

