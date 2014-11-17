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
//--------------------------------------------------------------------------------
//
//	File:		EMUUSBAudioEngine.h
//
//	Contains:	Support for the USB Audio Class Stream Interface.
//
//	Technology:	Mac OS X
//
//--------------------------------------------------------------------------------

#ifndef _EMUUSBAUDIOENGINE_H
#define _EMUUSBAUDIOENGINE_H

#include <libkern/OSByteOrder.h>
#include <libkern/c++/OSCollectionIterator.h>
#include <libkern/c++/OSMetaClass.h>

#include <IOKit/IOLib.h>
#include <IOKit/IOSyncer.h>
#include <IOKit/IOService.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOMemoryCursor.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IOMultiMemoryDescriptor.h>

#include <IOKit/audio/IOAudioDevice.h>
#include <IOKit/audio/IOAudioPort.h>
#include <IOKit/audio/IOAudioTypes.h>
#include <IOKit/audio/IOAudioDefines.h>
#include <IOKit/audio/IOAudioLevelControl.h>
#include <IOKit/audio/IOAudioEngine.h>
#include <IOKit/audio/IOAudioStream.h>

#include <IOKit/usb/IOUSBPipe.h>
#include <IOKit/usb/IOUSBDevice.h>
#include <IOKit/usb/IOUSBInterface.h>

#include "EMUUSBAudioCommon.h"
#include "EMUUSBAudioDevice.h"
#include "EMUUSBAudioLevelControl.h"
#include "EMUUSBAudioMuteControl.h"
#include "USBAudioObject.h"
#include "EMUUSBAudioClip.h"

class EMUUSBAudioDevice;

//	-----------------------------------------------------------------
#define	kSampleRate_44100				44100
#define kDefaultSamplingRate			kSampleRate_44100
#define	kBitDepth_16bits				16
#define kBitDepth_24bits				24
#define	kChannelCount_MONO				1
#define	kChannelCount_STEREO			2
#define kChannelCount_QUAD				4
#define kChannelCount_10				10 // this is stupid
#define kMinimumInterval				1
#define kMinimumFrameOffset				1
#define kUSB2FrameOffset				1// additional offset for high speed USB 2.0
#define kWallTimeExtraPrecision         10000
#define kWallTimeConstant				1000000
#define kRefreshInterval				128
#define kRefreshCount					8//was 8
#define kInvariantCoeff					1024
#define kInvariantCoeffM1				1023
#define kInvariantCoeffDiv2				512


// these should be dynamic based on poll interval
#if 1

#define kNumberOfFramesPerMillisecond 8
#define kPollIntervalShift 3 // log2 of kNumberOfFramesPerMillisecond
#define NUMBER_FRAMES 64

#else

#define kNumberOfFramesPerMillisecond 1
#define kPollIntervalShift 0
#define NUMBER_FRAMES 8

#endif

#define RECORD_NUM_USB_FRAME_LISTS				4
#define RECORD_FRAME_LISTS_LIMIT				RECORD_NUM_USB_FRAME_LISTS - 1
#define RECORD_NUM_USB_FRAMES_PER_LIST			NUMBER_FRAMES
#define RECORD_NUM_USB_FRAME_LISTS_TO_QUEUE		4

#define PLAY_NUM_USB_FRAME_LISTS				4
#define PLAY_NUM_USB_FRAMES_PER_LIST			NUMBER_FRAMES
#define PLAY_NUM_USB_FRAME_LISTS_TO_QUEUE		2
// was 2
#define kMaxAttempts							3

#define FRAMESIZE_QUEUE_SIZE				    128

class IOSyncer;
class EMUUSBAudioEngine;
class EMUUSBAudioPlugin;

typedef struct FrameListWriteInfo {
    EMUUSBAudioEngine *				audioEngine;
    UInt32								frameListNum;
    UInt32								retryCount;
} FrameListWriteInfo;

class EMUUSBAudioEngine : public IOAudioEngine {
    friend class EMUUSBAudioDevice;

    OSDeclareDefaultStructors (EMUUSBAudioEngine);

public:
    virtual bool init (OSDictionary *properties);
    virtual void free ();
    virtual bool initHardware (IOService *provider);
    virtual bool start (IOService *provider);
    virtual void stop (IOService *provider);
	virtual bool requestTerminate (IOService * provider, IOOptionBits options);
    virtual bool terminate (IOOptionBits options = 0);
	virtual void detach (IOService *provider) ;
	virtual void close(IOService *forClient, IOOptionBits options = 0);

    virtual IOReturn performAudioEngineStart ();
    virtual IOReturn performAudioEngineStop ();
#if 1
	virtual IOReturn hardwareSampleRateChanged(const IOAudioSampleRate *sampleRate);
#endif
	static void sampleRateHandler (void * target, void * parameter, IOReturn result, IOUSBIsocFrame * pFrames);
#if PREPINPUT
	static void prepInputHandler(void* object, void* frameListIndex, IOReturn result, IOUSBLowLatencyIsocFrame* pFrames);
#endif
    static void readHandler (void * object, void * frameListIndex, IOReturn result, IOUSBLowLatencyIsocFrame * pFrames);
    static void writeHandler (void * object, void * parameter, IOReturn result, IOUSBLowLatencyIsocFrame * pFrames);
	virtual IOReturn pluginDeviceRequest (IOUSBDevRequest * request, IOUSBCompletion * completion);
	virtual void pluginSetConfigurationApp (const char * bundleID);
	virtual void registerPlugin (EMUUSBAudioPlugin * thePlugin);
	static void	pluginLoaded (EMUUSBAudioEngine * usbAudioEngineObject);
	UInt32 getHardwareSampleRate() { return hardwareSampleRate;}
	
	void addSoftVolumeControls(void);
	static	IOReturn softwareVolumeChangedHandler (OSObject *target, IOAudioControl *audioControl, SInt32 oldValue, SInt32 newValue);
	static	IOReturn softwareMuteChangedHandler (OSObject *target, IOAudioControl *audioControl, SInt32 oldValue, SInt32 newValue);
	
protected:
	IOUSBIsocCompletion					sampleRateCompletion;
	IOUSBIsocFrame						mSampleRateFrame;
	UInt64								nextSynchReadFrame;
	

	#if DEBUGTIMESTAMPS
		UInt64								mLastTimeStamp_nanos;
		UInt64								mLastStampDifference;
		SInt64								mStampDrift;
		UInt64								mLastWrapFrame;
	#endif
	
	volatile UInt32						shouldStop;
#if LOCKING
	IOLock*								mLock;
	IOLock*								mWriteLock;
	IOLock*								mFormatLock;
#endif
	
#if PREPINPUT
	IOUSBLowLatencyIsocFrame *			mClearIsocFrames;
	IOUSBLowLatencyIsocCompletion *		mClearInputCompletion;
#endif

	bool								mDidOutputVolumeChange;
	bool								mIsOutputMuted;
	IOAudioToggleControl*				mOuputMuteControl;
	EMUUSBAudioSoftLevelControl*		mOutputVolume;
	
	bool								mDidInputVolumeChange;
	bool								mIsInputMuted;
	IOAudioToggleControl*				mInputMuteControl;
	EMUUSBAudioSoftLevelControl*		mInputVolume;

	/// stream-specific parameters, state variables and data
	struct StreamInfo {
		UInt32		bufferOffset;
		UInt32		multFactor;
		UInt32		maxFrameSize;
		UInt32		numUSBFrameLists;
		UInt32		numUSBFramesPerList;
		UInt32		numUSBTimeFrames;
		UInt32		numUSBFrameListsToQueue;
		UInt32		bufferSize;
		UInt32		numChannels;
		UInt32		frameOffset;
		UInt8		streamDirection;
		UInt8		interfaceNumber;
		UInt8		alternateSettingID;
		volatile UInt32						currentFrameList;
		
		IOUSBInterface				  *streamInterface;
		IOAudioStream				  *audioStream;
		IOUSBPipe					  *pipe;
		IOUSBPipe					  *associatedPipe;
		IOUSBLowLatencyIsocFrame	  *usbIsocFrames;
		IOUSBLowLatencyIsocCompletion *usbCompletion;
		// you want ddescriptors? we got descriptors!
		IOBufferMemoryDescriptor	*usbBufferDescriptor;
		IOBufferMemoryDescriptor	*bufferMemoryDescriptor;
		IOSubMemoryDescriptor		**bufferDescriptors;
		void *						bufferPtr;		
		UInt64						usbFrameToQueueAt;
		UInt64 *					frameQueuedForList;
	};
	
	StreamInfo						mInput;
	StreamInfo						mOutput;

	// engine data (i.e., not stream-specific)
	IOSyncer *							mSyncer;
	EMUUSBAudioDevice *					usbAudioDevice;
	IOUSBController *					mBus;// DT 
	IOMultiMemoryDescriptor *			theWrapRangeDescriptor;
	IOSubMemoryDescriptor *				theWrapDescriptors[2];
	IOMemoryDescriptor *				neededSampleRateDescriptor;
	void *								readBuffer;
	UInt32 *							aveSampleRateBuf;		// 4 byte value
	IOTimerEventSource *				startTimer;
	thread_call_t						mPluginInitThread;
	IOAudioStream *						mainStream;
	IONotifier *						mPluginNotification;
	EMUUSBAudioPlugin *					mPlugin;
	
	OSArray *							mStreamInterfaces; // array of streams

	static const long					kNumberOfStartingFramesToDrop = 2; // need to drop the first frames because the device can't flush the first frames.

	// parameters and variables common across streams (e.g., sample rate, bit resolution, etc.)
	UInt32								previouslyPreparedBufferOffset;
	UInt32								safeToEraseTo;
	UInt32								lastSafeErasePoint;
	UInt32								readUSBFrameListSize;
	UInt32								averageSampleRate;
	UInt32								hardwareSampleRate;
	UInt32								mChannelWidth;	// 16 or 24 bit
	UInt32								bytesPerSampleFrame;
	UInt32								fractionalSamplesRemaining;
	UInt8								refreshInterval;
	UInt8								framesUntilRefresh;
	UInt8								mAnchorResetCount;
	UInt8								mHubSpeed;
	UInt8								mPollInterval;
	Boolean								inReadCompletion;
	Boolean								inWriteCompletion;
	Boolean								usbStreamRunning;
	Boolean								startingEngine;
	Boolean								terminatingDriver;
	UInt32								mDropStartingFrames;
	Boolean								needTimeStamps;
	UInt32								lastInputSize;
	UInt32								lastInputFrames;
	UInt32								runningInputCount;
	UInt32								runningOutputCount;
	SInt32								lastDelta;
	UInt32								lastNonZeroFrame;
	UInt32								nextExpectedFrame;
	UInt32								nextExpectedOutputFrame;
	
	UInt32								frameSizeQueue[FRAMESIZE_QUEUE_SIZE];
	UInt32								frameSizeQueueFront, frameSizeQueueBack;
	
	void	GetDeviceInfo (void);
	IOReturn	PrepareWriteFrameList (UInt32 usbFrameListIndex);
	IOReturn	SetSampleRate (EMUUSBAudioConfigObject *usbAudio, UInt32 sampleRate);
	IOReturn	AddAvailableFormatsFromDevice (EMUUSBAudioConfigObject *usbAudio,
												UInt8 ourInterfaceNumber);
	IOReturn	CheckForAssociatedEndpoint (EMUUSBAudioConfigObject *usbAudio,
												UInt8 ourInterfaceNumber,
												UInt8 alernateSettingID);
	IOReturn	GetDefaultSettings (IOUSBInterface *streamInterface,
								    IOAudioSampleRate * sampleRate);

    static bool audioDevicePublished (EMUUSBAudioEngine *audioEngine, void *ref, IOService *newService);
 	static void waitForFirstUSBFrameCompletion (OSObject * owner, IOTimerEventSource * sender);

	virtual bool willTerminate (IOService * provider, IOOptionBits options);
	virtual OSString * getGlobalUniqueID ();
    IOReturn readFrameList (UInt32 frameListNum);
    IOReturn writeFrameList (UInt32 frameListNum);

    IOReturn startUSBStream();
    IOReturn stopUSBStream ();

    virtual UInt32 getCurrentSampleFrame (void);

    virtual IOAudioStreamDirection getDirection ();
    virtual void *getSampleBuffer (void);
	UInt32 getSampleBufferSize (void);
	void GatherInputSamples(IOUSBLowLatencyIsocFrame* pFrames);
	void CoalesceInputSamples(SInt32 numBytesToCoalesce, IOUSBLowLatencyIsocFrame * pFrames);
	virtual void resetClipPosition (IOAudioStream *audioStream, UInt32 clipSampleFrame);
    virtual IOReturn clipOutputSamples (const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream);
	virtual IOReturn convertInputSamples (const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream);
	
    virtual IOReturn performFormatChange (IOAudioStream *audioStream, const IOAudioStreamFormat *newFormat, const IOAudioSampleRate *newSampleRate);
	void CalculateSamplesPerFrame (UInt32 sampleRate, UInt16 * averageFrameSize, UInt16 * additionalSampleFrameFreq);
	IOReturn initBuffers();
#if PREPINPUT
	void prepInputPipe();
#endif

	IOReturn getAnchor(UInt64* frame, AbsoluteTime* time);
	AbsoluteTime generateTimeStamp (UInt32 usbFrameIndex, UInt32 preWrapBytes, UInt32 byteCount);
	IOReturn eraseOutputSamples(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream);

	IOReturn hardwareSampleRateChangedAux(const IOAudioSampleRate *sampleRate, StreamInfo &info);

	void				findAudioStreamInterfaces(IOUSBInterface *pAudioControlIfc); // AC mod

	void	setupChannelNames();
	void	PushFrameSize(UInt32 frameSize);
	void	AddToLastFrameSize(SInt32 toAdd);
	UInt32	PopFrameSize();
	void	ClearFrameSizes();

};

#endif /* _EMUUSBAUDIOENGINE_H */
