//
//  EMUUSBAudioClip.h
//  EMUUSBAudio
//
//  Created by Wouter Pasman on 06/10/14.
//  Copyright (c) 2014 com.emu. All rights reserved.
//

#ifndef __EMUUSBAudio__EMUUSBAudioClip__
#define __EMUUSBAudio__EMUUSBAudioClip__



#include <libkern/OSTypes.h>


extern "C" {
    //	floating point types
    typedef	float				Float32;
    typedef double				Float64;
    
    typedef struct _sPreviousValues {
        Float32	xl_1;
        Float32	xr_1;
        Float32	xl_2;
        Float32	xr_2;
        Float32	yl_1;
        Float32	yr_1;
        Float32	yl_2;
        Float32	yr_2;
    } PreviousValues;
    
    // aml 2.21.02 added structure for 1st order phase compensator
    // use in case of 2nd order crossover filter
    typedef struct _sPreviousValues1stOrder {
        Float32	xl_1;
        Float32	xr_1;
        Float32	yl_1;
        Float32	yr_1;
    } PreviousValues1stOrder;
    
    UInt32 CalculateOffset (UInt64 nanoseconds, UInt32 sampleRate);
    
    
    IOReturn	clipEMUUSBAudioToOutputStream (const void *mixBuf,
                                               void *sampleBuf,
                                               UInt32 firstSampleFrame,
                                               UInt32 numSampleFrames,
                                               const IOAudioStreamFormat *streamFormat);
    
    IOReturn	convertFromEMUUSBAudioInputStreamNoWrap (const void *sampleBuf,
                                                         void *destBuf,
                                                         UInt32 firstSampleFrame,
                                                         UInt32 numSampleFrames,
                                                         const IOAudioStreamFormat *streamFormat);
    
    void SmoothVolume(
                      Float32* theMixBuffer,
                      const Float32& targetVolume,
                      const Float32& lastVolume,
                      long theFirstSample,
                      long numSampleFrames,
                      long usedNumberOfSamples,
                      long numberOfChannels);
	
    void Volume(
                Float32* theMixBuffer,
                const Float32& currentVolume,
                long theFirstSample,
                long usedNumberOfSamples);
	
    
    void GetDbToGainLookup(long value,long fullRange,Float32& returnedValue);
}

#endif

