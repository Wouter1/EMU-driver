//
//  LowPassFilter.cpp
//  EMUUSBAudio
//
//  Created by Wouter Pasman on 25/11/14.
//  Copyright (c) 2014 Wouter Pasman. All rights reserved.
//

#include "LowPassFilter.h"
#include "EMUUSBLogging.h"

void LowPassFilter::init(UInt64 inputx, UInt64 expected_t) {
    debugIOLog("LowPassFilter::filter init %lld", expected_t);
    x = inputx;
    dx = expected_t;
    u=0;

}


UInt64 LowPassFilter::filter(UInt64 inputx) {
    UInt64 xnext;
    SInt64 unext,du,F;
    
    
    xnext = x + dx ; // the next filtered output
    unext = inputx - xnext; // error u
    
    du = unext - u; // change of the error, for damping
    F = K * unext + DA * du; // force on spring
    dx = dx + F/M ;
    
    x = xnext;
    u= unext; // update the filter
    
    debugIOLogT("filter %lld -> %lld", inputx, x);
    return x;
}

double LowPassFilter::getRelativeDist(SInt64 val) {
    return (double)(val - (SInt64)x) / (double)dx;
}