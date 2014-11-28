//
//  FixedSizeRingBufferT.h
//  EMUUSBAudio
//
//  Created by Wouter Pasman on 28/11/14.
//  Copyright (c) 2014 Wouter Pasman. All rights reserved.
//

#ifndef EMUUSBAudio_FixedSizeRingBufferT_h
#define EMUUSBAudio_FixedSizeRingBufferT_h

#include "RingBufferT.h"

/*! Default implementation for RingBufferT. Fixed size. 
 
 * Partially thread safe:
 *  read and write can be called in parallel from different threads
 *  read should not be called from multiple threads at same time
 *  write idem.
 */
template <typename TYPE, UInt16 SIZE>

class FixedSizeRingBufferT: public RingBufferT<TYPE> {
private:
	TYPE buffer[SIZE];
    UInt16 readhead; // index of next read. range [0,SIZE>
    UInt16 writehead; // index of next write. range [0,SIZE>
    
public:
    
    IOReturn init() {
		readhead=0;
        writehead=0;
        return kIOReturnSuccess;
	}
    
    
    IOReturn push(TYPE object, AbsoluteTime time) {
        UInt16 newwritehead = writehead+1;
        if (newwritehead== SIZE) newwritehead=0;
        if (newwritehead == readhead) {
            return kIOReturnOverrun ;
        }
        buffer[writehead]= object;
        writehead = newwritehead;
        if (writehead == 0) notifyWrap(time);
        return kIOReturnSuccess;
	}
    
    IOReturn pop(TYPE * data) {
        if (readhead == writehead) {
            return kIOReturnUnderrun;
        }
        *data = buffer[readhead];
        readhead = readhead+1;
        if (readhead == SIZE) readhead=0;
        return kIOReturnSuccess;
    }
    

    IOReturn push(TYPE *objects, UInt16 num, AbsoluteTime time) {
        if (num > vacant()) { return kIOReturnOverrun ; }
        
        for ( UInt16 n = 0; n<num; n++) {
            buffer[writehead++] = objects[n];
            if (writehead == SIZE) { writehead = 0; notifyWrap(time); }
        }
        return kIOReturnSuccess;
    }
    
    
    IOReturn pop(TYPE *objects, UInt16 num) {
        if (num > available()) { return kIOReturnUnderrun; }
        
        for (UInt16 n = 0; n < num ; n++) {
            objects[n] = buffer[readhead++];
            if (readhead==SIZE) { readhead = 0; }
        }
        return kIOReturnSuccess;
    }
    
    void notifyWrap(AbsoluteTime time) {
        // default: do nothing
    }
    
    UInt16 available() {
        // +SIZE because % does not properly handle negative
        UInt32 avail = (SIZE + writehead - readhead ) % SIZE;
        return avail;
    }
    
    UInt16 vacant() {
        // +2*SIZE because % does not properly handle negative
        UInt32 vacant =  (2*SIZE + readhead - writehead - 1 ) % SIZE;
        return vacant;
        
    }
    
};


#endif
