//
//  RingBufferDefault.h
//  EMUUSBAudio
//
//  Created by Wouter Pasman on 28/11/14.
//  Copyright (c) 2014 Wouter Pasman. All rights reserved.
//

#ifndef EMUUSBAudio_RingBufferDefault_h
#define EMUUSBAudio_RingBufferDefault_h


#include "RingBufferT.h"
#include "EMUUSBLogging.h"

/*! Default implementation for RingBufferT.
 This is still a template because of the TYPE but actually this is a complete
 implementation.
 
 * Partially thread safe:
 *  read and write can be called in parallel from different threads
 *  read should not be called from multiple threads at same time
 *  write idem.
 
 * We do not need full thread safety because there is only 1 producer (GatherInputSamples)
 * and one consumer (IOAudioEngine).
 */
template <typename TYPE>

class RingBufferDefault: public RingBufferT<TYPE> {
private:
	TYPE *buffer=0; //
    UInt32 size=0; // number of elements in buffer.
    UInt32 readhead; // index of next read. range [0,SIZE>
    UInt32 writehead; // index of next write. range [0,SIZE>
    
public:
    
    IOReturn init(UInt32 newSize) override {
        debugIOLogR("ring buffer allocate %d",newSize);
        if (newSize<=0) {
            return kIOReturnBadArgument;
        }

        free(); // just in case free was not done of old buffer
        
        size=newSize;
		readhead=0;
        writehead=0;
    
        // allocate buffer as last step as this is flag that ring is ready for use.
        buffer=(TYPE *)IOMalloc(size * sizeof(TYPE));
        if (buffer==0) {
            size=0;
            return kIOReturnNoResources;
        }
        return kIOReturnSuccess;
	}
    
    void free() {
        if (buffer){
            debugIOLogR("ring buffer freed %d",size);
            IOFree(buffer,size * sizeof(TYPE));
            buffer=0;
            size=0;
        }
    }
    
    
    IOReturn push(TYPE object, AbsoluteTime time) override{
        if (!buffer) {
            return kIOReturnNotReady;
        }
        UInt32 newwritehead = writehead+1;
        if (newwritehead== size) newwritehead=0;
        if (newwritehead == readhead) {
            return kIOReturnOverrun ;
        }
        buffer[writehead]= object;
        writehead = newwritehead;
        if (writehead == 0) notifyWrap(time);
        return kIOReturnSuccess;
	}
    
    IOReturn pop(TYPE * data) override{
        if (!buffer) {
            return kIOReturnNotReady;
        }

        if (readhead == writehead) {
            return kIOReturnUnderrun;
        }
        *data = buffer[readhead];
        readhead = readhead+1;
        if (readhead == size) readhead=0;
        return kIOReturnSuccess;
    }
    
    
    IOReturn push(TYPE *objects, UInt32 num, AbsoluteTime time) override{
        if (!buffer) {
            return kIOReturnNotReady;
        }

        if (num > vacant()) {
            doLog("warning. Ignoring overrun");
            //return kIOReturnOverrun ; }
        }
        for ( UInt32 n = 0; n<num; n++) {
            buffer[writehead++] = objects[n];
            if (writehead == size) { writehead = 0; notifyWrap(time); }
        }
        return kIOReturnSuccess;
    }


    IOReturn pop(TYPE *objects, UInt32 num) override{
        if (!buffer) {
            return kIOReturnNotReady;
        }

        if (num > available()) { return kIOReturnUnderrun; }
        
        for (UInt32 n = 0; n < num ; n++) {
            objects[n] = buffer[readhead++];
            if (readhead==size) { readhead = 0; }
        }
        return kIOReturnSuccess;
    }
    
    void notifyWrap(AbsoluteTime time) {
        // default: do nothing
    }
    
    UInt32 available() override{
        if (!buffer) {
            return 0;
        }

        // +SIZE because % does not properly handle negative
        UInt32 avail = (size + writehead - readhead ) % size;
        return avail;
    }
    
    UInt32 vacant() override{
        if (!buffer) {
            return 0;
        }

        // +2*SIZE because % does not properly handle negative
        UInt32 vacant =  (2*size + readhead - writehead - 1 ) % size;
        return vacant;
        
    }
    
};

#endif
