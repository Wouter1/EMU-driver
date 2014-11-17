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

/*! initial time in ms for USB callback timer. Normally kRefreshInterval is used. */
#define kMinimumInterval				1
#define kMinimumFrameOffset				1
#define kUSB2FrameOffset				1// additional offset for high speed USB 2.0
#define kWallTimeExtraPrecision         10000
/*! something nanoseconds used in waitForFirstUSBFrameCompletion, maybe for initial sync?*/
#define kWallTimeConstant				1000000
/*! main rate for USB callback timer (ms) */
#define kRefreshInterval				128
/*! after this number of reads from the USB, the time is re-anchored. See getAnchorFrameAndTimeStamp*/
#define kRefreshCount					8
/*! params for very simple lowpass filter in jitter filter */
#define kInvariantCoeff					1024
#define kInvariantCoeffM1				1023
#define kInvariantCoeffDiv2				512


// these should be dynamic based on poll interval
/* Wouter: I think this confusion between 1 and 8 is caused by differences in USB 1 vs 2.  Earlier versions of the specification divide bus time into 1-millisecond frames, each of which can carry multiple transactions to multiple destinations. (A transaction contains two or more packets: a token packet and one or more data packets, a handshake packet, or both.) The USB 2.0 specification divides the 1-millisecond frame into eight, 125-microsecond microframes, each of which can carry multiple transactions to multiple destinations.
  
 This driver seems hard coded to do 1000 requests per second (see CalculateSamplesPerFrame.). What puzzles me is that this is the maximum.
 
 Note on the docu that I added: A number of functions here are obligatory implementations of the IOAudioEngine. Others are support functions for our convenience. It is unfortunate that the distinction is unclear and also that I have to document functions that should (and probably do) already have documentation in the interface definition.  Maybe I'm missing something?
*/
#if 1

/*! Number of USB frames per millisecond for USB2 */
#define kNumberOfFramesPerMillisecond 8
#define kPollIntervalShift 3 // log2 of kNumberOfFramesPerMillisecond
/*! Number of frames that we transfer in a request to USB. USB transfers frames once per millisecond
 But we prepare NUMBER_FRAMES so that we do not have to deal with that every ms but only every NUMBER_FRAMES ms.
 These frames are grouped into a single request, the larger this number the larger the chunks we get from USB.

 Technically this size should be irrelevant, because the array is refreshed in memory anyway and because the
 timestamps we need are stored in the USB frames as long as we need them.

 However, it seems that the exact time at which we call takeTimeStamp is critical as well. 
 

 */

#define NUMBER_FRAMES 64
//#define NUMBER_FRAMES 16


#else

/*! Wouter: this looks like old USB1. Not supported anymore */
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
#if 1
	virtual IOReturn hardwareSampleRateChanged(const IOAudioSampleRate *sampleRate);
#endif
	static void sampleRateHandler (void * target, void * parameter, IOReturn result, IOUSBIsocFrame * pFrames);
#if PREPINPUT
	static void prepInputHandler(void* object, void* frameListIndex, IOReturn result, IOUSBLowLatencyIsocFrame* pFrames);
#endif
    /*!readHandler is the callback from USB completion. Updates mInput.usbFrameToQueueAt.

     @discussion Wouter: This implements IOUSBLowLatencyIsocCompletionAction and the callback function for USB frameread. 
     Warning: This routine locks the IO. Probably because the code is not thread safe.
     Note: The original code also calls it explicitly from convertInputSamples but that hangs the system (I think because of the lock).
     
     
     @param object the parent audiohandler
     @param frameListIndex the frameList number that completed and triggered this call.
     @param result  this handler will do special actions if set values different from kIOReturnSuccess. This probably indicates the USB read status. 
     @param pFrames the frames that need checking. Expects that all RECORD_NUM_USB_FRAMES_PER_LIST frames are available completely.
     
     */
    static void readCompleted (void * object, void * frameListIndex, IOReturn result, IOUSBLowLatencyIsocFrame * pFrames);
    
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
    
    /*!
     @abstract  the state or our mInput/mOutput USB stream.
     @discussion To handle an USB stream the program joggles with a set of USB frames,
     callback handlers etc. These are grouped in this object. 
     
     See also the USB Device interface guide.
     
     The functions used to read from and write to isochronous endpoints are ReadIsochPipeAsync and WriteIsochPipeAsync. Both functions include the following two parameters:
     
      * numFrames—The number of frames for which to transfer data
     
      * frameList—A pointer to an array of structures that describe the frames. specifies the list of transfers you want to occur.
     */
	struct StreamInfo {
        /*! The point where the next raw USB byte can be written in bufferPtr. Always in [0, bufferSize> */
		UInt32		bufferOffset;
        
        /*! Wouter: =numChannels * #bytes per sample for this stream =  frame size. 
         this will be 6 for stereo 24 bit audio. */
		UInt32		multFactor;
        
        /*! 'specifiying how many bytes to read or write' (Wouter: from  USH.h).
         The max number of bytes that we can receive in a single frame which is part of a USB frameList.
         Is set to (averageFrameSamples + 1) * mInput.multFactor. Eg 97*6=582 for 96kHz/24 stereo  */
		UInt32		maxFrameSize;
        
        /*! This is the length of the bufferDescriptors array and usbIsocFrames.
        // I think sizes of 4 and 8 are usual.*/
		UInt32		numUSBFrameLists;
        /*!Wouter: the number of usb frames in our lists. Hard set to RECORD_NUM_USB_FRAMES_PER_LIST or PLAY_NUM_USB_FRAMES_PER_LIST (64 usually). USB calls back to us only after 'completion' (I guess all frames were read, or something went wrong), calling back to readHandler.
         */
		UInt32		numUSBFramesPerList;
        /*! = mInput.numUSBFramesPerList / kNumberOfFramesPerMillisecond = 8 usually. 
         Used as increment for usbFrameToQueueAt*/
		UInt32		numUSBTimeFrames;
        /*!
         @abstract Number of frames we have in use for reading USB.
         @discussion Hard set to RECORD_NUM_USB_FRAME_LISTS_TO_QUEUE or PLAY_NUM_USB_FRAME_LISTS_TO_QUEUE. Typically 2 or 4.  */
		UInt32		numUSBFrameListsToQueue;

		/*! 
         size of the bufferPtr array. See bufferMemoryDescriptor.
         @discussion 
         numSamplesInBuffer * multFactor = # bytes in the buffer.
         where numSamplesInBuffer =PAGE_SIZE * (2 + (sampleRate.whole > 48000) + (sampleRate.whole > 96000))
         and PAGE_SIZE=4096 bytes. 
         */
        UInt32		bufferSize;
        
        // Wouter: The nummer of channels coming in. Typically 1 (mono) or 2 (stereo).
		UInt32		numChannels;
		UInt32		frameOffset;
		UInt8		streamDirection;
		UInt8		interfaceNumber;
		UInt8		alternateSettingID;
        /*! the framelist that we are expecting to complete from next. Basically runs from 0 to numUSBFrameListsToQueue-1 and then
         restarts at 0. Updated after readHandler handled the block. */
		volatile UInt32						currentFrameList;
		
		IOUSBInterface				  *streamInterface;
		IOAudioStream				  *audioStream;
        /*! IOUSBPipe used for isochronous reading input streams from USB*/
		IOUSBPipe					  *pipe;
		IOUSBPipe					  *associatedPipe;
        
        /*! @discussion Wouter : array of IOUSBLowLatencyIsocFrame containing USB status for a frame. size (for mInput) = numUSBFrameLists * numUSBFramesPerList */
		IOUSBLowLatencyIsocFrame	  *usbIsocFrames;
        
        /*! @abstract
          array IOUSBLowLatencyIsocCompletion[numUSBFrameLists] 
         @discussion for each frameList there is this callback on completion.
         each one also contains a parameter for the callback.
         The parameter is:
         - for write buffer: number of bytes from the 0 wrap, 0 if this buffer didn't wrap
         - for read buffer; the frameListNum
		*/
        IOUSBLowLatencyIsocCompletion *usbCompletion;
        
		// you want ddescriptors? we got descriptors!
        /*!  Big mem block to store all data from reading USB data according to the framelists.
          size mOutput.bufferSize or mInput.numUSBFrameLists * readUSBFrameListSize bytes */
		IOBufferMemoryDescriptor	*usbBufferDescriptor;
        
        /*! The memory descriptor for the The intermediate ring buffer of size bufferSize.*/
		IOBufferMemoryDescriptor	*bufferMemoryDescriptor;
        /*! array of pointers to IOMemoryDescriptor of length [frameListnum]. This is where raw USB data will come in. 
         @discussion Contains copy of the received USB data.
         When a framelist is complete, readhHandler copies the data from the frame list
         to the mInput buffer so that the frameList can be redeployed.
         We need this (1) to have a fixed ring buffer as HAL is expecting us to have
         (2) to free up the framelist so that we can redeploy it to continue reading
         (3) so that we can do int-to-float conversion 'offline'.
         However it seems that these are no strong reasons, probably we could just simulate a ring buffer
         and redeploy the frameList only after conversion for the HAL.
         */
		IOSubMemoryDescriptor		**bufferDescriptors;
        
        /*! shortcut to bufferMemoryDescriptor actual buffer bytes. Really UInt8*. */
		void *						bufferPtr;
        
        /*! the expected USB MBus Frame number where we are looking for. Initially this is at frameOffset from the current frame number */
		UInt64						usbFrameToQueueAt;
        /*! an array of size [frameListnum] holding usbFrameToQueueAt for each frame when it was requested for read */
		UInt64 *					frameQueuedForList;
        
        
        /*! Used in GatherInputSamples to keep track of which framelist we were converting. */
        UInt64 previousFrameList;
        /*! used in GatherInputSamples to keep track of which frame in the list we were left at (first non-converted frame index) */
        UInt32 frameIndex;
        /*! wrap timestamp for this framelist. 0 if no wrap occured */
        UInt64 frameListWrapTimeStamp;
        
        
	};
    
    /*! @discussion StreamInfo relevant for the reading-from-USB (recording). */
	StreamInfo						mInput;
    /*! @discussion StreamInfo relevant for the writing-to-USB (playback) */
	StreamInfo						mOutput;
    
	// engine data (i.e., not stream-specific)
	IOSyncer *							mSyncer;
	EMUUSBAudioDevice *					usbAudioDevice;
    
    /*! IOUSBController, handling general USB properties like current frame nr */
	IOUSBController *					mBus;// DT
	IOMultiMemoryDescriptor *			theWrapRangeDescriptor;
	IOSubMemoryDescriptor *				theWrapDescriptors[2];
	IOMemoryDescriptor *				neededSampleRateDescriptor;
    
    /*!  direct ptr to USB data buffer = mInput. usbBufferDescriptor. These are
     the buffers for each of the USB readFrameLists. Not clear why this is allocated as one big slot. */
	void *								readBuffer;
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
    /*! = maxFrameSize * numUSBFramesPerList; total byte size for buffering frameLists for USB reading. eg 582*64 = 37248.
     */
	UInt32								readUSBFrameListSize;
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
    
    /*!  this is TRUE until we receive the first USB packet. */
	Boolean								startingEngine;
	Boolean								terminatingDriver;
    
    /*! number of initial frames that are dropped. See kNumberOfStartingFramesToDrop */
	UInt32								mDropStartingFrames;
	Boolean								needTimeStamps;
	UInt32								lastInputSize;
	UInt32								lastInputFrames;
	UInt32								runningInputCount;
	UInt32								runningOutputCount;
	SInt32								lastDelta;
	UInt32								lastNonZeroFrame;
    
    /*! The value we expect for firstSampleFrame in next call to convertInputSamples. 
     The reading of our input buffer should be continuous, not jump around. */
	UInt32								nextExpectedFrame;
	UInt32								nextExpectedOutputFrame;
	
    /*! FIFO Queue for framesizes.  Used to communicate USB input frames to the USB writer. */
	UInt32								frameSizeQueue[FRAMESIZE_QUEUE_SIZE];
    /*! frameSizeQueueFront points to first available element in frameSizeQueue*/
	UInt32								frameSizeQueueFront;
    /*!  frameSizeQueueBack points to first free element in frameSizeQueue. */
    UInt32 frameSizeQueueBack;
    
    /*! last received frame timestamp */
    AbsoluteTime previousfrTimestampNs;
    Boolean previousTimeWasFirstTime;
    /*! good wraps since start of audio input */
    UInt16 goodWraps;
    /*! position (time) for the filter (ns) */
    UInt64 x;
    /*! speed (wrap-time-difference) for the filter (ns) */
    UInt64 dx;
    /*! the deviation/drift for the filter (ns)*/
    UInt64 u;

	
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
	virtual OSString * getGlobalUniqueID ();
    
    /*!
     @abstract initializes the read of a frameList (typ. 64 frames) from USB.
     @discussion queues all numUSBFramesPerList frames in given frameListNum for reading.
     The callback when the read is complete is readHandler. There used to be multiple callbacks
     every mPollInterval
     Also it is requested to update the info every 1 ms. 
     @param frameListNum the frame list to use, in range [0-numUSBFrameLists> which is usually 0-8.
     orig docu said "frameListNum must be in the valid range 0 - 126".
     This makes no sense to me. Maybe this is a hardware requirement.
     */
    IOReturn readFrameList (UInt32 frameListNum);
    
    /*!  initializes the write a of a frame list (typ. 64 frames) to USB. called from writeHandler */
    IOReturn writeFrameList (UInt32 frameListNum);
    
    IOReturn startUSBStream();
    IOReturn stopUSBStream ();
    
    virtual UInt32 getCurrentSampleFrame (void);
    
    virtual IOAudioStreamDirection getDirection ();
    virtual void *getSampleBuffer (void);
	UInt32 getSampleBufferSize (void);
    /*!
     Copy input frames from given USB port framelist into the mInput and inform HAL about 
     timestamps when we recycle the ring buffer. Also updates mInput. bufferOffset.
     
     The idea is that this function can be called any number of times while we are waiting
     for the framelist read to finish.
     What is unclear is how this is done - there seems no memory of what was already done
     in previous calls to this routine and duplicate work may be done.
     
     You must lock IO (     IOLockLock(mLock)) before calling this. We can not do this ourselves
     because readHandler (who will need to call us) has to lock before this point and 
     locking multiple times ourselves will deadlock.

     This function always uses mInput.currentFrameList as the framelist to handle.
     
     @return kIOReturnSuccess if all frames were read properly, or kIOReturnStillOpen if there
     were still un-handled frames in the frame list.

     @discussion
     routine called by the readHandler.
     Expects that all RECORD_NUM_USB_FRAMES_PER_LIST frames are read completely.
     If not, an error may be logged. Not clear what will happen then higher up, 
     the mInput values will be updated properly in any case but of course with 
     less data than might be expected.
     
     This should be a low latency callback. I THINK that we should not do all the copy
     work here. But the time stamping that is done here is crucial. It is currently 
     halfway the code, it should be right at the start.
     
     Called exclusively from readHandler.
     This code directly uses readBuffer to access the bytes.
     
     This function does NOT check for buffer overrun. 
     
     This function modifies FrameIndex, lastInputSize, LastInputFrames, and runningInputCount. It may
     also alter bufferOffset but that will result in a warning in the logs.
     
     @param doTimeStamp true if function should also execute makeTimeStampForWrap if a wrap occurs.
     If false, the timestamp will be stored in frameListWrapTimestamp, and will be executed when 
     this function is called on the same frame again but then with doTimeStamp=true (which happens at read completion)
     */
	IOReturn GatherInputSamples(Boolean doTimeStamp);
    
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
#if PREPINPUT
	void prepInputPipe();
#endif
    
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
    
	void	setupChannelNames();
    
    /*! pushes a new frameSize into the frameSizeQueue and increases the frameSizeQueueBack.  Used to communicate USB input frames to the USB writer.*/
	void	PushFrameSize(UInt32 frameSize);
    /*! adds toAdd to the frameSizeQueue[frameSizeQueuefront]. Exclusively used for USB output frames. */
	void	AddToLastFrameSize(SInt32 toAdd);
    /*! get the frameSizeQueue at the front and increases the front. returns 0 if queue empty. Exclusively used for USB output frames.*/
	UInt32	PopFrameSize();
	void	ClearFrameSizes();

    /*! make a time stamp for a frame that has given frametime .
     we ignore the exact pos of the sample in the frame because measurements showed no relation between 
     this position and the time of the frame.
     @param frametime the timestamp for the USB frame that wrapped the buffer. I guess that the timestamp is for completion of the frame.
     */
    void makeTimeStampFromWrap(AbsoluteTime frametime);
    /*! as takeTimeStamp but takes nanoseconds instead of AbsoluteTime */
    void takeTimeStampNs(UInt64 timeStampNs, Boolean increment);

    /*! filter a given wrap time using a outlier reject and a mass-spring-damper filter 
     @param wrapTime the time (ns) of the next wrap. 
     @param initialize true if this is first call. This initializes internal vars x, dx, u */
    UInt64 filter(UInt64 wrapTime, Boolean initialize);
};

#endif /* defined(__EMUUSBAudio__EMUUSBAudioEngine__) */
