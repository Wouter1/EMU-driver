//
//  Queue.cpp
//  EMUUSBAudio
//
//  Created by Wouter Pasman on 27/11/14.
//  Copyright (c) 2014 com.emu. All rights reserved.
//

#include "Queue.h"
#include "EMUUSBLogging.h"



void Queue::push(UInt32 frameSize) {
    // FIXME this original code is NOT THREAD SAFE!
    
	frameSizeQueue[frameSizeQueueBack++] = frameSize;
	frameSizeQueueBack %= FRAMESIZE_QUEUE_SIZE;
	//debugIOLogC("queued framesize %d, ptr %d",frameSize,frameSizeQueueBack);
}

UInt32 Queue::pop() {
	if (frameSizeQueueFront == frameSizeQueueBack) {
		debugIOLogC("empty framesize queue");
		return 0;
	}
	UInt32 result = frameSizeQueue[frameSizeQueueFront++];
	frameSizeQueueFront %= FRAMESIZE_QUEUE_SIZE;
	//debugIOLogC("dequeued framesize %d, read %d",result,frameSizeQueueFront);
	return result;
}

//void Queue::AddToLastFrameSize(SInt32 toAdd) {
//    frameSizeQueue[frameSizeQueueFront] += toAdd;
//}

void Queue::clear() {
    frameSizeQueueFront = frameSizeQueueBack = 0;
}
