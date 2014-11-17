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
    
    /*!
     Convert a block of data from our input stream into a destination buffer.
     Assuming no wrap is needed for the source and destination.
     Copies range [firstSampleFrame...firstSampleFrame+numSampleFrames> from sampleBuf to destbuf
     also considering the source stream format. The dest format is always *float
     This is normally the case for destination already when the kernel calls us
     The input buffer may be different, this depends on our buffer sizes.
     @param sampleBuf the start of the source buffer
     @param destbuf the start of the dest buffer. This really is a float *.
     @param firstSampleFrame first frame index in source buf that should be used (dest buf starts at index 0, it already was indexed in IOAudioStream who calls us).
     @param numSampleFrames the num of frames that need conversion
     @param streamFormat the IOAudioStreamFormat.
     */
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

