//
//  RingBufferT.h
//  EMUUSBAudio
//
//  Created by Wouter Pasman on 28/11/14.
//  Copyright (c) 2014 Wouter Pasman. All rights reserved.
//

#ifndef __EMUUSBAudio__RingBufferT__
#define __EMUUSBAudio__RingBufferT__

#include <libkern/OSTypes.h>
#include <IOKit/IOLib.h>


/*
 RingBuffer. A ring buffer is a LIFO buffer with fixed size.
 
 Seems we can't use exceptions in the kernel, I get "cannot use 'throw' with
 exceptions disabled. Therefore all functions return IOReturn
 */

template<typename T> class RingBufferT {
public:
    /*! initializes the buffer. Also call the first time */
	
    IOReturn init();
    
    /*! put one object in the ring. May print warning if overflow 
     @param object the object to push
     @return kIOReturnSuccess if ok, possibly  kIOReturnOverrun */

	IOReturn push( T object);
    
    /*! puts num objects in the ring. prints error and continues if overflow. 
     @param objects array[num] of num objects to push
     @param num the number of objects in the array
     @return kIOReturnSuccess if ok, possibly  kIOReturnOverrun */

    IOReturn push(T *objects, UInt16 num);
    
    /* Get one object from the ring. 
     @param pointer to mem block to contain the object
     @return kIOReturnSuccess if ok, possibly  kIOReturnUnderrun */
    
	IOReturn pop(T * object);
    
    /*! pop num objects from the ring.
     caller has to ensure that the size of objects array is at least num 
     @return kIOReturnSuccess if ok, possibly  kIOReturnUnderrun */
    
    IOReturn pop(T *objects, UInt16 num);
    
    /*! get the number of objects in the ring */
    
    UInt16 available();
    
    /*! get the number of free slots in the ring */
    UInt16 vacant();
    
    /*! called when the write head wraps back to 0. Overwrite to get notification. */
    
    void notifyWrap();
    
};



#endif /* defined(__EMUUSBAudio__RingBufferT__) */
