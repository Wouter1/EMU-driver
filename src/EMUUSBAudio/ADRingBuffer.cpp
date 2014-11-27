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
        // regular operation after initial wraps. Mass-spring-damper filter.
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
    
    // for OVERRUN checks
    UInt32 readHeadPos = nextExpectedFrame * multFactor;
    
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



