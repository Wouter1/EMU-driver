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

#ifndef __EMUUSBAudio__EMUUSBAudioEngine__
#define __EMUUSBAudio__EMUUSBAudioEngine__


#include <libkern/OSByteOrder.h>
#include <libkern/c++/OSCollectionIterator.h>
#include <libkern/c++/OSMetaClass.h>

#include <IOKit/IOLib.h>
// CRAP! https://developer.apple.com/Library/mac/releasenotes/General/APIDiffsMacOSX10_8/Kernel.html
// IOKit/IOSyncer.h has been removed!
#include <IOSyncer.h>
#include <IOKit/IOService.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOMemoryCursor.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOSubMemoryDescriptor.h>
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

#include "StreamInfo.h"
#include "EMUUSBInputStream.h"

class EMUUSBAudioDevice;


class IOSyncer;
class EMUUSBAudioEngine;
class EMUUSBAudioPlugin;

typedef struct FrameListWriteInfo {
    EMUUSBAudioEngine *			audioEngine;
    UInt32						frameListNum;
    UInt32						retryCount;
} FrameListWriteInfo;



/*! connector from the input ring buffer to our IOAudioEngine.
 Collect and filter timestamps from wrap events in the ring
 and forward them to IOAudioEngine */
struct UsbInputRing: RingBufferDefault<UInt8>
{
    IOReturn    init(UInt32 newSize, IOAudioEngine *engine);
    
    void        free();
    
    /*! callback when a ring wraps.
     Give IOAudioEngine a time stamp now.
     We ignore the exact pos of the sample in the frame because 
     measurements showed no relation between this position and the time of 
     the frame that caused the wrap.
     
     @param time the timestamp for the USB frame that wrapped the buffer.
     I guess that the timestamp is for completion of the frame but I can't find
     it in the USB documentations.
     */
    void        notifyWrap(AbsoluteTime time);
    
private:
    /*! take timestamp, but in nanoseconds (instead of AbsoluteTime). */
    void        takeTimeStampNs(UInt64 timeStampNs, Boolean increment);
    
    /*! pointer to the engine, for calling takeTimeStamp. */
    IOAudioEngine   *theEngine;
    
    /*! low pass filter to smooth out wrap times */
    LowPassFilter   lpfilter;
    
    /*! first wraps we tell engine not to increment loop counter. */
    bool            isFirstWrap;
    
    /*! good wraps since start of audio input */
    UInt16          goodWraps;
    
    /*! last received frame timestamp */
    AbsoluteTime    previousfrTimestampNs;
};


/*!
 @abstract implements IOAudioEngine. Provides audio stream services.
 Uses USB services itself to read from EMU device.
 
 @discussion
 This engine currently combines a number of functionalities:
 1. convert USB data to 32 bit float,
 2. USB communication engine for input of isoc streams from EMU,
 3. converter 32 bit float sample data to USB data
 4. USB communication for output to EMU
 5. timing generators so that HAL can call us at the right times.
 
 All data structures for all of these are stuffed into this structure.
 
 the current size of this structure IMHO shows
 that this code is in extremely bad shape. We have code duplication and many
 fields in here that are too detailed in this scope. ALso there basically
 was nothing documented. The amount of fields in this object is large (61)
 and the code is more than 3000 lines. There is a lot of redundancy in the fields as well.
 
 Note that apple (and maybe, USB) has quite hard requirements on how data should
 be allocated to USB packets over time. Details are here
 https://developer.apple.com/library/mac/technotes/tn2274/_index.html.
 Failure to comply with the bandwidth rules for the current sample rate
 and format may not only result in audio corruption for that particular stream,
 but for all of the other streams on the same engine.
 
 Note2. Almost all comments in the code were reverse engineered. Wouter.
 */

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
	virtual IOReturn hardwareSampleRateChanged(const IOAudioSampleRate *sampleRate);
	static void sampleRateHandler (void * target, void * parameter, IOReturn result, IOUSBIsocFrame * pFrames);
    
    /*!
     queue a write from clipOutputSamples. This is called from clipOutputSamples.
     @param parameter the framelistnumber. Actually is a void * that is coming
     out of the StreamInfo struct in the kernel. This driver tries to put an UInt32 in it
     in a hacky way. probably to avoid memory allocation. Wouter: fixed this to UInt64, *  to UInt32 is KO on 64bit archi.
     */
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
    /*! a frame to communicate about sample rates. Callbacks are to sampleRateHandler */
	IOUSBIsocFrame						mSampleRateFrame;
    /*! the last USB frame that has been called for Read */
	UInt64								nextSynchReadFrame;
	
    
#if DEBUGTIMESTAMPS
    UInt64								mLastTimeStamp_nanos;
    UInt64								mLastStampDifference;
    SInt64								mStampDrift;
    UInt64								mLastWrapFrame;
#endif
	
	IOLock*								mWriteLock;
	IOLock*								mFormatLock;
	
    
	bool								mDidOutputVolumeChange;
	bool								mIsOutputMuted;
	IOAudioToggleControl*				mOuputMuteControl;
	EMUUSBAudioSoftLevelControl*		mOutputVolume;
	
	bool								mDidInputVolumeChange;
	bool								mIsInputMuted;
	IOAudioToggleControl*				mInputMuteControl;
	EMUUSBAudioSoftLevelControl*		mInputVolume;
    
    /*! Connect  EMUUSBInputStream close event. Can this be done easier?  */
    struct OurUSBInputStream: public EMUUSBInputStream {
    public:
        /*! init
         @param engine ptr to the audio engine
         @param ring  initialized UsbInputRing. Not started.
         @param frameQueue fully initialized FrameSizeQueue. */
        void    init(EMUUSBAudioEngine * engine, UsbInputRing * ring, FrameSizeQueue * frameQueue);
        void    notifyClosed();
        
    private:
        // pointer to the engine. This is just the parent
        EMUUSBAudioEngine *             theEngine;
    };
    
    
    
    /*! StreamInfo relevant for the reading-from-USB (recording). */
	OurUSBInputStream						usbInputStream;
    /*! the USB input ring. We need to pass it downwards and handle time signals */
    UsbInputRing                        usbInputRing;
    /*! Ring to store recent frame sizes */
    FrameSizeQueue                      frameSizeQueue;

    /*! StreamInfo relevant for the writing-to-USB (playback) */
	StreamInfo                          mOutput;
    
    
	// engine data (i.e., not stream-specific)
	IOSyncer *							mSyncer;
	EMUUSBAudioDevice *					usbAudioDevice;
    
    /*! IOUSBController, handling general USB properties like current frame nr */
	IOUSBController *					mBus;// DT
	IOMultiMemoryDescriptor *			theWrapRangeDescriptor;
	IOSubMemoryDescriptor *				theWrapDescriptors[2];
	IOMemoryDescriptor *				neededSampleRateDescriptor;
    
	UInt32 *							aveSampleRateBuf;		// 4 byte value
    /*!
     this is a timer with time kMinimumInterval that calls waitForFirstUSBFrameCompletion.
     It is added to our workLoop for handling.
     */
	IOTimerEventSource *				startTimer;
	thread_call_t						mPluginInitThread;
	IOAudioStream *						mainStream;
	IONotifier *						mPluginNotification;
	EMUUSBAudioPlugin *					mPlugin;
	
	OSArray *							mStreamInterfaces; // array of streams
    
	/*! orig doc: we need to drop the first frames because the device can't flush the first frames. */
    static const long					kNumberOfStartingFramesToDrop = 2;
    
	// parameters and variables common across streams (e.g., sample rate, bit resolution, etc.)
	UInt32								previouslyPreparedBufferOffset;
	UInt32								safeToEraseTo;
	UInt32								lastSafeErasePoint;
	UInt32								averageSampleRate;
    /*! selected rate, eg 96000. WARNING usually this is PLAIN ZERO ==0 not clear why this is not set properly. Avoid this!!!!!! */
	UInt32								hardwareSampleRate;
	UInt32								mChannelWidth;	// 16 or 24 bit
	UInt32								bytesPerSampleFrame;
	UInt32								fractionalSamplesRemaining;
	UInt8								refreshInterval;
	UInt8								framesUntilRefresh;
	UInt8								mAnchorResetCount;
	UInt8								mHubSpeed;
    /*! update frequency for IOUSBLowLatencyIsocFrame. see the documentation IOUSBPipe.
     0=only at end. 1..8 means 1..8 refresh calls per millisecond. Wouter: I disabled this to get the stuff working at all. */
	UInt8								mPollInterval;
    
    /*! guess: flag that is iff while we are inside the writeHandler. */
	Boolean								inWriteCompletion;
	Boolean								usbStreamRunning;
    
    
	Boolean								needTimeStamps;
	UInt32								runningOutputCount;
	SInt32								lastDelta;
	UInt32								lastNonZeroFrame;
    
	UInt32								nextExpectedOutputFrame;
	
    
    
    Boolean previousTimeWasFirstTime;
    
	
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
    
    /*!
     @abstract detects start of USB and then sets startingEngine=FALSE
     @discussion Check that the first USB frame has been read.
     Uses a timer to check every 50 microseconds if first USB frame did complete.
     After 60 tries this process time-outs.
     */
 	static void waitForFirstUSBFrameCompletion (OSObject * owner, IOTimerEventSource * sender);
    
	virtual bool willTerminate (IOService * provider, IOOptionBits options);
    
    /**
     Get stringDescriptor into buffer. Buffer has to be kStringBufferSize
     @param index the string index to be fetched. If null, the get fails.
     @return true if succes, false if failed. If failed, buffer is filled with "Unknown".
     */
    virtual Boolean getDescriptorString(char *buffer, UInt8 index);
    
	virtual OSString * getGlobalUniqueID ();
    
    
    /*!  initializes the write a of a frame list (typ. 64 frames) to USB. called from writeHandler */
    IOReturn writeFrameList (UInt32 frameListNum);
    
    /*! called from performAudioEngineStart */
    IOReturn startUSBStream();
    
    /*! called from performAudioEngineStop */
    IOReturn stopUSBStream ();
    
    virtual UInt32 getCurrentSampleFrame (void);
    
    virtual IOAudioStreamDirection getDirection ();
    virtual void *getSampleBuffer (void);
	UInt32 getSampleBufferSize (void);
    
    /*! compresses audio to a smaller block.
     @discussion when convertInputSamples skips bytes, this code attempts to connect the ends.
     This code is referring to pFrames which is void inside convertInputSamples. Therefore this
     code seems not safe to use. */
	void CoalesceInputSamples(SInt32 numBytesToCoalesce, IOUSBLowLatencyIsocFrame * pFrames);
	virtual void resetClipPosition (IOAudioStream *audioStream, UInt32 clipSampleFrame);
    
    /*! implements the IOAudioEngine interface. Called by the HAL, who does timing according to our timestamps when it thinks we are ready to process more data.
     the driver must clip any excess floating-point values under –1.0 and over 1.0 —which can happen when multiple clients are adding their values to existing values in the same frame—and then convert these values to whatever format is required by the hardware. When clipOutputSamples returns, the converted values have been written to the corresponding locations in the sample buffer. The DMA engine grabs the frames as it progresses through the sample buffer and the hardware plays them as sound.
     
     @param mixbuf a pointer to an array of sampleFrames. Each sampleFrame contains <numchan> floats, where <numchan> is the number of audio channels (eg, 2 for stereo). This is the audio that we need to play. The floats can be outside [-1,1] in which case we also need to clip the data.
     @param sampleBuf
     @param firstSampleFrame
     @param numSampleFrames number of sampleFrames in the mixbuf
     @param streamFormat
     @param audioStream
     */
    virtual IOReturn clipOutputSamples (const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream);
    
	/*!
     @abstract convert numSampleFrames samples from mInput into the destBuf. Implements IOAudioInterface input handler. Called from IOAudioStream::readInputSamples.
     @discussion
     this is called when the HAL needs audio samples from the input. HAL calls this based on
     timer info returned from "takeTimeStamp" (FIXME apple doc seems wrong. what's the real name?)
     The meaning of the arguments was reverse engineered from the IOAudioStream source code.
     
     @param samplebuf is the start of our frame buffer. We made this buffer ourselves. Should be read in the range [firstSampleFrame,firstSampleFrame+numSampleFrames>
     @param  destbuff is a frame * : start of the client’s buffer. frame = struct with <#channels> floats.
     simple test with audacity shows that the dest is expecting floats in range [-1,1].
     dest buffer should be filled in the range [0..numSampleFrames>.
     @param  firstsampleframe is the first frame that we should use from sourceBuf.
     @param  numSampleFrames is the number of frames that need to be converted.
     @param  streamFormat the IOAudioStreamFormat
     @param  audioStream the structure of the calling object, a ‘parent’ pointer.
     
     @returns kIOReturnSuccess. I noticed that if you just return without putting data in destBuf that audacity will keep waiting for the start of the music without actually storing. It's not clear if this part of the protocol with HAL.
     */
    virtual IOReturn convertInputSamples (const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream);
	
    /*! This gets called when the HAL wants to select one of the different formats that we made available via mainStream->addAvailableFormat */
    virtual IOReturn performFormatChange (IOAudioStream *audioStream, const IOAudioStreamFormat *newFormat, const IOAudioSampleRate *newSampleRate);
    
    /*! compute the averageFrameSamples
     @param sampleRate the target samplerate, eg 96000
     @param averageFrameSize output: =inSampleRate / 1000 = the average #bytes for the frame. Eg 96 for 96kHz samplerate.
     @param additionalSampleFrameFreq: = 1000/(inSampleRate%1000) (or 0) (Wouter:??) */
	void CalculateSamplesPerFrame (UInt32 sampleRate, UInt16 * averageFrameSize, UInt16 * additionalSampleFrameFreq);
	IOReturn initBuffers();
    
    /*! copy the mNewReferenceUSBFrame and mNewReferenceWallTime value from usbAudioDevice
     @param frame gets copy of mNewReferenceUSBFrame
     @param time gets copy of mNewReferenceWallTime
     @return kIOReturnSuccess. I don't see why it would ever fail. */
	IOReturn getAnchor(UInt64* frame, AbsoluteTime* time);
    
    /*! Generate estimated timestamp for the moment a byte in this frame was coming in on the USB stream.
     @discussion This timestamp should be very accurate and is used by HAL to create callbacks to us.
     @param usbFrameIndex the frame index number in the frameList
     @param preWrapBytes the the number of bytes before the wrap
     @param byteCount the total number of bytes in this frameList (including the ones before the wrap).
     byteCount MUST never == 0. If this computation is incorrect, we will encounter audio artifacts
     */
	AbsoluteTime generateTimeStamp (UInt32 usbFrameIndex, UInt32 preWrapBytes, UInt32 byteCount);
    
	IOReturn eraseOutputSamples(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream);
    
	IOReturn hardwareSampleRateChangedAux(const IOAudioSampleRate *sampleRate, StreamInfo &info);
    
	void				findAudioStreamInterfaces(IOUSBInterface *pAudioControlIfc); // AC mod
    
	void                setupChannelNames();
    
    /*! This is set true when we got signalled to terminate */
    Boolean								terminatingDriver;
    
    /*! buffer to temporarily store ring buffer data  for conversion to float */
    UInt8 *             buf;

};

#endif /* defined(__EMUUSBAudio__EMUUSBAudioEngine__) */
