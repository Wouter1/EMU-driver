//
//  ADRingBuffer.cpp
//  EMUUSBAudio
//
//  Created by Wouter Pasman on 06/11/14.
//  Copyright (c) 2014 com.emu. All rights reserved.
//

#include "ADRingBuffer.h"
#include "EMUUSBLogging.h"


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
