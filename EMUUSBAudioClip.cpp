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
#include <libkern/OSTypes.h>
#include <IOKit/IOReturn.h>
#include <IOKit/IOLib.h>

class IOMemoryDescriptor;

#include <IOKit/audio/IOAudioTypes.h>

#include "EMUUSBAudioClip.h"
#include "EMUUSBAudioCommon.h"

extern "C" {
//	floating point types
typedef	float				Float32;
typedef double				Float64;

#define FLOATLIB			FALSE

#if FLOATLIB
void CoeffsFilterOrder2 (Float32 *Coeff, Float32 CutOffFreq, Float32 AttAtCutOffFreq , Float64 SamplingRate);
#else
Boolean CoeffsFilterOrder2Table (Float32 *Coeff, UInt32 samplingRate);
#endif
void MonoFilter (Float32 *in, Float32 *low, Float32 *high, UInt32 frames, UInt32 samplingRate);
void StereoFilter (Float32 *in, Float32 *low, Float32 *high, UInt32 frames, UInt32 samplingRate, PreviousValues *theValue);
// aml 2.21.02 added new stereo filter
void StereoFilter4thOrder (Float32 *in, Float32 *low, Float32 *high, UInt32 frames, UInt32 samplingRate, PreviousValues *section1State, PreviousValues *section2State);
// aml 2.21.02 added new stereo filter with phase compensation
void StereoFilter4thOrderPhaseComp (Float32 *in, Float32 *low, Float32 *high, UInt32 frames, UInt32 samplingRate, PreviousValues *section1State, PreviousValues *section2State, PreviousValues *phaseCompState);

UInt32 CalculateOffset (UInt64 nanoseconds, UInt32 sampleRate) {
	return (UInt32)(((double)sampleRate / 1000000000.0) * nanoseconds);
}

inline static SInt16 Endian16_Swap(SInt16 inValue)
{
	return (((((UInt16)inValue)<<8) & 0xFF00) | ((((UInt16)inValue)>>8) & 0x00FF));
}

inline static SInt32 Endian32_Swap(SInt32 inValue)
{
	return (((((UInt32)inValue)<<24) & 0xFF000000) | ((((UInt32)inValue)<< 8) & 0x00FF0000) | ((((UInt32)inValue)>> 8) & 0x0000FF00) | ((((UInt32)inValue)>>24) & 0x000000FF));
}

#if	defined(__ppc__)

// aml new routines [3034710]

void Int8ToFloat32( SInt8 *src, float *dest, unsigned int count );
void NativeInt16ToFloat32( signed short *src, float *dest, unsigned int count, int bitDepth );
void SwapInt16ToFloat32( signed short *src, float *dest, unsigned int count, int bitDepth );
void NativeInt24ToFloat32( long *src, float *dest, unsigned int count, int bitDepth );
void SwapInt24ToFloat32( long *src, float *dest, unsigned int count, int bitDepth );
void NativeInt32ToFloat32( long *src, float *dest, unsigned int count, int bitDepth );
void SwapInt32ToFloat32( long *src, float *dest, unsigned int count, int bitDepth );

void Float32ToInt8( float *src, SInt8 *dst, unsigned int count );
void Float32ToNativeInt16( float *src, signed short *dst, unsigned int count );
void Float32ToSwapInt16( float *src, signed short *dst, unsigned int count );
void Float32ToNativeInt24( float *src, signed long *dst, unsigned int count );
void Float32ToSwapInt24( float *src, signed long *dst, unsigned int count );
void Float32ToNativeInt32( float *src, signed long *dst, unsigned int count );
void Float32ToSwapInt32( float *src, signed long *dst, unsigned int count );


static inline SInt16	SInt16BigToNativeEndian(SInt16 inValue) { return inValue; }
static inline SInt16	SInt16NativeToBigEndian(SInt16 inValue) { return inValue; }

static inline SInt16	SInt16LittleToNativeEndian(SInt16 inValue) { return Endian16_Swap(inValue); }
static inline SInt16	SInt16NativeToLittleEndian(SInt16 inValue) { return Endian16_Swap(inValue); }

static inline SInt32	SInt32BigToNativeEndian(SInt32 inValue) { return inValue; }
static inline SInt32	SInt32NativeToBigEndian(SInt32 inValue) { return inValue; }

static inline SInt32	SInt32LittleToNativeEndian(SInt32 inValue) { return Endian32_Swap(inValue); }
static inline SInt32	SInt32NativeToLittleEndian(SInt32 inValue) { return Endian32_Swap(inValue); }

#elif defined(__i386__)

static inline SInt16	SInt16BigToNativeEndian(SInt16 inValue) { return Endian16_Swap(inValue); }
static inline SInt16	SInt16NativeToBigEndian(SInt16 inValue) { return Endian16_Swap(inValue); }

static inline SInt16	SInt16LittleToNativeEndian(SInt16 inValue) { return inValue; }
static inline SInt16	SInt16NativeToLittleEndian(SInt16 inValue) { return inValue; }

static inline SInt32	SInt32BigToNativeEndian(SInt32 inValue) { return Endian32_Swap(inValue); }
static inline SInt32	SInt32NativeToBigEndian(SInt32 inValue) { return Endian32_Swap(inValue); }

static inline SInt32	SInt32LittleToNativeEndian(SInt32 inValue) { return inValue; }
static inline SInt32	SInt32NativeToLittleEndian(SInt32 inValue) { return inValue; }

#endif

#define	kMaxClipSInt8		0.9921875
#define kFloat32ToSInt8		((Float32)0x80)
#define	kMaxClipSInt16		0.9999694824219
#define kFloat32ToSInt16	((Float32)0x8000)
#define	kMaxClipSInt24		0.9999998807907
#define	kMaxClipSInt32		0.9999999995343
#define kFloat32ToSInt32	((Float64)0x80000000)

inline static Float32 ClipFloat32ForSInt8(Float32 inSample)
{
	// Float32 maxClip = kMaxSampleSInt8 / (kMaxSampleSInt8 + 1.0);
	if(inSample > kMaxClipSInt8) return kMaxClipSInt8;
	if(inSample < -1.0) return -1.0;
	return inSample;
}

inline static Float32 ClipFloat32ForSInt16(Float32 inSample)
{
	// Float32 maxClip = kMaxSampleSInt16 / (kMaxSampleSInt16 + 1.0);
	if(inSample > kMaxClipSInt16) return kMaxClipSInt16;
	if(inSample < -1.0) return -1.0;
	return inSample;
}

inline static Float32 ClipFloat32ForSInt24(Float32 inSample)
{
	// Float32 maxClip = kMaxSampleSInt24 / (kMaxSampleSInt24 + 1.0);
	if(inSample > kMaxClipSInt24) return kMaxClipSInt24;
	if(inSample < -1.0) return -1.0;
	return inSample;
}

inline static Float32 ClipFloat32ForSInt32(Float32 inSample)
{
	// Float64 maxClip64 = kMaxSampleSInt32 / (kMaxSampleSInt32 + 1.0);
	// Float32 maxClip = maxClip64;
	if(inSample > kMaxClipSInt32) return kMaxClipSInt32;
	if(inSample < -1.0) return -1.0;
	return inSample;
}

//	Float32 -> SInt8
#if defined(__i386__)
static void	ClipFloat32ToSInt8_4(const Float32* inInputBuffer, SInt8* outOutputBuffer, UInt32 inNumberSamples)
{
	register UInt32 theLeftOvers = inNumberSamples % 4;
	
	while(inNumberSamples > theLeftOvers)
	{
		register Float32 theFloat32Value1 = *(inInputBuffer + 0);
		register Float32 theFloat32Value2 = *(inInputBuffer + 1);
		register Float32 theFloat32Value3 = *(inInputBuffer + 2);
		register Float32 theFloat32Value4 = *(inInputBuffer + 3);
		
		inInputBuffer += 4;
		
		theFloat32Value1 = ClipFloat32ForSInt8(theFloat32Value1);
		theFloat32Value2 = ClipFloat32ForSInt8(theFloat32Value2);
		theFloat32Value3 = ClipFloat32ForSInt8(theFloat32Value3);
		theFloat32Value4 = ClipFloat32ForSInt8(theFloat32Value4);
		
		*(outOutputBuffer + 0) = (SInt8)(theFloat32Value1 * kFloat32ToSInt8);
		*(outOutputBuffer + 1) = (SInt8)(theFloat32Value2 * kFloat32ToSInt8);
		*(outOutputBuffer + 2) = (SInt8)(theFloat32Value3 * kFloat32ToSInt8);
		*(outOutputBuffer + 3) = (SInt8)(theFloat32Value4 * kFloat32ToSInt8);
		
		outOutputBuffer += 4;
		inNumberSamples -= 4;
	}
	
	while(inNumberSamples > 0)
	{
		register Float32	theFloat32Value = *inInputBuffer;
		
		++inInputBuffer;
		
		theFloat32Value = ClipFloat32ForSInt8(theFloat32Value);
		
		*outOutputBuffer = (SInt8)(theFloat32Value * kFloat32ToSInt8);
		
		++outOutputBuffer;
		
		--inNumberSamples;
	}
}

//	Float32 -> SInt16
static void	ClipFloat32ToSInt16LE_4(const Float32* inInputBuffer, SInt16* outOutputBuffer, UInt32 inNumberSamples)
{
	register UInt32 theLeftOvers = inNumberSamples % 4;

	while(inNumberSamples > theLeftOvers)
	{
		register Float32 theFloat32Value1 = *(inInputBuffer + 0);
		register Float32 theFloat32Value2 = *(inInputBuffer + 1);
		register Float32 theFloat32Value3 = *(inInputBuffer + 2);
		register Float32 theFloat32Value4 = *(inInputBuffer + 3);
		
		inInputBuffer += 4;
		
		theFloat32Value1 = ClipFloat32ForSInt16(theFloat32Value1);
		theFloat32Value2 = ClipFloat32ForSInt16(theFloat32Value2);
		theFloat32Value3 = ClipFloat32ForSInt16(theFloat32Value3);
		theFloat32Value4 = ClipFloat32ForSInt16(theFloat32Value4);
		
		*(outOutputBuffer + 0) = SInt16NativeToLittleEndian((SInt16)(theFloat32Value1 * kFloat32ToSInt16));
		*(outOutputBuffer + 1) = SInt16NativeToLittleEndian((SInt16)(theFloat32Value2 * kFloat32ToSInt16));
		*(outOutputBuffer + 2) = SInt16NativeToLittleEndian((SInt16)(theFloat32Value3 * kFloat32ToSInt16));
		*(outOutputBuffer + 3) = SInt16NativeToLittleEndian((SInt16)(theFloat32Value4 * kFloat32ToSInt16));
		
		outOutputBuffer += 4;
		inNumberSamples -= 4;
	}
	
	while(inNumberSamples > 0)
	{
		register Float32	theFloat32Value = *inInputBuffer;
		
		++inInputBuffer;
		
		theFloat32Value = ClipFloat32ForSInt16(theFloat32Value);
		
		*outOutputBuffer = SInt16NativeToLittleEndian((SInt16)(theFloat32Value * kFloat32ToSInt16));
		
		++outOutputBuffer;
		
		--inNumberSamples;
	}
}

//	Float32 -> SInt24
//	we use the MaxSInt32 value because of how we munge the data
static void	ClipFloat32ToSInt24LE_4(const Float32* inInputBuffer, SInt32* outOutputBuffer, UInt32 inNumberSamples)
{
	register UInt32 theLeftOvers = inNumberSamples % 4;
	
	while(inNumberSamples > theLeftOvers)
	{
		register Float32 theFloat32Value1 = *(inInputBuffer + 0);
		register Float32 theFloat32Value2 = *(inInputBuffer + 1);
		register Float32 theFloat32Value3 = *(inInputBuffer + 2);
		register Float32 theFloat32Value4 = *(inInputBuffer + 3);
		
		inInputBuffer += 4;
		
		theFloat32Value1 = ClipFloat32ForSInt24(theFloat32Value1);
		theFloat32Value2 = ClipFloat32ForSInt24(theFloat32Value2);
		theFloat32Value3 = ClipFloat32ForSInt24(theFloat32Value3);
		theFloat32Value4 = ClipFloat32ForSInt24(theFloat32Value4);

		// Multiply by kFloat32ToSInt32 instead of kFloat32toSInt24 to make the binary operations below work properly.
		register UInt32 a = (UInt32)(SInt32)(theFloat32Value1 * kFloat32ToSInt32);
		register UInt32 b = (UInt32)(SInt32)(theFloat32Value2 * kFloat32ToSInt32);
		register UInt32 c = (UInt32)(SInt32)(theFloat32Value3 * kFloat32ToSInt32);
		register UInt32 d = (UInt32)(SInt32)(theFloat32Value4 * kFloat32ToSInt32);
		
		#if	defined(__ppc__)
		
			//						a    b    c    d
			//	IN REGISTER:		123X 456X 789X ABCX
			//	OUT REGISTERS:		3216 5498 7CBA
			//	OUT MEMORY:			3216 5498 7CBA

			//	each sample in the 4 registers looks like this: 123X, where X
			//	is the unused byte we need to munge all four so that they look
			//	like this in three registers: 3216 5498 7CBA. We want to avoid
			//	any non-aligned memory writes if at all possible.
			
			register SInt32	theOutputValue1 = ((a << 16) & 0xFF000000) | (a & 0x00FF0000) | ((a >> 16) & 0x0000FF00) | ((b >> 8) & 0x000000FF);	// 3216
			register SInt32	theOutputValue2 = ((b << 8) & 0xFF000000) | ((b >> 8) & 0x00FF0000) | (c & 0x0000FF00) | ((c >> 16) & 0x000000FF);
			register SInt32	theOutputValue3 = (c & 0xFF000000) | ((d << 8) & 0x00FF0000) | ((d >> 8) & 0x0000FF00) | ((d >> 24) & 0x000000FF);
			
		#elif defined(__i386__)
			//						a    b    c    d					a    b    c    d
			//	IN REGISTER:		123X 456X 789X ABCX					abc0 def0 ghi0 jkl0
			//	OUT REGISTERS:		6123 8945 ABC7						fabc hide jklg
			//	OUT MEMORY:			3216 5498 7CBA
		
			register SInt32 theOutputValue1 = ((b << 16) & 0xFF000000) | (a >> 8);
			register SInt32 theOutputValue2 = ((c << 8) & 0xFFFF0000) | ((b >> 16) & 0x0000FFFF);
			register SInt32 theOutputValue3 = (d & 0xFFFFFF00) | ((c >> 24) & 0x000000FF);
		
		#endif
		
		//	store everything back to memory
		*(outOutputBuffer + 0) = theOutputValue1;
		*(outOutputBuffer + 1) = theOutputValue2;
		*(outOutputBuffer + 2) = theOutputValue3;
	
		outOutputBuffer += 3;
		
		inNumberSamples -= 4;
	}
	
	SInt8* theOutputBuffer = (SInt8*)outOutputBuffer;
	while(inNumberSamples > 0)
	{
		register Float32 theFloat32Value = *inInputBuffer;
		++inInputBuffer;
		
		theFloat32Value = ClipFloat32ForSInt24(theFloat32Value);
		
		// Multiply by kFloat32ToSInt32 instead of kFloat32toSInt24 to make the binary operations below work properly.
		register SInt32 theSInt32Value = (SInt32)(theFloat32Value * kFloat32ToSInt32);
		
		// Byte swapping will be handled automatically by the CPU if necessary.
		*(theOutputBuffer + 0) = (SInt8)((((UInt32)theSInt32Value) >> 8) & 0x000000FF);
		*(theOutputBuffer + 1) = (SInt8)((((UInt32)theSInt32Value) >> 16) & 0x000000FF);
		*(theOutputBuffer + 2) = (SInt8)((((UInt32)theSInt32Value) >> 24) & 0x000000FF);
		
		theOutputBuffer += 3;
		
		--inNumberSamples;
	}
}

//	Float32 -> SInt32
static void	ClipFloat32ToSInt32LE_4(const Float32* inInputBuffer, SInt32* outOutputBuffer, UInt32 inNumberSamples)
{
	register UInt32 theLeftOvers = inNumberSamples % 4;
	
	while(inNumberSamples > theLeftOvers)
	{
		register Float32 theFloat32Value1 = *(inInputBuffer + 0);
		register Float32 theFloat32Value2 = *(inInputBuffer + 1);
		register Float32 theFloat32Value3 = *(inInputBuffer + 2);
		register Float32 theFloat32Value4 = *(inInputBuffer + 3);
		
		inInputBuffer += 4;
		
		theFloat32Value1 = ClipFloat32ForSInt32(theFloat32Value1);
		theFloat32Value2 = ClipFloat32ForSInt32(theFloat32Value2);
		theFloat32Value3 = ClipFloat32ForSInt32(theFloat32Value3);
		theFloat32Value4 = ClipFloat32ForSInt32(theFloat32Value4);
		
		*(outOutputBuffer + 0) = SInt32NativeToLittleEndian((SInt32)(theFloat32Value1 * kFloat32ToSInt32));
		*(outOutputBuffer + 1) = SInt32NativeToLittleEndian((SInt32)(theFloat32Value2 * kFloat32ToSInt32));
		*(outOutputBuffer + 2) = SInt32NativeToLittleEndian((SInt32)(theFloat32Value3 * kFloat32ToSInt32));
		*(outOutputBuffer + 3) = SInt32NativeToLittleEndian((SInt32)(theFloat32Value4 * kFloat32ToSInt32));
		
		outOutputBuffer += 4;
		inNumberSamples -= 4;
	}
	
	while(inNumberSamples > 0)
	{
		register Float32 theFloat32Value = *inInputBuffer;
		++inInputBuffer;
		
		theFloat32Value = ClipFloat32ForSInt32(theFloat32Value);

		*outOutputBuffer = SInt32NativeToLittleEndian((SInt32)(theFloat32Value * kFloat32ToSInt32));
		
		++outOutputBuffer;
		
		--inNumberSamples;
	}
}
#endif

IOReturn clipEMUUSBAudioToOutputStream(const void* mixBuf, void* sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{
    if(!streamFormat)
	{
        return kIOReturnBadArgument;
    }
	
	UInt32		theNumberSamples	= numSampleFrames * streamFormat->fNumChannels;
	UInt32		theFirstSample		= firstSampleFrame * streamFormat->fNumChannels;
	Float32*	theMixBuffer		= ((Float32*)mixBuf) + theFirstSample;

	// aml, added optimized routines [3034710]
	switch(streamFormat->fBitWidth)
	{
		case 8:
			{
				SInt8* theOutputBufferSInt8 = ((SInt8*)sampleBuf) + theFirstSample;
				#if	defined(__ppc__)
					Float32ToInt8(theMixBuffer, theOutputBufferSInt8, theNumberSamples);
				#elif defined(__i386__)
					ClipFloat32ToSInt8_4(theMixBuffer, theOutputBufferSInt8, theNumberSamples);
				#endif	
				//ClipFloat32ToSInt8_4(theMixBuffer, theOutputBufferSInt8, theNumberSamples);
			}
			break;

		case 16:
			{
				SInt16* theOutputBufferSInt16 = ((SInt16*)sampleBuf) + theFirstSample;

				#if	defined(__ppc__)
					Float32ToSwapInt16(theMixBuffer, theOutputBufferSInt16, theNumberSamples);
				#elif defined(__i386__)
					ClipFloat32ToSInt16LE_4(theMixBuffer, theOutputBufferSInt16, theNumberSamples);
				#endif	
				//ClipFloat32ToSInt16LE_4(theMixBuffer, theOutputBufferSInt16, theNumberSamples);
			}
			break;

		case 20:
		case 24:
			{
				SInt32* theOutputBufferSInt24 = (SInt32*)(((UInt8*)sampleBuf) + (theFirstSample * 3));

				#if	defined(__ppc__)
					Float32ToSwapInt24(theMixBuffer, theOutputBufferSInt24, theNumberSamples);
				#elif defined(__i386__)
					ClipFloat32ToSInt24LE_4(theMixBuffer, theOutputBufferSInt24, theNumberSamples);
				#endif	
				//ClipFloat32ToSInt24LE_4(theMixBuffer, theOutputBufferSInt24, theNumberSamples);
			}
			break;

		case 32:
			{
				SInt32* theOutputBufferSInt32 = ((SInt32*)sampleBuf) + theFirstSample;

				#if	defined(__ppc__)
					Float32ToSwapInt32(theMixBuffer, theOutputBufferSInt32, theNumberSamples);
				#elif defined(__i386__)
					ClipFloat32ToSInt32LE_4(theMixBuffer, theOutputBufferSInt32, theNumberSamples);
				#endif	
				//ClipFloat32ToSInt32LE_4(theMixBuffer, theOutputBufferSInt32, theNumberSamples);
			}
			break;
	};

    return kIOReturnSuccess;
}

const float kOneOverMaxSInt8Value = 1.0/128.0f;
const float kOneOverMaxSInt16Value = 1.0/32768.0f;
// const float kOneOverMaxSInt24Value = 1.0/8388608.0f;
const float kOneOverMaxSInt24Value = 0.00000011920928955078125f;
const float kOneOverMaxSInt32Value = 1.0/2147483648.0f;

IOReturn convertFromEMUUSBAudioInputStreamNoWrap (const void *sampleBuf,
												void *destBuf,
												UInt32 firstSampleFrame,
												UInt32 numSampleFrames,
												const IOAudioStreamFormat *streamFormat) {
	UInt32	numSamplesLeft;
	float 	*floatDestBuf;

    floatDestBuf = (float *)destBuf;
	numSamplesLeft = numSampleFrames * streamFormat->fNumChannels;

	//	debugIOLog ("destBuf = %p, firstSampleFrame = %ld, numSampleFrames = %ld", destBuf, firstSampleFrame, numSampleFrames);

	switch (streamFormat->fBitWidth) 
	{
		case 8:
			SInt8 *inputBuf8;

			inputBuf8 = &(((SInt8 *)sampleBuf)[firstSampleFrame * streamFormat->fNumChannels]);
			#if defined(__ppc__)
				Int8ToFloat32(inputBuf8, floatDestBuf, numSamplesLeft);
			#elif defined(__i386__)
				while (numSamplesLeft-- > 0) 
				{	
					*(floatDestBuf++) = (float)(*(inputBuf8++)) * kOneOverMaxSInt8Value;
				}
			#endif

			break;
		case 16:
			SInt16 *inputBuf16;
			
			inputBuf16 = &(((SInt16 *)sampleBuf)[firstSampleFrame * streamFormat->fNumChannels]);

			#if defined(__ppc__)
				SwapInt16ToFloat32(inputBuf16, floatDestBuf, numSamplesLeft, 16);
			#elif defined(__i386__)
				while (numSamplesLeft-- > 0) 
				{	
					*(floatDestBuf++) = (float)(*(inputBuf16++)) * kOneOverMaxSInt16Value;
				}
			#endif

			break;
		case 20:
		case 24:
			register SInt8 *inputBuf24;

			// Multiply by 3 because 20 and 24 bit samples are packed into only three bytes, so we have to index bytes, not shorts or longs
			inputBuf24 = &(((SInt8 *)sampleBuf)[firstSampleFrame * streamFormat->fNumChannels * 3]);

			#if defined(__ppc__)
				SwapInt24ToFloat32((long *)inputBuf24, floatDestBuf, numSamplesLeft, 24);
			#elif defined(__i386__)
				register SInt32 inputSample;
				
				// [rdar://4311684] - Fixed 24-bit input convert routine. /thw
				while (numSamplesLeft-- > 1) 
				{	
					inputSample = (* (UInt32 *)inputBuf24) & 0x00FFFFFF;
					// Sign extend if necessary
					if (inputSample > 0x7FFFFF)
					{
						inputSample |= 0xFF000000;
					}
					inputBuf24 += 3;
					*(floatDestBuf++) = (float)inputSample * kOneOverMaxSInt24Value;
				}
				// Convert last sample. The following line does the same work as above without going over the edge of the buffer.
				inputSample = SInt32 ((UInt32 (*(UInt16 *) inputBuf24) & 0x0000FFFF) | (SInt32 (*(inputBuf24 + 2)) << 16));
				*(floatDestBuf++) = (float)inputSample * kOneOverMaxSInt24Value;
			#endif

			break;
		case 32:
			register SInt32 *inputBuf32;
			inputBuf32 = &(((SInt32 *)sampleBuf)[firstSampleFrame * streamFormat->fNumChannels]);

			#if defined(__ppc__)
				SwapInt32ToFloat32(inputBuf32, floatDestBuf, numSamplesLeft, 32);
			#elif defined(__i386__)
				while (numSamplesLeft-- > 0) {	
					*(floatDestBuf++) = (float)(*(inputBuf32++)) * kOneOverMaxSInt32Value;
				}
			#endif

			break;
	}

    return kIOReturnSuccess;
}
#if FLOATLIB
/*
	***CoeffsFilterOrder2***

	This function fills in the order2 filter coefficients table used in the MonoFilter & StereoFilter functions
	Float32  *Coeff			: is a pointer to the coefficients table
	Float32  CutOffFreq		: is the cut off frequency of the filter (in Hertz)
	Float32  AttAtCutOffFreq: is the attenuation at the cut off frequency (in linear)
	Float64  SamplingRate	: is the sampling rate frequency (in Hertz)
*/
void CoeffsFilterOrder2 (Float32 *Coeff, Float32 CutOffFreq, Float32 AttAtCutOffFreq , Float64 SamplingRate)
{
	Float32	k, nu0, pi=3.14159, Att, norm;

	nu0 = (Float32) (CutOffFreq / SamplingRate);
	Att = 1 / AttAtCutOffFreq;
	k = 1/(tan(pi*nu0));
	norm = k*(k+Att)+1;

	/*
	the first 3 coefficients are Num[0], Num[1] & Num[2] in that order
	the last 2 coeffients are Den[1] & Den[2]
	where [.] is the z exposant
	*/
	Coeff[0] = 1.0 / norm;
	Coeff[1] = 2.0 / norm;
	Coeff[2] = 1.0 / norm;
	Coeff[3] = 2*(1-k*k) / norm;
	Coeff[4] = (k*(k-Att)+1) / norm;

	return;
}
#else
/*
    ***CoeffsFilterOrder2Table***

    This function choose an order2 filter coefficients table used in the MonoFilter & StereoFilter functions
    The coefficients table depend on the sampling rate
    Float32  *Coeff		: is a pointer on the coefficients table
    Float64  SamplingRate	: is the sampling rate frequency (in Hertz)
    return: - FALSE if the sampling rate frequency doesn't exist
            - TRUE  otherwise...
*/
Boolean CoeffsFilterOrder2Table (Float32 *Coeff, UInt32 samplingRate)
{
    Boolean 	success = TRUE;

    switch ( samplingRate )
    {
        case 8000:  Coeff[0] =  0.00208054389804601669;
                    Coeff[1] =  0.00416108779609203339;
                    Coeff[2] =  0.00208054389804601669;
                    Coeff[3] = -1.86687481403350830078;
                    Coeff[4] =  0.87519699335098266602;
                    break;
        case 11025: Coeff[0] =  0.00111490569543093443;
                    Coeff[1] =  0.00222981139086186886;
                    Coeff[2] =  0.00111490569543093443;
                    Coeff[3] = -1.90334117412567138672;
                    Coeff[4] =  0.90780085325241088867;
                    break;
        case 22050: Coeff[0] =  0.00028538206242956221;
                    Coeff[1] =  0.00057076412485912442;
                    Coeff[2] =  0.00028538206242956221;
                    Coeff[3] = -1.95164430141448974609;
                    Coeff[4] =  0.95278578996658325195;
                    break;
        case 44100: Coeff[0] =  0.00007220284896902740;
                    Coeff[1] =  0.00014440569793805480;
                    Coeff[2] =  0.00007220284896902740;
                    Coeff[3] = -1.97581851482391357422;
                    Coeff[4] =  0.97610741853713989258;
                    break;
        case 48000: Coeff[0] =  0.00006100598693592474;
                    Coeff[1] =  0.00012201197387184948;
                    Coeff[2] =  0.00006100598693592474;
                    Coeff[3] = -1.97778332233428955078;
                    Coeff[4] =  0.97802722454071044922;
                    break;
        case 96000: Coeff[0] =  0.00001533597242087126;
                    Coeff[1] =  0.00003067194484174252;
                    Coeff[2] =  0.00001533597242087126;
                    Coeff[3] = -1.98889136314392089844;
                    Coeff[4] =  0.98895263671875000000;
                    break;
        default:    // debugIOLog("\nNot a registered frequency...");
                    success = FALSE;
                    break;
    }

    return(success);
}
#endif


// aml 2.18.02 adding fourth order coefficient setting functions
Boolean Set4thOrderCoefficients (Float32 *b0, Float32 *b1, Float32 *b2, Float32 *a1, Float32 *a2, UInt32 samplingRate)
{
    Boolean 	success = TRUE;

    switch ( samplingRate )
    {
        case 8000:  *b0 =  0.00782020803350;
                    *b1 =  0.01564041606699;
                    *b2 =  0.00782020803350;
                    *a1 = -1.73472576880928;
                    *a2 =  0.76600660094326;
                    break;
       case 11025:  *b0 =  0.00425905333005;
                    *b1 =  0.00851810666010;
                    *b2 =  0.00425905333005;
                    *a1 = -1.80709136077571;
                    *a2 =  0.82412757409590;
                    break;
       case 22050:  *b0 =  0.00111491512001;
                    *b1 =  0.00222983024003;
                    *b2 =  0.00111491512001;
                    *a1 = -1.90335434048751;
                    *a2 =  0.90781400096756;
                    break;
        case 44100: *b0 =  0.00028538351548666;
                    *b1 =  0.00057076703097332;
                    *b2 =  0.00028538351548666;
                    *a1 = -1.95165117996464;
                    *a2 =  0.95279271402659;
                    break;
       case 48000:  *b0 =  0.00024135904904198;
                    *b1 =  0.00048271809808396;
                    *b2 =  0.00024135904904198;
                    *a1 = -1.95557824031504;
                    *a2 =  0.95654367651120;
                    break;
        case 96000: *b0 =  0.00006100617875806425;
                    *b1 =  0.0001220123575161285;
                    *b2 =  0.00006100617875806425;
                    *a1 = -1.977786483776763;
                    *a2 =  0.9780305084917958;
                    break;
        default:    // debugIOLog("\nNot a registered frequency...");
                    success = FALSE;
                    break;
    }

    return(success);
}

// aml 2.18.02 adding 4th order phase compensator coefficient setting function
// this function sets the parameters of a second order all-pass filter that is used to compensate for the phase
// shift of the 4th order lowpass IIR filter used in the iSub crossover.  Note that a0 and b2 are both 1.0.
Boolean Set4thOrderPhaseCompCoefficients (Float32 *b0, Float32 *b1, Float32 *a1, Float32 *a2, UInt32 samplingRate)
{
    Boolean 	success = TRUE;

    switch ( samplingRate )
    {
        case 8000:  *a1 = -1.734725768809275;
                    *a2 =  0.7660066009432638;
                    *b0 =  *a2;
                    *b1 =  *a1;
                    break;
        case 11025: *a1 = -1.807091360775707;
                    *a2 =  0.8241275740958973;
                    *b0 =  *a2;
                    *b1 =  *a1;
                    break;
        case 22050: *a1 = -1.903354340487510;
                    *a2 =  0.9078140009675627;
                    *b0 =  *a2;
                    *b1 =  *a1;
                    break;
        case 44100: *a1 = -1.951651179964643;
                    *a2 =  0.9527927140265903;
                    *b0 =  *a2;
                    *b1 =  *a1;
                    break;
        case 48000: *a1 = -1.955578240315035;
                    *a2 =  0.9565436765112033;
                    *b0 =  *a2;
                    *b1 =  *a1;
                    break;
        case 96000: *a1 = -1.977786483776763;
                    *a2 =  0.9780305084917958;
                    *b0 =  *a2;
                    *b1 =  *a1;
                    break;
        default:    // debugIOLog("\nNot a registered frequency...");
                    success = FALSE;
                    break;
    }

    return(success);
}

// aml 2.18.02 adding 2nd order phase compensator coefficient setting function
// this function sets the parameters of a first order all-pass filter that is used to compensate for the phase
// shift when using a 2nd order lowpass IIR filter for the iSub crossover.  Note that a0 and b1 are both 1.0.
Boolean Set2ndOrderPhaseCompCoefficients (float *b0, float *a1, UInt32 samplingRate)
{
    Boolean 	success = TRUE;

    switch ( samplingRate )
    {
        case 8000:  *a1 = -0.7324848836653277;
                    *b0 =  *a1;
                    break;
        case 11025: *a1 = -0.7985051758519318;
                    *b0 =  *a1;
                    break;
        case 22050: *a1 = -0.8939157008398341;
                    *b0 =  *a1;
                    break;
        case 44100: *a1 = -0.9455137594199962;
                    *b0 =  *a1;
                    break;
        case 48000: *a1 = -0.9498297607998617;
                    *b0 =  *a1;
                    break;
        case 96000: *a1 = -0.9745963490718829;
                    *b0 =  *a1;
                    break;
        default:    // debugIOLog("\nNot a registered frequency...");
                    success = FALSE;
                    break;
    }

    return(success);
}

/*
	***MonoFilter***

	Mono Order2 Filter
	Float32  *in	: is a pointer to the entry array -> signal to filter...
	Float32  *low	: is a pointer to the low-pass filtered signal
	Float32  *high 	: is a pointer to the high-pass filtered signal
	UInt32   samples: is the number of samples in each array
	Float64  SamplingRate	: is the sampling rate frequency (in Hertz)
	At the n instant: x is x[n], xx is x[n-1], xxx is x[n-2] (it's the same for y)
*/
void MonoFilter (Float32 *in, Float32 *low, Float32 *high, UInt32 frames, UInt32 samplingRate)
{
	UInt32	idx;
#if !FLOATLIB
	Boolean	success;
#endif
	Float32	LP_Coeff[5] = {0.0, 0.0, 0.0, 0.0, 0.0};
	Float32	x, xx, xxx, y, yy, yyy;

	// init
#if FLOATLIB
	CoeffsFilterOrder2 (LP_Coeff, 120, 1/sqrt(2), 44100);
#else
	success = CoeffsFilterOrder2Table (LP_Coeff, samplingRate);
    if (success == FALSE) goto End;
#endif
	x=xx=xxx=y=yy=yyy=0;
	// convolution
	for ( idx = 0 ; idx < frames ; idx++ )
	{
		x = in[idx];
		// Low-pass filter
		y = (LP_Coeff[0]*x + LP_Coeff[1]*xx + LP_Coeff[2]*xxx - LP_Coeff[3]*yy - LP_Coeff[4]*yyy);
		// Update
		xxx = xx;
		xx = x;
		yyy = yy;
		yy = y;
		// Storage
		low[idx] = y;
		high[idx] = x-y;
	}

#if !FLOATLIB
End:
#endif
	return;
}

/*
    ***StereoFilter***

    Stereo Order2 Filter
    Float32  *in		: is a pointer on the entry array -> signal to filter...
    Float32  *low		: is a pointer on the low-pass filtered signal
    Float32  *high 		: is a pointer on the high-pass filtered signal
    UInt32   samples		: is the number of samples in each array
    Float64  SamplingRate	: is the sampling rate frequency (in Hertz)
    At the n instant: x is x[n], x_1 is x[n-1], x_2 is x[n-2] (it's the same for y)
*/
void StereoFilter (Float32 *in, Float32 *low, Float32 *high, UInt32 frames, UInt32 SamplingRate, PreviousValues *theValue)
{
    UInt32	idx;
    Boolean	success;
    Float32	LP_Coeff[5] = {0.0, 0.0, 0.0, 0.0, 0.0};
    Float32	xl, xr, yl, yr;

    // Get the filter coefficents
    //CoeffsFilterOrder2 (&LP_Coeff, 120, 0.707, SamplingRate); //don't used because of the tan() function
    success = CoeffsFilterOrder2Table (LP_Coeff, SamplingRate);
    if (success == FALSE)  goto End;
    // convolution
    for ( idx = 0 ; idx < frames ; idx ++ )
    {
        xl = in[2*idx];
        xr = in[2*idx+1];
     // Low-pass filter
        yl = (LP_Coeff[0]*xl + LP_Coeff[1]*theValue->xl_1 + LP_Coeff[2]*theValue->xl_2 - LP_Coeff[3]*theValue->yl_1 - LP_Coeff[4]*theValue->yl_2);
        yr = (LP_Coeff[0]*xr + LP_Coeff[1]*theValue->xr_1 + LP_Coeff[2]*theValue->xr_2 - LP_Coeff[3]*theValue->yr_1 - LP_Coeff[4]*theValue->yr_2);
     // Update
        theValue->xl_2 = theValue->xl_1;
        theValue->xr_2 = theValue->xr_1;
        theValue->xl_1 = xl;
        theValue->xr_1 = xr;
        theValue->yl_2 = theValue->yl_1;
        theValue->yr_2 = theValue->yr_1;
        theValue->yl_1 = yl;
        theValue->yr_1 = yr;
     // Storage
        low[2*idx] = yl;
        low[2*idx+1] = yr;
        high[2*idx] = xl-yl;
        high[2*idx+1] = xr-yr;
    }
End:
    return;
}
}


// aml 2.15.02, stereo filter that runs twice for double the rolloff
// tried to make this more efficient and readable that previous filter
// ideally all this will move into a class which will keep it's own storage etc.
void StereoFilter4thOrder (Float32 *in, Float32 *low, Float32 *high, UInt32 frames, UInt32 SamplingRate, PreviousValues *theValue, PreviousValues *theValue2)
{
    UInt32	i;
//    Float32	LP_Coeff[5];
    Float32	inL, inR, outL1, outR1, outL, outR;

    Float32	b0 = 0.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    Float32	inLTap1, inLTap2, inRTap1, inRTap2;
    Float32	outLTap1, outLTap2, outRTap1, outRTap2;
    Float32	inLTap1_2, inLTap2_2, inRTap1_2, inRTap2_2;
    Float32	outLTap1_2, outLTap2_2, outRTap1_2, outRTap2_2;

    // copy to local variables to avoid structure referencing during inner loop
    inLTap1 = theValue->xl_1;
    inLTap2 = theValue->xl_2;
    inRTap1 = theValue->xr_1;
    inRTap2 = theValue->xr_2;

    outLTap1 = theValue->yl_1;
    outLTap2 = theValue->yl_2;
    outRTap1 = theValue->yr_1;
    outRTap2 = theValue->yr_2;

    inLTap1_2 = theValue2->xl_1;
    inLTap2_2 = theValue2->xl_2;
    inRTap1_2 = theValue2->xr_1;
    inRTap2_2 = theValue2->xr_2;

    outLTap1_2 = theValue2->yl_1;
    outLTap2_2 = theValue2->yl_2;
    outRTap1_2 = theValue2->yr_1;
    outRTap2_2 = theValue2->yr_2;

    // set all coefficients
    if (Set4thOrderCoefficients (&b0, &b1, &b2, &a1, &a2, SamplingRate) == FALSE)
        return;
    
// old way:
//    success = CoeffsFilterOrder2Table (LP_Coeff, SamplingRate);
//    if (success == FALSE)  goto End;

//    b0 = LP_Coeff[0];
//    b1 = LP_Coeff[1];
//    b2 = LP_Coeff[2];
//    a1 = LP_Coeff[3];
//    a2 = LP_Coeff[4];

    for ( i = 0 ; i < frames ; i ++ )
    {
        inL = in[2*i];
        inR = in[2*i+1];
        // Low-pass filter first pass
        outL1 = (b0*inL + b1*inLTap1 + b2*inLTap2 - a1*outLTap1 - a2*outLTap2);
        outR1 = (b0*inR + b1*inRTap1 + b2*inRTap2 - a1*outRTap1 - a2*outRTap2);
        // update filter taps
        inLTap2 = inLTap1;
        inRTap2 = inRTap1;
        inLTap1 = inL;
        inRTap1 = inR;
        outLTap2 = outLTap1;
        outRTap2 = outRTap1;
        outLTap1 = outL1;
        outRTap1 = outR1;
        // Low-pass filter second pass
        outL = (b0*outL1 + b1*inLTap1_2 + b2*inLTap2_2 - a1*outLTap1_2 - a2*outLTap2_2);
        outR = (b0*outR1 + b1*inRTap1_2 + b2*inRTap2_2 - a1*outRTap1_2 - a2*outRTap2_2);
        // update filter taps
        inLTap2_2 = inLTap1_2;
        inRTap2_2 = inRTap1_2;
        inLTap1_2 = outL1;
        inRTap1_2 = outR1;
        outLTap2_2 = outLTap1_2;
        outRTap2_2 = outRTap1_2;
        outLTap1_2 = outL;
        outRTap1_2 = outR;

        // Storage
        low[2*i] = outL;
        low[2*i+1] = outR;
        high[2*i] = inL-outL;
        high[2*i+1] = inR-outR;
    }

    // update state structures
    theValue->xl_1 = inLTap1;
    theValue->xl_2 = inLTap2;
    theValue->xr_1 = inRTap1;
    theValue->xr_2 = inRTap2;

    theValue->yl_1 = outLTap1;
    theValue->yl_2 = outLTap2;
    theValue->yr_1 = outRTap1;
    theValue->yr_2 = outRTap2;

    theValue2->xl_1 = inLTap1_2;
    theValue2->xl_2 = inLTap2_2;
    theValue2->xr_1 = inRTap1_2;
    theValue2->xr_2 = inRTap2_2;

    theValue2->yl_1 = outLTap1_2;
    theValue2->yl_2 = outLTap2_2;
    theValue2->yr_1 = outRTap1_2;
    theValue2->yr_2 = outRTap2_2;

    return;
}


// aml 2.15.02, stereo filter that runs twice for double the rolloff
// tried to make this more efficient and readable that previous filter
// ideally all this will move into a class which will keep it's own storage etc.
void StereoFilter4thOrderPhaseComp (Float32 *in, Float32 *low, Float32 *high, UInt32 frames, UInt32 SamplingRate, PreviousValues *section1State, PreviousValues *section2State, PreviousValues *phaseCompState)
{
    UInt32	i;
    Float32	inL, inR, outL1, outR1, outL, outR, inPhaseCompL, inPhaseCompR;

    // shared coefficients for second order sections
    Float32	b0 = 0.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    // coefficients for phase compensator
    Float32	bp0 = 0.0, bp1 = 0.0, ap1 = 0.0, ap2 = 0.0;
    // taps for second order section 1
    Float32	inLTap1, inLTap2, inRTap1, inRTap2;
    Float32	outLTap1, outLTap2, outRTap1, outRTap2;
    // taps for second order section 2
    Float32	inLTap1_2, inLTap2_2, inRTap1_2, inRTap2_2;
    Float32	outLTap1_2, outLTap2_2, outRTap1_2, outRTap2_2;
    // taps for phase compensator
    Float32	inLTap1_p, inLTap2_p, inRTap1_p, inRTap2_p;
    Float32	outLTap1_p, outLTap2_p, outRTap1_p, outRTap2_p;

    // copy to state local variables to avoid structure referencing during inner loop
    // section 1
    inLTap1 = section1State->xl_1;
    inLTap2 = section1State->xl_2;
    inRTap1 = section1State->xr_1;
    inRTap2 = section1State->xr_2;

    outLTap1 = section1State->yl_1;
    outLTap2 = section1State->yl_2;
    outRTap1 = section1State->yr_1;
    outRTap2 = section1State->yr_2;
    
    // section 2
    inLTap1_2 = section2State->xl_1;
    inLTap2_2 = section2State->xl_2;
    inRTap1_2 = section2State->xr_1;
    inRTap2_2 = section2State->xr_2;

    outLTap1_2 = section2State->yl_1;
    outLTap2_2 = section2State->yl_2;
    outRTap1_2 = section2State->yr_1;
    outRTap2_2 = section2State->yr_2;
    
    // phase compensator
    inLTap1_p = phaseCompState->xl_1;
    inLTap2_p = phaseCompState->xl_2;
    inRTap1_p = phaseCompState->xr_1;
    inRTap2_p = phaseCompState->xr_2;

    outLTap1_p = phaseCompState->yl_1;
    outLTap2_p = phaseCompState->yl_2;
    outRTap1_p = phaseCompState->yr_1;
    outRTap2_p = phaseCompState->yr_2;
 
    // set all coefficients
    if (Set4thOrderCoefficients (&b0, &b1, &b2, &a1, &a2, SamplingRate) == FALSE)
        return;
    if (Set4thOrderPhaseCompCoefficients (&bp0, &bp1, &ap1, &ap2, SamplingRate) == FALSE)
        return;

    for ( i = 0 ; i < frames ; i ++ )
    {
        inL = in[2*i];
        inR = in[2*i+1];
        
        // Low-pass filter first pass
        outL1 = b0*inL + b1*inLTap1 + b2*inLTap2 - a1*outLTap1 - a2*outLTap2;
        outR1 = b0*inR + b1*inRTap1 + b2*inRTap2 - a1*outRTap1 - a2*outRTap2;
 
        // update section 1 filter taps
        inLTap2 = inLTap1;
        inRTap2 = inRTap1;
        inLTap1 = inL;
        inRTap1 = inR;
        outLTap2 = outLTap1;
        outRTap2 = outRTap1;
        outLTap1 = outL1;
        outRTap1 = outR1;
        
        // Low-pass filter second pass
        outL = b0*outL1 + b1*inLTap1_2 + b2*inLTap2_2 - a1*outLTap1_2 - a2*outLTap2_2;
        outR = b0*outR1 + b1*inRTap1_2 + b2*inRTap2_2 - a1*outRTap1_2 - a2*outRTap2_2;
        
        // update section 2 filter taps
        inLTap2_2 = inLTap1_2;
        inRTap2_2 = inRTap1_2;
        inLTap1_2 = outL1;
        inRTap1_2 = outR1;
        outLTap2_2 = outLTap1_2;
        outRTap2_2 = outRTap1_2;
        outLTap1_2 = outL;
        outRTap1_2 = outR;

        // phase compensate the input, note that b2 is 1.0
        inPhaseCompL = bp0*inL + bp1*inLTap1_p + inLTap2_p - ap1*outLTap1_p - ap2*outLTap2_p;
        inPhaseCompR = bp0*inR + bp1*inRTap1_p + inRTap2_p - ap1*outRTap1_p - ap2*outRTap2_p;
        
        // update phase compensate filter taps
        inLTap2_p = inLTap1_p;
        inRTap2_p = inRTap1_p;
        inLTap1_p = inL;
        inRTap1_p = inR;
        outLTap2_p = outLTap1_p;
        outRTap2_p = outRTap1_p;
        outLTap1_p = inPhaseCompL;
        outRTap1_p = inPhaseCompR;

        // Storage
        low[2*i] = outL;
        low[2*i+1] = outR;
        high[2*i] = inPhaseCompL-outL;
        high[2*i+1] = inPhaseCompR-outR;
    }

    // update state structures
    
    // section 1 state
    section1State->xl_1 = inLTap1;
    section1State->xl_2 = inLTap2;
    section1State->xr_1 = inRTap1;
    section1State->xr_2 = inRTap2;

    section1State->yl_1 = outLTap1;
    section1State->yl_2 = outLTap2;
    section1State->yr_1 = outRTap1;
    section1State->yr_2 = outRTap2;

    // section 2 state
    section2State->xl_1 = inLTap1_2;
    section2State->xl_2 = inLTap2_2;
    section2State->xr_1 = inRTap1_2;
    section2State->xr_2 = inRTap2_2;

    section2State->yl_1 = outLTap1_2;
    section2State->yl_2 = outLTap2_2;
    section2State->yr_1 = outRTap1_2;
    section2State->yr_2 = outRTap2_2;

    // phase compensator state
    phaseCompState->xl_1 = inLTap1_p;
    phaseCompState->xl_2 = inLTap2_p;
    phaseCompState->xr_1 = inRTap1_p;
    phaseCompState->xr_2 = inRTap2_p;

    phaseCompState->yl_1 = outLTap1_p;
    phaseCompState->yl_2 = outLTap2_p;
    phaseCompState->yr_1 = outRTap1_p;
    phaseCompState->yr_2 = outRTap2_p;

    return;
}

// aml new routines [3034710]
#pragma mark еее New clipping routines
#if	defined(__ppc__)

// this behaves incorrectly in Float32ToSwapInt24 if not declared volatile
#define __lwbrx( index, base )	({ register long result; __asm__ __volatile__("lwbrx %0, %1, %2" : "=r" (result) : "b%" (index), "r" (base) : "memory" ); result; } )

#define __lhbrx(index, base)	\
  ({ register signed short lhbrxResult; \
	 __asm__ ("lhbrx %0, %1, %2" : "=r" (lhbrxResult) : "b%" (index), "r" (base) : "memory"); \
	 /*return*/ lhbrxResult; } )
	// dsw: make signed to get sign-extension

#define __rlwimi( rA, rS, cnt, mb, me ) \
	({ __asm__ __volatile__( "rlwimi %0, %2, %3, %4, %5" : "=r" (rA) : "0" (rA), "r" (rS), "n" (cnt), "n" (mb), "n" (me) ); /*return*/ rA; })

#define __stwbrx( value, index, base ) \
	__asm__( "stwbrx %0, %1, %2" : : "r" (value), "b%" (index), "r" (base) : "memory" )

#define __rlwimi_volatile( rA, rS, cnt, mb, me ) \
	({ __asm__ __volatile__( "rlwimi %0, %2, %3, %4, %5" : "=r" (rA) : "0" (rA), "r" (rS), "n" (cnt), "n" (mb), "n" (me) ); /*return*/ rA; })

#define __stfiwx( value, offset, addr )			\
	asm( "stfiwx %0, %1, %2" : /*no result*/ : "f" (value), "b%" (offset), "r" (addr) : "memory" )

static inline double __fctiw( register double B )
{
	register double result;
	asm( "fctiw %0, %1" : "=f" (result) : "f" (B)  );
	return result;
}

// aml, adding 8 bit version
void Int8ToFloat32( SInt8 *src, float *dest, unsigned int count )
{
	register float bias;
	register long exponentMask = ((0x97UL - 8) << 23) | 0x8000;	//FP exponent + bias for sign
	register long int0, int1, int2, int3;
	register float float0, float1, float2, float3;
	register unsigned long loopCount;
	union
	{
		float	f;
		long	i;
	}exponent;

	exponent.i = exponentMask;
	bias = exponent.f;

	src--;
	if( count >= 8 )
	{
		//Software Cycle 1
		int0 = (++src)[0];

		//Software Cycle 2
		int1 = (++src)[0];
		int0 += exponentMask;

		//Software Cycle 3
		int2 = (++src)[0];
		int1 += exponentMask;
		((long*) dest)[0] = int0;

		//Software Cycle 4
		int3 = (++src)[0];
		int2 += exponentMask;
		((long*) dest)[1] = int1;
		//delay one loop for the store to complete

		//Software Cycle 5
		int0 = (++src)[0];
		int3 += exponentMask;
		((long*) dest)[2] = int2;
		float0 = dest[0];

		//Software Cycle 6
		int1 = (++src)[0];
		int0 += exponentMask;
		((long*) dest)[3] = int3;
		float1 = dest[1];
		float0 -= bias;

		//Software Cycle 7
		int2 = (++src)[0];
		int1 += exponentMask;
		((long*) dest)[4] = int0;
		float2 = dest[2];
		float1 -= bias;

		dest--;
		//Software Cycle 8
		int3 = (++src)[0];
		int2 += exponentMask;
		((long*) dest)[6] = int1;
		float3 = dest[4];
		float2 -= bias;
		(++dest)[0] = float0;

		count -= 8;
		loopCount = count / 4;
		count &= 3;
		while( loopCount-- )
		{

			//Software Cycle A
			int0 = (++src)[0];
			int3 += exponentMask;
			((long*) dest)[6] = int2;
			float0 = dest[4];
			float3 -= bias;
			(++dest)[0] = float1;

			//Software Cycle B
			int1 = (++src)[0];
			int0 += exponentMask;
			((long*) dest)[6] = int3;
			float1 = dest[4];
			float0 -= bias;
			(++dest)[0] = float2;

			//Software Cycle C
			int2 = (++src)[0];
			int1 += exponentMask;
			((long*) dest)[6] = int0;
			float2 = dest[4];
			float1 -= bias;
			(++dest)[0] = float3;

			//Software Cycle D
			int3 = (++src)[0];
			int2 += exponentMask;
			((long*) dest)[6] = int1;
			float3 = dest[4];
			float2 -= bias;
			(++dest)[0] = float0;
		}

		//Software Cycle 7
		int3 += exponentMask;
		((long*) dest)[6] = int2;
		float0 = dest[4];
		float3 -= bias;
		(++dest)[0] = float1;

		//Software Cycle 6
		((long*) dest)[6] = int3;
		float1 = dest[4];
		float0 -= bias;
		(++dest)[0] = float2;

		//Software Cycle 5
		float2 = dest[4];
		float1 -= bias;
		(++dest)[0] = float3;

		//Software Cycle 4
		float3 = dest[4];
		float2 -= bias;
		(++dest)[0] = float0;

		//Software Cycle 3
		float3 -= bias;
		(++dest)[0] = float1;

		//Software Cycle 2
		(++dest)[0] = float2;

		//Software Cycle 1
		(++dest)[0] = float3;

		dest++;
	}


	while( count-- )
	{
		register long value = (++src)[0];
		value += exponentMask;
		((long*) dest)[0] = value;
		dest[0] -= bias;
		dest++;
	}
}

// bitDepth may be less than 16, e.g. for low-aligned 12 bit samples
void NativeInt16ToFloat32( signed short *src, float *dest, unsigned int count, int bitDepth )
{
	register float bias;
	register long exponentMask = ((0x97UL - bitDepth) << 23) | 0x8000;	//FP exponent + bias for sign
	register long int0, int1, int2, int3;
	register float float0, float1, float2, float3;
	register unsigned long loopCount;
	union
	{
		float	f;
		long	i;
	} exponent;

	exponent.i = exponentMask;
	bias = exponent.f;

	src--;
	if( count >= 8 )
	{
		//Software Cycle 1
		int0 = (++src)[0];

		//Software Cycle 2
		int1 = (++src)[0];
		int0 += exponentMask;

		//Software Cycle 3
		int2 = (++src)[0];
		int1 += exponentMask;
		((long*) dest)[0] = int0;

		//Software Cycle 4
		int3 = (++src)[0];
		int2 += exponentMask;
		((long*) dest)[1] = int1;
		//delay one loop for the store to complete

		//Software Cycle 5
		int0 = (++src)[0];
		int3 += exponentMask;
		((long*) dest)[2] = int2;
		float0 = dest[0];

		//Software Cycle 6
		int1 = (++src)[0];
		int0 += exponentMask;
		((long*) dest)[3] = int3;
		float1 = dest[1];
		float0 -= bias;

		//Software Cycle 7
		int2 = (++src)[0];
		int1 += exponentMask;
		((long*) dest)[4] = int0;
		float2 = dest[2];
		float1 -= bias;

		dest--;
		//Software Cycle 8
		int3 = (++src)[0];
		int2 += exponentMask;
		((long*) dest)[6] = int1;
		float3 = dest[4];
		float2 -= bias;
		(++dest)[0] = float0;

		count -= 8;
		loopCount = count / 4;
		count &= 3;
		while( loopCount-- )
		{

			//Software Cycle A
			int0 = (++src)[0];
			int3 += exponentMask;
			((long*) dest)[6] = int2;
			float0 = dest[4];
			float3 -= bias;
			(++dest)[0] = float1;

			//Software Cycle B
			int1 = (++src)[0];
			int0 += exponentMask;
			((long*) dest)[6] = int3;
			float1 = dest[4];
			float0 -= bias;
			(++dest)[0] = float2;

			//Software Cycle C
			int2 = (++src)[0];
			int1 += exponentMask;
			((long*) dest)[6] = int0;
			float2 = dest[4];
			float1 -= bias;
			(++dest)[0] = float3;

			//Software Cycle D
			int3 = (++src)[0];
			int2 += exponentMask;
			((long*) dest)[6] = int1;
			float3 = dest[4];
			float2 -= bias;
			(++dest)[0] = float0;
		}

		//Software Cycle 7
		int3 += exponentMask;
		((long*) dest)[6] = int2;
		float0 = dest[4];
		float3 -= bias;
		(++dest)[0] = float1;

		//Software Cycle 6
		((long*) dest)[6] = int3;
		float1 = dest[4];
		float0 -= bias;
		(++dest)[0] = float2;

		//Software Cycle 5
		float2 = dest[4];
		float1 -= bias;
		(++dest)[0] = float3;

		//Software Cycle 4
		float3 = dest[4];
		float2 -= bias;
		(++dest)[0] = float0;

		//Software Cycle 3
		float3 -= bias;
		(++dest)[0] = float1;

		//Software Cycle 2
		(++dest)[0] = float2;

		//Software Cycle 1
		(++dest)[0] = float3;

		dest++;
	}


	while( count-- )
	{
		register long value = (++src)[0];
		value += exponentMask;
		((long*) dest)[0] = value;
		dest[0] -= bias;
		dest++;
	}
}


// bitDepth may be less than 16, e.g. for low-aligned 12 bit samples
void SwapInt16ToFloat32( signed short *src, float *dest, unsigned int count, int bitDepth )
{
	register float bias;
	register long exponentMask = ((0x97UL - bitDepth) << 23) | 0x8000;	//FP exponent + bias for sign
	register long int0, int1, int2, int3;
	register float float0, float1, float2, float3;
	register unsigned long loopCount;
	union
	{
		float	f;
		long	i;
	}exponent;

	exponent.i = exponentMask;
	bias = exponent.f;

	src--;
	if( count >= 8 )
	{
		//Software Cycle 1
		int0 = __lhbrx(0, ++src);

		//Software Cycle 2
		int1 = __lhbrx(0, ++src);
		int0 += exponentMask;

		//Software Cycle 3
		int2 = __lhbrx(0, ++src);
		int1 += exponentMask;
		((long*) dest)[0] = int0;

		//Software Cycle 4
		int3 = __lhbrx(0, ++src);
		int2 += exponentMask;
		((long*) dest)[1] = int1;
		//delay one loop for the store to complete

		//Software Cycle 5
		int0 = __lhbrx(0, ++src);
		int3 += exponentMask;
		((long*) dest)[2] = int2;
		float0 = dest[0];

		//Software Cycle 6
		int1 = __lhbrx(0, ++src);
		int0 += exponentMask;
		((long*) dest)[3] = int3;
		float1 = dest[1];
		float0 -= bias;

		//Software Cycle 7
		int2 = __lhbrx(0, ++src);
		int1 += exponentMask;
		((long*) dest)[4] = int0;
		float2 = dest[2];
		float1 -= bias;

		dest--;
		//Software Cycle 8
		int3 = __lhbrx(0, ++src);
		int2 += exponentMask;
		((long*) dest)[6] = int1;
		float3 = dest[4];
		float2 -= bias;
		(++dest)[0] = float0;

		count -= 8;
		loopCount = count / 4;
		count &= 3;
		while( loopCount-- )
		{

			//Software Cycle A
			int0 = __lhbrx(0, ++src);
			int3 += exponentMask;
			((long*) dest)[6] = int2;
			float0 = dest[4];
			float3 -= bias;
			(++dest)[0] = float1;

			//Software Cycle B
			int1 = __lhbrx(0, ++src);
			int0 += exponentMask;
			((long*) dest)[6] = int3;
			float1 = dest[4];
			float0 -= bias;
			(++dest)[0] = float2;

			//Software Cycle C
			int2 = __lhbrx(0, ++src);
			int1 += exponentMask;
			((long*) dest)[6] = int0;
			float2 = dest[4];
			float1 -= bias;
			(++dest)[0] = float3;

			//Software Cycle D
			int3 = __lhbrx(0, ++src);
			int2 += exponentMask;
			((long*) dest)[6] = int1;
			float3 = dest[4];
			float2 -= bias;
			(++dest)[0] = float0;
		}

		//Software Cycle 7
		int3 += exponentMask;
		((long*) dest)[6] = int2;
		float0 = dest[4];
		float3 -= bias;
		(++dest)[0] = float1;

		//Software Cycle 6
		((long*) dest)[6] = int3;
		float1 = dest[4];
		float0 -= bias;
		(++dest)[0] = float2;

		//Software Cycle 5
		float2 = dest[4];
		float1 -= bias;
		(++dest)[0] = float3;

		//Software Cycle 4
		float3 = dest[4];
		float2 -= bias;
		(++dest)[0] = float0;

		//Software Cycle 3
		float3 -= bias;
		(++dest)[0] = float1;

		//Software Cycle 2
		(++dest)[0] = float2;

		//Software Cycle 1
		(++dest)[0] = float3;

		dest++;
	}


	while( count-- )
	{
		register long value = __lhbrx(0, ++src);
		value += exponentMask;
		((long*) dest)[0] = value;
		dest[0] -= bias;
		dest++;
	}
}

void NativeInt24ToFloat32( long *src, float *dest, unsigned int count, int bitDepth )
{
	union
	{
		double			d[4];
		unsigned int	i[8];
	} transfer;
	register double			dBias;
	register unsigned int	loopCount, load0SignMask;
	register unsigned long	load0, load1, load2;
	register unsigned long	int0, int1, int2, int3;
	register double			d0, d1, d2, d3;
	register float		f0, f1, f2, f3;

	transfer.i[0] = transfer.i[2] = transfer.i[4] = transfer.i[6] = (0x434UL - bitDepth) << 20; //0x41C00000UL;
	transfer.i[1] = 0x00800000;
	int0 = int1 = int2 = int3 = 0;
	load0SignMask = 0x80000080UL;

	dBias = transfer.d[0];

	src--;
	dest--;

	if( count >= 8 )
	{
		count -= 8;
		loopCount = count / 4;
		count &= 3;

		//Virtual cycle 1
		load0 = (++src)[0];

		//Virtual cycle 2
		load1 = (++src)[0];
		load0 ^= load0SignMask;

		//Virtual cycle 3
		load2 = (++src)[0];
		load1 ^= 0x00008000UL;
		int0 = load0 >> 8;
		int1 = __rlwimi( int1, load0, 16, 8, 15);

		//Virtual cycle 4
		//No load3 -- already loaded last cycle
		load2 ^= 0x00800000UL;
		int1 = __rlwimi( int1, load1, 16, 16, 31);
		int2 = __rlwimi( int2, load1, 8, 8, 23 );
		transfer.i[1] = int0;

		//Virtual cycle 5
		load0 = (++src)[0];
		int2 = __rlwimi( int2, load2, 8, 24, 31 );
		int3 = load2 & 0x00FFFFFF;
		transfer.i[3] = int1;

		//Virtual cycle 6
		load1 = (++src)[0];
		load0 ^= load0SignMask;
		transfer.i[5] = int2;
		d0 = transfer.d[0];

		//Virtual cycle 7
		load2 = (++src)[0];
		load1 ^= 0x00008000UL;
		int0 = load0 >> 8;
		int1 = __rlwimi( int1, load0, 16, 8, 15 );
		transfer.i[7] = int3;
		d1 = transfer.d[1];
		d0 -= dBias;

		//Virtual cycle 8
		//No load3 -- already loaded last cycle
		load2 ^= 0x00800000UL;
		int1 = __rlwimi( int1, load1, 16, 16, 31);
		int2 = __rlwimi( int2, load1, 8, 8, 23 );
		transfer.i[1] = int0;
		d2 = transfer.d[2];
		d1 -= dBias;
		f0 = d0;

		while( loopCount-- )
		{
			//Virtual cycle A
			load0 = (++src)[0];
			int2 = __rlwimi( int2, load2, 8, 24, 31 );
			int3 = load2 & 0x00FFFFFF;
			transfer.i[3] = int1;
			d3 = transfer.d[3];
			d2 -= dBias;
			f1 = d1;
			(++dest)[0] = f0;

			//Virtual cycle B
			load1 = (++src)[0];
			load0 ^= load0SignMask;
			transfer.i[5] = int2;
			d0 = transfer.d[0];
			d3 -= dBias;
			f2 = d2;
			(++dest)[0] = f1;

			//Virtual cycle C
			load2 = (++src)[0];
			load1 ^= 0x00008000UL;
			int0 = load0 >> 8;
			int1 = __rlwimi( int1, load0, 16, 8, 15 );
			transfer.i[7] = int3;
			d1 = transfer.d[1];
			d0 -= dBias;
			f3 = d3;
			(++dest)[0] = f2;

			//Virtual cycle D
			load2 ^= 0x00800000UL;
			int1 = __rlwimi( int1, load1, 16, 16, 31);
			int2 = __rlwimi( int2, load1, 8, 8, 23 );
			transfer.i[1] = int0;
			d2 = transfer.d[2];
			d1 -= dBias;
			f0 = d0;
			(++dest)[0] = f3;
		}

		//Virtual cycle 8
		int2 = __rlwimi( int2, load2, 8, 24, 31 );
		int3 = load2 & 0x00FFFFFF;
		transfer.i[3] = int1;
		d3 = transfer.d[3];
		d2 -= dBias;
		f1 = d1;
		(++dest)[0] = f0;

		//Virtual cycle 7
		transfer.i[5] = int2;
		d0 = transfer.d[0];
		d3 -= dBias;
		f2 = d2;
		(++dest)[0] = f1;

		//Virtual cycle 6
		transfer.i[7] = int3;
		d1 = transfer.d[1];
		d0 -= dBias;
		f3 = d3;
		(++dest)[0] = f2;

		//Virtual cycle 5
		d2 = transfer.d[2];
		d1 -= dBias;
		f0 = d0;
		(++dest)[0] = f3;

		//Virtual cycle 4
		d3 = transfer.d[3];
		d2 -= dBias;
		f1 = d1;
		(++dest)[0] = f0;

		//Virtual cycle 3
		d3 -= dBias;
		f2 = d2;
		(++dest)[0] = f1;

		//Virtual cycle 2
		f3 = d3;
		(++dest)[0] = f2;

		//Virtual cycle 1
		(++dest)[0] = f3;
	}

	src = (long*) ((char*) src + 1 );
	while( count-- )
	{
		int0 = ((unsigned char*)(src = (long*)( (char*) src + 3 )))[0];
		int1 = ((unsigned short*)( (char*) src + 1 ))[0];
		int0 ^= 0x00000080UL;
		int1 = __rlwimi( int1, int0, 16, 8, 15 );
		transfer.i[1] = int1;

		d0 = transfer.d[0];
		d0 -= dBias;
		f0 = d0;
		(++dest)[0] = f0;
   }
}

// CAUTION: bitDepth is ignored
void SwapInt24ToFloat32( long *src, float *dest, unsigned int count, int bitDepth )
{
	union
	{
		double		d[4];
		unsigned int	i[8];
	}transfer;
	register double			dBias;
	register unsigned int	loopCount, load2SignMask;
	register unsigned long	load0, load1, load2;
	register unsigned long	int0, int1, int2, int3;
	register double			d0, d1, d2, d3;
	register float		f0, f1, f2, f3;

	transfer.i[0] = transfer.i[2] = transfer.i[4] = transfer.i[6] = 0x41400000UL;
	transfer.i[1] = 0x80000000;
	int0 = int1 = int2 = int3 = 0;
	load2SignMask = 0x80000080UL;

	dBias = transfer.d[0];

	src--;
	dest--;

	if( count >= 8 )
	{
		count -= 8;
		loopCount = count / 4;
		count &= 3;

		//Virtual cycle 1
		load0 = (++src)[0];

		//Virtual cycle 2
		load1 = (++src)[0];
		load0 ^= 0x00008000;

		//Virtual cycle 3
		load2 = (++src)[0];
		load1 ^= 0x00800000UL;
		int0 = load0 >> 8;
		int1 = __rlwimi( int1, load0, 16, 8, 15);

		//Virtual cycle 4
		//No load3 -- already loaded last cycle
		load2 ^= load2SignMask;
		int1 = __rlwimi( int1, load1, 16, 16, 31);
		int2 = __rlwimi( int2, load1, 8, 8, 23 );
		__stwbrx( int0, 0, &transfer.i[1]);

		//Virtual cycle 5
		load0 = (++src)[0];
		int2 = __rlwimi( int2, load2, 8, 24, 31 );
		int3 = load2 & 0x00FFFFFF;
		__stwbrx( int1, 0, &transfer.i[3]);

		//Virtual cycle 6
		load1 = (++src)[0];
		load0 ^= 0x00008000;
		__stwbrx( int2, 0, &transfer.i[5]);
		d0 = transfer.d[0];

		//Virtual cycle 7
		load2 = (++src)[0];
		load1 ^= 0x00800000UL;
		int0 = load0 >> 8;
		int1 = __rlwimi( int1, load0, 16, 8, 15 );
		__stwbrx( int3, 0, &transfer.i[7]);
		d1 = transfer.d[1];
		d0 -= dBias;

		//Virtual cycle 8
		//No load3 -- already loaded last cycle
		load2 ^= load2SignMask;
		int1 = __rlwimi( int1, load1, 16, 16, 31);
		int2 = __rlwimi( int2, load1, 8, 8, 23 );
		__stwbrx( int0, 0, &transfer.i[1]);
		d2 = transfer.d[2];
		d1 -= dBias;
		f0 = d0;

		while( loopCount-- )
		{
			//Virtual cycle A
			load0 = (++src)[0];
			int2 = __rlwimi( int2, load2, 8, 24, 31 );
			int3 = load2 & 0x00FFFFFF;
			__stwbrx( int1, 0, &transfer.i[3]);
			d3 = transfer.d[3];
			d2 -= dBias;
			f1 = d1;
			(++dest)[0] = f0;

			//Virtual cycle B
			load1 = (++src)[0];
			load0 ^= 0x00008000;
			__stwbrx( int2, 0, &transfer.i[5]);
			d0 = transfer.d[0];
			d3 -= dBias;
			f2 = d2;
			(++dest)[0] = f1;

			//Virtual cycle C
			load2 = (++src)[0];
			load1 ^= 0x00800000UL;
			int0 = load0 >> 8;
			int1 = __rlwimi( int1, load0, 16, 8, 15 );
			__stwbrx( int3, 0, &transfer.i[7]);
			d1 = transfer.d[1];
			d0 -= dBias;
			f3 = d3;
			(++dest)[0] = f2;

			//Virtual cycle D
			load2 ^= load2SignMask;
			int1 = __rlwimi( int1, load1, 16, 16, 31);
			int2 = __rlwimi( int2, load1, 8, 8, 23 );
			__stwbrx( int0, 0, &transfer.i[1]);
			d2 = transfer.d[2];
			d1 -= dBias;
			f0 = d0;
			(++dest)[0] = f3;
		}

		//Virtual cycle 8
		int2 = __rlwimi( int2, load2, 8, 24, 31 );
		int3 = load2 & 0x00FFFFFF;
		__stwbrx( int1, 0, &transfer.i[3]);
		d3 = transfer.d[3];
		d2 -= dBias;
		f1 = d1;
		(++dest)[0] = f0;

		//Virtual cycle 7
		__stwbrx( int2, 0, &transfer.i[5]);
		d0 = transfer.d[0];
		d3 -= dBias;
		f2 = d2;
		(++dest)[0] = f1;

		//Virtual cycle 6
		__stwbrx( int3, 0, &transfer.i[7]);
		d1 = transfer.d[1];
		d0 -= dBias;
		f3 = d3;
		(++dest)[0] = f2;

		//Virtual cycle 5
		d2 = transfer.d[2];
		d1 -= dBias;
		f0 = d0;
		(++dest)[0] = f3;

		//Virtual cycle 4
		d3 = transfer.d[3];
		d2 -= dBias;
		f1 = d1;
		(++dest)[0] = f0;

		//Virtual cycle 3
		d3 -= dBias;
		f2 = d2;
		(++dest)[0] = f1;

		//Virtual cycle 2
		f3 = d3;
		(++dest)[0] = f2;

		//Virtual cycle 1
		(++dest)[0] = f3;
	}

	if( count > 0 )
	{

		int1 = ((unsigned char*) src)[6];
		int0 = ((unsigned short*)(++src))[0];
		int1 ^= 0x80;
		int1 = __rlwimi( int1, int0, 8, 8, 23 );
		__stwbrx( int1, 0, &transfer.i[1]);
		d0 = transfer.d[0];
		d0 -= dBias;
		f0 = d0;
		(++dest)[0] = f0;

		src = (long*) ((char*)src - 1 );
		while( --count )
		{
			int0 = (src = (long*)( (char*) src + 3 ))[0];
			int0 ^= 0x80UL;
			int0 &= 0x00FFFFFFUL;
			__stwbrx( int0, 0, &transfer.i[1]);

			d0 = transfer.d[0];
			d0 -= dBias;
			f0 = d0;
			(++dest)[0] = f0;
		}
	}
}

// bitDepth may be less than 32, e.g. for 24 bits low-aligned in 32-bit words
void NativeInt32ToFloat32( long *src, float *dest, unsigned int count, int bitDepth )
{
	union
	{
		double		d[4];
		unsigned int	i[8];
	}transfer;
	register double dBias;
	register unsigned int loopCount;
	register long	int0, int1, int2, int3;
	register double		d0, d1, d2, d3;
	register float	f0, f1, f2, f3;

	transfer.i[0] = transfer.i[2] = transfer.i[4] = transfer.i[6] = (0x434UL - bitDepth) << 20;
		//0x41400000UL;
	transfer.i[1] = 0x80000000;

	dBias = transfer.d[0];

	src--;
	dest--;

	if( count >= 8 )
	{
		count -= 8;
		loopCount = count / 4;
		count &= 3;

		//Virtual cycle 1
		int0 = (++src)[0];

		//Virtual cycle 2
		int1 = (++src)[0];
		int0 ^= 0x80000000UL;

		//Virtual cycle 3
		int2 = (++src)[0];
		int1 ^= 0x80000000UL;
		transfer.i[1] = int0;

		//Virtual cycle 4
		int3 = (++src)[0];
		int2 ^= 0x80000000UL;
		transfer.i[3] = int1;

		//Virtual cycle 5
		int0 = (++src)[0];
		int3 ^= 0x80000000UL;
		transfer.i[5] = int2;

		//Virtual cycle 6
		int1 = (++src)[0];
		int0 ^= 0x80000000UL;
		transfer.i[7] = int3;
		d0 = transfer.d[0];

		//Virtual cycle 7
		int2 = (++src)[0];
		int1 ^= 0x80000000UL;
		transfer.i[1] = int0;
		d1 = transfer.d[1];
		d0 -= dBias;

		//Virtual cycle 8
		int3 = (++src)[0];
		int2 ^= 0x80000000UL;
		transfer.i[3] = int1;
		d2 = transfer.d[2];
		d1 -= dBias;
		f0 = d0;

		while( loopCount-- )
		{
			//Virtual cycle A
			int0 = (++src)[0];
			int3 ^= 0x80000000UL;
			transfer.i[5] = int2;
			d3 = transfer.d[3];
			d2 -= dBias;
			f1 = d1;
			(++dest)[0] = f0;

			//Virtual cycle B
			int1 = (++src)[0];
			int0 ^= 0x80000000UL;
			transfer.i[7] = int3;
			d0 = transfer.d[0];
			d3 -= dBias;
			f2 = d2;
			(++dest)[0] = f1;

			//Virtual cycle C
			int2 = (++src)[0];
			int1 ^= 0x80000000UL;
			transfer.i[1] = int0;
			d1 = transfer.d[1];
			d0 -= dBias;
			f3 = d3;
			(++dest)[0] = f2;

			//Virtual cycle D
			int3 = (++src)[0];
			int2 ^= 0x80000000UL;
			transfer.i[3] = int1;
			d2 = transfer.d[2];
			d1 -= dBias;
			f0 = d0;
			(++dest)[0] = f3;
		}

		//Virtual cycle 8
		int3 ^= 0x80000000UL;
		transfer.i[5] = int2;
		d3 = transfer.d[3];
		d2 -= dBias;
		f1 = d1;
		(++dest)[0] = f0;

		//Virtual cycle 7
		transfer.i[7] = int3;
		d0 = transfer.d[0];
		d3 -= dBias;
		f2 = d2;
		(++dest)[0] = f1;

		//Virtual cycle 6
		d1 = transfer.d[1];
		d0 -= dBias;
		f3 = d3;
		(++dest)[0] = f2;

		//Virtual cycle 5
		d2 = transfer.d[2];
		d1 -= dBias;
		f0 = d0;
		(++dest)[0] = f3;

		//Virtual cycle 4
		d3 = transfer.d[3];
		d2 -= dBias;
		f1 = d1;
		(++dest)[0] = f0;

		//Virtual cycle 3
		d3 -= dBias;
		f2 = d2;
		(++dest)[0] = f1;

		//Virtual cycle 2
		f3 = d3;
		(++dest)[0] = f2;

		//Virtual cycle 1
		(++dest)[0] = f3;
	}


	while( count-- )
	{
		int0 = (++src)[0];
		int0 ^= 0x80000000UL;
		transfer.i[1] = int0;

		d0 = transfer.d[0];
		d0 -= dBias;
		f0 = d0;
		(++dest)[0] = f0;
	}
}

// bitDepth may be less than 32, e.g. for 24 bits low-aligned in 32-bit words
void SwapInt32ToFloat32( long *src, float *dest, unsigned int count, int bitDepth )
{
	union
	{
		double		d[4];
		unsigned int	i[8];
	}transfer;
	register double dBias;
	register unsigned int loopCount;
	register long	int0, int1, int2, int3;
	register double		d0, d1, d2, d3;
	register float	f0, f1, f2, f3;

	transfer.i[0] = transfer.i[2] = transfer.i[4] = transfer.i[6] = (0x434UL - bitDepth) << 20;
		//0x41400000UL;
	transfer.i[1] = 0x80000000;

	dBias = transfer.d[0];

	src--;
	dest--;

	if( count >= 8 )
	{
		count -= 8;
		loopCount = count / 4;
		count &= 3;

		//Virtual cycle 1
		int0 = __lwbrx( 0, ++src);

		//Virtual cycle 2
		int1 = __lwbrx( 0, ++src);
		int0 ^= 0x80000000UL;

		//Virtual cycle 3
		int2 = __lwbrx( 0, ++src);
		int1 ^= 0x80000000UL;
		transfer.i[1] = int0;

		//Virtual cycle 4
		int3 = __lwbrx( 0, ++src);
		int2 ^= 0x80000000UL;
		transfer.i[3] = int1;

		//Virtual cycle 5
		int0 = __lwbrx( 0, ++src);
		int3 ^= 0x80000000UL;
		transfer.i[5] = int2;

		//Virtual cycle 6
		int1 = __lwbrx( 0, ++src);
		int0 ^= 0x80000000UL;
		transfer.i[7] = int3;
		d0 = transfer.d[0];

		//Virtual cycle 7
		int2 = __lwbrx( 0, ++src);
		int1 ^= 0x80000000UL;
		transfer.i[1] = int0;
		d1 = transfer.d[1];
		d0 -= dBias;

		//Virtual cycle 8
		int3 = __lwbrx( 0, ++src);
		int2 ^= 0x80000000UL;
		transfer.i[3] = int1;
		d2 = transfer.d[2];
		d1 -= dBias;
		f0 = d0;

		while( loopCount-- )
		{
			//Virtual cycle A
			int0 = __lwbrx( 0, ++src);
			int3 ^= 0x80000000UL;
			transfer.i[5] = int2;
			d3 = transfer.d[3];
			d2 -= dBias;
			f1 = d1;
			(++dest)[0] = f0;

			//Virtual cycle B
			int1 = __lwbrx( 0, ++src);
			int0 ^= 0x80000000UL;
			transfer.i[7] = int3;
			d0 = transfer.d[0];
			d3 -= dBias;
			f2 = d2;
			(++dest)[0] = f1;

			//Virtual cycle C
			int2 = __lwbrx( 0, ++src);
			int1 ^= 0x80000000UL;
			transfer.i[1] = int0;
			d1 = transfer.d[1];
			d0 -= dBias;
			f3 = d3;
			(++dest)[0] = f2;

			//Virtual cycle D
			int3 = __lwbrx( 0, ++src);
			int2 ^= 0x80000000UL;
			transfer.i[3] = int1;
			d2 = transfer.d[2];
			d1 -= dBias;
			f0 = d0;
			(++dest)[0] = f3;
		}

		//Virtual cycle 8
		int3 ^= 0x80000000UL;
		transfer.i[5] = int2;
		d3 = transfer.d[3];
		d2 -= dBias;
		f1 = d1;
		(++dest)[0] = f0;

		//Virtual cycle 7
		transfer.i[7] = int3;
		d0 = transfer.d[0];
		d3 -= dBias;
		f2 = d2;
		(++dest)[0] = f1;

		//Virtual cycle 6
		d1 = transfer.d[1];
		d0 -= dBias;
		f3 = d3;
		(++dest)[0] = f2;

		//Virtual cycle 5
		d2 = transfer.d[2];
		d1 -= dBias;
		f0 = d0;
		(++dest)[0] = f3;

		//Virtual cycle 4
		d3 = transfer.d[3];
		d2 -= dBias;
		f1 = d1;
		(++dest)[0] = f0;

		//Virtual cycle 3
		d3 -= dBias;
		f2 = d2;
		(++dest)[0] = f1;

		//Virtual cycle 2
		f3 = d3;
		(++dest)[0] = f2;

		//Virtual cycle 1
		(++dest)[0] = f3;
	}


	while( count-- )
	{
		int0 = __lwbrx( 0, ++src);
		int0 ^= 0x80000000UL;
		transfer.i[1] = int0;

		d0 = transfer.d[0];
		d0 -= dBias;
		f0 = d0;
		(++dest)[0] = f0;
	}
}

// aml adding missing function

void Float32ToInt8( float *src, SInt8 *dst, unsigned int count )
{
	register double		scale = 2147483648.0;
	register double		round = 128.0;
	unsigned long		loopCount = count / 4;
	long				buffer[2];
	register float		startingFloat;
	register double 	scaled;
	register double 	converted;
	register SInt8		copy;

	//
	//	The fastest way to do this is to set up a staggered loop that models a 7 stage virtual pipeline:
	//
	//		stage 1:		load the src value
	//		stage 2:		scale it to LONG_MIN...LONG_MAX and add a rounding value to it
	//		stage 3:		convert it to an integer within the FP register
	//		stage 4:		store that to the stack
	//		stage 5:		 (do nothing to let the store complete)
	//		stage 6:		load the high half word from the stack
	//		stage 7:		store it to the destination
	//
	//	We set it up so that at any given time 7 different pieces of data are being worked on at a time.
	//	Because of the do nothing stage, the inner loop had to be unrolled by one, so in actuality, each
	//	inner loop iteration represents two virtual clock cycles that push data through our virtual pipeline.
	//
	//	The reason why this works is that this allows us to break data dependency chains and insert 5 real 
	//	operations in between every virtual pipeline stage. This means 5 instructions between each data 
	//	dependency, which is just enough to keep all of our real pipelines happy. The data flow follows 
	//	standard pipeline diagrams:
	//
	//					stage1	stage2	stage3	stage4	stage5	stage6	stage7
	//	virtual cycle 1:	data1	-		-		-		-		-		-
	//	virtual cycle 2:	data2	data1	-		-		-		-		-
	//	virtual cycle 3:	data3	data2	data1	-		-		-		-
	//	virtual cycle 4:	data4	data3	data2	data1	-		-		-		
	//	virtual cycle 5:	data5	data4	data3	data2	data1	-		-			   
	//	virtual cycle 6:	data6	data5	data4	data3	data2	data1	-	
	//
	//	inner loop:
	//	  virtual cycle A:	data7	data6	data5	data4	data3	data2	data1					  
	//	  virtual cycle B:	data8	data7	data6	data5	data4	data3	data2	
	//
	//	virtual cycle 7 -		dataF	dataE	dataD	dataC	dataB	dataA
	//	virtual cycle 8 -		-		dataF	dataE	dataD	dataC	dataB	
	//	virtual cycle 9 -		-		-		dataF	dataE	dataD	dataC
	//	virtual cycle 10	-		-		-		-		dataF	dataE	dataD  
	//	virtual cycle 11	-		-		-		-		-		dataF	dataE	
	//	virtual cycle 12	-		-		-		-		-		-		dataF						 
	
	if( count >= 6 )
	{
		//virtual cycle 1
		startingFloat = (src++)[0];
		
		//virtual cycle 2
		scaled = startingFloat * scale + round;
		startingFloat = (src++)[0];
		
		//virtual cycle 3
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		startingFloat = (src++)[0];
		
		//virtual cycle 4
		__stfiwx( converted, 0, buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		startingFloat = (src++)[0];
		
		//virtual cycle 5
		__stfiwx( converted, sizeof(float), buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		startingFloat = (src++)[0];
		
		//virtual cycle 6
		copy = ((SInt8*) buffer)[0];	
		__stfiwx( converted, 0, buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		startingFloat = (src++)[0];
		
		count -= 6;
		loopCount = count / 2;
		count &= 1;
		while( loopCount-- )
		{
			register float	startingFloat2;
			register double scaled2;
			register double converted2;
			register SInt8	copy2;
			
			//virtual Cycle A
			(dst++)[0] = copy;
			__asm__ __volatile__ ( "fctiw %0, %1" : "=f" (converted2) : "f" ( scaled ) );
			copy2 = ((SInt8*) buffer)[4];		
			__asm__ __volatile__ ( "fmadd %0, %1, %2, %3" : "=f" (scaled2) : "f" ( startingFloat), "f" (scale), "f" (round) );
			__asm__ __volatile__ ( "stfiwx %0, %1, %2" : : "f" (converted), "b%" (sizeof(float)), "r" (buffer) : "memory" );
			startingFloat2 = (src++)[0];

			//virtual cycle B
			(dst++)[0] = copy2;
			__asm__ __volatile__ ( "fctiw %0, %1" : "=f" (converted) : "f" ( scaled2 ) );
			copy = ((SInt8*) buffer)[0];
			__asm__ __volatile__ ( "fmadd %0, %1, %2, %3" : "=f" (scaled) : "f" ( startingFloat2), "f" (scale), "f" (round) );
			__asm__ __volatile__ ( "stfiwx %0, %1, %2" : : "f" (converted2), "b%" (0), "r" (buffer) : "memory" );
			startingFloat = (src++)[0];
		}
		
		//Virtual Cycle 7
		(dst++)[0] = copy;
		copy = ((SInt8*) buffer)[4];
		__stfiwx( converted, sizeof(float), buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		
		//Virtual Cycle 8
		(dst++)[0] = copy;
		copy = ((SInt8*) buffer)[0];
		__stfiwx( converted, 0, buffer );
		converted = __fctiw( scaled );
	
		//Virtual Cycle 9
		(dst++)[0] = copy;
		copy = ((SInt8*) buffer)[4];
		__stfiwx( converted, sizeof(float), buffer );
		
		//Virtual Cycle 10
		(dst++)[0] = copy;
		copy = ((SInt8*) buffer)[0];
	
		//Virtual Cycle 11
		(dst++)[0] = copy;
		copy = ((SInt8*) buffer)[4];

		//Virtual Cycle 11
		(dst++)[0] = copy;
	}

	//clean up any extras
	while( count-- )
	{
		double scaled = src[0] * scale + round;
		double converted = __fctiw( scaled );
		__stfiwx( converted, 0, buffer );
		dst[0] = buffer[0] >> 24;
		src++;
		dst++;
	}
}


void Float32ToNativeInt16( float *src, signed short *dst, unsigned int count )
{
	register double		scale = 2147483648.0;
	register double		round = 32768.0;
	unsigned long		loopCount = count / 4;
	long				buffer[2];
	register float		startingFloat;
	register double scaled;
	register double converted;
	register short		copy;

	//
	//	The fastest way to do this is to set up a staggered loop that models a 7 stage virtual pipeline:
	//
	//		stage 1:		load the src value
	//		stage 2:		scale it to LONG_MIN...LONG_MAX and add a rounding value to it
	//		stage 3:		convert it to an integer within the FP register
	//		stage 4:		store that to the stack
	//		stage 5:		 (do nothing to let the store complete)
	//		stage 6:		load the high half word from the stack
	//		stage 7:		store it to the destination
	//
	//	We set it up so that at any given time 7 different pieces of data are being worked on at a time.
	//	Because of the do nothing stage, the inner loop had to be unrolled by one, so in actuality, each
	//	inner loop iteration represents two virtual clock cycles that push data through our virtual pipeline.
	//
	//	The reason why this works is that this allows us to break data dependency chains and insert 5 real 
	//	operations in between every virtual pipeline stage. This means 5 instructions between each data 
	//	dependency, which is just enough to keep all of our real pipelines happy. The data flow follows 
	//	standard pipeline diagrams:
	//
	//					stage1	stage2	stage3	stage4	stage5	stage6	stage7
	//	virtual cycle 1:	data1	-		-		-		-		-		-
	//	virtual cycle 2:	data2	data1	-		-		-		-		-
	//	virtual cycle 3:	data3	data2	data1	-		-		-		-
	//	virtual cycle 4:	data4	data3	data2	data1	-		-		-		
	//	virtual cycle 5:	data5	data4	data3	data2	data1	-		-			   
	//	virtual cycle 6:	data6	data5	data4	data3	data2	data1	-	
	//
	//	inner loop:
	//	  virtual cycle A:	data7	data6	data5	data4	data3	data2	data1					  
	//	  virtual cycle B:	data8	data7	data6	data5	data4	data3	data2	
	//
	//	virtual cycle 7 -		dataF	dataE	dataD	dataC	dataB	dataA
	//	virtual cycle 8 -		-		dataF	dataE	dataD	dataC	dataB	
	//	virtual cycle 9 -		-		-		dataF	dataE	dataD	dataC
	//	virtual cycle 10	-		-		-		-		dataF	dataE	dataD  
	//	virtual cycle 11	-		-		-		-		-		dataF	dataE	
	//	virtual cycle 12	-		-		-		-		-		-		dataF						 
	
	if( count >= 6 )
	{
		//virtual cycle 1
		startingFloat = (src++)[0];
		
		//virtual cycle 2
		scaled = startingFloat * scale + round;
		startingFloat = (src++)[0];
		
		//virtual cycle 3
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		startingFloat = (src++)[0];
		
		//virtual cycle 4
		__stfiwx( converted, 0, buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		startingFloat = (src++)[0];
		
		//virtual cycle 5
		__stfiwx( converted, sizeof(float), buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		startingFloat = (src++)[0];
		
		//virtual cycle 6
		copy = ((short*) buffer)[0];
		__stfiwx( converted, 0, buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		startingFloat = (src++)[0];
		
		count -= 6;
		loopCount = count / 2;
		count &= 1;
		while( loopCount-- )
		{
			register float	startingFloat2;
			register double scaled2;
			register double converted2;
			register short	copy2;
			
			//virtual Cycle A
			(dst++)[0] = copy;
			__asm__ __volatile__ ( "fctiw %0, %1" : "=f" (converted2) : "f" ( scaled ) );
			copy2 = ((short*) buffer)[2];
			__asm__ __volatile__ ( "fmadd %0, %1, %2, %3" : "=f" (scaled2) : "f" ( startingFloat), "f" (scale), "f" (round) );
			__asm__ __volatile__ ( "stfiwx %0, %1, %2" : : "f" (converted), "b%" (sizeof(float)), "r" (buffer) : "memory" );
			startingFloat2 = (src++)[0];

			//virtual cycle B
			(dst++)[0] = copy2;
			__asm__ __volatile__ ( "fctiw %0, %1" : "=f" (converted) : "f" ( scaled2 ) );
			copy = ((short*) buffer)[0];
			__asm__ __volatile__ ( "fmadd %0, %1, %2, %3" : "=f" (scaled) : "f" ( startingFloat2), "f" (scale), "f" (round) );
			__asm__ __volatile__ ( "stfiwx %0, %1, %2" : : "f" (converted2), "b%" (0), "r" (buffer) : "memory" );
			startingFloat = (src++)[0];
		}
		
		//Virtual Cycle 7
		(dst++)[0] = copy;
		copy = ((short*) buffer)[2];
		__stfiwx( converted, sizeof(float), buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		
		//Virtual Cycle 8
		(dst++)[0] = copy;
		copy = ((short*) buffer)[0];
		__stfiwx( converted, 0, buffer );
		converted = __fctiw( scaled );
	
		//Virtual Cycle 9
		(dst++)[0] = copy;
		copy = ((short*) buffer)[2];
		__stfiwx( converted, sizeof(float), buffer );
		
		//Virtual Cycle 10
		(dst++)[0] = copy;
		copy = ((short*) buffer)[0];
	
		//Virtual Cycle 11
		(dst++)[0] = copy;
		copy = ((short*) buffer)[2];

		//Virtual Cycle 11
		(dst++)[0] = copy;
	}

	//clean up any extras
	while( count-- )
	{
		double scaled = src[0] * scale + round;
		double converted = __fctiw( scaled );
		__stfiwx( converted, 0, buffer );
		dst[0] = buffer[0] >> 16;
		src++;
		dst++;
	}
}

void Float32ToSwapInt16( float *src, signed short *dst, unsigned int count )
{
	register double		scale = 2147483648.0;
	register double		round = 32768.0;
	unsigned long	loopCount = count / 4;
	long		buffer[2];
	register float	startingFloat;
	register double scaled;
	register double converted;
	register short	copy;

	//
	//	The fastest way to do this is to set up a staggered loop that models a 7 stage virtual pipeline:
	//
	//		stage 1:		load the src value
	//		stage 2:		scale it to LONG_MIN...LONG_MAX and add a rounding value to it
	//		stage 3:		convert it to an integer within the FP register
	//		stage 4:		store that to the stack
	//		stage 5:		 (do nothing to let the store complete)
	//		stage 6:		load the high half word from the stack
	//		stage 7:		store it to the destination
	//
	//	We set it up so that at any given time 7 different pieces of data are being worked on at a time.
	//	Because of the do nothing stage, the inner loop had to be unrolled by one, so in actuality, each
	//	inner loop iteration represents two virtual clock cycles that push data through our virtual pipeline.
	//
	//	The reason why this works is that this allows us to break data dependency chains and insert 5 real 
	//	operations in between every virtual pipeline stage. This means 5 instructions between each data 
	//	dependency, which is just enough to keep all of our real pipelines happy. The data flow follows 
	//	standard pipeline diagrams:
	//
	//					stage1	stage2	stage3	stage4	stage5	stage6	stage7
	//	virtual cycle 1:	data1	-		-		-		-		-		-
	//	virtual cycle 2:	data2	data1	-		-		-		-		-
	//	virtual cycle 3:	data3	data2	data1	-		-		-		-
	//	virtual cycle 4:	data4	data3	data2	data1	-		-		-		
	//	virtual cycle 5:	data5	data4	data3	data2	data1	-		-			   
	//	virtual cycle 6:	data6	data5	data4	data3	data2	data1	-	
	//
	//	inner loop:
	//	  virtual cycle A:	data7	data6	data5	data4	data3	data2	data1					  
	//	  virtual cycle B:	data8	data7	data6	data5	data4	data3	data2	
	//
	//	virtual cycle 7 -		dataF	dataE	dataD	dataC	dataB	dataA
	//	virtual cycle 8 -		-		dataF	dataE	dataD	dataC	dataB	
	//	virtual cycle 9 -		-		-		dataF	dataE	dataD	dataC
	//	virtual cycle 10	-		-		-		-		dataF	dataE	dataD  
	//	virtual cycle 11	-		-		-		-		-		dataF	dataE	
	//	virtual cycle 12	-		-		-		-		-		-		dataF						 
	
	if( count >= 6 )
	{
		//virtual cycle 1
		startingFloat = (src++)[0];
		
		//virtual cycle 2
		scaled = startingFloat * scale + round;
		startingFloat = (src++)[0];
		
		//virtual cycle 3
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		startingFloat = (src++)[0];
		
		//virtual cycle 4
		__stfiwx( converted, 0, buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		startingFloat = (src++)[0];
		
		//virtual cycle 5
		__stfiwx( converted, sizeof(float), buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		startingFloat = (src++)[0];
		
		//virtual cycle 6
		copy = ((short*) buffer)[0];
		__stfiwx( converted, 0, buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		startingFloat = (src++)[0];
		
		count -= 6;
		loopCount = count / 2;
		count &= 1;
		while( loopCount-- )
		{
			register float	startingFloat2;
			register double scaled2;
			register double converted2;
			register short	copy2;
			
			//virtual Cycle A
//			  (dst++)[0] = copy;
			__asm__ __volatile__ ( "sthbrx %0, 0, %1" : : "r" (copy), "r" (dst) );
			__asm__ __volatile__ ( "fctiw %0, %1" : "=f" (converted2) : "f" ( scaled ) );
			copy2 = ((short*) buffer)[2];
			__asm__ __volatile__ ( "fmadd %0, %1, %2, %3" : "=f" (scaled2) : "f" ( startingFloat), "f" (scale), "f" (round) );
			__asm__ __volatile__ ( "stfiwx %0, %1, %2" : : "f" (converted), "b%" (sizeof(float)), "r" (buffer) : "memory" );
			startingFloat2 = (src)[0];	src+=2;

			//virtual cycle B
//			  (dst++)[0] = copy2;
			dst+=2;
			__asm__ __volatile__ ( "sthbrx %0, %1, %2" : : "r" (copy2), "r" (-2), "r" (dst) );	
			__asm__ __volatile__ ( "fctiw %0, %1" : "=f" (converted) : "f" ( scaled2 ) );
			copy = ((short*) buffer)[0];
			__asm__ __volatile__ ( "fmadd %0, %1, %2, %3" : "=f" (scaled) : "f" ( startingFloat2), "f" (scale), "f" (round) );
			__asm__ __volatile__ ( "stfiwx %0, %1, %2" : : "f" (converted2), "b%" (0), "r" (buffer) : "memory" );
			startingFloat = (src)[-1];	
		}
		
		//Virtual Cycle 7
		__asm__ __volatile__ ( "sthbrx %0, 0, %1" : : "r" (copy),  "r" (dst) );	 dst++;
		copy = ((short*) buffer)[2];
		__stfiwx( converted, sizeof(float), buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		
		//Virtual Cycle 8
		__asm__ __volatile__ ( "sthbrx %0, 0, %1" : : "r" (copy),  "r" (dst) );	 dst++;
		copy = ((short*) buffer)[0];
		__stfiwx( converted, 0, buffer );
		converted = __fctiw( scaled );
	
		//Virtual Cycle 9
		__asm__ __volatile__ ( "sthbrx %0, 0, %1" : : "r" (copy),  "r" (dst) );	 dst++;
		copy = ((short*) buffer)[2];
		__stfiwx( converted, sizeof(float), buffer );
		
		//Virtual Cycle 10
		__asm__ __volatile__ ( "sthbrx %0, 0, %1" : : "r" (copy),  "r" (dst) );	 dst++;
		copy = ((short*) buffer)[0];
	
		//Virtual Cycle 11
		__asm__ __volatile__ ( "sthbrx %0, 0, %1" : : "r" (copy),  "r" (dst) );	 dst++;
		copy = ((short*) buffer)[2];

		//Virtual Cycle 11
		__asm__ __volatile__ ( "sthbrx %0, 0, %1" : : "r" (copy),  "r" (dst) );	 dst++;
	}

	//clean up any extras
	while( count-- )
	{
		double scaled = src[0] * scale + round;
		double converted = __fctiw( scaled );
		__stfiwx( converted, 0, buffer );
		copy = buffer[0] >> 16;
		__asm__ __volatile__ ( "sthbrx %0, 0, %1" : : "r" (copy),  "r" (dst) );	 
		src++;
		dst++;
	}
}


void Float32ToNativeInt24( float *src, signed long *dst, unsigned int count )
{
	register double		scale = 2147483648.0;
	register double		round = 0.5 * 256.0;
	unsigned long	loopCount = count / 4;
	long		buffer[4];
	register float	startingFloat, startingFloat2;
	register double scaled, scaled2;
	register double converted, converted2;
	register long	copy1;//, merge1, rotate1;
	register long	copy2;//, merge2, rotate2;
	register long	copy3;//, merge3, rotate3;
	register long	copy4;//, merge4, rotate4;
	register double		oldSetting;


	//Set the FPSCR to round to -Inf mode
	{
		union
		{
			double	d;
			int		i[2];
		}setting;
		register double newSetting;

		//Read the the current FPSCR value
		asm volatile ( "mffs %0" : "=f" ( oldSetting ) );

		//Store it to the stack
		setting.d = oldSetting;

		//Read in the low 32 bits and mask off the last two bits so they are zero
		//in the integer unit. These two bits set to zero means round to nearest mode.
		//Finally, then store the result back
		setting.i[1] |= 3;

		//Load the new FPSCR setting into the FP register file again
		newSetting = setting.d;

		//Change the FPSCR to the new setting
		asm volatile( "mtfsf 7, %0" : : "f" (newSetting ) );
	}


	//
	//	The fastest way to do this is to set up a staggered loop that models a 7 stage virtual pipeline:
	//
	//		stage 1:		load the src value
	//		stage 2:		scale it to LONG_MIN...LONG_MAX and add a rounding value to it
	//		stage 3:		convert it to an integer within the FP register
	//		stage 4:		store that to the stack
	//		stage 5:		 (do nothing to let the store complete)
	//		stage 6:		load the high half word from the stack
	//		stage 7:		merge with later data to form a 32 bit word
	//		stage 8:		possible rotate to correct byte order
	//		stage 9:		store it to the destination
	//
	//	We set it up so that at any given time 7 different pieces of data are being worked on at a time.
	//	Because of the do nothing stage, the inner loop had to be unrolled by one, so in actuality, each
	//	inner loop iteration represents two virtual clock cycles that push data through our virtual pipeline.
	//
	//	The reason why this works is that this allows us to break data dependency chains and insert 5 real
	//	operations in between every virtual pipeline stage. This means 5 instructions between each data
	//	dependency, which is just enough to keep all of our real pipelines happy. The data flow follows
	//	standard pipeline diagrams:
	//
	//				stage1	stage2	stage3	stage4	stage5	stage6	stage7	stage8	stage9
	//	virtual cycle 1:	data1	-	-	-		-		-		-		-		-
	//	virtual cycle 2:	data2	data1	-	-		-		-		-		-		-
	//	virtual cycle 3:	data3	data2	data1	-		-		-		-		-		-
	//	virtual cycle 4:	data4	data3	data2	data1	-		-		-		-		-
	//	virtual cycle 5:	data5	data4	data3	data2	data1	-		-		-		-
	//	virtual cycle 6:	data6	data5	data4	data3	data2	data1	-		-		-
	//	virtual cycle 7:	data7	data6	data5	data4	data3	data2	data1	-		-
	//	virtual cycle 8:	data8	data7	data6	data5	data4	data3	data2	data1	-
	//
	//	inner loop:
	//	  virtual cycle A:	data9	data8	data7	data6	data5	data4	data3	data2	data1
	//	  virtual cycle B:	data10	data9	data8	data7	data6	data5	data4	data3	data2
	//	  virtual cycle C:	data11	data10	data9	data8	data7	data6	data5	data4	data3
	//	  virtual cycle D:	data12	data11	data10	data9	data8	data7	data6	data5	data4
	//
	//	virtual cycle 9		-	dataH	dataG	dataF	dataE	dataD	dataC	dataB	dataA
	//	virtual cycle 10	-	-	dataH	dataG	dataF	dataE	dataD	dataC	dataB
	//	virtual cycle 11	-	-	-	dataH	dataG	dataF	dataE	dataD	dataC
	//	virtual cycle 12	-	-	-	-		dataH	dataG	dataF	dataE	dataD
	//	virtual cycle 13	-	-	-	-		-		dataH	dataG	dataF	dataE
	//	virtual cycle 14	-	-	-	-		-		-		dataH	dataG	dataF
	//	virtual cycle 15	-	-	-	-		-		-		-	dataH	dataG	
	//	virtual cycle 16	-	-	-	-		-		-		-	-	dataH

	src--;
	dst--;

	if( count >= 8 )
	{
		//virtual cycle 1
		startingFloat = (++src)[0];

		//virtual cycle 2
		scaled = startingFloat * scale + round;
		startingFloat = (++src)[0];

		//virtual cycle 3
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		startingFloat = (++src)[0];

		//virtual cycle 4
		__stfiwx( converted, 0, buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		startingFloat = (++src)[0];

		//virtual cycle 5
		__stfiwx( converted, sizeof(float), buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		startingFloat = (++src)[0];

		//virtual cycle 6
		copy1 = buffer[0];
		__stfiwx( converted, 2 * sizeof( float), buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		startingFloat = (++src)[0];

		//virtual cycle 7
		copy2 = buffer[1];
		__stfiwx( converted, 3 * sizeof(float), buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		startingFloat = (++src)[0];

		//virtual cycle 8
		copy1 = __rlwimi( copy1, copy2, 8, 24, 31 );
		copy3 = buffer[2];
		__stfiwx( converted, 0, buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		startingFloat = (++src)[0];

		count -= 8;
		loopCount = count / 4;
		count &= 3;
		while( loopCount-- )
		{
			//virtual cycle A
			//no store yet						//store
			//no rotation needed for copy1,				//rotate
			__asm__ __volatile__( "fmadds %0, %1, %2, %3" : "=f"(scaled2) : "f" (startingFloat), "f" ( scale ), "f" ( round ));		//scale for clip and add rounding
			startingFloat2 = (++src)[0];				//load the float
			__asm__ __volatile__( "fctiw %0, %1" : "=f" (converted2) : "f" ( scaled ) );				//convert to int and clip
			 copy4 = buffer[3];						//load clipped int back in
			copy2 = __rlwimi_volatile( copy2, copy3, 8, 24, 7 );			//merge
			__stfiwx( converted, 1 * sizeof(float), buffer );		//store clipped int

			//virtual Cycle B
			__asm__ __volatile__( "fmadds %0, %1, %2, %3" : "=f"(scaled) : "f" (startingFloat2), "f" ( scale ), "f" ( round ));		//scale for clip and add rounding
			startingFloat = (++src)[0];					//load the float
			 __asm__ __volatile__( "fctiw %0, %1" : "=f" (converted) : "f" ( scaled2 ) );				//convert to int and clip
			(++dst)[0] = copy1;						//store
			copy3 = __rlwimi_volatile( copy3, copy4, 8, 24, 15 );	//merge with adjacent pixel
			copy1 = buffer[0];						//load clipped int back in
			copy2 = __rlwimi_volatile( copy2, copy2, 8, 0, 31 );	//rotate
			__stfiwx( converted2, 2 * sizeof(float), buffer );		//store clipped int

			//virtual Cycle C
			__asm__ __volatile__( "fmadds %0, %1, %2, %3" : "=f"(scaled2) : "f" (startingFloat), "f" ( scale ), "f" ( round ));		//scale for clip and add rounding
			startingFloat2 = (++src)[0];				//load the float
			//We dont store copy 4 so no merge needs to be done to it	//merge with adjacent pixel
			converted2 = __fctiw( scaled );				//convert to int and clip
			(++dst)[0] = copy2;						//store
			copy3 = __rlwimi_volatile( copy3, copy3, 16, 0, 31 );		//rotate
			copy2 = buffer[1];						//load clipped int back in
			__stfiwx( converted, 3 * sizeof(float), buffer );		//store clipped int

			//virtual Cycle D
			__asm__ ( "fmadds %0, %1, %2, %3" : "=f"(scaled) : "f" (startingFloat2), "f" ( scale ), "f" ( round ));		//scale for clip and add rounding
			startingFloat = (++src)[0];					//load the float
			converted = __fctiw( scaled2 );				//convert to int and clip
			//We dont store copy 4 so no rotation needs to be done to it//rotate
			(++dst)[0] = copy3;						//store
			copy1 = __rlwimi_volatile( copy1, copy2, 8, 24, 31 );		//merge with adjacent pixel
			 __stfiwx( converted2, 0 * sizeof(float), buffer );		//store clipped int
			copy3 = buffer[2];						//load clipped int back in
		}

		//virtual cycle 9
		//no store yet						//store
		//no rotation needed for copy1,				//rotate
		copy2 = __rlwimi( copy2, copy3, 8, 24, 7 );		//merge
		copy4 = buffer[3];					//load clipped int back in
		__stfiwx( converted, 1 * sizeof(float), buffer );	//store clipped int
		converted2 = __fctiw( scaled );				//convert to int and clip
		scaled2 = startingFloat * scale + round;		//scale for clip and add rounding

		//virtual Cycle 10
		(++dst)[0] = copy1;						//store
		copy2 = __rlwimi( copy2, copy2, 8, 0, 31 );			//rotate
		copy3 = __rlwimi( copy3, copy4, 8, 24, 15 );		//merge with adjacent pixel
		copy1 = buffer[0];					//load clipped int back in
		__stfiwx( converted2, 2 * sizeof(float), buffer );	//store clipped int
		converted = __fctiw( scaled2 );				//convert to int and clip

		//virtual Cycle 11
		(++dst)[0] = copy2;						//store
		copy3 = __rlwimi( copy3, copy3, 16, 0, 31 );		//rotate
		//We dont store copy 4 so no merge needs to be done to it//merge with adjacent pixel
		copy2 = buffer[1];					//load clipped int back in
		__stfiwx( converted, 3 * sizeof(float), buffer );	//store clipped int

		//virtual Cycle 12
		(++dst)[0] = copy3;						//store
		//We dont store copy 4 so no rotation needs to be done to it//rotate
		copy1 = __rlwimi( copy1, copy2, 8, 24, 31 );		//merge with adjacent pixel
		copy3 = buffer[2];						//load clipped int back in

		//virtual cycle 13
		//no store yet						//store
		//no rotation needed for copy1,				//rotate
		copy2 = __rlwimi( copy2, copy3, 8, 24, 7 );		//merge
		copy4 = buffer[3];					//load clipped int back in

		//virtual Cycle 14
		(++dst)[0] = copy1;						//store
		copy2 = __rlwimi( copy2, copy2, 8, 0, 31 );			//rotate
		copy3 = __rlwimi( copy3, copy4, 8, 24, 15 );		//merge with adjacent pixel

		//virtual Cycle 15
		(++dst)[0] = copy2;						//store
		copy3 = __rlwimi( copy3, copy3, 16, 0, 31 );		//rotate

		//virtual Cycle 16
		(++dst)[0] = copy3;						//store
	}

	//clean up any extras
	dst++;
	while( count-- )
	{
		startingFloat = (++src)[0];				//load the float
		scaled = startingFloat * scale + round;			//scale for clip and add rounding
		converted = __fctiw( scaled );				//convert to int and clip
		__stfiwx( converted, 0, buffer );			//store clipped int
		copy1 = buffer[0];					//load clipped int back in
		((signed char*) dst)[0] = copy1 >> 24;
		dst = (signed long*) ((signed char*) dst + 1 );
		((unsigned short*) dst)[0] = copy1 >> 8;
		dst = (signed long*) ((unsigned short*) dst + 1 );
	}

	//restore the old FPSCR setting
	__asm__ __volatile__ ( "mtfsf 7, %0" : : "f" (oldSetting) );
}

void Float32ToSwapInt24( float *src, signed long *dst, unsigned int count )
{
	register double		scale = 2147483648.0;
	register double		round = 0.5 * 256.0;
	unsigned long	loopCount = count / 4;
	long		buffer[4];
	register float	startingFloat, startingFloat2;
	register double scaled, scaled2;
	register double converted, converted2;
	register long	copy1;
	register long	copy2;
	register long	copy3;
	register long	copy4;
	register double		oldSetting;


	//Set the FPSCR to round to -Inf mode
	{
		union
		{
			double	d;
			int		i[2];
		}setting;
		register double newSetting;

		//Read the the current FPSCR value
		asm volatile ( "mffs %0" : "=f" ( oldSetting ) );

		//Store it to the stack
		setting.d = oldSetting;

		//Read in the low 32 bits and mask off the last two bits so they are zero
		//in the integer unit. These two bits set to zero means round to nearest mode.
		//Finally, then store the result back
		setting.i[1] |= 3;

		//Load the new FPSCR setting into the FP register file again
		newSetting = setting.d;

		//Change the FPSCR to the new setting
		asm volatile( "mtfsf 7, %0" : : "f" (newSetting ) );
	}


	//
	//	The fastest way to do this is to set up a staggered loop that models a 7 stage virtual pipeline:
	//
	//		stage 1:		load the src value
	//		stage 2:		scale it to LONG_MIN...LONG_MAX and add a rounding value to it
	//		stage 3:		convert it to an integer within the FP register
	//		stage 4:		store that to the stack
	//		stage 5:		 (do nothing to let the store complete)
	//		stage 6:		load the high half word from the stack
	//		stage 7:		merge with later data to form a 32 bit word
	//		stage 8:		possible rotate to correct byte order
	//		stage 9:		store it to the destination
	//
	//	We set it up so that at any given time 7 different pieces of data are being worked on at a time.
	//	Because of the do nothing stage, the inner loop had to be unrolled by one, so in actuality, each
	//	inner loop iteration represents two virtual clock cycles that push data through our virtual pipeline.
	//
	//	The reason why this works is that this allows us to break data dependency chains and insert 5 real
	//	operations in between every virtual pipeline stage. This means 5 instructions between each data
	//	dependency, which is just enough to keep all of our real pipelines happy. The data flow follows
	//	standard pipeline diagrams:
	//
	//				stage1	stage2	stage3	stage4	stage5	stage6	stage7	stage8	stage9
	//	virtual cycle 1:	data1	-	-	-		-		-		-		-		-
	//	virtual cycle 2:	data2	data1	-	-		-		-		-		-		-
	//	virtual cycle 3:	data3	data2	data1	-		-		-		-		-		-
	//	virtual cycle 4:	data4	data3	data2	data1	-		-		-		-		-
	//	virtual cycle 5:	data5	data4	data3	data2	data1	-		-		-		-
	//	virtual cycle 6:	data6	data5	data4	data3	data2	data1	-		-		-
	//	virtual cycle 7:	data7	data6	data5	data4	data3	data2	data1	-		-
	//	virtual cycle 8:	data8	data7	data6	data5	data4	data3	data2	data1	-
	//
	//	inner loop:
	//	  virtual cycle A:	data9	data8	data7	data6	data5	data4	data3	data2	data1
	//	  virtual cycle B:	data10	data9	data8	data7	data6	data5	data4	data3	data2
	//	  virtual cycle C:	data11	data10	data9	data8	data7	data6	data5	data4	data3
	//	  virtual cycle D:	data12	data11	data10	data9	data8	data7	data6	data5	data4
	//
	//	virtual cycle 9		-	dataH	dataG	dataF	dataE	dataD	dataC	dataB	dataA
	//	virtual cycle 10	-	-	dataH	dataG	dataF	dataE	dataD	dataC	dataB
	//	virtual cycle 11	-	-	-	dataH	dataG	dataF	dataE	dataD	dataC
	//	virtual cycle 12	-	-	-	-		dataH	dataG	dataF	dataE	dataD
	//	virtual cycle 13	-	-	-	-		-		dataH	dataG	dataF	dataE
	//	virtual cycle 14	-	-	-	-		-		-		dataH	dataG	dataF
	//	virtual cycle 15	-	-	-	-		-		-		-	dataH	dataG	
	//	virtual cycle 16	-	-	-	-		-		-		-	-	dataH

	src--;
	dst--;

	if( count >= 8 )
	{
		//virtual cycle 1
		startingFloat = (++src)[0];

		//virtual cycle 2
		scaled = startingFloat * scale + round;
		startingFloat = (++src)[0];

		//virtual cycle 3
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		startingFloat = (++src)[0];

		//virtual cycle 4
		__stfiwx( converted, 0, buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		startingFloat = (++src)[0];

		//virtual cycle 5
		__stfiwx( converted, sizeof(float), buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		startingFloat = (++src)[0];

		//virtual cycle 6
		copy1 = __lwbrx( 0, buffer );
		__stfiwx( converted, 2 * sizeof( float), buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		startingFloat = (++src)[0];

		//virtual cycle 7
		copy2 = __lwbrx( 4, buffer );
		__stfiwx( converted, 3 * sizeof(float), buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		startingFloat = (++src)[0];

		//virtual cycle 8
		copy1 = __rlwimi( copy1, copy2, 8, 0, 7 );
		copy3 = __lwbrx( 8, buffer );;
		__stfiwx( converted, 0, buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		startingFloat = (++src)[0];

		count -= 8;
		loopCount = count / 4;
		count &= 3;
		while( loopCount-- )
		{
			//virtual cycle A
			//no store yet						//store
			copy1 = __rlwimi( copy1, copy1, 8, 0, 31 );			//rotate
			__asm__ __volatile__( "fmadds %0, %1, %2, %3" : "=f"(scaled2) : "f" (startingFloat), "f" ( scale ), "f" ( round ));		//scale for clip and add rounding
			startingFloat2 = (++src)[0];				//load the float
			__asm__ __volatile__( "fctiw %0, %1" : "=f" (converted2) : "f" ( scaled ) );				//convert to int and clip
			 copy4 = __lwbrx( 12, buffer );						//load clipped int back in
			copy2 = __rlwimi_volatile( copy2, copy3, 8, 0, 15 );			//merge
			__stfiwx( converted, 1 * sizeof(float), buffer );		//store clipped int

			//virtual Cycle B
			__asm__ __volatile__( "fmadds %0, %1, %2, %3" : "=f"(scaled) : "f" (startingFloat2), "f" ( scale ), "f" ( round ));		//scale for clip and add rounding
			startingFloat = (++src)[0];					//load the float
			 __asm__ __volatile__( "fctiw %0, %1" : "=f" (converted) : "f" ( scaled2 ) );				//convert to int and clip
			(++dst)[0] = copy1;						//store
			copy4 = __rlwimi_volatile( copy4, copy3, 24, 0, 7 );	//merge with adjacent pixel
			copy1 = __lwbrx( 0, buffer );						//load clipped int back in
			copy2 = __rlwimi_volatile( copy2, copy2, 16, 0, 31 );	//rotate
			__stfiwx( converted2, 2 * sizeof(float), buffer );		//store clipped int

			//virtual Cycle C
			__asm__ __volatile__( "fmadds %0, %1, %2, %3" : "=f"(scaled2) : "f" (startingFloat), "f" ( scale ), "f" ( round ));		//scale for clip and add rounding
			startingFloat2 = (++src)[0];				//load the float
			converted2 = __fctiw( scaled );				//convert to int and clip
			(++dst)[0] = copy2;						//store
			copy2 = __lwbrx( 4, buffer );						//load clipped int back in
			__stfiwx( converted, 3 * sizeof(float), buffer );		//store clipped int


			//virtual Cycle D
			__asm__ ( "fmadds %0, %1, %2, %3" : "=f"(scaled) : "f" (startingFloat2), "f" ( scale ), "f" ( round ));		//scale for clip and add rounding
			startingFloat = (++src)[0];					//load the float
			converted = __fctiw( scaled2 );				//convert to int and clip
			(++dst)[0] = copy4;						//store
			copy1 = __rlwimi_volatile( copy1, copy2, 8, 0, 7 );		//merge with adjacent pixel
			 __stfiwx( converted2, 0 * sizeof(float), buffer );		//store clipped int
			copy3 = __lwbrx( 8, buffer );						//load clipped int back in
		}

		//virtual cycle A
		//no store yet						//store
		copy1 = __rlwimi( copy1, copy1, 8, 0, 31 );			//rotate
		__asm__ __volatile__( "fmadds %0, %1, %2, %3" : "=f"(scaled2) : "f" (startingFloat), "f" ( scale ), "f" ( round ));		//scale for clip and add rounding
		__asm__ __volatile__( "fctiw %0, %1" : "=f" (converted2) : "f" ( scaled ) );				//convert to int and clip
			copy4 = __lwbrx( 12, buffer );						//load clipped int back in
		copy2 = __rlwimi_volatile( copy2, copy3, 8, 0, 15 );			//merge
		__stfiwx( converted, 1 * sizeof(float), buffer );		//store clipped int

		//virtual Cycle B
			__asm__ __volatile__( "fctiw %0, %1" : "=f" (converted) : "f" ( scaled2 ) );				//convert to int and clip
		(++dst)[0] = copy1;						//store
		copy4 = __rlwimi_volatile( copy4, copy3, 24, 0, 7 );	//merge with adjacent pixel
		copy1 = __lwbrx( 0, buffer );						//load clipped int back in
		copy2 = __rlwimi_volatile( copy2, copy2, 16, 0, 31 );	//rotate
		__stfiwx( converted2, 2 * sizeof(float), buffer );		//store clipped int

		//virtual Cycle C
		(++dst)[0] = copy2;						//store
		copy2 = __lwbrx( 4, buffer );						//load clipped int back in
		__stfiwx( converted, 3 * sizeof(float), buffer );		//store clipped int


		//virtual Cycle D
		(++dst)[0] = copy4;						//store
		copy1 = __rlwimi_volatile( copy1, copy2, 8, 0, 7 );		//merge with adjacent pixel
		copy3 = __lwbrx( 8, buffer );						//load clipped int back in

		//virtual cycle A
		//no store yet						//store
		copy1 = __rlwimi( copy1, copy1, 8, 0, 31 );			//rotate
		copy4 = __lwbrx( 12, buffer );						//load clipped int back in
		copy2 = __rlwimi_volatile( copy2, copy3, 8, 0, 15 );			//merge

		//virtual Cycle B
		(++dst)[0] = copy1;						//store
		copy4 = __rlwimi_volatile( copy4, copy3, 24, 0, 7 );	//merge with adjacent pixel
		copy2 = __rlwimi_volatile( copy2, copy2, 16, 0, 31 );	//rotate

		//virtual Cycle C
		(++dst)[0] = copy2;						//store


		//virtual Cycle D
		(++dst)[0] = copy4;						//store
	}

	//clean up any extras
	dst++;
	while( count-- )
	{
		startingFloat = (++src)[0];				//load the float
		scaled = startingFloat * scale + round;			//scale for clip and add rounding
		converted = __fctiw( scaled );				//convert to int and clip
		__stfiwx( converted, 0, buffer );			//store clipped int
		copy1 = __lwbrx( 0, buffer);					//load clipped int back in
		((signed char*) dst)[0] = copy1 >> 16;
		dst = (signed long*) ((signed char*) dst + 1 );
		((unsigned short*) dst)[0] = copy1;
		dst = (signed long*) ((unsigned short*) dst + 1 );
	}

	//restore the old FPSCR setting
	__asm__ __volatile__ ( "mtfsf 7, %0" : : "f" (oldSetting) );
}


void Float32ToSwapInt32( float *src, signed long *dst, unsigned int count )
{
	register double		scale = 2147483648.0;
	unsigned long	loopCount = count / 4;
	long			buffer[2];
	register float		startingFloat;
	register double scaled;
	register double converted;
	register long		copy;
	register double		oldSetting;

	//Set the FPSCR to round to -Inf mode
	{
		union
		{
			double	d;
			int		i[2];
		}setting;
		register double newSetting;
		
		//Read the the current FPSCR value
		asm volatile ( "mffs %0" : "=f" ( oldSetting ) );
		
		//Store it to the stack
		setting.d = oldSetting;
		
		//Read in the low 32 bits and mask off the last two bits so they are zero 
		//in the integer unit. These two bits set to zero means round to nearest mode.
		//Finally, then store the result back
		setting.i[1] &= 0xFFFFFFFC;
		
		//Load the new FPSCR setting into the FP register file again
		newSetting = setting.d;
		
		//Change the FPSCR to the new setting
		asm volatile( "mtfsf 7, %0" : : "f" (newSetting ) );
	}


	//
	//	The fastest way to do this is to set up a staggered loop that models a 7 stage virtual pipeline:
	//
	//		stage 1:		load the src value
	//		stage 2:		scale it to LONG_MIN...LONG_MAX and add a rounding value to it
	//		stage 3:		convert it to an integer within the FP register
	//		stage 4:		store that to the stack
	//		stage 5:		 (do nothing to let the store complete)
	//		stage 6:		load the high half word from the stack
	//		stage 7:		store it to the destination
	//
	//	We set it up so that at any given time 7 different pieces of data are being worked on at a time.
	//	Because of the do nothing stage, the inner loop had to be unrolled by one, so in actuality, each
	//	inner loop iteration represents two virtual clock cycles that push data through our virtual pipeline.
	//
	//	The reason why this works is that this allows us to break data dependency chains and insert 5 real 
	//	operations in between every virtual pipeline stage. This means 5 instructions between each data 
	//	dependency, which is just enough to keep all of our real pipelines happy. The data flow follows 
	//	standard pipeline diagrams:
	//
	//				stage1	stage2	stage3	stage4	stage5	stage6	stage7
	//	virtual cycle 1:	data1	-	-	-		-		-		-
	//	virtual cycle 2:	data2	data1	-	-		-		-		-
	//	virtual cycle 3:	data3	data2	data1	-		-		-		-
	//	virtual cycle 4:	data4	data3	data2	data1	-		-		-		
	//	virtual cycle 5:	data5	data4	data3	data2	data1	-		-			   
	//	virtual cycle 6:	data6	data5	data4	data3	data2	data1	-	
	//
	//	inner loop:
	//	  virtual cycle A:	data7	data6	data5	data4	data3	data2	data1					  
	//	  virtual cycle B:	data8	data7	data6	data5	data4	data3	data2	
	//
	//	virtual cycle 7		-	dataF	dataE	dataD	dataC	dataB	dataA
	//	virtual cycle 8		-	-	dataF	dataE	dataD	dataC	dataB	
	//	virtual cycle 9		-	-	-	dataF	dataE	dataD	dataC
	//	virtual cycle 10	-	-	-	-		dataF	dataE	dataD  
	//	virtual cycle 11	-	-	-	-		-		dataF	dataE	
	//	virtual cycle 12	-	-	-	-		-		-		dataF						 
	
	if( count >= 6 )
	{
		//virtual cycle 1
		startingFloat = (src++)[0];
		
		//virtual cycle 2
		scaled = startingFloat * scale;
		startingFloat = (src++)[0];
		
		//virtual cycle 3
		converted = __fctiw( scaled );
		scaled = startingFloat * scale;
		startingFloat = (src++)[0];
		
		//virtual cycle 4
		__stfiwx( converted, 0, buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale;
		startingFloat = (src++)[0];
		
		//virtual cycle 5
		__stfiwx( converted, sizeof(float), buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale;
		startingFloat = (src++)[0];
		
		//virtual cycle 6
		copy = buffer[0];
		__stfiwx( converted, 0, buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale;
		startingFloat = (src++)[0];
		
		count -= 6;
		loopCount = count / 2;
		count &= 1;
		while( loopCount-- )
		{
			register float	startingFloat2;
			register double scaled2;
			register double converted2;
			register long	copy2;
			
			//virtual Cycle A
//			  (dst++)[0] = copy;
			__asm__ __volatile__ ( "stwbrx %0, 0, %1" : : "r" (copy), "r" (dst) );
			__asm__ __volatile__ ( "fctiw %0, %1" : "=f" (converted2) : "f" ( scaled ) );
			copy2 = buffer[1];
			__asm__ __volatile__ ( "fmuls %0, %1, %2" : "=f" (scaled2) : "f" ( startingFloat), "f" (scale) );
			__asm__ __volatile__ ( "stfiwx %0, %1, %2" : : "f" (converted), "b%" (sizeof(*buffer)), "r" (buffer) : "memory" );
			startingFloat2 = (src)[0];	src+=2;

			//virtual cycle B
//			  (dst++)[0] = copy2;
			dst+=2;
			__asm__ __volatile__ ( "stwbrx %0, %1, %2" : : "r" (copy2), "r" (-sizeof(dst[0])), "r" (dst) );	 
			__asm__ __volatile__ ( "fctiw %0, %1" : "=f" (converted) : "f" ( scaled2 ) );
			copy = buffer[0];
			__asm__ __volatile__ ( "fmuls %0, %1, %2" : "=f" (scaled) : "f" ( startingFloat2), "f" (scale) );
			__asm__ __volatile__ ( "stfiwx %0, %1, %2" : : "f" (converted2), "b%" (0), "r" (buffer) : "memory" );
			startingFloat = (src)[-1];	
		}
		
		//Virtual Cycle 7
		__asm__ __volatile__ ( "stwbrx %0, 0, %1" : : "r" (copy),  "r" (dst) );	 dst++;
		copy = buffer[1];
		__stfiwx( converted, sizeof(float), buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale;
		
		//Virtual Cycle 8
		__asm__ __volatile__ ( "stwbrx %0, 0, %1" : : "r" (copy),  "r" (dst) );	 dst++;
		copy =	buffer[0];
		__stfiwx( converted, 0, buffer );
		converted = __fctiw( scaled );
	
		//Virtual Cycle 9
		__asm__ __volatile__ ( "stwbrx %0, 0, %1" : : "r" (copy),  "r" (dst) );	 dst++;
		copy = buffer[1];
		__stfiwx( converted, sizeof(float), buffer );
		
		//Virtual Cycle 10
		__asm__ __volatile__ ( "stwbrx %0, 0, %1" : : "r" (copy),  "r" (dst) );	 dst++;
		copy = buffer[0];
	
		//Virtual Cycle 11
		__asm__ __volatile__ ( "stwbrx %0, 0, %1" : : "r" (copy),  "r" (dst) );	 dst++;
		copy = buffer[1];

		//Virtual Cycle 11
		__asm__ __volatile__ ( "stwbrx %0, 0, %1" : : "r" (copy),  "r" (dst) );	 dst++;
	}

	//clean up any extras
	while( count-- )
	{
		double scaled = src[0] * scale;
		double converted = __fctiw( scaled );
		__stfiwx( converted, 0, buffer );
		copy = buffer[0];
		__asm__ __volatile__ ( "stwbrx %0, 0, %1" : : "r" (copy),  "r" (dst) );	 
		src++;
		dst++;
	}
	
	//restore the old FPSCR setting
	__asm__ __volatile__ ( "mtfsf 7, %0" : : "f" (oldSetting) );
}

void Float32ToNativeInt32( float *src, signed long *dst, unsigned int count )
{
	register double		scale = 2147483648.0;
	unsigned long	loopCount;
	register float	startingFloat;
	register double scaled;
	register double converted;
	register double		oldSetting;

	//Set the FPSCR to round to -Inf mode
	{
		union
		{
			double	d;
			int		i[2];
		}setting;
		register double newSetting;
		
		//Read the the current FPSCR value
		asm volatile ( "mffs %0" : "=f" ( oldSetting ) );
		
		//Store it to the stack
		setting.d = oldSetting;
		
		//Read in the low 32 bits and mask off the last two bits so they are zero 
		//in the integer unit. These two bits set to zero means round to -infinity mode.
		//Finally, then store the result back
		setting.i[1] &= 0xFFFFFFFC;
		
		//Load the new FPSCR setting into the FP register file again
		newSetting = setting.d;
		
		//Change the FPSCR to the new setting
		asm volatile( "mtfsf 7, %0" : : "f" (newSetting ) );
	}

	//
	//	The fastest way to do this is to set up a staggered loop that models a 7 stage virtual pipeline:
	//
	//		stage 1:		load the src value
	//		stage 2:		scale it to LONG_MIN...LONG_MAX and add a rounding value to it
	//		stage 3:		convert it to an integer within the FP register
	//		stage 4:		store that to the destination
	//
	//	We set it up so that at any given time 7 different pieces of data are being worked on at a time.
	//	Because of the do nothing stage, the inner loop had to be unrolled by one, so in actuality, each
	//	inner loop iteration represents two virtual clock cycles that push data through our virtual pipeline.
	//
	//	The data flow follows standard pipeline diagrams:
	//
	//				stage1	stage2	stage3	stage4	
	//	virtual cycle 1:	data1	-	-	-	   
	//	virtual cycle 2:	data2	data1	-	-	   
	//	virtual cycle 3:	data3	data2	data1	-			 
	//
	//	inner loop:
	//	  virtual cycle A:	data4	data3	data2	data1					  
	//	  virtual cycle B:	data5	data4	data3	data2	
	//	  ...
	//	virtual cycle 4		-	dataD	dataC	dataB	
	//	virtual cycle 5		-	-		dataD	dataC
	//	virtual cycle 6		-	-	-	dataD  
	
	if( count >= 3 )
	{
		//virtual cycle 1
		startingFloat = (src++)[0];
		
		//virtual cycle 2
		scaled = startingFloat * scale;
		startingFloat = (src++)[0];
		
		//virtual cycle 3
		converted = __fctiw( scaled );
		scaled = startingFloat * scale;
		startingFloat = (src++)[0];
				
		count -= 3;
		loopCount = count / 2;
		count &= 1;
		while( loopCount-- )
		{
			register float	startingFloat2;
			register double scaled2;
			register double converted2;
			//register short	copy2;
			
			//virtual Cycle A
			startingFloat2 = (src)[0];
			__asm__ __volatile__ ( "fmul %0, %1, %2" : "=f" (scaled2) : "f" ( startingFloat), "f" (scale) );
			__asm__ __volatile__ ( "stfiwx %0, %1, %2" : : "f" (converted), "b%" (0), "r" (dst) : "memory" );
			__asm__ __volatile__ ( "fctiw %0, %1" : "=f" (converted2) : "f" ( scaled ) );

			//virtual cycle B
		   startingFloat = (src)[1];	 src+=2; 
			__asm__ __volatile__ ( "fmul %0, %1, %2" : "=f" (scaled) : "f" ( startingFloat2), "f" (scale) );
			__asm__ __volatile__ ( "stfiwx %0, %1, %2" : : "f" (converted2), "b%" (4), "r" (dst) : "memory" );
			__asm__ __volatile__ ( "fctiw %0, %1" : "=f" (converted) : "f" ( scaled2 ) );
			dst+=2;
		}
		
		//Virtual Cycle 4
		__stfiwx( converted, 0, dst++ );
		converted = __fctiw( scaled );
		__asm__ __volatile__ ( "fmul %0, %1, %2" : "=f" (scaled) : "f" ( startingFloat), "f" (scale) );
		
		//Virtual Cycle 5
		__stfiwx( converted, 0, dst++ );
		converted = __fctiw( scaled );
	
		//Virtual Cycle 6
		__stfiwx( converted, 0, dst++ );
	}

	//clean up any extras
	while( count-- )
	{
		double scaled = src[0] * scale;
		double converted = __fctiw( scaled );
		__stfiwx( converted, 0, dst );
		dst++;
		src++;
	}

	//restore the old FPSCR setting
	asm volatile( "mtfsf 7, %0" : : "f" (oldSetting) );
}

#endif

////////////////////////////////////////////////////////////////////////////////////////////
//
// Volumes
// 
// Note that all float args are references.  They cannot be passed in by value (PPC thing).
//

// Can't put this in the function, otherwise PPC will give some BS about a link error when loading the kext (_TEXT too big).
const float kDB_To_Gain[] = {
2.511886e-004, 2.822957e-004, 3.172550e-004, 3.565436e-004, 
4.006978e-004, 4.503199e-004, 5.060872e-004, 5.687607e-004, 
6.391956e-004, 7.183532e-004, 8.073136e-004, 9.072907e-004, 
1.019649e-003, 1.145922e-003, 1.287832e-003, 1.447316e-003, 
1.626551e-003, 1.827982e-003, 2.054358e-003, 2.308768e-003, 
2.594684e-003, 2.916008e-003, 3.277125e-003, 3.682961e-003, 
4.139057e-003, 4.651635e-003, 5.227691e-003, 5.875084e-003, 
6.602651e-003, 7.420318e-003, 8.339246e-003, 9.371972e-003, 
1.053259e-002, 1.183694e-002, 1.330282e-002, 1.495023e-002, 
1.680166e-002, 1.888236e-002, 2.122074e-002, 2.384870e-002, 
2.680211e-002, 3.012127e-002, 3.385146e-002, 3.804361e-002, 
4.275490e-002, 4.804964e-002, 5.400008e-002, 6.068741e-002, 
6.820290e-002, 7.664910e-002, 8.614127e-002, 9.680895e-002, 
1.087977e-001, 1.222711e-001, 1.374131e-001, 1.544303e-001, 
1.735548e-001, 1.950477e-001, 2.192023e-001, 2.463481e-001, 
2.768557e-001, 3.111414e-001, 3.496729e-001, 3.929762e-001, 
4.416420e-001, 4.963347e-001, 5.578005e-001, 6.268781e-001, 
7.045103e-001, 7.917563e-001, 8.898069e-001, 1.000000e+000 };

const long kTableSize = sizeof(kDB_To_Gain) / sizeof(float);
	
void GetDbToGainLookup(
	long value,
	long fullRange, ///< Upper bound of the value arg.
	Float32& returnedValue)
{
	if (fullRange <= 0) { returnedValue = 1.0; return; } // assert or something here
	
	float index = (float)value / (float)fullRange;
	
	index *= (float)kTableSize-1.0;

	long lindex=(long)index;
	float fraction=index-lindex;

	if(lindex<0) 
	{
		returnedValue = kDB_To_Gain[0];
		return;
	}
	else if(lindex>=(kTableSize-1)) 
	{
		returnedValue = kDB_To_Gain[kTableSize-1];
		return;
	}
	
	float lookup1=kDB_To_Gain[lindex++];
	float lookup2=kDB_To_Gain[lindex];
	
	Float32 usedValue = (Float32)(lookup1+fraction*(lookup2-lookup1));
	
	returnedValue = usedValue;
}

/////////////////////////////////////////
// Like Michael McDonald, smoooooth...
/////////////////////////////////////////

void SmoothVolume(
	Float32* theMixBuffer,
	const Float32& targetVolume,
	const Float32& lastVolume,
	long theFirstSample,
	long numSampleFrames,
	long usedNumberOfSamples,
	long numberOfChannels)
{
	Float32 theDifference = (targetVolume - lastVolume) / (float)numSampleFrames;
	Float32 currentVolume = lastVolume;
	
	for (long i = theFirstSample; i < usedNumberOfSamples; i += numberOfChannels)
	{
		for (long n = 0; n < numberOfChannels; n++)
		{
			theMixBuffer[i + n] *= currentVolume;
		}

		currentVolume += theDifference;
	}
}

void Volume(
	Float32* theMixBuffer,
	const Float32& currentVolume,
	long theFirstSample,
	long usedNumberOfSamples)
{
	for (long index = theFirstSample; index < usedNumberOfSamples; index++) 
	{
		theMixBuffer[index] *= currentVolume;
	}
}


