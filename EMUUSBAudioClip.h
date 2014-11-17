/*
   This file is part of the EMU CA0189 USB Audio Driver.

   Copyright (C) 2008 EMU Systems/Creative Technology Ltd. 

   This driver is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This driver is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library.   If not, a copy of the GNU Lesser General Public 
   License can be found at <http://www.gnu.org/licenses/>.
*/
#ifndef _APPLEUSBAUDIOCLIP_H
#define _APPLEUSBAUDIOCLIP_H

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
