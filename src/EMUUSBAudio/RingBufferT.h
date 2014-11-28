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
 RingBuffer. A ring buffer is a FIFO buffer with fixed size.
 This is just the interface definition. Check RingBufferDefault
 
 Seems we can't use exceptions in the kernel, I get "cannot use 'throw' with
 exceptions disabled". Therefore all functions return IOReturn.
 */

template<typename T> class RingBufferT {
public:
    /*! initializes the buffer. Also call the first time 
     @param size the size of the ring buffer.   
     */
	
    virtual IOReturn init(UInt32 size) = 0;
    
    /*! put one object in the ring. May print warning if overflow 
     @param object the object to push
     @param time the time stamp associated with this object.
     @return kIOReturnSuccess if ok, possibly  kIOReturnOverrun */

	virtual IOReturn push( T object, AbsoluteTime time) = 0;
    
    /*! puts num objects in the ring. prints error and continues if overflow. 
     @param objects array[num] of num objects to push
     @param num the number of objects in the array
     @param time the time stamp associated with the objects.
     @return kIOReturnSuccess if ok, possibly  kIOReturnOverrun */

    virtual IOReturn push(T *objects, UInt16 num, AbsoluteTime time) = 0 ;
    
    /* Get one object from the ring. 
     @param pointer to mem block to contain the object
     @return kIOReturnSuccess if ok, possibly  kIOReturnUnderrun */
    
	virtual IOReturn pop(T * object) = 0;
    
    /*! pop num objects from the ring.
     caller has to ensure that the size of objects array is at least num 
     @return kIOReturnSuccess if ok, possibly  kIOReturnUnderrun */
    
    virtual IOReturn pop(T *objects, UInt16 num) = 0;
    
    /*! get the number of objects in the ring */
    
    virtual UInt16 available() = 0;
    
    /*! get the number of free slots in the ring */
    virtual UInt16 vacant() = 0;
    
    /*! called when the write head wraps back to 0. Overwrite to get notification. 
     @param time the time passed with the push of the object that caused the wrap
     */
    
    virtual void notifyWrap(AbsoluteTime time) = 0;
    
};



#endif /* defined(__EMUUSBAudio__RingBufferT__) */
