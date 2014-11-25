//
//  ADRingBuffer.h
//  EMUUSBAudio
//
//  Created by Wouter Pasman on 06/11/14.
//  Copyright (c) 2014 com.emu. All rights reserved.
//

#ifndef __EMUUSBAudio__ADRingBuffer__
#define __EMUUSBAudio__ADRingBuffer__
#include "LowPassFilter.h"

class ADRingBuffer: public StreamInfo {
public:
    
    /*! our low pass filter to smooth out wrap times */
    LowPassFilter lpfilter;
protected:
    
};


#endif /* defined(__EMUUSBAudio__ADRingBuffer__) */
