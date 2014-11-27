//
//  Queue.h
//  EMUUSBAudio
//
//  Created by Wouter Pasman on 27/11/14.
//  Copyright (c) 2014 com.emu. All rights reserved.
//

#ifndef __EMUUSBAudio__Queue__
#define __EMUUSBAudio__Queue__


#include <libkern/OSTypes.h>


#define FRAMESIZE_QUEUE_SIZE				    128

/*!
 Simple queue with fixed size UINT32. 
 Used to remember previous frame sizes and pass them from ADRing to DARing.
 NOTICE: original code, not thread safe. Probably should be fixed. 
 */
class Queue {
    public:
        /*! push element in queue */
        void push(UInt32 frameSize);
    
        /*! pop element from queue */
        UInt32 pop();
    
        /*! clear the queue */
        void clear();
    
    
        
    private:
        /*! FIFO Queue for framesizes.  Used to communicate USB input frames to the USB writer. */
        UInt32		frameSizeQueue[FRAMESIZE_QUEUE_SIZE];
            
        /*! frameSizeQueueFront points to first available element in frameSizeQueue*/
        UInt32		frameSizeQueueFront=0;
        
            /*!  frameSizeQueueBack points to first free element in frameSizeQueue. */
        UInt32      frameSizeQueueBack=0;

};
#endif /* defined(__EMUUSBAudio__Queue__) */
