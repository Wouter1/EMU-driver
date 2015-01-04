//
//  EMUUSBOutputStream.cpp
//  EMUUSBAudio
//
//  Created by Wouter Pasman on 31/12/14.
//  Copyright (c) 2014 com.emu. All rights reserved.
//

#include "EMUUSBOutputStream.h"
#include "EMUUSBLogging.h"
#include "EMUUSBAudioCommon.h"


IOReturn   EMUUSBOutputStream::init() {
    ReturnIf(initialized, kIOReturnStillOpen);
    // memleak if fail
    theWrapDescriptors[0] = OSTypeAlloc (IOSubMemoryDescriptor);
	theWrapDescriptors[1] = OSTypeAlloc (IOSubMemoryDescriptor);
	ReturnIf (((NULL == theWrapDescriptors[0]) || (NULL == theWrapDescriptors[1])), kIOReturnNoMemory);
    
    frameSizeQueue = NULL;
    initialized=true;
    
    return kIOReturnSuccess;
}

IOReturn EMUUSBOutputStream::start(FrameSizeQueue *frameQueue) {
    debugIOLogC("EMUUSBOutputStream::start");

    started = true;
    shouldStop = 0;
	safeToEraseTo = 0;
	lastSafeErasePoint = 0;

    frameSizeQueue = frameQueue;
    
    for (UInt32 frameListNum = currentFrameList; frameListNum < numUSBFrameListsToQueue; frameListNum++)  {
		debugIOLog("write frame list %d",frameListNum);
		writeFrameList(frameListNum);
	}

    return kIOReturnSuccess;
}


IOReturn EMUUSBOutputStream::stop() {
   debugIOLogC("EMUUSBOutputStream::stop");
    
    ReturnIf(!started, kIOReturnNotOpen);
    started = false;
    if (shouldStop == 0) {
        shouldStop=1;
    }
    return kIOReturnSuccess;
}

void EMUUSBOutputStream::free() {
    if (started) {
        doLog("EMUUSBOutputStream::free BUG free() called without stop()");
    }
    debugIOLogC("EMUUSBOutputStream::free");
    if (theWrapRangeDescriptor) {
		theWrapRangeDescriptor->release ();
		theWrapDescriptors[0]->release ();
		theWrapDescriptors[1]->release ();
		theWrapRangeDescriptor = NULL;
	}
    initialized=false;
}


IOReturn EMUUSBOutputStream::writeFrameList (UInt32 frameListNum) {
    ReturnIf(!started, kIOReturnNoDevice);
    ReturnIf(!pipe,kIOReturnNoDevice);
    
    //debugIOLogW("EMUUSBAudioEngine::writeFrameList %d",frameListNum);
	IOReturn	result = kIOReturnError;
    result = PrepareWriteFrameList (frameListNum);
    ReturnIf (kIOReturnSuccess != result, result);
    
    if (needTimeStamps) {
        result = pipe->Write (theWrapRangeDescriptor,
                                      kAppleUSBSSIsocContinuousFrame,//usbFrameToQueueAt,
                                      numUSBFramesPerList, &usbIsocFrames[frameListNum * numUSBFramesPerList], &usbCompletion[frameListNum], 1);//mPollInterval);//1);
        needTimeStamps = FALSE;
    } else {
        result = pipe->Write(bufferDescriptors[frameListNum],kAppleUSBSSIsocContinuousFrame,//usbFrameToQueueAt, //
                                     numUSBFramesPerList,&usbIsocFrames[frameListNum * numUSBFramesPerList], &usbCompletion[frameListNum], 1);
    }
    
    if (result != kIOReturnSuccess) {
        debugIOLog("write failed:%x",result);
    }
	return result;
}

void EMUUSBOutputStream::writeCompleted (void * object, void * parameter, IOReturn result, IOUSBLowLatencyIsocFrame * pFrames) {
    //debugIOLogW("+EMUUSBAudioEngine::writeCompleted ");
	EMUUSBOutputStream *	self = (EMUUSBOutputStream *) object;
    if (!self->streamInterface) return;
    if (kIOReturnSuccess != result && kIOReturnAborted != result) {
        doLog("** writeCompleted bad result %x",result);
        return;
    }
    
    
    // FIXME: simplistic, flawed locking mechanism
    if (self->inWriteCompletion)
    {
        debugIOLogW("*** Already in write completion!");
		return;
    }
    self->inWriteCompletion = TRUE;
    
    //		IOLockLock(self->mWriteLock);
    
    // FIXME use '%' instead of this weird multiply trick
    // FIXME why update list already here?
    // FIXME remove this weird construction, it basically just recovers
    // the original frame number in a very unintelligeble and possibly buggy way.
    // We get orig number only because numUSBFrameLists == actual #framelists and
    // the completion order is the same as the Write order. But recovering the real
    // list number is critical as we would restart an already running Write if we
    // get this wrong.
    self->currentFrameList = (self->currentFrameList + 1) * (self->currentFrameList < (self->numUSBFrameLists - 1));
    
    if (!self->shouldStop) {
        UInt32	frameListToWrite = (self->currentFrameList - 1) + self->numUSBFrameListsToQueue;
        frameListToWrite -= self->numUSBFrameLists * (frameListToWrite >= self->numUSBFrameLists);
        self->writeFrameList (frameListToWrite);
    } else {
        self->shouldStop++;
        //debugIOLogC("writeHandler: now stopped %d of %d",self->shouldStop,self->numUSBFrameListsToQueue);
    }
    
    // shouldStop counter starts at 1 when stop is initiated and increases 1 for
    // every completed framelist
    if (self->shouldStop == self->numUSBFrameListsToQueue + 1) {
        debugIOLogW("All playback streams stopped");
        self->started = false;
        self->notifyClosed();
    }

    //		IOLockUnlock(self->mWriteLock);
    
Exit:
	self->inWriteCompletion = FALSE;
	return;
}


IOReturn EMUUSBOutputStream::PrepareWriteFrameList (UInt32 listNr) {
    debugIOLogW ("+EMUUSBAudioEngine::PrepareWriteFrameList");
    ReturnIf(!started, kIOReturnNoDevice);
    
    //ReturnIf(frameSizeQueue.available() < numUSBFramesPerList, kIOReturnUnderrun);
    ReturnIf(!audioStream, kIOReturnNoDevice);
    
	UInt32			sampleBufferSize = audioStream->getSampleBufferSize() ;
    ReturnIf(!sampleBufferSize, kIOReturnNoMemory);
    
	IOReturn		result = kIOReturnError;// default
    UInt32			thisFrameListSize = 0;
    UInt32			thisFrameSize = 0;
    UInt32			firstFrame = listNr * numUSBFramesPerList;
    UInt32			numBytesToBufferEnd = sampleBufferSize - previouslyPreparedBufferOffset;
    UInt32			lastPreparedByte = previouslyPreparedBufferOffset;
    bool			haveWrapped = false;
    //    UInt16			integerSamplesInFrame,
    UInt16 stockSamplesInFrame;
    
    
    // Set to number of bytes from the 0 wrap, 0 if this buffer didn't wrap
    usbCompletion[listNr].target = (void *)this;
    usbCompletion[listNr].action = writeCompleted;
    usbCompletion[listNr].parameter = 0;
    
    // FIXME this only works for cases where bInterval=8.
    const UInt16 stockSamplesInFrameDivisor = 1000; // * kNumberOfFramesPerMillisecond;
    stockSamplesInFrame = averageSampleRate / stockSamplesInFrameDivisor;
    
    
    //debugIOLogW("PrepareWriteFrameList stockSamplesInFrame %d numUSBFramesPerList %d", stockSamplesInFrame, numUSBFramesPerList);
    for (UInt32 numUSBFramesPrepared = 0; numUSBFramesPrepared < numUSBFramesPerList; ++numUSBFramesPrepared) {
        
        if (frameSizeQueue->pop(&thisFrameSize) != kIOReturnSuccess) {
            debugIOLog("frameSizeQueue empty, guessing some queue size. May need fix..");
            thisFrameSize = stockSamplesInFrame * multFactor;
        }
        
        
        if (thisFrameSize >= numBytesToBufferEnd) {
            //debugIOLogC("param has something %d", numUSBFramesPrepared);
            lastPreparedByte = thisFrameSize - numBytesToBufferEnd;
            usbCompletion[listNr].parameter = (void *)(UInt64)(((numUSBFramesPrepared + 1) << 16) | lastPreparedByte);
            theWrapDescriptors[0]->initSubRange (usbBufferDescriptor, previouslyPreparedBufferOffset, sampleBufferSize - previouslyPreparedBufferOffset, kIODirectionInOut);
            numBytesToBufferEnd = sampleBufferSize - lastPreparedByte;// reset
            haveWrapped = true;
        } else {
            thisFrameListSize += thisFrameSize;
            lastPreparedByte += thisFrameSize;
            numBytesToBufferEnd -= thisFrameSize;
            //			debugIOLogC("no param");
        }
        usbIsocFrames[firstFrame + numUSBFramesPrepared].frStatus = -1;
        usbIsocFrames[firstFrame + numUSBFramesPrepared].frActCount = 0;
        usbIsocFrames[firstFrame + numUSBFramesPrepared].frReqCount = thisFrameSize;
    }
    //debugIOLogC("Done with the numUSBFrames loop");
    //debugIOLogW("num actual data frames in list %d",numUSBFramesPerList - contiguousZeroes);
    if (haveWrapped) {
        needTimeStamps = TRUE;
        theWrapDescriptors[1]->initSubRange (usbBufferDescriptor, 0, lastPreparedByte, kIODirectionInOut);
        
        if (NULL != theWrapRangeDescriptor) {
            theWrapRangeDescriptor->release ();
            theWrapRangeDescriptor = NULL;
        }
        
        theWrapRangeDescriptor = IOMultiMemoryDescriptor::withDescriptors ((IOMemoryDescriptor **)theWrapDescriptors, 2, kIODirectionInOut, true);
    } else {
        bufferDescriptors[listNr]->initSubRange (usbBufferDescriptor, previouslyPreparedBufferOffset, thisFrameListSize, kIODirectionInOut);
        FailIf (NULL == bufferDescriptors[listNr], Exit);
    }
    
    //debugIOLogW("EMUUSBAudioEngine::PrepareWriteFrameList %d size %d %s", listNr, thisFrameSize,haveWrapped?"-":"wrap");
    //debugIOLogW("PrepareWriteFrameList: lastPrepareFrame %d safeToEraseTo %d",lastPreparedByte / multFactor, safeToEraseTo / multFactor);
    
    // maybe useful for the erase head?
    safeToEraseTo = lastSafeErasePoint;
    lastSafeErasePoint = previouslyPreparedBufferOffset;
    previouslyPreparedBufferOffset = lastPreparedByte;
    result = kIOReturnSuccess;
    
Exit:
	return result;
}

