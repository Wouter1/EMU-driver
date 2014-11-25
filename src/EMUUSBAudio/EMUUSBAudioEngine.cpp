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
//	File:		EMUUSBAudioEngine.cpp
//
//	Contains:	Support for the USB Audio Class Stream Interface.
//			This includes support for setting sample rate (via
//			a sample rate endpoint control and appropriate
//			sized construction of USB isochronous frame lists),
//			channel depth selection and bit depth selection.
//
//	Technology:	Mac OS X
//
//--------------------------------------------------------------------------------


#include "EMUUSBAudioEngine.h"
#include "EMUUSBAudioPlugin.h"
#include "EMUUSBDeviceDefines.h"
#include "EMUUSBUserClient.h"
#include "EMUUSBLogging.h"

// HACK where is math.h ??
#define abs(x) ( (x)<0? -(x): x)

#define super IOAudioEngine

const SInt32 kSoftVolumeLookupRange = 7200;
const SInt32 kSoftDBRange = -72;

OSDefineMetaClassAndStructors(EMUUSBAudioEngine, IOAudioEngine)

#pragma mark -IOKit Routines-

void EMUUSBAudioEngine::free () {
	debugIOLog ("+EMUUSBAudioEngine[%p]::free ()", this);
    
	if (NULL != startTimer) {
		startTimer->cancelTimeout ();
		startTimer->release ();
		startTimer = NULL;
	}
#if LOCKING
	if (NULL != mLock) {
		IOLockFree(mLock);
		mLock = NULL;
	}
	if (NULL != mWriteLock) {
		IOLockFree(mWriteLock);
		mWriteLock = NULL;
	}
	if (NULL != mFormatLock) {
		if (!IOLockTryLock(mFormatLock)) {
			IOLockUnlock(mFormatLock);
		}
		IOLockFree(mFormatLock);
		mFormatLock = NULL;
	}
	
#endif
	if (NULL != mInput.frameQueuedForList) {
		delete [] mInput.frameQueuedForList;
		mInput.frameQueuedForList = NULL;
	}
	if (NULL != mOutput.frameQueuedForList) {
		delete [] mOutput.frameQueuedForList;
		mOutput.frameQueuedForList = NULL;
	}
    
	if (neededSampleRateDescriptor) {
		neededSampleRateDescriptor->complete();
		neededSampleRateDescriptor->release();
		neededSampleRateDescriptor = NULL;
	}
#if PREPINPUT
	if (mClearInputCompletion) {
		IOFree(mClearInputCompletion, sizeof(IOUSBLowLatencyIsocCompletion));
		mClearInputCompletion = NULL;
	}
	if (mClearIsocFrames) {
		IOFree(mClearIsocFrames, kNumFramesToClear * sizeof(IOUSBLowLatencyIsocFrame));
		mClearIsocFrames = NULL;
	}
#endif
	RELEASEOBJ(mInput.pipe);
	RELEASEOBJ(mOutput.pipe);
	RELEASEOBJ(mInput.associatedPipe);
	RELEASEOBJ(mOutput.associatedPipe);
	
	if (aveSampleRateBuf) {
		IOFree (aveSampleRateBuf, sizeof(UInt32));// expt try aveSampleBuf extended to account for larger read buf
		aveSampleRateBuf = NULL;
	}
    
	if (mSyncer) {
		mSyncer->release ();
		mSyncer = NULL;
	}
	
	if (mInput.usbBufferDescriptor) {
		mInput.usbBufferDescriptor->complete();
		mInput.usbBufferDescriptor->release();
		mInput.usbBufferDescriptor= NULL;
	}
	if (mOutput.usbBufferDescriptor) {
		mOutput.usbBufferDescriptor->complete();
		mOutput.usbBufferDescriptor->release();
		mOutput.usbBufferDescriptor= NULL;
	}
	readBuffer = NULL;
	if (theWrapRangeDescriptor) {
		theWrapRangeDescriptor->release ();
		theWrapDescriptors[0]->release ();
		theWrapDescriptors[1]->release ();
		theWrapRangeDescriptor = NULL;
	}
    
	if (mOutput.bufferMemoryDescriptor) {
		mOutput.bufferMemoryDescriptor->complete();
		mOutput.bufferMemoryDescriptor->release();
		mOutput.bufferMemoryDescriptor = NULL;
	}
	if (mInput.bufferMemoryDescriptor) {
		mInput.bufferMemoryDescriptor->complete();
		mInput.bufferMemoryDescriptor->release();
		mInput.bufferMemoryDescriptor = NULL;
	}
	mInput.bufferPtr = NULL;
	mOutput.bufferPtr = NULL;
	if (NULL != mOutput.bufferDescriptors) {
		for (UInt32 i = 0; i < mOutput.numUSBFrameLists; ++i) {
			if (NULL != mOutput.bufferDescriptors[i]) {
				mOutput.bufferDescriptors[i]->complete();
				mOutput.bufferDescriptors[i]->release();
				mOutput.bufferDescriptors[i] = NULL;
			}
		}
		IOFree (mOutput.bufferDescriptors, mOutput.numUSBFrameLists * sizeof (IOSubMemoryDescriptor *));
		mOutput.bufferDescriptors = NULL;
	}
	if (NULL != mInput.bufferDescriptors) {
		for (UInt32 i = 0; i < mInput.numUSBFrameLists; ++i) {
			if (NULL != mInput.bufferDescriptors[i]) {
				mInput.bufferDescriptors[i]->complete();
				mInput.bufferDescriptors[i]->release();
				mInput.bufferDescriptors[i] = NULL;
			}
		}
		IOFree (mInput.bufferDescriptors, mInput.numUSBFrameLists * sizeof (IOSubMemoryDescriptor *));
		mInput.bufferDescriptors = NULL;
	}
	
	if (NULL != mInput.usbIsocFrames) {
		IOFree (mInput.usbIsocFrames, mInput.numUSBFrameLists * mInput.numUSBFramesPerList * sizeof (IOUSBLowLatencyIsocFrame));
		mInput.usbIsocFrames = NULL;
	}
	if (NULL != mOutput.usbIsocFrames) {
		IOFree (mOutput.usbIsocFrames, mOutput.numUSBFrameLists * mOutput.numUSBFramesPerList * sizeof (IOUSBLowLatencyIsocFrame));
		mOutput.usbIsocFrames = NULL;
	}
	
	if (NULL != mInput.usbCompletion) {
		IOFree (mInput.usbCompletion, mInput.numUSBFrameLists * sizeof (IOUSBLowLatencyIsocCompletion));
		mInput.usbCompletion = NULL;
	}
	if (NULL != mOutput.usbCompletion) {
		IOFree (mOutput.usbCompletion, mOutput.numUSBFrameLists * sizeof (IOUSBLowLatencyIsocCompletion));
		mOutput.usbCompletion = NULL;
	}
    
	RELEASEOBJ(mInput.audioStream);
	RELEASEOBJ(mOutput.audioStream);
	//RELEASEOBJ(mainStream);
	
	RELEASEOBJ(mStreamInterfaces); // (AC mod)
	
	super::free ();
	debugIOLog ("-EMUUSBAudioEngine[%p]::free()", this);
}

bool EMUUSBAudioEngine::init (OSDictionary *properties) {
#pragma unused (properties)
	Boolean			result =FALSE;
    
	debugIOLogC("+EMUUSBAudioEngine[%p]::init ()", this);
	FailIf (FALSE == super::init (NULL), Exit);
    
	// Change this to use defines from the IOAudioFamily when they are available
	setProperty ("IOAudioStreamSampleFormatByteOrder", "Little Endian");
	readBuffer = mInput.bufferPtr = mOutput.bufferPtr = NULL;// initialize both the read, input and output to NULL
	mSyncer = IOSyncer::create (FALSE);
	result = TRUE;
    mPlugin = NULL;
	mInput.associatedPipe = mOutput.associatedPipe = NULL; // (AC mod)
	neededSampleRateDescriptor = NULL;
	mInput.usbCompletion = mOutput.usbCompletion= NULL;
	mInput.usbIsocFrames = mOutput.usbIsocFrames = NULL;

    
Exit:
	debugIOLogC("EMUUSBAudioEngine[%p]::init ()", this);
	return result;
}

bool EMUUSBAudioEngine::requestTerminate (IOService * provider, IOOptionBits options) {
	debugIOLog ("-EMUUSBAudioEngine[%p]::requestTerminate (%p, %x)", this, provider, options);
	return (usbAudioDevice == provider || mInput.streamInterface == provider || mOutput.streamInterface == provider);
}

bool EMUUSBAudioEngine::start (IOService * provider) {
	IONotifier *				audioDeviceNotifier;
	bool						resultCode = false;
	EMUUSBAudioConfigObject *		usbAudio;
	OSDictionary *				matchingDictionary;
	OSString *					name;
    
	debugIOLog ("+EMUUSBAudioEngine[%p]::start (%p)", this, provider);
    
	// Find out what interface number the driver is being instantiated against so that we always ask about
	// our particular interface and not some other interface the device might have.
    
	//<AC mod>
	IOUSBInterface *usbInterface = OSDynamicCast (IOUSBInterface, provider->getProvider()); // AC mod
	findAudioStreamInterfaces(usbInterface);
    
	/*FailIf (NULL == streamInterface, Exit);
     ourInterfaceNumber = streamInterface->GetInterfaceNumber ();
     
     debugIOLogC("ourInterfaceNumber = %u\n", ourInterfaceNumber);*/
	//</AC mod>
	
	matchingDictionary = serviceMatching ("EMUUSBAudioDevice");
	name = OSString::withCString ("*");
	matchingDictionary->setObject (kIOAudioDeviceNameKey, name);
	name->release ();
	
	// Device name must exist.
	// If it does, it already has its configuration descriptor, so we can call
	// EMUUSBAudioDevice::ControlsStreamNumber
	//<AC mod>
	// this looks pretty useless, actually (AC)
#if 1
	debugIOLog ("Adding notification with custom matching dictionary");
	audioDeviceNotifier = addNotification (gIOMatchedNotification,
                                           matchingDictionary,
                                           (IOServiceNotificationHandler)&audioDevicePublished,
                                           this,
                                           NULL);
    
	mSyncer->wait (FALSE);
	audioDeviceNotifier->remove ();
	mSyncer->reinit ();
#else
	usbAudioDevice = OSDynamicCast (EMUUSBAudioDevice,provider);
#endif
	//</AC mod>
    
	FailIf (NULL == usbAudioDevice, Exit);
	debugIOLogC("GetUSBAudioConfigObject");
	usbAudio = usbAudioDevice->GetUSBAudioConfigObject ();
	FailIf (NULL == usbAudio, Exit);
	// This will cause the driver to not load on any device that has _only_ a low frequency effect output terminal
	FailIf (usbAudio->GetNumOutputTerminals (0, 0) == 1 && usbAudio->GetIndexedOutputTerminalType (0, 0, 0) == OUTPUT_LOW_FREQUENCY_EFFECTS_SPEAKER, Exit);
    
	debugIOLogC("+IOAudioEngine::start");
	resultCode = super::start (provider, usbAudioDevice);
	usbAudioDevice->release();
	debugIOLogC("-IOAudioEngine::start");
	debugIOLog ("-EMUUSBAudioEngine[%p]::start (%p) = %d", this, provider, resultCode);
    
Exit:
	return resultCode;
}

void EMUUSBAudioEngine::stop (IOService * provider) {
    debugIOLogC("+EMUUSBAudioEngine[%p]::stop (%p)", this, provider);
    
	if (NULL != mPluginNotification) {
		mPluginNotification->remove ();
		mPluginNotification = NULL;
	}
    
	if (mPlugin) {
		mPlugin->close (this);
		mPlugin = NULL;
	}
	RELEASEOBJ(usbAudioDevice);
	if (mInput.streamInterface && mInput.streamInterface->isOpen()) {
		debugIOLogC("closing stream");
		mInput.streamInterface->close (this);
		mInput.streamInterface = NULL;
	}
	if (mOutput.streamInterface && mOutput.streamInterface->isOpen()) {
		debugIOLogC("closing stream");
		mOutput.streamInterface->close (this);
		mOutput.streamInterface = NULL;
	}
    
	super::stop (provider);
    
	debugIOLog ("-EMUUSBAudioEngine[%p]::stop (%p) - rc=%d", this, provider, getRetainCount());
}

bool EMUUSBAudioEngine::terminate (IOOptionBits options) {
	debugIOLogC("terminating the EMUUSBAudioEngine\n");
	return super::terminate(options);
}

void EMUUSBAudioEngine::detach(IOService *provider) {
	debugIOLogC("EMUUSBAudioEngine[%p]: detaching %p",this,provider);
	super::detach(provider);
}

void EMUUSBAudioEngine::close(IOService *forClient, IOOptionBits options) {
	debugIOLogC("EMUUSBAudioEngine[%p]: closing for %p",this,forClient);
	super::close(forClient,options);
}

#pragma mark -USB Audio driver-

IOReturn EMUUSBAudioEngine::AddAvailableFormatsFromDevice (EMUUSBAudioConfigObject *usbAudio, UInt8 ourInterfaceNumber) {
	IOReturn		result = kIOReturnError;
	IOAudioStream *audioStream = (ourInterfaceNumber == mInput.interfaceNumber) ? mInput.audioStream : mOutput.audioStream;
	if (usbAudio && audioStream) {
		IOAudioStreamFormat					streamFormat;
		IOAudioStreamFormatExtension		streamFormatExtension;
		IOAudioSampleRate					lowSampleRate;
		IOAudioSampleRate					highSampleRate;
		UInt32 *							sampleRates;
		UInt16								numAltInterfaces = usbAudio->GetNumAltStreamInterfaces(ourInterfaceNumber);
		UInt8								numSampleRates;
		UInt8								altSettingIndx = 0;
		UInt8								candidateAC3AltSetting = 0;
		Boolean								hasNativeAC3Format = FALSE;
		Boolean								hasDigitalOutput = FALSE;
        
		debugIOLog ("There are %d alternate stream interfaces on interface %d", numAltInterfaces, ourInterfaceNumber);
        
		// Find all of the available formats on the device.
		for (altSettingIndx = 1; altSettingIndx < numAltInterfaces; ++altSettingIndx) {
			UInt32	direction = usbAudio->GetIsocEndpointDirection(ourInterfaceNumber, altSettingIndx);
			UInt32	pollInterval = (UInt32) (1 << ((UInt32) usbAudio->GetEndpointPollInterval(ourInterfaceNumber, altSettingIndx, direction) -1));
			debugIOLogC("direction %d pollInterval %d", direction, pollInterval);
			if ((1 == pollInterval) || (8 == pollInterval)) {// exclude all interfaces that use microframes for now
				numSampleRates = usbAudio->GetNumSampleRates(ourInterfaceNumber, altSettingIndx);
				sampleRates = usbAudio->GetSampleRates (ourInterfaceNumber, altSettingIndx);
                
				streamFormat.fNumChannels = usbAudio->GetNumChannels (ourInterfaceNumber, altSettingIndx);
				streamFormat.fBitDepth = usbAudio->GetSampleSize (ourInterfaceNumber, altSettingIndx);
				streamFormat.fBitWidth = usbAudio->GetSubframeSize (ourInterfaceNumber, altSettingIndx) * 8;
				streamFormat.fAlignment = kIOAudioStreamAlignmentLowByte;
				streamFormat.fByteOrder = kIOAudioStreamByteOrderLittleEndian;
				streamFormat.fDriverTag = (ourInterfaceNumber << 16) | altSettingIndx;
                
				streamFormatExtension.fVersion = kFormatExtensionCurrentVersion;
				streamFormatExtension.fFlags = 0;
				streamFormatExtension.fFramesPerPacket = 1;
				streamFormatExtension.fBytesPerPacket = usbAudio->GetNumChannels (ourInterfaceNumber, altSettingIndx) * usbAudio->GetSubframeSize (ourInterfaceNumber, altSettingIndx);
                
				switch (usbAudio->GetFormat(ourInterfaceNumber, altSettingIndx)) {
					case PCM:
						streamFormat.fSampleFormat = kIOAudioStreamSampleFormatLinearPCM;
						streamFormat.fNumericRepresentation = kIOAudioStreamNumericRepresentationSignedInt;
						streamFormat.fIsMixable = TRUE;
						if (2 == streamFormat.fNumChannels && 16 == streamFormat.fBitDepth && 16 == streamFormat.fBitWidth)
							candidateAC3AltSetting = altSettingIndx;
						break;
					case AC3:	// just starting to stub something in for AC-3 support
						// since we don't *really* support AC3 yet, make it unavailable for now (AC)
						debugIOLog ("variable bit rate AC-3 audio format type!");
						continue;	// We're not supporting this at the moment, so just skip it.
						streamFormat.fSampleFormat = kIOAudioStreamSampleFormatAC3;
						streamFormat.fIsMixable = FALSE;
						streamFormat.fNumChannels = 6;
						streamFormat.fNumericRepresentation = kIOAudioStreamNumericRepresentationSignedInt;
						streamFormat.fBitDepth = 16;
						streamFormat.fBitWidth = 16;
						streamFormat.fByteOrder = kIOAudioStreamByteOrderBigEndian;
                        
						streamFormatExtension.fFlags = USBToHostLong (usbAudio->GetAC3BSID (ourInterfaceNumber, altSettingIndx));
                        //				streamFormatExtension.fFramesPerPacket = usbAudio->GetSamplesPerFrame (ourInterfaceNumber, altSettingIndx);
						streamFormatExtension.fFramesPerPacket = 1536;
                        //				streamFormatExtension.fBytesPerPacket = ((usbAudio->GetMaxBitRate (ourInterfaceNumber, altSettingIndx) * 1024 / 8) + 500) / 1000;
						streamFormatExtension.fBytesPerPacket = streamFormatExtension.fFramesPerPacket * streamFormat.fNumChannels * usbAudio->GetSubframeSize (ourInterfaceNumber, altSettingIndx);
						break;
					case IEC1937_AC3:
						debugIOLog ("IEC1937 AC-3 audio format type!");
						hasNativeAC3Format = TRUE;
						streamFormat.fSampleFormat = kIOAudioStreamSampleFormat1937AC3;
						streamFormat.fNumericRepresentation = kIOAudioStreamNumericRepresentationSignedInt;
						streamFormat.fIsMixable = FALSE;
						streamFormatExtension.fFramesPerPacket = 1536;
						streamFormatExtension.fBytesPerPacket = streamFormatExtension.fFramesPerPacket * streamFormat.fNumChannels * usbAudio->GetSubframeSize (ourInterfaceNumber, altSettingIndx);
						break;
					default:
						debugIOLog ("interface format = %x", usbAudio->GetFormat (ourInterfaceNumber, altSettingIndx));
						debugIOLog ("interface doesn't support a format that we can deal with, so we're not making it available");
						continue;	// skip this alternate interface
				}
                
				debugIOLog ("Interface %d, Alt %d has a ", ourInterfaceNumber, altSettingIndx);
				debugIOLog ("%d bit interface, ", streamFormat.fBitDepth);
				debugIOLog ("%d channels, and ", streamFormat.fNumChannels);
				debugIOLog ("%d sample rates, which are:", numSampleRates);
                
				if (numSampleRates) {
					for (UInt8 rateIndx = 0; rateIndx < numSampleRates; ++rateIndx) {
						debugIOLog (" %d", sampleRates[rateIndx]);
						lowSampleRate.whole = sampleRates[rateIndx];
						lowSampleRate.fraction = 0;
						audioStream->addAvailableFormat (&streamFormat, &streamFormatExtension, &lowSampleRate, &lowSampleRate);
						if (kIOAudioStreamSampleFormatLinearPCM == streamFormat.fSampleFormat) {
							streamFormat.fIsMixable = FALSE;
							audioStream->addAvailableFormat (&streamFormat, &streamFormatExtension, &lowSampleRate, &lowSampleRate);
							streamFormat.fIsMixable = TRUE;		// set it back to TRUE for next time through the loop
						}
					}
					debugIOLog ("");
				} else if (sampleRates) {
					debugIOLog (" %d to %d", sampleRates[0], sampleRates[1]);
					lowSampleRate.whole = sampleRates[0];
					lowSampleRate.fraction = 0;
					highSampleRate.whole = sampleRates[1];
					highSampleRate.fraction = 0;
					audioStream->addAvailableFormat (&streamFormat, &streamFormatExtension, &lowSampleRate, &highSampleRate);
					if (kIOAudioStreamSampleFormatLinearPCM == streamFormat.fSampleFormat) {
						streamFormat.fIsMixable = FALSE;
						audioStream->addAvailableFormat (&streamFormat, &streamFormatExtension, &lowSampleRate, &highSampleRate);
					}
				}
			}
		}
		
		switch (usbAudio->GetOutputTerminalType (usbAudioDevice->mInterfaceNum, 0, altSettingIndx)) {
			case EXTERNAL_DIGITAL_AUDIO_INTERFACE:
			case EXTERNAL_SPDIF_INTERFACE:
			case EMBEDDED_DVD_AUDIO:
				hasDigitalOutput = TRUE;
				break;
			default:
				hasDigitalOutput = FALSE;
		}
        
		if (TRUE == hasDigitalOutput && FALSE == hasNativeAC3Format && 0 != candidateAC3AltSetting && kIOAudioStreamDirectionOutput == getDirection ()) {
			numSampleRates = usbAudio->GetNumSampleRates (ourInterfaceNumber, candidateAC3AltSetting);
			sampleRates = usbAudio->GetSampleRates (ourInterfaceNumber, candidateAC3AltSetting);
            
			streamFormat.fNumChannels = usbAudio->GetNumChannels (ourInterfaceNumber, candidateAC3AltSetting);
			streamFormat.fBitDepth = usbAudio->GetSampleSize (ourInterfaceNumber, candidateAC3AltSetting);
			streamFormat.fBitWidth = usbAudio->GetSubframeSize (ourInterfaceNumber, candidateAC3AltSetting) * 8;
			streamFormat.fAlignment = kIOAudioStreamAlignmentLowByte;
			streamFormat.fByteOrder = kIOAudioStreamByteOrderLittleEndian;
			streamFormat.fDriverTag = (ourInterfaceNumber << 16) | candidateAC3AltSetting;
			streamFormat.fSampleFormat = kIOAudioStreamSampleFormat1937AC3;
			streamFormat.fNumericRepresentation = kIOAudioStreamNumericRepresentationSignedInt;
			streamFormat.fIsMixable = FALSE;
            
			streamFormatExtension.fVersion = kFormatExtensionCurrentVersion;
			streamFormatExtension.fFlags = 0;
			streamFormatExtension.fFramesPerPacket = 1536;
			streamFormatExtension.fBytesPerPacket = streamFormatExtension.fFramesPerPacket * streamFormat.fNumChannels * usbAudio->GetSubframeSize (ourInterfaceNumber, candidateAC3AltSetting);
            
			if (numSampleRates) {
				for (UInt8 rateIndx = 0; rateIndx < numSampleRates; ++rateIndx) {
					lowSampleRate.whole = sampleRates[rateIndx];
					lowSampleRate.fraction = 0;
					audioStream->addAvailableFormat (&streamFormat, &streamFormatExtension, &lowSampleRate, &lowSampleRate);
				}
			} else if (sampleRates) {
				lowSampleRate.whole = sampleRates[0];
				lowSampleRate.fraction = 0;
				highSampleRate.whole = sampleRates[1];
				highSampleRate.fraction = 0;
				audioStream->addAvailableFormat (&streamFormat, &streamFormatExtension, &lowSampleRate, &highSampleRate);
			}
		}
        
		result = kIOReturnSuccess;
	}
	return result;
}

bool EMUUSBAudioEngine::audioDevicePublished (EMUUSBAudioEngine * audioEngine, void * ref, IOService * newService) {
	EMUUSBAudioDevice *		audioDevice;
	IOUSBInterface *		thisControlInterface;
	IOUSBInterface *		thisStreamInterface;
	bool					resultCode = false;
    
	debugIOLog ("+EMUUSBAudioEngine::audioDevicePublished (%p, %p, %p)", audioEngine, (UInt32*)ref, newService);
	//	This one is a trick : because we are not sure in which order the parts of the
	//	USB driver will be loaded, we have to wait until the stream interface finds a corresponding
	//	USB partner. This is the test that is telling us when we can stop waiting
	FailIf (NULL == audioEngine, Exit);
	FailIf (NULL == newService, Exit);
    
	audioDevice = OSDynamicCast (EMUUSBAudioDevice, newService);
	FailIf (NULL == audioDevice, Exit);
    
	thisControlInterface = OSDynamicCast (IOUSBInterface, audioDevice->getProvider ());
	FailIf (NULL == thisControlInterface, Exit);
    
	for (int i = 0; i < audioEngine->mStreamInterfaces->getCount(); ++i) {
		thisStreamInterface = OSDynamicCast (IOUSBInterface, audioEngine->mStreamInterfaces->getObject(i)); // AC mod
		FailIf (NULL == thisStreamInterface, Exit);
		UInt8 ourInterfaceNumber = thisStreamInterface->GetInterfaceNumber();
		if (thisControlInterface->GetDevice () == thisStreamInterface->GetDevice ()) {
			if (audioDevice->ControlsStreamNumber (ourInterfaceNumber)) {
				debugIOLog ("++EMUUSBAudioEngine[%p]: found device (%p) for Audio Engine (%p)", audioEngine, audioDevice, audioEngine);
				audioEngine->usbAudioDevice = audioDevice;
				audioEngine->usbAudioDevice->retain();
				audioEngine->mSyncer->signal (kIOReturnSuccess, FALSE);
				resultCode = TRUE;	// Success!
			} else {
				resultCode = FALSE;
			}
		}
	}
    
Exit:
	debugIOLog ("-EMUUSBAudioEngine::audioDevicePublished (%p, %p, %p)", audioEngine, (UInt32 *)ref, newService);
	return resultCode;
}

void EMUUSBAudioEngine::CalculateSamplesPerFrame (UInt32 inSampleRate, UInt16 * averageFrameSamples, UInt16 * additionalSampleFrameFreq) {
	UInt32		subFrameDivisor = 1000;// * (8 / mPollInterval);
	UInt32		divisor = inSampleRate % subFrameDivisor;
    
	*averageFrameSamples = inSampleRate / subFrameDivisor;
	*additionalSampleFrameFreq = 0;
	if (divisor)
		*additionalSampleFrameFreq = subFrameDivisor / divisor;
}

IOReturn EMUUSBAudioEngine::CheckForAssociatedEndpoint (EMUUSBAudioConfigObject *usbAudio, UInt8 ourInterfaceNumber, UInt8 alternateSettingID) {
	IOReturn		result = kIOReturnSuccess;
#if !CUSTOMDEVICE
	UInt8			address = usbAudio->GetIsocEndpointAddress (ourInterfaceNumber, alternateSettingID, mDirection);
 	UInt8			syncType = usbAudio->GetIsocEndpointSyncType (ourInterfaceNumber, alternateSettingID, address);
	debugIOLogC("CheckForAssociatedEndpoint address %d, syncType %d", address, syncType);
	if ((kAsynchSyncType == syncType)) {
		IOUSBFindEndpointRequest	associatedEndpoint;
		debugIOLog ("checking endpoint %d for an associated endpoint", address);
		UInt8	assocEndpoint = usbAudio->GetIsocAssociatedEndpointAddress (ourInterfaceNumber, alternateSettingID, address);
		if (assocEndpoint != 0) {
			debugIOLog ("This endpoint has an associated synch endpoint! %d", assocEndpoint);
			refreshInterval = usbAudio->GetIsocAssociatedEndpointRefreshInt (ourInterfaceNumber, alternateSettingID, assocEndpoint);
			debugIOLog ("The refresh interval is %d", refreshInterval);
			framesUntilRefresh = 1 << refreshInterval;		// the same as 2^refreshInterval
			// The hardware might not need to be updated as often as we were planning on (currently we queue 10 lists with 10ms of audio each).
			// If they don't need to be updated that often, then just keep everything at 10ms intervals to keep things standard.
			if (framesUntilRefresh < numUSBFramesPerList) {
				debugIOLog ("Need to adjust numUSBFramesPerList, %ld < %ld", framesUntilRefresh, numUSBFramesPerList);
				if (NULL != mUSBIsocFrames) {
					debugIOLog ("Disposing current mUSBIsocFrames [%p]", mUSBIsocFrames);
					IOFree (mUSBIsocFrames, numUSBFrameLists * numUSBFramesPerList * sizeof(IOUSBLowLatencyIsocFrame));
					mUSBIsocFrames = NULL;
				}
				if (!usbCompletion) {
					debugIOLogC("Disposing off current usbCompletion %p", usbCompletion);
					IOFree(usbCompletion, numUSBFrameLists * sizeof(IOUSBLowLatencyIsocCompletion));
					usbCompletion = NULL;
				}
				numUSBFramesPerList = framesUntilRefresh;		// It needs to be updated more often, so run as the device requests.
				mUSBIsocFrames = (IOUSBLowLatencyIsocFrame *)IOMalloc(numUSBFrameLists * numUSBFramesPerList * sizeof(IOUSBLowLatencyIsocFrame));
				debugIOLog ("mUSBIsocFrames is now %p", mUSBIsocFrames);
				FailIf (NULL == mUSBIsocFrames, Exit);
				usbCompletion = (IOUSBLowLatencyIsocCompletion *)IOMalloc(numUSBFrameLists * sizeof(IOUSBLowLatencyIsocCompletion));
				debugIOLogC("usbCompletion is %p", usbCompletion);
				FailIf(!usbCompletion, Exit);
			}
			associatedEndpoint.type = kUSBIsoc;
			associatedEndpoint.direction = kUSBIn;	// The associated endpoint always goes "in"
			associatedEndpoint.maxPacketSize = 3 + (kUSBDeviceSpeedHigh == mHubSpeed);
			associatedEndpoint.interval = 0xFF;		// don't care
			if (streamInterface->getDirection() == kIOAudioStreamDirectionOutput) {
				mOutput.associatedPipe = streamInterface->FindNextPipe(NULL, &associatedEndpoint);
				FailWithAction (NULL == mOutput.associatedPipe, result = kIOReturnError, Exit);
			} else {
				mInput.associatedPipe = streamInterface->FindNextPipe(NULL, &associatedEndpoint);
				FailWithAction (NULL == mInput.associatedPipe, result = kIOReturnError, Exit);
                
			}
            
			if (NULL == neededSampleRateDescriptor) {
				debugIOLogC("allocating neededSampleRateDescriptor");
				aveSampleRateBuf = (UInt32 *)IOMalloc (sizeof (UInt32));
				FailIf (NULL == aveSampleRateBuf, Exit);
				bzero(aveSampleRateBuf, 4);
				neededSampleRateDescriptor = IOMemoryDescriptor::withAddress(aveSampleRateBuf, 4, kIODirectionIn);
				FailIf (NULL == neededSampleRateDescriptor, Exit);
				neededSampleRateDescriptor->prepare();
			}
			mSampleRateFrame.frStatus = -1;//[0]
			mSampleRateFrame.frReqCount = 3 + (kUSBDeviceSpeedHigh == mHubSpeed); // frReqCount is 3 for USB 1.0
			mSampleRateFrame.frActCount = 0;
			sampleRateCompletion.target = (void *)this;
			sampleRateCompletion.action = sampleRateHandler;
			sampleRateCompletion.parameter = 0;
			
			if (streamInterface->getDirection() == kIOAudioStreamDirectionOutput) {
				mOutput.associatedPipe->retain();
			} else {
				mInput.associatedPipe->retain();
			}
		} else {
			debugIOLog ("Couldn't find the associated synch endpoint!");
		}
	} else {
		debugIOLog ("This endpoint does not have an associated synch endpoint");
	}
#else// custom interface not compliant with the audio specification
	IOUSBFindEndpointRequest	assocReq;
#ifdef DEBUGLOGGING
	UInt8						assocEndpoint = usbAudio->GetIsocEndpointAddress(ourInterfaceNumber, alternateSettingID, kIOAudioStreamDirectionOutput);
	debugIOLogC("assocEndpoint is %d", assocEndpoint);
#endif
	assocReq.type = kUSBIsoc;
	assocReq.direction = kUSBIn;
	assocReq.maxPacketSize = 3 + (kUSBDeviceSpeedHigh == mHubSpeed);
	assocReq.interval = 0xff;
    // Wouter fixed. '=' instead of '==' in this code seems wrong. Compare line 663 below.
	if (ourInterfaceNumber == mOutput.interfaceNumber) {
		mOutput.associatedPipe = mOutput.streamInterface->FindNextPipe(NULL, &assocReq);
		FailWithAction(NULL == mOutput.associatedPipe, result = kIOReturnError, Exit);
	} else {
		mInput.associatedPipe = mInput.streamInterface->FindNextPipe(NULL, &assocReq);
		FailWithAction(NULL == mInput.associatedPipe, result = kIOReturnError, Exit);
	}
	framesUntilRefresh = kEMURefreshRate;// hardcoded value
	refreshInterval = 5;
	debugIOLogC("framesUntilRefresh %d", framesUntilRefresh);
	debugIOLogC("found sync endpoint");
	if (NULL == neededSampleRateDescriptor) {
		debugIOLogC("allocating neededSampleRateDescriptor");
		aveSampleRateBuf = (UInt32 *)IOMalloc (sizeof (UInt32));
		FailIf (NULL == aveSampleRateBuf, Exit);
		bzero(aveSampleRateBuf, 4);
		neededSampleRateDescriptor = IOMemoryDescriptor::withAddress(aveSampleRateBuf, 4, kIODirectionIn);
		FailIf (NULL == neededSampleRateDescriptor, Exit);
		neededSampleRateDescriptor->prepare();
	}
	mSampleRateFrame.frStatus = -1;//[0]
	mSampleRateFrame.frReqCount = 3 + (kUSBDeviceSpeedHigh == mHubSpeed); // frReqCount is 3 for USB 1.0
	mSampleRateFrame.frActCount = 0;
	sampleRateCompletion.target = (void *)this;
	sampleRateCompletion.action = sampleRateHandler;
	sampleRateCompletion.parameter = 0;
    
	if (ourInterfaceNumber == mOutput.interfaceNumber) {
		mOutput.associatedPipe->retain();
	} else {
		mInput.associatedPipe->retain();
	}
#endif
Exit:
	return result;
}

IOReturn EMUUSBAudioEngine::clipOutputSamples (const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream) {
    
    return kIOReturnSuccess; // HACK. we focus on input first.
    
    
	IOReturn			result = kIOReturnError;
    
    //	IOLockLock(mFormatLock);
    
	
	SInt32 offsetFrames = previouslyPreparedBufferOffset / mOutput.multFactor;
	debugIOLog2("clipOutputSamples firstSampleFrame=%u, numSampleFrames=%d, firstSampleFrame-offsetFrames =%d ",firstSampleFrame,numSampleFrames,firstSampleFrame-offsetFrames);
    
	if (firstSampleFrame != nextExpectedOutputFrame) {
		debugIOLogC("**** Output Hiccup!! firstSampleFrame=%d, nextExpectedOutputFrame=%d",firstSampleFrame,nextExpectedOutputFrame);
	}
	nextExpectedOutputFrame = firstSampleFrame + numSampleFrames;
	
	if (firstSampleFrame > numSampleFrames) {
		debugIOLogC("> offset delta %d",firstSampleFrame-offsetFrames);
	}
	if ((SInt32)firstSampleFrame - offsetFrames > 4000) {
		debugIOLogC("****** derail alert!");
	}
    
	//if (offsetFrames > firstSampleFrame && firstSampleFrame > numSampleFrames) {
	//	debugIOLogC("*** offset ahead by %d",offsetFrames-firstSampleFrame);
	//}
	
	if (0 == shouldStop && TRUE != inWriteCompletion) {
		UInt64	curUSBFrameNumber = mBus->GetFrameNumber();
		if (mOutput.usbFrameToQueueAt < curUSBFrameNumber) {
			debugIOLog2("***** output usbFrameToQueueAt < curUSBFrameNumber, %lld %lld", mOutput.usbFrameToQueueAt, curUSBFrameNumber);
			mOutput.usbFrameToQueueAt  = curUSBFrameNumber + mOutput.frameOffset;//kMinimumFrameOffset;
		}
		UInt64	framesLeftInQueue = mOutput.usbFrameToQueueAt - curUSBFrameNumber;
		SInt64	frameLimit = mOutput.numUSBTimeFrames * (mOutput.numUSBFrameListsToQueue -1);
		if (framesLeftInQueue <  frameLimit) {
			debugIOLog2 ("**** queue a write from clipOutputSamples: framesLeftInQueue = %ld", (UInt32)framesLeftInQueue);
			writeHandler (this, mOutput.usbCompletion[mOutput.currentFrameList].parameter, kIOReturnSuccess,
                          &mOutput.usbIsocFrames[mOutput.currentFrameList * mOutput.numUSBFramesPerList]);
		}
	} else if (TRUE == inWriteCompletion) {
		debugIOLogC("**in completion");
	}
	
    // software volume
	if(mOutputVolume && streamFormat->fSampleFormat == kIOAudioStreamSampleFormatLinearPCM)
	{
		UInt32 usedNumberOfSamples = ((firstSampleFrame + numSampleFrames) * streamFormat->fNumChannels);
		UInt32 theFirstSample = firstSampleFrame * streamFormat->fNumChannels;
		
		if (mDidOutputVolumeChange)
		{
			mDidOutputVolumeChange = false;
			
			SmoothVolume(((Float32*)mixBuf),
                         mOutputVolume->GetTargetVolume(),
                         mOutputVolume->GetLastVolume(),
                         theFirstSample,
                         numSampleFrames,
                         usedNumberOfSamples,
                         streamFormat->fNumChannels);
            
			mOutputVolume->SetLastVolume(mOutputVolume->GetTargetVolume());
		}
		else
		{
			Volume(((Float32*)mixBuf),
                   mOutputVolume->GetTargetVolume(),
                   theFirstSample,
                   usedNumberOfSamples);
		}
	}
	
	//debugIOLogC("clipOutputSamples: numSampleFrames = %d",numSampleFrames);
	if (TRUE == streamFormat->fIsMixable) {
		if (mPlugin)
			mPlugin->pluginProcess ((Float32*)mixBuf + (firstSampleFrame * streamFormat->fNumChannels), numSampleFrames, streamFormat->fNumChannels);
        
		result = clipEMUUSBAudioToOutputStream (mixBuf, sampleBuf, firstSampleFrame, numSampleFrames, streamFormat);
	} else {
		UInt32	offset = firstSampleFrame * mOutput.multFactor;
        
		memcpy ((UInt8 *)sampleBuf + offset, (UInt8 *)mixBuf, numSampleFrames * mOutput.multFactor);
		result = kIOReturnSuccess;
	}
    //	IOLockUnlock(mFormatLock);
	return result;
}



IOReturn EMUUSBAudioEngine::GatherInputSamples(Boolean doTimeStamp) {
	UInt32			numBytesToCopy = 0; // number of bytes to move the dest ptr by each time
	UInt8*			buffStart = (UInt8*) mInput.bufferPtr;
    UInt32         totalreceived=0;
	UInt8*			dest = buffStart + mInput.bufferOffset;
    
    // for OVERRUN checks
    UInt32 readHeadPos = nextExpectedFrame * mInput.multFactor;

    debugIOLogR("+GatherInputSamples %d", mInput.bufferOffset / mInput.multFactor);
    // check if we moved to the next frame. This is done when primary USB interrupt occurs.
    
    // handle outstanding wraps so that we have it available for this round
    if (doTimeStamp && mInput.frameListWrapTimeStamp!=0) {
        makeTimeStampFromWrap(mInput.frameListWrapTimeStamp);
        mInput.frameListWrapTimeStamp = 0;
    }
    
    if (mInput.previousFrameList != mInput.currentFrameList) {
        debugIOLogR("GatherInputSamples going from framelist %d to %d. lastindex=%d",mInput.previousFrameList , mInput.currentFrameList,mInput.frameIndex);
        if (mInput.frameIndex != RECORD_NUM_USB_FRAMES_PER_LIST) {
            doLog("***** Previous framelist was not handled completely, only %d",mInput.frameIndex);
        }
        if (mInput.frameListWrapTimeStamp != 0) {
            doLog("wrap         is still open!!"); // we can't erase, we would loose sync point....
        }
        mInput.previousFrameList = mInput.currentFrameList;
        mInput.frameIndex=0;
    }
    
    IOUSBLowLatencyIsocFrame * pFrames = &mInput.usbIsocFrames[mInput.currentFrameList * mInput.numUSBFramesPerList];
	if (mInput.bufferSize <= mInput.bufferOffset) {
        // sanity checking to prevent going beyond the end of the allocated dest buffer
		mInput.bufferOffset = 0;
        debugIOLogR("BUG EMUUSBAudioEngine::GatherInputSamples wrong offset");
    }
    
    
    

    
    while(mInput.frameIndex < RECORD_NUM_USB_FRAMES_PER_LIST
          && -1 != pFrames[mInput.frameIndex].frStatus // no transport at all
          && kUSBLowLatencyIsochTransferKey != pFrames[mInput.frameIndex].frStatus // partially transported
          //&& (doTimeStamp || mInput.frameIndex < 33) // HACK for testing partial processing
          )
    {
        UInt8*			source = (UInt8*) readBuffer + (mInput.currentFrameList * readUSBFrameListSize)
                                + mInput.maxFrameSize * mInput.frameIndex;
        if (mDropStartingFrames <= 0)
        {
            UInt32	byteCount = pFrames[mInput.frameIndex].frActCount;
            if (byteCount != lastInputSize) {
                debugIOLogR("Happy Leap Sample!  new size = %d, current size = %d",byteCount,lastInputSize);
                lastInputSize = byteCount;
                lastInputFrames = byteCount / mInput.multFactor;
            }
            totalreceived += byteCount;
            
            if (mInput.bufferOffset <  readHeadPos && mInput.bufferOffset + byteCount > readHeadPos) {
                // this is not a water tight test but will catch most cases as
                // the write head step sizes are small. Only when read head is at 0 we may miss.
                doLog("****** OVERRUN: write head is overtaking read head!");
            }
            
            runningInputCount += lastInputFrames;
            //	debugIOLogC("push %d",lastInputFrames);
            // save the # of frames to the framesize queue so that we generate an appropriately sized output packet
            PushFrameSize(lastInputFrames);
            SInt32	numBytesToEnd = mInput.bufferSize - mInput.bufferOffset; // assumes that bufferOffset <= bufferSize
            if (byteCount < numBytesToEnd) { // no wrap
                memcpy(dest, source, byteCount);
                mInput.bufferOffset += byteCount;
                numBytesToCopy = byteCount;// number of bytes the dest ptr will be moved by
            } else { // wrapping around - end up at bufferStart or bufferStart + an offset
                UInt32	overflow = byteCount - numBytesToEnd;
                memcpy(dest, source, numBytesToEnd); // copy data into the remaining portion of the dest buffer
                dest = (UInt8*) buffStart;	// wrap around to the start
                mInput.bufferOffset = overflow; // remember the location the dest ptr will be set to
                if (overflow)	// copy the remaining bytes into the front of the dest buffer
                    memcpy(dest, source + numBytesToEnd, overflow);
                mInput.frameListWrapTimeStamp = pFrames[mInput.frameIndex].frTimeStamp;

                numBytesToCopy = overflow;
            }
        }
        else if(pFrames[mInput.frameIndex].frActCount && mDropStartingFrames > 0) // first frames might have zero length
        {
            mDropStartingFrames--;
        }
        
        //		debugIOLogC("GatherInputSamples frameIndex is %d, numBytesToCopy %d", frameIndex, numBytesToCopy);
        mInput.frameIndex++;
        //source += mInput.maxFrameSize; // each frame's frReqCount is set to maxFrameSize
        dest += numBytesToCopy;
    }

    // now check, why did we leave loop? Was all normal or failure? log failures.
    if (mInput.frameIndex != RECORD_NUM_USB_FRAMES_PER_LIST) {
        return kIOReturnStillOpen; // caller has to decide what to do if this happens.
    } else {
         // reached end of list reached.
        if (doTimeStamp && mInput.frameListWrapTimeStamp!=0) {
            makeTimeStampFromWrap(mInput.frameListWrapTimeStamp);
            mInput.frameListWrapTimeStamp=0;
        }
    }
    debugIOLogR("-GatherInputSamples received %d first open frame=%d",totalreceived, mInput.frameIndex);
    return kIOReturnSuccess;
}


//expected wrap time in ns.
// HACK we need to do this more flexible. This depends on sample rate.
#define EXPECTED_WRAP_TIME 128000000


void EMUUSBAudioEngine::makeTimeStampFromWrap(AbsoluteTime wt) {
    UInt64 wrapTimeNs;
    absolutetime_to_nanoseconds(wt,&wrapTimeNs);

    if (goodWraps >= 5) {
        // regular operation after initial wraps. Mass-spring-damper filter.
        takeTimeStampNs(filter(wrapTimeNs,FALSE),TRUE);
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
                    takeTimeStampNs(filter(wrapTimeNs,TRUE),FALSE);
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

void EMUUSBAudioEngine::takeTimeStampNs(UInt64 timeStampNs, Boolean increment) {
    AbsoluteTime t;
    nanoseconds_to_absolutetime(timeStampNs, &t);
    takeTimeStamp(increment, &t);

}

#define K 1     // spring constant for filter
#define M 10000 // mass for the filter
#define DA 200  // 2 Sqrt[M K] for critical damping.

UInt64 EMUUSBAudioEngine::filter(UInt64 inputx, Boolean initialize) {
    UInt64 xnext;
    SInt64 unext,du,F;
    
    if (initialize) {
        x = inputx;
        dx = EXPECTED_WRAP_TIME;
        u=0;
        return inputx;
    }
    
    xnext = x + dx ; // the next filtered output
    unext = inputx - xnext; // error u
    
    du = unext - u; // change of the error, for damping
    if (abs(du) < EXPECTED_WRAP_TIME/2000) {
        F = K * unext + DA * du; // force on spring
        dx = dx + F/M ;
    }
    x = xnext;
    u= unext; // update the filter
    
    debugIOLogT("filter %lld -> %lld", inputx, x);
    return x;
}


void EMUUSBAudioEngine::addSoftVolumeControls()
{
	SInt32		maxValue = kSoftVolumeLookupRange;
	SInt32		minValue = 0;
	SInt32		initialValue = (SInt32)kSoftVolumeLookupRange;
	IOFixed		minDB = kSoftDBRange << 16; // 16.16 fixed
	IOFixed		maxDB = 0;
	UInt32		cntrlID = 0;
	const char *channelName = 0;
	
    // output
	mDidOutputVolumeChange = false;
	mIsOutputMuted = false;
    
	mOutputVolume = EMUUSBAudioSoftLevelControl::create(initialValue,
														minValue,
														maxValue,
														minDB,
														maxDB,
														kIOAudioControlChannelIDAll,
														channelName,
														cntrlID,
														kIOAudioLevelControlSubTypeVolume,
														kIOAudioControlUsageOutput);
    
	if (mOutputVolume)
	{
		mOutputVolume->setLinearScale(true); // here, false is true and true is false
		
		mOutputVolume->setValueChangeHandler(softwareVolumeChangedHandler, this);
		addDefaultAudioControl(mOutputVolume);
		mOutputVolume->release();
	}
	
	mOuputMuteControl = IOAudioToggleControl::createMuteControl(false,
                                                                kIOAudioControlChannelIDAll,
                                                                0,
                                                                cntrlID,
                                                                kIOAudioControlUsageOutput);
    
	if (mOuputMuteControl)
	{
		mOuputMuteControl->setValueChangeHandler(softwareMuteChangedHandler, this);
		addDefaultAudioControl(mOuputMuteControl);
		mOuputMuteControl->release();
	}
	
    // input
	mDidInputVolumeChange = false;
	mIsInputMuted = false;
    
	mInputVolume = EMUUSBAudioSoftLevelControl::create(initialValue,
                                                       minValue,
                                                       maxValue,
                                                       minDB,
                                                       maxDB,
                                                       kIOAudioControlChannelIDAll,
                                                       channelName,
                                                       cntrlID,
                                                       kIOAudioLevelControlSubTypeVolume,
                                                       kIOAudioControlUsageInput);
    
	if (mInputVolume)
	{
		mInputVolume->setLinearScale(true); // here, false is true and true is false
		
		mInputVolume->setValueChangeHandler(softwareVolumeChangedHandler, this);
		addDefaultAudioControl(mInputVolume);
		mInputVolume->release();
	}
	
	mInputMuteControl = IOAudioToggleControl::createMuteControl(false,
																kIOAudioControlChannelIDAll,
																0,
																cntrlID,
																kIOAudioControlUsageInput);
    
	if (mInputMuteControl)
	{
		mInputMuteControl->setValueChangeHandler(softwareMuteChangedHandler, this);
		addDefaultAudioControl(mInputMuteControl);
		mInputMuteControl->release();
	}
}

IOReturn EMUUSBAudioEngine::softwareVolumeChangedHandler(OSObject * target, IOAudioControl * audioControl, SInt32 oldValue, SInt32 newValue)
{
    IOReturn				result = kIOReturnSuccess;
	EMUUSBAudioEngine *		device = OSDynamicCast(EMUUSBAudioEngine, target);
	
	if (audioControl->getUsage() == kIOAudioControlUsageOutput)
	{
		debugIOLogC("EMUUSBAudioEngine::softwareVolumeChangedHandler output: channel= %d oldValue= %d newValue= %d", audioControl->getChannelID(), oldValue, newValue);
		
		Float32 volume;
		::GetDbToGainLookup(newValue, kSoftVolumeLookupRange, volume);
        
		device->mOutputVolume->SetVolume(volume);
		
		// set the volume if muted, but don't set the target.
		if (device->mIsOutputMuted == false)
		{
			device->mDidOutputVolumeChange = true;
			device->mOutputVolume->SetTargetVolume(volume);
		}
	}
	else if (audioControl->getUsage() == kIOAudioControlUsageInput)
	{
		debugIOLogC("EMUUSBAudioEngine::softwareVolumeChangedHandler input: channel= %d oldValue= %d newValue= %d", audioControl->getChannelID(), oldValue, newValue);
		
		Float32 volume;
		::GetDbToGainLookup(newValue, kSoftVolumeLookupRange, volume);
		
		device->mInputVolume->SetVolume(volume);
		
		// set the volume if muted, but don't set the target.
		if (device->mIsInputMuted == false)
		{
			device->mDidInputVolumeChange = true;
			device->mInputVolume->SetTargetVolume(volume);
		}
	}
	
	return result;
}

IOReturn EMUUSBAudioEngine::softwareMuteChangedHandler(OSObject * target, IOAudioControl * audioControl, SInt32 oldValue, SInt32 newValue)
{
	EMUUSBAudioEngine* device = OSDynamicCast(EMUUSBAudioEngine, target);
    
	if (audioControl->getUsage() == kIOAudioControlUsageOutput)
	{
		device->mDidOutputVolumeChange = true;
		device->mIsOutputMuted = (bool)newValue;
        
        // mute off
		if (newValue == 0) {
			device->mOutputVolume->SetTargetVolume(device->mOutputVolume->GetVolume());
		}
        // mute on
		else if (newValue == 1) {
			device->mOutputVolume->SetTargetVolume(0.0);
		}
	}
	else if (audioControl->getUsage() == kIOAudioControlUsageInput)
	{
		device->mDidInputVolumeChange = true;
		device->mIsInputMuted = (bool)newValue;
        
        // mute off
		if (newValue == 0) {
			device->mInputVolume->SetTargetVolume(device->mInputVolume->GetVolume());
		}
        // mute on
		else if (newValue == 1) {
			device->mInputVolume->SetTargetVolume(0.0);
		}
	}
    
	return kIOReturnSuccess;
}



IOReturn EMUUSBAudioEngine::convertInputSamples (const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame,
                                                 UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat,
                                                 IOAudioStream *audioStream) {
	UInt32		lastSampleByte = (firstSampleFrame + numSampleFrames) * mInput.multFactor;// max number of bytes to get
	IOReturn	result;
    
    debugIOLogR("+convertInputSamples firstSampleFrame=%u, numSampleFrames=%d srcbuf=%p dest=%p byteorder=%d bitWidth=%d numchannels=%d",firstSampleFrame,numSampleFrames,sampleBuf,destBuf,streamFormat->fByteOrder,streamFormat->fBitWidth,streamFormat->fNumChannels);

    if (startingEngine) {
        return kIOReturnUnderrun;
    }
    
    
	if (firstSampleFrame != nextExpectedFrame) {
		debugIOLogC("****** HICCUP firstSampleFrame=%d, nextExpectedFrame=%d",firstSampleFrame,nextExpectedFrame);
    }
    UInt32 firstSampleByte =firstSampleFrame* mInput.multFactor;
    debugIOLogTD("C %d %d",firstSampleByte, mInput.bufferOffset);

    if (!startingEngine && !shouldStop) {
        Boolean haveLock=IOLockTryLock(mLock);
        if (haveLock) {
            GatherInputSamples(false);
            IOLockUnlock(mLock);
        }
    }
    
    // current write head inside that range? Then this is going to cause serious distortion!
    if (firstSampleByte<=mInput.bufferOffset && mInput.bufferOffset<lastSampleByte) {
        doLog("****** STUTTER IOAudioEngine convertInputSamples is reading over the write head");
        // do not return, keep going and hope we will get in sync.... If we return, everything will start throwing above us.
    }
    
    // at this point, mInput should contain sufficient info to handle the request.
    // It's the caller that "ensures" this using
    // "sophisticated techniques and extremely accurate timing mechanisms".
    // I don't like this black box approach but we have to live with it.
    
    debugIOLog2("convertFromEMUUSBAudioInputStreamNoWrap destBuf = %p, firstSampleFrame = %d, numSampleFrames = %d", destBuf, firstSampleFrame, numSampleFrames);
    result = convertFromEMUUSBAudioInputStreamNoWrap (sampleBuf, destBuf, firstSampleFrame, numSampleFrames, streamFormat);
	if (mPlugin)
		mPlugin->pluginProcessInput ((float *)destBuf + (firstSampleFrame * streamFormat->fNumChannels), numSampleFrames, streamFormat->fNumChannels);
    

    // set the expected starting point for the next read. IOAudioDevice may jump around depending on timestamps we feed it....
    // FIXME
	nextExpectedFrame = firstSampleFrame + numSampleFrames;
	if (nextExpectedFrame*mInput.multFactor >= mInput.bufferSize) {
		nextExpectedFrame -= mInput.bufferSize / mInput.multFactor;
	}
	
    // software volume
	if(mInputVolume) // should check for sample format if needed
	{
		UInt32 usedNumberOfSamples = numSampleFrames * streamFormat->fNumChannels;
		
		if (mDidInputVolumeChange)
		{
			mDidInputVolumeChange = false;
            
			SmoothVolume(((Float32*)destBuf),
                         mInputVolume->GetTargetVolume(),
                         mInputVolume->GetLastVolume(),
                         0,
                         numSampleFrames,
                         usedNumberOfSamples,
                         streamFormat->fNumChannels);
            
			mInputVolume->SetLastVolume(mInputVolume->GetTargetVolume());
		}
		else
		{
			Volume(((Float32*)destBuf),
                   mInputVolume->GetTargetVolume(),
                   0,
                   usedNumberOfSamples);
		}
	}
    debugIOLogC("-convertInputSamples");

	return result;
}

AbsoluteTime EMUUSBAudioEngine::generateTimeStamp(UInt32 usbFrameIndex, UInt32 preWrapBytes, UInt32 byteCount) {
	UInt64			time_nanos = 0;
	AbsoluteTime	time = 0; //{0,0};
	AbsoluteTime	refWallTime = 0; // {0, 0};
	UInt64			referenceWallTime_nanos = 0;
	UInt64			referenceFrame = 0ull;
	
 	UInt32			usedFrameIndex = usbFrameIndex >> kPollIntervalShift;
 	UInt64			thisFrameNum = mInput.frameQueuedForList[mInput.currentFrameList] + usedFrameIndex;
	
	byteCount *= kNumberOfFramesPerMillisecond;
	
	//debugIOLogT("**** generateTimeStamp usbFrameIndex %d usedFrameIndex %d byteCount %d",usbFrameIndex,usedFrameIndex,byteCount);
	
	if (kIOReturnSuccess == getAnchor(&referenceFrame, &refWallTime)) {
		bool	referenceAhead = referenceFrame > thisFrameNum;
		if (!referenceAhead) {
			time_nanos = thisFrameNum - referenceFrame;
			if (byteCount)
				time_nanos *= byteCount;
			time_nanos += preWrapBytes;
		} else {
			time_nanos = referenceFrame - thisFrameNum;
			if (byteCount)
				time_nanos *= byteCount;
			time_nanos -= preWrapBytes;
		}
		if (!usbAudioDevice)
			return time;
		time_nanos *= usbAudioDevice->mWallTimePerUSBCycle / kWallTimeExtraPrecision;
		if (byteCount) {
			time_nanos /= byteCount;// if byteCount is 0 PPC continues working (returns 0) but not i386 (obviously more correct)
		} else {
			//debugIOLogT("**** zero byteCount in generateTimeStamp ****");
		}
		absolutetime_to_nanoseconds(EmuAbsoluteTime(refWallTime), &referenceWallTime_nanos);
		if (!referenceAhead)
			time_nanos += referenceWallTime_nanos;
		else
			time_nanos = referenceWallTime_nanos - time_nanos;
		time_nanos += (usbAudioDevice->mWallTimePerUSBCycle / kWallTimeExtraPrecision);
        debugIOLogT("stampTime is %llu", time_nanos);
		nanoseconds_to_absolutetime(time_nanos, EmuAbsoluteTimePtr(&time));
	}
	return time;
}

IOReturn EMUUSBAudioEngine::eraseOutputSamples(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream)
{
	super::eraseOutputSamples (mixBuf, sampleBuf, firstSampleFrame, numSampleFrames, streamFormat, audioStream);
	
	/*UInt32 start = firstSampleFrame * streamFormat->fNumChannels * (streamFormat->fBitWidth / 8);
     UInt32 stop = start + numSampleFrames * streamFormat->fNumChannels * (streamFormat->fBitWidth / 8);
     for (int i = start; i < stop; ++i) {
     ((char *)sampleBuf)[i] = 1;
     }*/
	return kIOReturnSuccess;
}


UInt32 EMUUSBAudioEngine::getCurrentSampleFrame() {
	UInt32	currentSampleFrame = 0;
	if (mOutput.audioStream) {
		//currentSampleFrame = mOutput.bufferOffset;
		currentSampleFrame = safeToEraseTo;
		currentSampleFrame /= mOutput.multFactor;
	}
	return currentSampleFrame;
}

IOReturn EMUUSBAudioEngine::GetDefaultSettings(IOUSBInterface  *streamInterface,
											   IOAudioSampleRate * defaultSampleRate) {
	IOReturn				result = kIOReturnError;
	if (!usbAudioDevice)
		return result;
	EMUUSBAudioConfigObject*	usbAudio = usbAudioDevice->GetUSBAudioConfigObject();
	if (usbAudio && !usbAudioDevice->mUHCI) {// exclude UHCI for now
		bool				found = false;
		UInt8				newAltSettingID = 0;
		IOAudioSampleRate	newSampleRate;
		
		UInt8 ourInterfaceNumber = streamInterface->GetInterfaceNumber ();
        
		// first, figure out whether the stream is input or output
		UInt8 direction = usbAudio->GetIsocEndpointDirection(ourInterfaceNumber, newAltSettingID);
		if (direction == 0xFF) {
			debugIOLogC("can't resolve direction from endpoints, resorting to slimy hack instead");
			// interface 1 is output and interface 2 is input in the current 0202USB and 0404USB devices
			direction = (ourInterfaceNumber == 1) ? kUSBOut : kUSBIn;
		}
		
		debugIOLogC("direction of interface %d is %d (In=%d,Out=%d)",ourInterfaceNumber,direction,kUSBIn,kUSBOut);
		
		// reference the appropriate StreamInfo for input or output
		StreamInfo &info = (direction == kUSBIn) ? mInput : mOutput;
		info.interfaceNumber = ourInterfaceNumber;
		info.streamDirection = direction;
		info.streamInterface = streamInterface;
		
		newSampleRate.fraction = 0;
		newSampleRate.whole = usbAudioDevice->getHardwareSampleRate();// get the sample rate the device was set to
		debugIOLogC("hardware sample rate is %d", newSampleRate.whole);
		info.numChannels = kChannelCount_10;// try 4 channels first - uh... make that 10... uh... this is stupid.
		mChannelWidth = kBitDepth_24bits;
		UInt32	altChannelWidth = kBitDepth_16bits;
		
		while (!found && info.numChannels >= kChannelCount_STEREO) {// will never be a mono device
			debugIOLogC("Finding interface %d numChannels %d sampleRate %d", ourInterfaceNumber, info.numChannels, newSampleRate.whole);
			newAltSettingID = usbAudio->FindAltInterfaceWithSettings (ourInterfaceNumber, info.numChannels, mChannelWidth, newSampleRate.whole);
			if (255 == newAltSettingID) {// try finding 16 bit setting
				newAltSettingID = usbAudio->FindAltInterfaceWithSettings(ourInterfaceNumber, info.numChannels, altChannelWidth, newSampleRate.whole);
				mChannelWidth = altChannelWidth;
			}
			
			if (255 != newAltSettingID) {
#ifdef DEBUGLOGGING
				debugIOLogC("newAltSettingID %d", newAltSettingID);
				UInt16	format = usbAudio->GetFormat(ourInterfaceNumber, newAltSettingID);
				debugIOLogC("format is %d", format);
#endif
                
				UInt32	pollInterval = (UInt32) usbAudio->GetEndpointPollInterval(ourInterfaceNumber, newAltSettingID, info.streamDirection);
				mPollInterval = 1 << (pollInterval -1);
				debugIOLogC("direction is %d pollInterval is %d", info.streamDirection, mPollInterval);
				if ((1 == mPollInterval) || (8 == mPollInterval)) {
					debugIOLogC("found channel count %d", info.numChannels);
					info.multFactor = 3 * info.numChannels;
					found = true;
					break;
				}
			}
			info.numChannels -= 2;
			mChannelWidth = kBitDepth_24bits;// restore to 24 bit setting
		}
		if (!found) {// last resort - get anything
			mChannelWidth = kBitDepth_16bits;
			debugIOLogC("last resort in GetDefaultSettings");
			info.numChannels = kChannelCount_STEREO;
			newAltSettingID = usbAudio->FindAltInterfaceWithSettings (ourInterfaceNumber, info.numChannels, mChannelWidth, newSampleRate.whole);
			info.multFactor = kChannelCount_STEREO * 2;
			if (255 == newAltSettingID) {
				//	try for a stereo 16-bit interface with any sample rate
				newAltSettingID = usbAudio->FindAltInterfaceWithSettings (ourInterfaceNumber, info.numChannels, mChannelWidth);
				newSampleRate.whole = usbAudio->GetHighestSampleRate (ourInterfaceNumber, newAltSettingID);			// we'll run at the highest sample rate that the device has
			}
		}
		debugIOLog ("Default sample rate is %d", newSampleRate.whole);
		debugIOLog ("Default alternate setting ID is %d", newAltSettingID);
		if(newSampleRate.whole) {
			*defaultSampleRate = newSampleRate;
			info.alternateSettingID = newAltSettingID;
			result = kIOReturnSuccess;
		}
	}
	return result;
}

IOAudioStreamDirection EMUUSBAudioEngine::getDirection () {
	return (IOAudioStreamDirection) -1;
}

Boolean EMUUSBAudioEngine::getDescriptorString(char *buffer, UInt8 index) {
    IOReturn	res = kIOReturnSuccess;
    IOUSBDevice *usbDevice =mInput.streamInterface->GetDevice();

    buffer[0] = 0;
	if (index != 0) {
		res = usbDevice->GetStringDescriptor (index, buffer, kStringBufferSize);
    }
	if (buffer[0] == 0 || res != kIOReturnSuccess) {
		strncpy (buffer, "Unknown",kStringBufferSize-1);
        return false;
	}
    return true;
}


OSString * EMUUSBAudioEngine::getGlobalUniqueID () {
    OSString *			uniqueID = NULL;
	OSNumber *			usbLocation = NULL;
	UInt32				locationID ;
	UInt8				interfaceNumber = 0;
	char				productString[kStringBufferSize];
	char				manufacturerString[kStringBufferSize];
    char                locationIDString[kStringBufferSize];
    char                uniqueIDStr[MAX_ID_SIZE];
    
    IOUSBDevice *usbDevice =mInput.streamInterface->GetDevice();
    
    getDescriptorString(manufacturerString, usbDevice->GetManufacturerStringIndex ());
    getDescriptorString(productString,usbDevice->GetProductStringIndex ());
	interfaceNumber = mInput.streamInterface->GetInterfaceNumber ();
    usbLocation = OSDynamicCast (OSNumber, usbDevice->getProperty (kUSBDevicePropertyLocationID));
    if (NULL != usbLocation) {
        locationID = usbLocation->unsigned32BitValue ();
        snprintf (locationIDString,kStringBufferSize, "%x", locationID);
    } else {
        strncpy (locationIDString, "Unknown location",kStringBufferSize);
    }

    snprintf (uniqueIDStr, MAX_ID_SIZE, "EMUUSBAudioEngine:%s:%s:%s:%d",
                  manufacturerString, productString, locationIDString, interfaceNumber);
    
    uniqueID = OSString::withCString (uniqueIDStr);
    debugIOLog ("getGlobalUniqueID = %s", uniqueIDStr);
    
	return uniqueID;
}

void * EMUUSBAudioEngine::getSampleBuffer (void) {
	if (NULL != mainStream)
		return mainStream->getSampleBuffer();
	
	return NULL;
}

// returns either a valid buffer size or zero
UInt32 EMUUSBAudioEngine::getSampleBufferSize (void) {
	if (NULL != mainStream)
		return mainStream->getSampleBufferSize();
    
	return 0;
}

//--------------------------------------------------------------------------------
bool EMUUSBAudioEngine::initHardware (IOService *provider) {
	char						vendorIDCString[7];
	char						productIDCString[7];
    IOAudioStreamFormat			streamFormat;
	IOReturn					resultCode = kIOReturnError;
	bool						resultBool = false;
	UInt16						terminalType;
    
	UInt16						averageFrameSamples = 0;
	UInt16						additionalSampleFrameFreq = 0;
	UInt32						index = 0;
	EMUUSBAudioConfigObject*	usbAudio;
    
    debugIOLog ("+EMUUSBAudioEngine[%p]::initHardware (%p)", this, provider);
	terminatingDriver = FALSE;
#if LOCKING
	mLock = NULL;
	mWriteLock = NULL;
	mFormatLock = NULL;
#endif
    FailIf (FALSE == super::initHardware (provider), Exit);
    
	FailIf (NULL == usbAudioDevice, Exit); // (AC mod)
    
    usbAudio = usbAudioDevice->GetUSBAudioConfigObject();
    FailIf (NULL == usbAudio, Exit);
    
	mInput.audioStream = OSTypeAlloc(IOAudioStream); // new IOAudioStream
	FailIf(NULL == mInput.audioStream, Exit);
	mOutput.audioStream = OSTypeAlloc(IOAudioStream); // new IOAudioStream
	if (NULL == mOutput.audioStream) {
		mInput.audioStream->release();
		mInput.audioStream = NULL;
		return resultBool;
	}
	// Iterate through our assocated streams and perform initialization on each
	// This will also sort out which stream is input and which is output
    for (int i = 0; i < mStreamInterfaces->getCount(); ++i) {
		IOUSBInterface *streamInterface = OSDynamicCast(IOUSBInterface,mStreamInterfaces->getObject(i));
		FailIf(NULL == streamInterface, Exit);
		if (kIOReturnSuccess != GetDefaultSettings(streamInterface, &sampleRate)) {
			mInput.audioStream->release();
			mInput.audioStream = NULL;
			mOutput.audioStream->release();
			mOutput.audioStream = NULL;
			return resultBool;
		}
	}
    
	debugIOLogC("input alternateID %d",mInput.alternateSettingID);
	debugIOLogC("output alternatID %d",mOutput.alternateSettingID);
    
    
	// setup input stream (AC)
	if (!mInput.audioStream->initWithAudioEngine (this, (IOAudioStreamDirection) mInput.streamDirection, 1)) {
		mInput.audioStream->release();
		return resultBool;
	}
	// look for a streaming output terminal that's connected to a non-streaming input terminal
	//debugIOLog ("This is an input type endpoint (mic, etc.)");
	index = 0;
	do {
		terminalType = usbAudio->GetIndexedInputTerminalType (usbAudioDevice->mInterfaceNum, 0, index++);		// Change this to not use mControlInterface
	} while (terminalType == INPUT_UNDEFINED && index < 256);
    
	mInput.audioStream->setTerminalType (terminalType);
	
	mInput.numUSBFrameLists = RECORD_NUM_USB_FRAME_LISTS;
	mInput.numUSBFramesPerList = RECORD_NUM_USB_FRAMES_PER_LIST;
	mInput.numUSBFrameListsToQueue = RECORD_NUM_USB_FRAME_LISTS_TO_QUEUE;
	
	mInput.numUSBTimeFrames = mInput.numUSBFramesPerList / kNumberOfFramesPerMillisecond;
	
#if LOCKING
	mLock = IOLockAlloc();
	FailIf(!mLock, Exit);
	mWriteLock = IOLockAlloc();
	FailIf(!mWriteLock, Exit);
	mFormatLock = IOLockAlloc();
	FailIf(!mFormatLock, Exit);
#endif
	// alloc memory required to clear the input pipe
#if PREPINPUT
	mClearIsocFrames = (IOUSBLowLatencyIsocFrame*) IOMalloc(kNumFramesToClear * sizeof(IOUSBLowLatencyIsocFrame));
	mClearInputCompletion = (IOUSBLowLatencyIsocCompletion*) IOMalloc(sizeof(IOUSBLowLatencyIsocCompletion));
	FailIf(NULL == mClearIsocFrames || NULL == mClearInputCompletion, Exit);
#endif
	mInput.usbIsocFrames = (IOUSBLowLatencyIsocFrame *)IOMalloc (mInput.numUSBFrameLists * mInput.numUSBFramesPerList * sizeof (IOUSBLowLatencyIsocFrame));
	FailIf (NULL == mInput.usbIsocFrames, Exit);
	mInput.usbCompletion = (IOUSBLowLatencyIsocCompletion *)IOMalloc (mInput.numUSBFrameLists * sizeof (IOUSBLowLatencyIsocCompletion));
	FailIf (NULL == mInput.usbCompletion, Exit);
	bzero(mInput.usbCompletion, mInput.numUSBFrameLists * sizeof(IOUSBLowLatencyIsocCompletion));
	mInput.bufferDescriptors = (IOSubMemoryDescriptor **)IOMalloc (mInput.numUSBFrameLists * sizeof (IOSubMemoryDescriptor *));
	FailIf (NULL == mInput.bufferDescriptors, Exit);
	bzero (mInput.bufferDescriptors, mInput.numUSBFrameLists * sizeof (IOSubMemoryDescriptor *));
    
	// setup output stream
	if (!mOutput.audioStream->initWithAudioEngine (this, (IOAudioStreamDirection) mOutput.streamDirection, 1)) {
		mOutput.audioStream->release();
		return resultBool;
	}
    
	//debugIOLog ("This is an output type endpoint (speaker, etc.)");
	index = 0;
	do {
		terminalType = usbAudio->GetIndexedOutputTerminalType (usbAudioDevice->mInterfaceNum, 0, index++);		// Change this to not use mControlInterface
	} while (terminalType == OUTPUT_UNDEFINED && index < 256);
	mOutput.audioStream->setTerminalType (terminalType);
	mOutput.numUSBFrameLists = PLAY_NUM_USB_FRAME_LISTS;
	mOutput.numUSBFramesPerList = PLAY_NUM_USB_FRAMES_PER_LIST;
	mOutput.numUSBFrameListsToQueue = PLAY_NUM_USB_FRAME_LISTS_TO_QUEUE;
    
	mOutput.numUSBTimeFrames = mOutput.numUSBFramesPerList / kNumberOfFramesPerMillisecond;
    
	// Get the hub speed
	mHubSpeed = usbAudioDevice->getHubSpeed();
	
	mInput.frameQueuedForList = mOutput.frameQueuedForList = NULL;
	
	if (kUSBDeviceSpeedHigh == mHubSpeed) {
		// Allocate frame list time stamp array
		mInput.frameQueuedForList = new UInt64[mInput.numUSBFrameLists];
		FailIf (NULL == mInput.frameQueuedForList, Exit);
		mOutput.frameQueuedForList = new UInt64[mOutput.numUSBFrameLists];
		FailIf (NULL == mOutput.frameQueuedForList, Exit);
	}
    
	mOutput.usbIsocFrames = (IOUSBLowLatencyIsocFrame *)IOMalloc (mOutput.numUSBFrameLists * mOutput.numUSBFramesPerList * sizeof (IOUSBLowLatencyIsocFrame));
	FailIf (NULL == mOutput.usbIsocFrames, Exit);
    
	
	mOutput.usbCompletion = (IOUSBLowLatencyIsocCompletion *)IOMalloc (mOutput.numUSBFrameLists * sizeof (IOUSBLowLatencyIsocCompletion));
	FailIf (NULL == mOutput.usbCompletion, Exit);
	bzero(mOutput.usbCompletion, mOutput.numUSBFrameLists * sizeof(IOUSBLowLatencyIsocCompletion));
    
	
	mOutput.bufferDescriptors = (IOSubMemoryDescriptor **)IOMalloc (mOutput.numUSBFrameLists * sizeof (IOSubMemoryDescriptor *));
	FailIf (NULL == mOutput.bufferDescriptors, Exit);
	bzero (mOutput.bufferDescriptors, mOutput.numUSBFrameLists * sizeof (IOSubMemoryDescriptor *));
    
	//needed for output (AC)
	theWrapDescriptors[0] = OSTypeAlloc (IOSubMemoryDescriptor);
	theWrapDescriptors[1] = OSTypeAlloc (IOSubMemoryDescriptor);
	FailIf (((NULL == theWrapDescriptors[0]) || (NULL == theWrapDescriptors[1])), Exit);
    
	FailIf (kIOReturnSuccess != AddAvailableFormatsFromDevice (usbAudio,mInput.interfaceNumber), Exit);
	FailIf (kIOReturnSuccess != AddAvailableFormatsFromDevice (usbAudio,mOutput.interfaceNumber), Exit);
	beginConfigurationChange();
	debugIOLogC("sampleRate %d", sampleRate.whole);
	
	CalculateSamplesPerFrame (sampleRate.whole, &averageFrameSamples, &additionalSampleFrameFreq);
	// this calcs (frame size, etc.) could probably be simplified, but I'm leaving them this way for now (AC)
	mInput.multFactor = mInput.numChannels * (mChannelWidth / 8);
	mOutput.multFactor = mOutput.numChannels * (mChannelWidth / 8);
	mInput.maxFrameSize = (averageFrameSamples + 1) * mInput.multFactor;
	mOutput.maxFrameSize = (averageFrameSamples + 1) * mOutput.multFactor;
	debugIOLogC("in initHardware about to call initBuffers");
	
	initBuffers();
	setSampleRate(&sampleRate);
	completeConfigurationChange();
    
    
	// Tell the IOAudioFamily what format we are going to be running in.
	// first, for input
	streamFormat.fNumChannels = usbAudio->GetNumChannels (mInput.interfaceNumber, mInput.alternateSettingID);
	streamFormat.fBitDepth = usbAudio->GetSampleSize (mInput.interfaceNumber, mInput.alternateSettingID);
	streamFormat.fBitWidth = usbAudio->GetSubframeSize (mInput.interfaceNumber, mInput.alternateSettingID) * 8;
	streamFormat.fAlignment = kIOAudioStreamAlignmentLowByte;
	streamFormat.fByteOrder = kIOAudioStreamByteOrderLittleEndian;
	streamFormat.fDriverTag = (mInput.interfaceNumber << 16) | mInput.alternateSettingID;
	debugIOLogC("initHardware input NumChannels %d, bitWidth, %d", streamFormat.fNumChannels, streamFormat.fBitWidth);
	switch (usbAudio->GetFormat (mInput.interfaceNumber, mInput.alternateSettingID)) {
		case PCM:
			streamFormat.fSampleFormat = kIOAudioStreamSampleFormatLinearPCM;
			streamFormat.fNumericRepresentation = kIOAudioStreamNumericRepresentationSignedInt;
			streamFormat.fIsMixable = TRUE;
			break;
		case AC3:	// just starting to stub something in for AC-3 support
			streamFormat.fSampleFormat = kIOAudioStreamSampleFormatAC3;
			streamFormat.fNumericRepresentation = kIOAudioStreamNumericRepresentationSignedInt;
			streamFormat.fIsMixable = FALSE;
			streamFormat.fNumChannels = 6;
			streamFormat.fBitDepth = 16;
			streamFormat.fBitWidth = 16;
			streamFormat.fByteOrder = kIOAudioStreamByteOrderBigEndian;
			break;
		case IEC1937_AC3:
			streamFormat.fSampleFormat = kIOAudioStreamSampleFormat1937AC3;
			streamFormat.fNumericRepresentation = kIOAudioStreamNumericRepresentationSignedInt;
			streamFormat.fIsMixable = FALSE;
			break;
		default:
			FailMessage ("!!!!interface doesn't support a format that we can deal with!!!!\n", Exit);
			break;
	}
	debugIOLogC("opening input stream interface");
	FailIf (FALSE == mInput.streamInterface->open (this), Exit);		// Have to open the interface because calling setFormat will call performFormatChange, which expects the interface to be open.
	debugIOLogC("setting input format");
	resultCode = mInput.audioStream->setFormat (&streamFormat);
	//	FailIf (kIOReturnSuccess != resultCode, Exit);
	debugIOLogC("adding input audio stream");
	resultCode = addAudioStream (mInput.audioStream);
	FailIf (kIOReturnSuccess != resultCode, Exit);
	
	// now for output
	streamFormat.fNumChannels = usbAudio->GetNumChannels (mOutput.interfaceNumber, mOutput.alternateSettingID);
	streamFormat.fBitDepth = usbAudio->GetSampleSize (mOutput.interfaceNumber, mOutput.alternateSettingID);
	streamFormat.fBitWidth = usbAudio->GetSubframeSize (mOutput.interfaceNumber, mOutput.alternateSettingID) * 8;
	streamFormat.fAlignment = kIOAudioStreamAlignmentLowByte;
	streamFormat.fByteOrder = kIOAudioStreamByteOrderLittleEndian;
	streamFormat.fDriverTag = (mOutput.interfaceNumber << 16) | mOutput.alternateSettingID;
	debugIOLogC("initHardware output NumChannels %d, bitWidth, %d", streamFormat.fNumChannels, streamFormat.fBitWidth);
	switch (usbAudio->GetFormat (mOutput.interfaceNumber, mOutput.alternateSettingID)) {
		case PCM:
			streamFormat.fSampleFormat = kIOAudioStreamSampleFormatLinearPCM;
			streamFormat.fNumericRepresentation = kIOAudioStreamNumericRepresentationSignedInt;
			streamFormat.fIsMixable = TRUE;
			break;
		case AC3:	// just starting to stub something in for AC-3 support
			streamFormat.fSampleFormat = kIOAudioStreamSampleFormatAC3;
			streamFormat.fNumericRepresentation = kIOAudioStreamNumericRepresentationSignedInt;
			streamFormat.fIsMixable = FALSE;
			streamFormat.fNumChannels = 6;
			streamFormat.fBitDepth = 16;
			streamFormat.fBitWidth = 16;
			streamFormat.fByteOrder = kIOAudioStreamByteOrderBigEndian;
			break;
		case IEC1937_AC3:
			streamFormat.fSampleFormat = kIOAudioStreamSampleFormat1937AC3;
			streamFormat.fNumericRepresentation = kIOAudioStreamNumericRepresentationSignedInt;
			streamFormat.fIsMixable = FALSE;
			break;
		default:
			FailMessage ("!!!!interface doesn't support a format that we can deal with!!!!\n", Exit);
			break;
	}
	FailIf (FALSE == mOutput.streamInterface->open (this), Exit);		// Have to open the interface because calling setFormat will call performFormatChange, which expects the interface to be open.
    
	resultCode = mOutput.audioStream->setFormat (&streamFormat);
	//	FailIf (kIOReturnSuccess != resultCode, Exit);
	resultCode = addAudioStream (mOutput.audioStream);
	FailIf (kIOReturnSuccess != resultCode, Exit);
    
    
    // Verify that this 'start' request is targeting a USB Audio Stream interface
    // (i.e. it must be an audio class and a stream subclass).
	// seems like using input should be fine (AC)
#if !CUSTOMDEVICE
    FailIf(kUSBAudioClass != usbAudio->GetInterfaceClass (mInput.interfaceNumber, mInput.alternateSettingID), Exit);
#else
	FailIf(VENDOR_SPECIFIC != usbAudio->GetInterfaceClass(mInput.interfaceNumber, mInput.alternateSettingID), Exit);
#endif
    FailIf (kUSBAudioStreamInterfaceSubclass != usbAudio->GetInterfaceSubClass (mInput.interfaceNumber, mInput.alternateSettingID), Exit);
    
	startTimer = IOTimerEventSource::timerEventSource (this, waitForFirstUSBFrameCompletion);
	FailIf (NULL == startTimer, Exit);
	//workLoop->addEventSource (startTimer); //HACK testing if this screws up our timing.
    
	usbAudioDevice->doControlStuff(this, mOutput.interfaceNumber, mOutput.alternateSettingID);
	usbAudioDevice->doControlStuff(this, mInput.interfaceNumber, mInput.alternateSettingID);
	//usbAudioDevice->addCustomAudioControls(this);	// add any custom controls
    usbAudioDevice->activateAudioEngine (this, FALSE);
    
    resultBool = TRUE;
    
	snprintf (vendorIDCString, sizeof(vendorIDCString),"0x%04X", mInput.streamInterface->GetDevice()->GetVendorID ());
	snprintf (productIDCString, sizeof(productIDCString),"0x%04X", mInput.streamInterface->GetDevice()->GetProductID ());
	
	setupChannelNames();
    
	//setProperty (vendorIDCString, productIDCString);		// Ask for plugin to load (if it exists)
	IOService::registerService ();
    
Exit:
    debugIOLogC("EMUUSBAudioEngine[%p]::initHardware(%p), resultCode = %x, resultBool = %d", this, provider, resultCode, resultBool);
    return resultBool;
}

void EMUUSBAudioEngine::registerPlugin (EMUUSBAudioPlugin * thePlugin) {
	mPlugin = thePlugin;
	mPluginInitThread = thread_call_allocate ((thread_call_func_t)pluginLoaded, (thread_call_param_t)this);
    debugIOLogC("EMUUSBAudioEngine::registerPlugin");
	if (NULL != mPluginInitThread) {
		thread_call_enter (mPluginInitThread);
	}
}

void EMUUSBAudioEngine::pluginLoaded (EMUUSBAudioEngine * usbAudioEngineObject) {
	if (usbAudioEngineObject->mPlugin) {
		usbAudioEngineObject->mPlugin->open (usbAudioEngineObject);
        
		IOReturn result = usbAudioEngineObject->mPlugin->pluginInit (usbAudioEngineObject, usbAudioEngineObject->mInput.streamInterface->GetDevice()->GetVendorID (), usbAudioEngineObject->mInput.streamInterface->GetDevice()->GetProductID ());
		if (result == kIOReturnSuccess) {
			debugIOLog ("success initing the plugin");
			usbAudioEngineObject->mPlugin->pluginSetDirection ((IOAudioStreamDirection) usbAudioEngineObject->mInput.streamDirection);
			usbAudioEngineObject->mPlugin->pluginSetFormat (usbAudioEngineObject->mainStream->getFormat (), &usbAudioEngineObject->sampleRate);
		} else {
			debugIOLog ("Error initing the plugin");
			usbAudioEngineObject->mPlugin->close (usbAudioEngineObject);
			usbAudioEngineObject->mPlugin = NULL;
		}
        
		if (NULL != usbAudioEngineObject->mPluginNotification) {
			usbAudioEngineObject->mPluginNotification->remove ();
			usbAudioEngineObject->mPluginNotification = NULL;
		}
	}
	if (usbAudioEngineObject->mPluginInitThread) {// guaranteed to exist if we come in here
		thread_call_free(usbAudioEngineObject->mPluginInitThread);
		usbAudioEngineObject->mPluginInitThread = NULL;
	}
}

IOReturn EMUUSBAudioEngine::pluginDeviceRequest (IOUSBDevRequest * request, IOUSBCompletion * completion) {
	IOReturn			result = kIOReturnBadArgument;
    
	if (request)
		result = usbAudioDevice->deviceRequest (request, usbAudioDevice, completion);
	
	return result;
}

void EMUUSBAudioEngine::pluginSetConfigurationApp (const char * bundleID) {
	if (bundleID)
		usbAudioDevice->setConfigurationApp (bundleID);
}


IOReturn EMUUSBAudioEngine::performAudioEngineStart () {
    IOReturn			resultCode = kIOReturnSuccess;
    
    // Wouter: removed timebomb.
    
    debugIOLog ("+EMUUSBAudioEngine[%p]::performAudioEngineStart ()", this);
	// Reset timestamping mechanism
    
    //HACK to support another hack for timestamps.
    previousfrTimestampNs = 0;
    goodWraps = 0;

    
	if (mPlugin)  {
		debugIOLogC("starting plugin");
		mPlugin->pluginStart ();
	}
    
    if (!usbStreamRunning) {
		debugIOLogC("about to start USB stream(s)");
        resultCode = startUSBStream();
	}
    
	debugIOLogC("++EMUUSBAudioEngine[%p]::performEngineStart result is 0x%x direction %d", this, resultCode, mInput.streamDirection);
    return resultCode;
}

IOReturn EMUUSBAudioEngine::performAudioEngineStop() {
    debugIOLogC("+EMUUSBAudioEngine[%p]::performAudioEngineStop ()", this);
	if (mPlugin)
		mPlugin->pluginStop ();
    
    if (usbStreamRunning)
        stopUSBStream ();
    
    debugIOLogC("-EMUUSBAudioEngine[%p]::performAudioEngineStop()", this);
    return kIOReturnSuccess;
}

IOReturn EMUUSBAudioEngine::performFormatChange (IOAudioStream *audioStream, const IOAudioStreamFormat *newFormat, const IOAudioSampleRate *newSampleRate) {
	if (!newFormat)
		return kIOReturnSuccess;
    
    //	IOLockLock(mFormatLock);
	IOReturn	result = kIOReturnError;
	UInt8			streamDirection;
	bool			needToRestartEngine = false;
	
	if (audioStream == mInput.audioStream) {
		debugIOLogC("new format from INPUT");
		streamDirection = mInput.streamDirection;
	} else if (audioStream == mOutput.audioStream) {
		debugIOLogC("new format from OUTPUT");
		streamDirection = mOutput.streamDirection;
	} else {
		debugIOLogC("unrecognized stream");
		goto Exit;
	}
	debugIOLog ("+EMUUSBAudioEngine::performFormatChange existing  sampleRate is %d direction %d", sampleRate.whole, streamDirection);
	debugIOLogC("newFormat->fBitDepth %d, newFormat->fNumChannels %d", newFormat->fBitDepth, newFormat->fNumChannels);
	FailIf(NULL == usbAudioDevice, Exit);
    
	// stop the engine before doing nasty stuff to the buffers
	if (usbStreamRunning) {
		performAudioEngineStop();
		needToRestartEngine = true;
	}
	
    {
		EMUUSBAudioConfigObject*	usbAudio = usbAudioDevice->GetUSBAudioConfigObject();
		UInt16					alternateFrameSize = 0;
		//UInt8					ourInterfaceNumber = (UInt8)(newFormat->fDriverTag >> 16);
		UInt8					newAlternateSettingID = (UInt8)(newFormat->fDriverTag);
		bool					needToChangeChannels = false;// default
		bool					sampleRateChanged = false;
        
		FailIf (NULL == usbAudio, Exit);
		debugIOLog ("fDriverTag = 0x%x", newFormat->fDriverTag);
        
		bool needNewBuffers = false;
		
		if (newFormat->fNumChannels != audioStream->format.fNumChannels) {
			
			needToChangeChannels = true;
			needNewBuffers = true;
            
			debugIOLog ("Need to adjust channel controls, cur = %d, new = %d", audioStream->format.fNumChannels, newFormat->fNumChannels);
			
			if (kIOAudioStreamDirectionOutput == streamDirection) // not currently required since we don't have mono
				usbAudioDevice->setMonoState((1 == newFormat->fNumChannels));// mono when numChannels == 1
		}
		if (newSampleRate) {
			debugIOLog ("Changing sampling rate to %d", newSampleRate->whole);
			// special hack to make sure input channel count stays in sync w/ output when changing sample rate
			// at least from AMS, sample-rate changes always come from output stream (go figure)
			if (newFormat->fNumChannels != mInput.audioStream->format.fNumChannels && audioStream != mInput.audioStream) {
				needToChangeChannels = true;
				debugIOLog ("Need to adjust input channel controls, cur = %d, new = %d", mInput.audioStream->format.fNumChannels, newFormat->fNumChannels);
				mInput.audioStream->setFormat(newFormat,false);
            }
			sampleRate = *newSampleRate;
			sampleRateChanged = true;
		}
		
		if (mPlugin) {
			mPlugin->pluginSetFormat (newFormat, &sampleRate);
		}
		
		
		if (audioStream == mInput.audioStream || sampleRateChanged)
		{
            UInt8	newDirection = usbAudio->GetIsocEndpointDirection (mInput.interfaceNumber, newAlternateSettingID);
            if (FALSE == usbAudio->VerifySampleRateIsSupported(mInput.interfaceNumber, newAlternateSettingID, sampleRate.whole)) {
                newAlternateSettingID = usbAudio->FindAltInterfaceWithSettings (mInput.interfaceNumber, newFormat->fNumChannels, newFormat->fBitDepth, sampleRate.whole);
                mPollInterval = (UInt32) (1 << ((UInt32) usbAudio->GetEndpointPollInterval(mInput.interfaceNumber, newAlternateSettingID, newDirection) -1));
                if ((1 != mPollInterval) || (8 != mPollInterval)) {// disallow selection of endpoints with sub ms polling interval NB - assumes that sub ms device will not use a poll interval of 1 (every microframe)
                    newAlternateSettingID = 255;
                }
                debugIOLog ("%d channel %d bit @ %d Hz is not supported. Suggesting alternate setting %d", newFormat->fNumChannels,
                            newFormat->fBitDepth, sampleRate.whole, newAlternateSettingID);
                
                FailIf (255 == newAlternateSettingID, Exit);
            }
            
            FailIf (newDirection != mInput.streamDirection, Exit);
            debugIOLog ("++about to set input : ourInterfaceNumber = %d & newAlternateSettingID = %d", mInput.interfaceNumber, newAlternateSettingID);
            beginConfigurationChange();
            
            UInt8	address = usbAudio->GetIsocEndpointAddress(mInput.interfaceNumber, newAlternateSettingID, mInput.streamDirection);
            alternateFrameSize = usbAudio->GetEndpointMaxPacketSize(mInput.interfaceNumber, newAlternateSettingID, address);
            averageSampleRate = sampleRate.whole;	// Set this as the default until we are told otherwise
            debugIOLog ("averageSampleRate = %d", averageSampleRate);
            
            //if (streamDirection == mInput.streamDirection) {
			mInput.numChannels = newFormat->fNumChannels;
            //}
            mInput.alternateSettingID = newAlternateSettingID;
            mChannelWidth = newFormat->fBitWidth;
            mInput.multFactor = mInput.numChannels * (mChannelWidth / 8);
            debugIOLogC("alternateFrameSize is %d", alternateFrameSize);
            if (mInput.maxFrameSize != alternateFrameSize) {
                debugIOLogC("maxFrameSize %d alternateFrameSize %d", mInput.maxFrameSize, alternateFrameSize);
                mInput.maxFrameSize = alternateFrameSize;
                needNewBuffers = true;
            }
            
            // set the format - JH
			mInput.audioStream->setFormat(newFormat,false);
		}
	    if (audioStream == mOutput.audioStream || sampleRateChanged) {
            //now output
            
            UInt8 newDirection = usbAudio->GetIsocEndpointDirection (mOutput.interfaceNumber, newAlternateSettingID);
            if (FALSE == usbAudio->VerifySampleRateIsSupported(mOutput.interfaceNumber, newAlternateSettingID, sampleRate.whole)) {
                newAlternateSettingID = usbAudio->FindAltInterfaceWithSettings (mOutput.interfaceNumber, newFormat->fNumChannels, newFormat->fBitDepth, sampleRate.whole);
                mPollInterval = (UInt32) (1 << ((UInt32) usbAudio->GetEndpointPollInterval(mOutput.interfaceNumber, newAlternateSettingID, newDirection) -1));
                if ((1 != mPollInterval) || (8 != mPollInterval)) {// disallow selection of endpoints with sub ms polling interval NB - assumes that sub ms device will not use a poll interval of 1 (every microframe)
                    newAlternateSettingID = 255;
                }
                debugIOLog ("%d channel %d bit @ %d Hz is not supported. Suggesting alternate setting %d", newFormat->fNumChannels,
                            newFormat->fBitDepth, sampleRate.whole, newAlternateSettingID);
                
                FailIf (255 == newAlternateSettingID, Exit);
            }
            FailIf (newDirection != mOutput.streamDirection, Exit);
            
            
            debugIOLog ("++about to set output : ourInterfaceNumber = %d & newAlternateSettingID = %d", mOutput.interfaceNumber, newAlternateSettingID);
            
            UInt8 address = usbAudio->GetIsocEndpointAddress(mOutput.interfaceNumber, newAlternateSettingID, mOutput.streamDirection);
            alternateFrameSize = usbAudio->GetEndpointMaxPacketSize(mOutput.interfaceNumber, newAlternateSettingID, address);
            
            //if (streamDirection == mOutput.streamDirection) {
			mOutput.numChannels = newFormat->fNumChannels;
            //}
            mOutput.alternateSettingID = newAlternateSettingID;
            mChannelWidth = newFormat->fBitWidth;
            mOutput.multFactor = mOutput.numChannels * (mChannelWidth / 8);
            debugIOLogC("alternateFrameSize is %d", alternateFrameSize);
            if (mOutput.maxFrameSize != alternateFrameSize) {
                debugIOLogC("maxFrameSize %d alternateFrameSize %d", mOutput.maxFrameSize, alternateFrameSize);
                mOutput.maxFrameSize = alternateFrameSize;
                needNewBuffers = true;
                
            }
            
            // set the format - JH
			mOutput.audioStream->setFormat(newFormat,false);
		}
        
		
		if (needNewBuffers) {
			if (kIOReturnSuccess!= initBuffers()) {
				debugIOLogC("problem with initBuffers in performFormatChange");
				goto Exit;
			}
		}
		
		// Set the sampling rate on the device
		SetSampleRate(usbAudio, sampleRate.whole);		// no need to check the error
        
		// I think this code is not needed, as we have no controls needing to be changed
#if 0
		if (needToChangeChannels) {
			removeAllDefaultAudioControls();
			usbAudioDevice->createControlsForInterface(this, mInput.interfaceNumber, newAlternateSettingID);
			usbAudioDevice->createControlsForInterface(this, mOutput.interfaceNumber, newAlternateSettingID);
			//usbAudioDevice->createControlsForInterface(this, ourInterfaceNumber, newAlternateSettingID);
		}
#endif
        
        
		debugIOLog ("called setNumSampleFramesPerBuffer with %d", mInput.bufferSize / mInput.multFactor);
		debugIOLog ("newFormat->fNumChannels = %d, newFormat->fBitWidth %d", newFormat->fNumChannels, newFormat->fBitWidth);
		// debugIOLog ("called setSampleOffset with %d", mInput.numUSBFramesPerList);
		completeConfigurationChange();
		result = kIOReturnSuccess;
	}
Exit:
	if (needToRestartEngine) {
		performAudioEngineStart();
	}
    //	IOLockUnlock(mFormatLock);
	debugIOLog ("-EMUUSBAudioEngine::performFormatChange, result = %x", result);
    return result;
}

// called from writeFrameList
IOReturn EMUUSBAudioEngine::PrepareWriteFrameList (UInt32 arrayIndex) {
    debugIOLogW ("+EMUUSBAudioEngine::PrepareWriteFrameList");

	IOReturn		result = kIOReturnError;// default
	UInt32			sampleBufferSize = (mOutput.audioStream) ? mOutput.audioStream->getSampleBufferSize() : 0;
	if (sampleBufferSize) {// sanity check just in-case we get a zero
		UInt32			thisFrameListSize = 0;
		UInt32			thisFrameSize = 0;
		UInt32			firstFrame = arrayIndex * mOutput.numUSBFramesPerList;
		UInt32			numBytesToBufferEnd = sampleBufferSize - previouslyPreparedBufferOffset;
		UInt32			lastPreparedByte = previouslyPreparedBufferOffset;
		bool			haveWrapped = false;
		UInt16			integerSamplesInFrame, stockSamplesInFrame;
        

		mOutput.usbCompletion[arrayIndex].target = (void *)this;
		mOutput.usbCompletion[arrayIndex].action = writeHandler;
		mOutput.usbCompletion[arrayIndex].parameter = 0;			// Set to number of bytes from the 0 wrap, 0 if this buffer didn't wrap
        
		const UInt16 stockSamplesInFrameDivisor = 1000 * kNumberOfFramesPerMillisecond;
		stockSamplesInFrame = averageSampleRate / stockSamplesInFrameDivisor;
        
		UInt16 contiguousZeroes = 0;
		
        debugIOLogW("PrepareWriteFrameList stockSamplesInFrame %d numUSBFramesPerList %d", stockSamplesInFrame, mOutput.numUSBFramesPerList);
		for (UInt32 numUSBFramesPrepared = 0; numUSBFramesPrepared < mOutput.numUSBFramesPerList; ++numUSBFramesPrepared) {
			integerSamplesInFrame = stockSamplesInFrame;// init to this stock value each time around
			//integerSamplesInFrame = lastInputFrames; // init to # of input frames;
			fractionalSamplesRemaining += averageSampleRate - (integerSamplesInFrame * 1000);
            debugIOLogW("PrepareWriteFrameList fractionalSamplesRemaining %d", fractionalSamplesRemaining);
			if (fractionalSamplesRemaining >= 1000) {
				++integerSamplesInFrame;
                debugIOLogW("inc integerSamplesInFrame");
				fractionalSamplesRemaining -= 1000;
			}
			// get the size of the corresponding input packet (this will hopefully keep our sample counts in sync)
#if 1
			thisFrameSize = PopFrameSize();
			if (!thisFrameSize) {
				thisFrameSize = stockSamplesInFrame;
			}
			
#else
			thisFrameSize = integerSamplesInFrame;
#endif
			runningOutputCount += thisFrameSize;
			//debugIOLogC("thisFrameSize %d",thisFrameSize);
			thisFrameSize *= mOutput.multFactor;
			//debugIOLogC("PrepareWriteFrameList fractionalSamplesRemaining %d thisFrameSize %d", fractionalSamplesRemaining, thisFrameSize);
			if (thisFrameSize >= numBytesToBufferEnd) {
                //		debugIOLogC("param has something %d", numUSBFramesPrepared);
                
#if 0
				//perform zero test
				bool allZeroes = true;
				for (int i = lastPreparedByte; i < lastPreparedByte + numBytesToBufferEnd; ++i) {
                    //debugIOLogC("%d",((Byte *) mOutput.bufferPtr)[i]);
                    if (((Byte *) mOutput.bufferPtr)[i] != 1) {
                        allZeroes = false;
                        break;
                    }
				}
				if (allZeroes) {
					++contiguousZeroes;
					//debugIOLogC("*** detected all zeroes in write buffer %d ",numUSBFramesPrepared);
				} else {
					contiguousZeroes = 0;
					//debugIOLogC("*** non-zero buffer %d",mOutput.usbFrameToQueueAt+numUSBFramesPrepared);
					if (mOutput.usbFrameToQueueAt+numUSBFramesPrepared - lastNonZeroFrame > 1) {
						debugIOLogW("*** non-zero discontinuity at %d",mOutput.usbFrameToQueueAt+numUSBFramesPrepared);
					}
					lastNonZeroFrame = mOutput.usbFrameToQueueAt+numUSBFramesPrepared;
				}
#endif
                
				lastPreparedByte = thisFrameSize - numBytesToBufferEnd;
				mOutput.usbCompletion[arrayIndex].parameter = (void *)(UInt64)(((numUSBFramesPrepared + 1) << 16) | lastPreparedByte);
				theWrapDescriptors[0]->initSubRange (mOutput.usbBufferDescriptor, previouslyPreparedBufferOffset, sampleBufferSize - previouslyPreparedBufferOffset, kIODirectionInOut);
				numBytesToBufferEnd = sampleBufferSize - lastPreparedByte;// reset
				haveWrapped = true;
			} else {
#if 0
			    //perform zero test
				bool allZeroes = true;
				for (int i = lastPreparedByte; i < lastPreparedByte + thisFrameSize; ++i) {
                    //debugIOLogC("%d",((Byte *) mOutput.bufferPtr)[i]);
                    if (((Byte *) mOutput.bufferPtr)[i] != 1) {
                        allZeroes = false;
                        break;
                    }
				}
				if (allZeroes) {
					++contiguousZeroes;
					//debugIOLogC("*** detected all zeroes in write buffer %d ",numUSBFramesPrepared);
				} else {
					contiguousZeroes = 0;
					//debugIOLogC("*** non-zero buffer %d",mOutput.usbFrameToQueueAt+numUSBFramesPrepared);
					if (mOutput.usbFrameToQueueAt+numUSBFramesPrepared - lastNonZeroFrame > 1) {
						debugIOLogW("*** non-zero discontinuity at %d --> %d",lastNonZeroFrame,mOutput.usbFrameToQueueAt+numUSBFramesPrepared);
					}
					lastNonZeroFrame = mOutput.usbFrameToQueueAt+numUSBFramesPrepared;
				}
#endif
				thisFrameListSize += thisFrameSize;
				lastPreparedByte += thisFrameSize;
				numBytesToBufferEnd -= thisFrameSize;
                //			debugIOLogC("no param");
			}
			mOutput.usbIsocFrames[firstFrame + numUSBFramesPrepared].frStatus = -1;
			mOutput.usbIsocFrames[firstFrame + numUSBFramesPrepared].frActCount = 0;
			mOutput.usbIsocFrames[firstFrame + numUSBFramesPrepared].frReqCount = thisFrameSize;
		}
        //debugIOLogC("Done with the numUSBFrames loop");
		debugIOLogW("num actual data frames in list %d",mOutput.numUSBFramesPerList - contiguousZeroes);
		if (haveWrapped) {
			needTimeStamps = TRUE;
			theWrapDescriptors[1]->initSubRange (mOutput.usbBufferDescriptor, 0, lastPreparedByte, kIODirectionInOut);
            
			if (NULL != theWrapRangeDescriptor) {
				theWrapRangeDescriptor->release ();
				theWrapRangeDescriptor = NULL;
			}
            
			theWrapRangeDescriptor = IOMultiMemoryDescriptor::withDescriptors ((IOMemoryDescriptor **)theWrapDescriptors, 2, kIODirectionInOut, true);
		} else {
			mOutput.bufferDescriptors[arrayIndex]->initSubRange (mOutput.usbBufferDescriptor, previouslyPreparedBufferOffset, thisFrameListSize, kIODirectionInOut);
			FailIf (NULL == mOutput.bufferDescriptors[arrayIndex], Exit);
		}
        
		debugIOLogW("PrepareWriteFrameList: lastPrepareFrame %d safeToEraseTo %d",lastPreparedByte / mOutput.multFactor, safeToEraseTo / mOutput.multFactor);
        
		safeToEraseTo = lastSafeErasePoint;
		lastSafeErasePoint = previouslyPreparedBufferOffset;
		previouslyPreparedBufferOffset = lastPreparedByte;
		result = kIOReturnSuccess;
	}
Exit:
	return result;
}


IOReturn EMUUSBAudioEngine::readFrameList (UInt32 frameListNum) {
    debugIOLogR("+ read frameList %d ", frameListNum);
    if (shouldStop) {
        debugIOLog("*** Read should have stopped. Who is calling this? Canceling call")
        return kIOReturnAborted;
    }
    
	IOReturn	result = kIOReturnError;
	if (mInput.pipe) {
		UInt32		firstFrame = frameListNum * mInput.numUSBFramesPerList;
		mInput.usbCompletion[frameListNum].target = (void*) this;
		mInput.usbCompletion[frameListNum].action = readCompleted;
		mInput.usbCompletion[frameListNum].parameter = (void*) (UInt64)frameListNum; // remember the frameListNum
        
		for (int i = 0; i < mInput.numUSBFramesPerList; ++i) {
			mInput.usbIsocFrames[firstFrame+i].frStatus = -1; // used to check if this frame was already received
			mInput.usbIsocFrames[firstFrame+i].frActCount = 0; // actual #bytes transferred
			mInput.usbIsocFrames[firstFrame+i].frReqCount = mInput.maxFrameSize; // #bytes to read.
			*(UInt64 *)(&(mInput.usbIsocFrames[firstFrame + i].frTimeStamp)) = 	0ul; //time when frame was procesed
		}
        //debugIOLogC("read framelist : %d frames, %d bytes per frame",mInput.numUSBFramesPerList, mInput.maxFrameSize);
        
		//result = mInput.pipe->Read(mInput.bufferDescriptors[frameListNum], mInput.usbFrameToQueueAt, mInput.numUSBFramesPerList, &mInput.usbIsocFrames[firstFrame], &mInput.usbCompletion[frameListNum], 1);//mPollInterval);
        
        // HACK. Disabled the 'refresh of frame list'. We read only when read calls back
        // HACK. always keep reading, instead of targeting explicit framenr - that seemed unreliable.
        // we now prefer to keep reading instead of hanging on a bad USB frame nr.
        // FIXME can we check somehow if we do not loose data on the USB stream this way?
        
        /*The updatefrequency is not so well documented. But in IOUSBInterfaceInterface192 I read:
         Specifies how often, in milliseconds, the frame list data should be updated. Valid range is 0 - 8. If 0, it means that the framelist should be updated at the end of the transfer.
         It appears that this number also has impact on the timing details in the frame list.
         If you set this to 0, there happens an additional 8ms for a full framelist once in a 
         few minutes in the timings.
         If you set this to 1, this jump is 8x more often, about once 30 seconds, but is only 1ms.
         We must keep these jumps small, to avoid timestamp errors and buffer overruns.
         */
		result = mInput.pipe->Read(mInput.bufferDescriptors[frameListNum], kAppleUSBSSIsocContinuousFrame, mInput.numUSBFramesPerList, &mInput.usbIsocFrames[firstFrame], &mInput.usbCompletion[frameListNum],1);
        if (result!=kIOReturnSuccess) {
            doLog("USB pipe READ error %x",result);
        }
		if (mInput.frameQueuedForList)
			mInput.frameQueuedForList[frameListNum] = mInput.usbFrameToQueueAt;
        
        
		mInput.usbFrameToQueueAt += mInput.numUSBTimeFrames;
	}
	return result;
}



void EMUUSBAudioEngine::readCompleted (void * object, void * frameListNrPtr, IOReturn result, IOUSBLowLatencyIsocFrame * pFrames) {
   
	EMUUSBAudioEngine *	engine = (EMUUSBAudioEngine *)object;

    // HACK we have numUSBFramesPerList frames, which one to check?? Print frame 0 info.
    debugIOLogR("+ readCompleted framelist %d currentFrameList %d result %x frametime %lld systime %lld",
                frameListnr, engine->mInput.currentFrameList, result, myFrames->frTimeStamp,systemTime);

    // CHECK is this really used?
//    if (TRUE == engine->startingEngine) {
//        doLog("EMUUSBAudioEngine::readCompleted SERIOUS: engine still starting up. Exit readCompleted.");
//        return; // wait till engine really started.
//    }
    engine->startingEngine = FALSE; // HACK if we turn off the timer to start the  thing...

    
    IOLockLock(engine->mLock);
    // HACK we MUST have the lock now as we must restart reading the list.
    // This means we may have to wait for GatherInputSamples to complete from a call from convertInputSamples.
    // should be short.
    // We must fix this problem.
    
    /*An "underrun error" occurs when the UART transmitter has completed sending a character and the transmit buffer is empty. In asynchronous modes this is treated as an indication that no data remains to be transmitted, rather than an error, since additional stop bits can be appended. This error indication is commonly found in USARTs, since an underrun is more serious in synchronous systems.
     */

	if (kIOReturnAborted != result) {
        if (engine->GatherInputSamples(true) != kIOReturnSuccess) {
            debugIOLog("***** EMUUSBAudioEngine::readCompleted failed to read all packets from USB stream!");
        }
	}
	
    // Wouter:  data collection from the USB read is complete.
    // Now start the read on the next block.
	if (!engine->shouldStop) {
        UInt32	frameListToRead;
		// (orig doc) keep incrementing until limit of numUSBFrameLists - 1 is reached.
        // also, we can wonder if we want to do it this way. Why not just check what comes in instead
        engine->mInput.currentFrameList =(engine->mInput.currentFrameList + 1) % RECORD_NUM_USB_FRAME_LISTS;

        // now we have already numUSBFrameListsToQueue-1 other framelist queues running.
        // We set our current list to the next one that is not yet running
        
        frameListToRead = engine->mInput.currentFrameList - 1 + engine->mInput.numUSBFrameListsToQueue;
        frameListToRead -= engine->mInput.numUSBFrameLists * (frameListToRead >= engine->mInput.numUSBFrameLists);// crop the number of framesToRead
        // seems something equal to frameListToRead = (frameListnr + engine->mInput.numUSBFrameListsToQueue) % engine->mInput.numUSBFrameLists;
        engine->readFrameList(frameListToRead); // restart reading (but for different framelist).
        
	} else  {// stop issued FIX THIS CODE
		debugIOLogR("++EMUUSBAudioEngine::readCompleted() - stopping: %d", engine->shouldStop);
		++engine->shouldStop;
		if (engine->shouldStop == (engine->mInput.numUSBFrameListsToQueue + 1) && TRUE == engine->terminatingDriver) {
			engine->mInput.streamInterface->close(engine);
			engine->mInput.streamInterface = NULL;
		}
	}
	IOLockUnlock(engine->mLock);
    
	debugIOLogR("- readCompleted currentFrameList=%d",engine->mInput.currentFrameList);
	return;
}


void EMUUSBAudioEngine::resetClipPosition (IOAudioStream *audioStream, UInt32 clipSampleFrame) {
	if (mPlugin)
		mPlugin->pluginReset();
}

void EMUUSBAudioEngine::sampleRateHandler(void* target, void* parameter, IOReturn result, IOUSBIsocFrame* pFrames) {
	if (target) {
		EMUUSBAudioEngine*	engine = (EMUUSBAudioEngine*) target;
		IOFixed				theSampleRate;
		UInt32				newSampleRate = 0;
		UInt32				isHighSpeed = (kUSBDeviceSpeedHigh == engine->mHubSpeed);
		if (kIOReturnSuccess == result) {
			IOFixed	fract;
			UInt32	oldSampleRate = engine->averageSampleRate;
			UInt16	fixed;
			newSampleRate = *(engine->aveSampleRateBuf);
			theSampleRate = USBToHostLong(newSampleRate);
			if (!isHighSpeed)
				theSampleRate = theSampleRate << 2;// full speed connection only
			fixed = theSampleRate >> 16;
			newSampleRate = fixed * 1000;
			fract = IOFixedMultiply(theSampleRate & 0x0000FFFF, 1000 << 16);
			newSampleRate += (fract & 0xFFFF0000) >> 16;
			if (engine->usbAudioDevice) {
				UInt32		engineSampleRate = engine->usbAudioDevice->getHardwareSampleRate();// get the sample rate the hardware was set to
                
				UInt32		maxSampleRateDelta = engineSampleRate + engineSampleRate /100;	// no more than 10% over
				UInt32		minSampleRateDelta = engineSampleRate - engineSampleRate /100;	// no more than 10% under
				if (newSampleRate && (newSampleRate < maxSampleRateDelta) && (newSampleRate > minSampleRateDelta) && oldSampleRate != newSampleRate) {
					engine->averageSampleRate = newSampleRate;
				}
			}
		}
        
		if (!engine->shouldStop) {
			engine->mSampleRateFrame.frStatus = -1;
			engine->mSampleRateFrame.frReqCount = 3 + isHighSpeed;// change required count based on USB speed
			engine->mSampleRateFrame.frActCount = 0;
			engine->nextSynchReadFrame += (1 << engine->refreshInterval);
			// i think we can get away with just the associated-input pipe here (AC)
			if (engine->mInput.associatedPipe)
				engine->mInput.associatedPipe->Read(engine->neededSampleRateDescriptor, engine->nextSynchReadFrame, 1,
                                                    &(engine->mSampleRateFrame), &(engine->sampleRateCompletion));
		}
	}
}

IOReturn EMUUSBAudioEngine::SetSampleRate (EMUUSBAudioConfigObject *usbAudio, UInt32 inSampleRate) {
	IOReturn				result = kIOReturnError;
	
    if (usbAudioDevice && usbAudioDevice->hasSampleRateXU()) {// try using the XU method to set the sample rate before using the default
        UInt8	sampleRateToSet = 0;
        switch (inSampleRate) {// all the supported sample rates
            case 44100:
                sampleRateToSet = sr_44kHz;
                break;
            case 48000:
                sampleRateToSet = sr_48kHz;
                break;
            case 88200:
                sampleRateToSet = sr_88kHz;
                break;
            case 96000:
                sampleRateToSet = sr_96kHz;
                break;
            case 176400:
                sampleRateToSet = sr_176kHz;
                break;
            case 192000:
                sampleRateToSet = sr_192kHz;
                break;
            default:
                return result;// we should never come in here
                break;
        }
        if (inSampleRate != usbAudioDevice->getHardwareSampleRate()) {
            result = usbAudioDevice->setExtensionUnitSettings(kClockRate, kClockRateSelector,(void*) &sampleRateToSet, kStdDataLen);
            if (kIOReturnSuccess == result) {// the engines are tied together
                usbAudioDevice->setHardwareSampleRate(inSampleRate);
                usbAudioDevice->setOtherEngineSampleRate(this, inSampleRate);
            } else {
                result = kIOReturnSuccess;
            }
        }
    } else if (usbAudio->IsocEndpointHasSampleFreqControl (mOutput.interfaceNumber, mOutput.alternateSettingID)) {// use the conventional method
		IOUSBDevRequest		devReq;
		UInt32				theSampleRate = OSSwapHostToLittleInt32 (inSampleRate);
        
		devReq.bmRequestType = USBmakebmRequestType (kUSBOut, kUSBClass, kUSBEndpoint);
		devReq.bRequest = SET_CUR;
		devReq.wValue = (SAMPLING_FREQ_CONTROL << 8) | 0;
		devReq.wIndex = usbAudio->GetIsocEndpointAddress (mOutput.interfaceNumber, mOutput.alternateSettingID, mOutput.streamDirection);
		devReq.wLength = 3 + (kUSBDeviceSpeedHigh == mHubSpeed);// USB 2.0 device has maxPacket size of 4
		devReq.pData = &theSampleRate;
        
		result = mOutput.streamInterface->GetDevice()->DeviceRequest (&devReq);
	}
	if (kIOReturnSuccess == result)
		hardwareSampleRate = inSampleRate;// remember the sample rate setting as the averageSampleRate could vary.
	return result;
}

IOReturn EMUUSBAudioEngine::startUSBStream() {
	const IOAudioStreamFormat *			inputFormat = mInput.audioStream->getFormat();
	const IOAudioStreamFormat *			outputFormat = mOutput.audioStream->getFormat();
	
	IOReturn							resultCode = kIOReturnError;
	IOUSBFindEndpointRequest			audioIsochEndpoint;
	EMUUSBAudioConfigObject *			usbAudio = usbAudioDevice->GetUSBAudioConfigObject();
	UInt16								averageFrameSamples = 0;
	UInt16								additionalSampleFrameFreq = 0;
    UInt8	address;
    UInt32	maxPacketSize;
    
	// if the stream is already running, get the heck out of here! (AC)
	if (usbStreamRunning) {
		return resultCode;
	}
    
    // Start the IO audio engine
    // Enable interrupts for this stream
    // The interrupt should be triggered at the start of the sample buffer
    // The interrupt handler should increment the fCurrentLoopCount and fLastLoopTime fields
	// Make sure we have enough bandwidth (return unused bandwidth)
    //	FailIf(mUSBBufferDescriptor == NULL, Exit);
	CalculateSamplesPerFrame(sampleRate.whole, &averageFrameSamples, &additionalSampleFrameFreq);
	debugIOLogC("averageFrameSamples=%d",averageFrameSamples);
    
	// bit width should be the same for both input and output
    //	FailIf(inputFormat->fBitWidth != outputFormat->fBitWidth, Exit);
	mChannelWidth = inputFormat->fBitWidth;
	
	mDropStartingFrames = kNumberOfStartingFramesToDrop;
	
	UInt32	newInputMultFactor = (inputFormat->fBitWidth / 8) * inputFormat->fNumChannels;
	UInt32	newOutputMultFactor = (outputFormat->fBitWidth / 8) * outputFormat->fNumChannels;
	
	UInt32	altFrameSampleSize = averageFrameSamples + 1;
	mInput.currentFrameList = mOutput.currentFrameList = 0;
    mInput.previousFrameList = 3; //  different from currentFrameList.
    
	safeToEraseTo = 0;
	lastSafeErasePoint = 0;
	mInput.bufferOffset = mOutput.bufferOffset = 0;
	ClearFrameSizes();
	startingEngine = TRUE;
	previouslyPreparedBufferOffset = 0;		// Start playing from the start of the buffer
	fractionalSamplesRemaining = 0;			// Reset our parital frame list info
    shouldStop = 0;
	runningInputCount = runningOutputCount = 0;
	lastDelta = 0;
	debugIOLogC("mInput.frameQueuedList");
	if (mInput.frameQueuedForList) {
		bzero(mInput.frameQueuedForList, sizeof(UInt64) * mInput.numUSBFrameLists);
	}
	if (mOutput.frameQueuedForList) {
		bzero(mOutput.frameQueuedForList, sizeof(UInt64) * mOutput.numUSBFrameLists);
	}
    
	debugIOLogC("Isoc Frames / usbCompletions");
	bzero(mInput.usbIsocFrames, mInput.numUSBFrameLists * mInput.numUSBFramesPerList * sizeof(IOUSBLowLatencyIsocFrame));
	bzero(mInput.usbCompletion, mInput.numUSBFrameLists * sizeof(IOUSBLowLatencyIsocCompletion));
    FailIf ((mInput.numUSBFrameLists < mInput.numUSBFrameListsToQueue), Exit);
	bzero(mOutput.usbIsocFrames, mOutput.numUSBFrameLists * mOutput.numUSBFramesPerList * sizeof(IOUSBLowLatencyIsocFrame));
	bzero(mOutput.usbCompletion, mOutput.numUSBFrameLists * sizeof(IOUSBLowLatencyIsocCompletion));
    FailIf ((mOutput.numUSBFrameLists < mOutput.numUSBFrameListsToQueue), Exit);
    
	SetSampleRate(usbAudio, sampleRate.whole);
	
	
	// if our buffer characteristics have changed (or they don't yet exist), initialize buffers now
	
	if (newInputMultFactor > mInput.multFactor || newOutputMultFactor > mOutput.multFactor
		|| !mInput.usbBufferDescriptor || !mOutput.usbBufferDescriptor) {
        debugIOLogC("startUSBStream: about to re-init buffers, input factor=%d (now %d), output factor=%d (now %d)",
                   mInput.multFactor,newInputMultFactor,mOutput.multFactor,newOutputMultFactor);
        mInput.maxFrameSize = altFrameSampleSize * newInputMultFactor;
        mOutput.maxFrameSize = altFrameSampleSize * newOutputMultFactor;
        mInput.multFactor = newInputMultFactor;
        mOutput.multFactor = newOutputMultFactor;
        debugIOLogC("pre initBuffers");
        beginConfigurationChange();
        initBuffers();
        completeConfigurationChange();
        debugIOLogC("post initBuffers");
	}
    
    // Wouter: this is bad example of code duplication.
    // Is it not possible to make this more modular?
    
	//first do the input
    
	// Allocate the pipe now so that we don't keep it open when we're not streaming audio to the device.
	FailIf (NULL == mInput.streamInterface, Exit);
    
	resultCode = mInput.streamInterface->SetAlternateInterface (this, mInput.alternateSettingID);
	FailIf (kIOReturnSuccess != resultCode, Exit);
	
	// Acquire a PIPE for the isochronous stream.
	audioIsochEndpoint.type = kUSBIsoc;
	audioIsochEndpoint.direction = mInput.streamDirection;
#if 1
	{
		debugIOLogC("createInputPipe");
		mInput.pipe = mInput.streamInterface->FindNextPipe (NULL, &audioIsochEndpoint);
		FailIf (NULL == mInput.pipe, Exit);
		mInput.pipe->retain ();
	}
	
	address = usbAudio->GetIsocEndpointAddress(mInput.interfaceNumber, mInput.alternateSettingID, mInput.streamDirection);
	maxPacketSize = usbAudio->GetEndpointMaxPacketSize(mInput.interfaceNumber, mInput.alternateSettingID, address);
    
    
	mInput.maxFrameSize = altFrameSampleSize * mInput.multFactor;
	if (mInput.maxFrameSize != maxPacketSize)
		mInput.maxFrameSize = maxPacketSize;
	mBus = mInput.streamInterface->GetDevice()->GetBus();// this will not change
    // Other possible variations - use a static variable that holds the offset number.
    // The var is set depending on the hub speed and whether the first write/ read failed with a late error.
    // When a late error is encountered (USB 2.0), increment the var until a max of 16 frames is reached.
    // NB - From testing and observation this work around does not help and has therefore been deleted.
	mInput.frameOffset = kMinimumFrameOffset + ((kUSBDeviceSpeedHigh == mHubSpeed) * kUSB2FrameOffset);
	mInput.usbFrameToQueueAt = mBus->GetFrameNumber() + mInput.frameOffset;	// start on an offset usb frame
    //	debugIOLogC("usbFrameToQueueAt is %x\n", usbFrameToQueueAt);
	
	//IOLog("the buffers are %x and %x\n", buffers[0], buffers[1]);
	
	*(UInt64 *) (&(mInput.usbIsocFrames[0].frTimeStamp)) = 0xFFFFFFFFFFFFFFFFull;
    
	if (NULL != mInput.associatedPipe) {
		nextSynchReadFrame = mInput.usbFrameToQueueAt;
		debugIOLogC("read from associated input pipe");
		//resultCode = mInput.associatedPipe->Read(neededSampleRateDescriptor, nextSynchReadFrame, 1,&(mSampleRateFrame), &sampleRateCompletion);
		resultCode = mInput.associatedPipe->Read(neededSampleRateDescriptor, kAppleUSBSSIsocContinuousFrame, 1,&(mSampleRateFrame), &sampleRateCompletion);
	}
#if PREPINPUT
	prepInputPipe();
#endif

    // we start reading on all framelists. USB will figure it out and take the next one in order
    // when it has data. We restart each framelist immediately in readCompleted when we get data.
	for (UInt32 frameListNum = mInput.currentFrameList; frameListNum < mInput.numUSBFrameListsToQueue; ++frameListNum) {
		readFrameList(frameListNum);
	}
    
#endif
	
	// and now the output
	
	resultCode = mOutput.streamInterface->SetAlternateInterface (this, mOutput.alternateSettingID);
	FailIf (kIOReturnSuccess != resultCode, Exit);
    
    
    
#if 0
	
	debugIOLogC("create output pipe");
	bzero(&audioIsochEndpoint,sizeof(audioIsochEndpoint));
	audioIsochEndpoint.type = kUSBIsoc;
	audioIsochEndpoint.direction = mOutput.streamDirection;
	mOutput.pipe = mOutput.streamInterface->FindNextPipe (NULL, &audioIsochEndpoint);
	FailIf (NULL == mOutput.pipe, Exit);
	mOutput.pipe->retain ();
	debugIOLogC("check for associated endpoint");
	CheckForAssociatedEndpoint (usbAudio,mOutput.interfaceNumber,mOutput.alternateSettingID);// result is ignored
    
	address = usbAudio->GetIsocEndpointAddress(mOutput.interfaceNumber, mOutput.alternateSettingID, mOutput.streamDirection);
	maxPacketSize = usbAudio->GetEndpointMaxPacketSize(mOutput.interfaceNumber, mOutput.alternateSettingID, address);
    
    
	mOutput.maxFrameSize = altFrameSampleSize * mOutput.multFactor;
	if (mOutput.maxFrameSize != maxPacketSize)
		mOutput.maxFrameSize = maxPacketSize;
	mBus = mOutput.streamInterface->GetDevice()->GetBus();// this will not change
    // Other possible variations - use a static variable that holds the offset number.
    // The var is set depending on the hub speed and whether the first write/ read failed with a late error.
    // When a late error is encountered (USB 2.0), increment the var until a max of 16 frames is reached.
    // NB - From testing and observation this work around does not help and has therefore been deleted.
	mOutput.frameOffset = kMinimumFrameOffset + ((kUSBDeviceSpeedHigh == mHubSpeed) * kUSB2FrameOffset);
	mOutput.usbFrameToQueueAt = mBus->GetFrameNumber() + mOutput.frameOffset;	// start on an offset usb frame
	*(UInt64 *) (&(mOutput.usbIsocFrames[0].frTimeStamp)) = 0xFFFFFFFFFFFFFFFFull;
    
	/*if (NULL != mOutput.associatedPipe) {
     nextSynchReadFrame = mOutput.usbFrameToQueueAt;
     debugIOLogC("read from associated output pipe");
     resultCode = mOutput.associatedPipe->Read(neededSampleRateDescriptor, nextSynchReadFrame, 1,
     &(mSampleRateFrame), &sampleRateCompletion);
     }*/
    
	for (UInt32 frameListNum = mOutput.currentFrameList; frameListNum < mOutput.numUSBFrameListsToQueue; ++frameListNum)  {
		debugIOLogC("write frame list %d",frameListNum);
		writeFrameList(frameListNum);
	}
#endif
	//don't know if this is useful or not
	//setClockIsStable(FALSE);
    
    
	// Spawn thread to take first time stamp
	startTimer->enable();
	startTimer->setTimeoutMS(kMinimumInterval);		// will call waitForFirstUSBFrameCompletion
    
    usbStreamRunning = TRUE;
    resultCode = kIOReturnSuccess;
    
Exit:
	if (kIOReturnSuccess != resultCode) {
		RELEASEOBJ(mInput.pipe);
		RELEASEOBJ(mOutput.pipe);
		RELEASEOBJ(mInput.associatedPipe);
		RELEASEOBJ(mOutput.associatedPipe);
	}
	
    return resultCode;
}

IOReturn EMUUSBAudioEngine::stopUSBStream () {
	debugIOLog ("+EMUUSBAudioEngine[%p]::stopUSBStream ()", this);
	usbStreamRunning = FALSE;
	shouldStop = shouldStop + (0 == shouldStop);
	if (NULL != mOutput.pipe) {
		if (FALSE == terminatingDriver)
			mOutput.pipe->SetPipePolicy (0, 0);// don't call USB to avoid deadlock
		
		// Have to close the current pipe so we can open a new one because changing the alternate interface will tear down the current pipe
		RELEASEOBJ(mOutput.pipe);
	}
	RELEASEOBJ(mOutput.associatedPipe);
	if (NULL != mInput.pipe) {
		if (FALSE == terminatingDriver)
			mInput.pipe->SetPipePolicy (0, 0);// don't call USB to avoid deadlock
		
		// Have to close the current pipe so we can open a new one because changing the alternate interface will tear down the current pipe
		RELEASEOBJ(mInput.pipe);
	}
	RELEASEOBJ(mInput.associatedPipe);
	
	if (FALSE == terminatingDriver) {
		// Don't call USB if we are being terminated because we could deadlock their workloop.
		if (NULL != mInput.streamInterface) // if we don't have an interface, message() got called and we are being terminated
			mInput.streamInterface->SetAlternateInterface (this, kRootAlternateSetting);
		if (NULL != mOutput.streamInterface) // if we don't have an interface, message() got called and we are being terminated
			mOutput.streamInterface->SetAlternateInterface (this, kRootAlternateSetting);
        
	}
    
	usbStreamRunning = FALSE;
	debugIOLog ("-EMUUSBAudioEngine[%p]::stopUSBStream ()", this);
	return kIOReturnSuccess;
}

IOReturn EMUUSBAudioEngine::getAnchor(UInt64* frame, AbsoluteTime*	time) {
	UInt64		theFrame = 0ull;
	IOReturn	result = kIOReturnError;// initialized to error
	AbsoluteTime	theTime;
	UInt32		attempts = kMaxAttempts;
    // Wouter: why is this done so complex? What makes this different from
    // just *frame = usbAudioDevice->mNewReferenceUSBFrame ?
	if (usbAudioDevice) {
		while(attempts && theFrame != usbAudioDevice->mNewReferenceUSBFrame) {
			theFrame = usbAudioDevice->mNewReferenceUSBFrame;
			theTime = usbAudioDevice->mNewReferenceWallTime;
			--attempts;
		}
		if (theFrame == usbAudioDevice->mNewReferenceUSBFrame) {
			result = kIOReturnSuccess;
			*frame = theFrame;
			*time = theTime;
		}
		debugIOLogT("getAnchor: frame %llu time %llu",theFrame,theTime);
	}
	return result;
}

void EMUUSBAudioEngine::waitForFirstUSBFrameCompletion (OSObject * owner, IOTimerEventSource * sender) {

    // Why do we need this? Can we just start reading right away and get rid of this whole test with timers?
    // Is this to avoid a hang-up and have some time out mechanism? Does USB not have any time-out options?
    
    // we're inside timer call. Maybe that's why we need to pass 'owner'?
	EMUUSBAudioEngine *				usbAudioEngineObject  = OSDynamicCast (EMUUSBAudioEngine, owner);
    //	AbsoluteTime						timeOffset;
	static UInt32					timeout = 60;
    
    
	FailIf ((NULL == usbAudioEngineObject) || (usbAudioEngineObject->isInactive()) || (0 != usbAudioEngineObject->shouldStop), Exit);
    

    
    
	if (0 == timeout || ( * (UInt64 *) ( & (usbAudioEngineObject->mInput.usbIsocFrames[0].frTimeStamp)) & 0xFFFFFFFF) != 0xFFFFFFFF) {
		AbsoluteTime	timeOffset, uptime;
		AbsoluteTime	systemTime;
		nanoseconds_to_absolutetime (kMinimumFrameOffset * kWallTimeConstant,EmuAbsoluteTimePtr(&timeOffset));
		clock_get_uptime(EmuAbsoluteTimePtr(&uptime));
		SUB_ABSOLUTETIME(&uptime, &timeOffset);
        // Init the time stamping in the audio engine.
        // first arg is true if the loop count should be incremented.
        // the 2nd arg is the AbsoluteTime.
        
        // HACK we now use previousfrTimesStamp.
		//usbAudioEngineObject->takeTimeStamp(FALSE, &uptime);
		clock_get_uptime(EmuAbsoluteTimePtr(&systemTime));
		debugIOLogT("Audio running. first time stamp %llu at systemTime %llu", uptime, systemTime);
        
		usbAudioEngineObject->startingEngine = FALSE;			// It's started now.
		usbAudioEngineObject->startTimer->cancelTimeout();
		usbAudioEngineObject->startTimer->disable ();
	} else {
		// Requeue timer to check for the first frame completing
		usbAudioEngineObject->startTimer->setTimeoutUS (50);		// will call us back in 50 micoseconds
		--timeout;
        debugIOLogT ("AUDIO NOT RUNNING, time-out of get timer (%ld)", timeout);
	}
    
Exit:
	return;
}

bool EMUUSBAudioEngine::willTerminate (IOService * provider, IOOptionBits options) {
    
	if (mInput.streamInterface == provider) {
		terminatingDriver = TRUE;
		if (FALSE == usbStreamRunning) {
			// Close our stream interface and go away because we're not running.
			mInput.streamInterface->close (this);
			mInput.streamInterface = NULL;
		} else {
			// Have the write completion routine clean everything up because we are running.
			if (0 == shouldStop) {
				++shouldStop;
			}
		}
	} else if (mOutput.streamInterface == provider) {
		terminatingDriver = TRUE;
		if (FALSE == usbStreamRunning) {
			// Close our stream interface and go away because we're not running.
			mOutput.streamInterface->close (this);
			mOutput.streamInterface = NULL;
		} else {
			// Have the write completion routine clean everything up because we are running.
			if (0 == shouldStop) {
				++shouldStop;
			}
		}
	}
    
	debugIOLog ("-EMUUSBAudioEngine[%p]::willTerminate", this);
    
	return super::willTerminate (provider, options);
}

IOReturn EMUUSBAudioEngine::writeFrameList (UInt32 frameListNum) {
	IOReturn	result = kIOReturnError;
    if (mOutput.pipe) {
		result = PrepareWriteFrameList (frameListNum);
		FailIf (kIOReturnSuccess != result, Exit);
        
		if (needTimeStamps) {
			result = mOutput.pipe->Write (theWrapRangeDescriptor, mOutput.usbFrameToQueueAt, mOutput.numUSBFramesPerList, &mOutput.usbIsocFrames[frameListNum * mOutput.numUSBFramesPerList], &mOutput.usbCompletion[frameListNum], 1);//mPollInterval);//1);
			needTimeStamps = FALSE;
		} else {
			result = mOutput.pipe->Write(mOutput.bufferDescriptors[frameListNum], mOutput.usbFrameToQueueAt, mOutput.numUSBFramesPerList,
                                         &mOutput.usbIsocFrames[frameListNum * mOutput.numUSBFramesPerList], &mOutput.usbCompletion[frameListNum], (TRUE == startingEngine));//mPollInterval);//(TRUE ==startingEngine) );
		}
		// keep track of this frame number for time stamping
		if (NULL != mOutput.frameQueuedForList)
			mOutput.frameQueuedForList[frameListNum] = mOutput.usbFrameToQueueAt;
        
		mOutput.usbFrameToQueueAt += mOutput.numUSBTimeFrames;
		
		FailIf (result != kIOReturnSuccess, Exit);
	}
Exit:
	return result;
}

void EMUUSBAudioEngine::writeHandler (void * object, void * parameter, IOReturn result, IOUSBLowLatencyIsocFrame * pFrames) {
    
	EMUUSBAudioEngine *	self = (EMUUSBAudioEngine*) object;
	if (!self->inWriteCompletion) {
		self->inWriteCompletion = TRUE;
        
		AbsoluteTime		time;
		UInt64              curUSBFrameNumber;
        
        
		FailIf (NULL == self->mOutput.streamInterface, Exit);
        
		curUSBFrameNumber = self->mBus->GetFrameNumber();
		if ((SInt64)(self->mOutput.usbFrameToQueueAt - curUSBFrameNumber) > (SInt32)(self->mOutput.numUSBTimeFrames * (self->mOutput.numUSBFrameListsToQueue / 2) + 1)) {
			// The frame list that this would have queued has already been queued by clipOutputSamples
			debugIOLogC("In writehandler Exit");
			goto Exit;
		}
        
        //		IOLockLock(self->mWriteLock);
		
		if (kIOReturnSuccess != result && kIOReturnAborted != result) {
			// skip ahead and see if that helps (say what? [AC])
			debugIOLog2("******** bad result system %d sub %d err %d",err_get_system(result),err_get_sub(result),err_get_code(result));
			if (self->mOutput.usbFrameToQueueAt <= curUSBFrameNumber) {
				self->mOutput.usbFrameToQueueAt = curUSBFrameNumber + self->mOutput.frameOffset;//kMinimumFrameOffset;
			}
		}
        
		//debugIOLogW("writehandler: frame %d, parameter %u",curUSBFrameNumber,(UInt32) parameter);
        
		if (0 != parameter) {
			// Take a timestamp
			AbsoluteTime systemTime;
			unsigned long long	/*systemTime,*/ stampTime;
			UInt32	byteOffset = ((UInt64)parameter) & 0x00FF;
			UInt32	frameIndex = (UInt32) (((UInt64) parameter >>16) - 1);
			if (kUSBDeviceSpeedHigh == self->mHubSpeed) {// consider the high speed bus case first
				//UInt32	byteCount = self->mOutput.maxFrameSize;
				UInt32	byteCount = self->lastInputFrames * self->mOutput.multFactor;
				UInt32	preWrapBytes = byteCount - byteOffset;
				debugIOLog2("** writeHandler time stamp **");
				time = self->generateTimeStamp(frameIndex, preWrapBytes, byteCount);
				//UInt32 loopCount;
				//self->getLoopCountAndTimeStamp(&loopCount,&time);
			} else {// USB 1.1 nonsense
				AbsoluteTime	timeOffset;
				time = pFrames[frameIndex].frTimeStamp;
				UInt64	nanos = (byteOffset * 1000000)/ pFrames[frameIndex].frActCount;// compute fractional part of sample frame
				nanoseconds_to_absolutetime(nanos, EmuAbsoluteTimePtr(&timeOffset));
				SUB_ABSOLUTETIME(&time, &timeOffset);
			}
			clock_get_uptime(EmuAbsoluteTimePtr(&systemTime));
			stampTime = AbsoluteTime_to_scalar(&time);
			debugIOLogT("write frameIndex %ld stampTime %llu system time %llu \n", frameIndex, stampTime, systemTime);
			//self->takeTimeStamp(TRUE, &time);
			int delta = self->runningInputCount - self->runningOutputCount;
			int drift = self->lastDelta - delta;
			self->lastDelta = delta;
			debugIOLogC("running counts: input %u, output %u, delta %d, drift %d",(unsigned int)self->runningInputCount,(unsigned int)self->runningOutputCount,delta, drift);
            //  FIXME drift should be accounted for??? Is this the source of our clicks??
#if 0
			if (drift < 100) { // should never be that big
				self->AddToLastFrameSize(-drift);
			}
#endif
            
		}
		
		self->mOutput.currentFrameList = (self->mOutput.currentFrameList + 1) * (self->mOutput.currentFrameList < (self->mOutput.numUSBFrameLists - 1));
        
		if (!self->shouldStop) {
			UInt32	frameListToWrite = (self->mOutput.currentFrameList - 1) + self->mOutput.numUSBFrameListsToQueue;
			frameListToWrite -= self->mOutput.numUSBFrameLists * (frameListToWrite >= self->mOutput.numUSBFrameLists);
			self->writeFrameList (frameListToWrite);
		} else {
			++self->shouldStop;
			debugIOLogC("writeHandler: should stop");
			if (self->shouldStop == (self->mOutput.numUSBFrameListsToQueue + 1) && TRUE == self->terminatingDriver) {
				self->mOutput.streamInterface->close (self);
				self->mOutput.streamInterface = NULL;
			}
		}
        //		IOLockUnlock(self->mWriteLock);
	} else {
		debugIOLogC("*** Already in write completion!");
		return;
	}
Exit:
	self->inWriteCompletion = FALSE;
	return;
}



#if UHCISUPPORT// still incomplete
void EMUUSBAudioEngine::writeHandlerUHCI(void* object, void* param, IOReturn result, IOUSBLowLatencyIsocFrame* pFrames) {
	EMUUSBAudioEngine*	engine = (EMUUSBAudioEngine*) object;
	if (!engine->inWriteCompletion && (NULL != engine->streamInterface)) {
		UInt64	curFrameNumber = mBus->GetFrameNumber();
		SInt64	diff = (SInt64) (engine->usbFrameToQueueAt - curFrameNumber);
		SInt32	expectedFrames = (SInt32) engine->numUSBFramesPerList * (engine->numUSBFrameListsToQueue / 2) - 1;
		engine->inWriteCompletion = TRUE;
		if ((kIOReturnAborted != result) && (kIOReturnSuccess != result)) {
			if (diff <= 0)
				engine->usbFrameToQueueAt = curFrameNumber + kMinimumFrameOffset;
		}
		engine->currentFrameList =  (engine->currentFrameList + 1) * (engine->currentFrameList != (engine->numUSBFrameLists - 1));
		if (!engine->shouldStop) {
			UInt32	frameListToWrite = (engine->currentFrameList - 1) + engine->numUSBFrameListsToQueue;
			frameListToWrite -= engine->numUSBFrameLists * (frameListToWrite >= engine->numUSBFrameLists);
			engine->writeFrameList(frameListToWrite);
		}
	}
	engine->inWriteCompletion = FALSE;
}
#endif



IOReturn EMUUSBAudioEngine::hardwareSampleRateChanged(const IOAudioSampleRate *newSampleRate) {
	IOReturn	result = kIOReturnError;
    if (usbAudioDevice && ((newSampleRate->whole != sampleRate.whole) || (newSampleRate->fraction != sampleRate.fraction))) {
        bool	engineWasRunning = (kIOAudioEngineRunning == state);
		
        if (engineWasRunning) {
            pauseAudioEngine();
		}
        beginConfigurationChange();
        setSampleRate(newSampleRate);
		
        
		completeConfigurationChange();
        if (!configurationChangeInProgress)
            sendNotification(kIOAudioEngineChangeNotification);
        
        if (engineWasRunning)
            resumeAudioEngine();
		result = kIOReturnSuccess;
    }
Exit:
    return result;
}

IOReturn EMUUSBAudioEngine::hardwareSampleRateChangedAux(const IOAudioSampleRate *newSampleRate, StreamInfo &info) {
	IOReturn	result = kIOReturnError;
	const IOAudioStreamFormat*			theFormat =info.audioStream->getFormat();
	EMUUSBAudioConfigObject*	usbAudio = usbAudioDevice->GetUSBAudioConfigObject();
	bool	needToChangeChannels = false;
	if (usbAudio && (info.multFactor != 0)) {
		// try to locate the interface first
		info.numChannels = theFormat->fNumChannels;
		mChannelWidth = theFormat->fBitWidth;
		info.multFactor = info.numChannels * (mChannelWidth / 8);
		UInt8 newAltSettingID = usbAudio->FindAltInterfaceWithSettings (info.interfaceNumber, info.numChannels, mChannelWidth, newSampleRate->whole);
		UInt32 pollInterval = (UInt32) (1 << ((UInt32) usbAudio->GetEndpointPollInterval(info.interfaceNumber, newAltSettingID, info.streamDirection) - 1));
		if ((255 == newAltSettingID) || (1 != pollInterval) || (8 != pollInterval)) {
			info.numChannels = kChannelCount_STEREO;
			newAltSettingID = usbAudio->FindAltInterfaceWithSettings (info.interfaceNumber, info.numChannels, mChannelWidth, newSampleRate->whole);
			info.multFactor = kChannelCount_STEREO * 3;
			needToChangeChannels = true;
		}
		FailIf(255 == newAltSettingID, Exit);
		info.alternateSettingID = newAltSettingID;
		
		UInt16		additionalSampleFrameFreq = 0;
		UInt16		averageFrameSamples = 0;
		UInt16		averageFrameSize = 0;
		UInt16		alternateFrameSize = 0;
		
        debugIOLog ("hardwareSampleRateChanged averageSampleRate = %d", averageSampleRate);
		CalculateSamplesPerFrame (sampleRate.whole, &averageFrameSamples, &additionalSampleFrameFreq);
		//			debugIOLog ("averageFrameSamples = %d, additionalSampleFrameFreq = %d", averageFrameSamples, additionalSampleFrameFreq);
		averageFrameSize = averageFrameSamples * info.multFactor;
		alternateFrameSize = (averageFrameSamples + 1) * info.multFactor;
		
		info.maxFrameSize = alternateFrameSize;
		//			debugIOLog ("hardwareSampleRateChanged averageFrameSize = %d, alternateFrameSize = %d multFactor %d", averageFrameSize, alternateFrameSize, multFactor);
		
		FailIf(kIOReturnSuccess != initBuffers(), Exit);
		if (needToChangeChannels) {
			IOAudioStreamFormat			streamFormat;
			
			streamFormat.fNumChannels = usbAudio->GetNumChannels(info.interfaceNumber, info.alternateSettingID);
			streamFormat.fBitDepth = usbAudio->GetSampleSize (info.interfaceNumber, info.alternateSettingID);
			streamFormat.fBitWidth = usbAudio->GetSubframeSize (info.interfaceNumber, info.alternateSettingID) * 8;
			streamFormat.fAlignment = kIOAudioStreamAlignmentLowByte;
			streamFormat.fByteOrder = kIOAudioStreamByteOrderLittleEndian;
			streamFormat.fDriverTag = (info.interfaceNumber << 16) | info.alternateSettingID;
			switch (usbAudio->GetFormat (info.interfaceNumber, info.alternateSettingID)) {
				case PCM:
					streamFormat.fSampleFormat = kIOAudioStreamSampleFormatLinearPCM;
					streamFormat.fNumericRepresentation = kIOAudioStreamNumericRepresentationSignedInt;
					streamFormat.fIsMixable = TRUE;
					break;
				case AC3:	// just starting to stub something in for AC-3 support
					streamFormat.fSampleFormat = kIOAudioStreamSampleFormatAC3;
					streamFormat.fNumericRepresentation = kIOAudioStreamNumericRepresentationSignedInt;
					streamFormat.fIsMixable = FALSE;
					streamFormat.fNumChannels = 6;
					streamFormat.fBitDepth = 16;
					streamFormat.fBitWidth = 16;
					streamFormat.fByteOrder = kIOAudioStreamByteOrderBigEndian;
					break;
				case IEC1937_AC3:
					streamFormat.fSampleFormat = kIOAudioStreamSampleFormat1937AC3;
					streamFormat.fNumericRepresentation = kIOAudioStreamNumericRepresentationSignedInt;
					streamFormat.fIsMixable = FALSE;
					break;
				default:
					FailMessage ("!!!!interface doesn't support a format that we can deal with!!!!\n", Exit);
					break;
			}
			info.audioStream->setFormat(&streamFormat);
			// no controls need changing
#if 0
			removeAllDefaultAudioControls();
			usbAudioDevice->createControlsForInterface(this, info.interfaceNumber, info.alternateSettingID);
#endif
		}
	}
Exit:
    return result;
	
}

IOReturn EMUUSBAudioEngine::initBuffers() {
	IOReturn						result = kIOReturnError;
	if (usbAudioDevice) {
		//EMUUSBAudioConfigObject*		usbAudio = usbAudioDevice->GetUSBAudioConfigObject();
		//UInt32							pollInterval = (UInt32) usbAudio->GetEndpointPollInterval(ourInterfaceNumber, alternateSettingID, mDirection);
        //	maxFrameSize = usbAudio->GetEndpointMaxPacketSize(ourInterfaceNumber, alternateSettingID, address);
		//mPollInterval = 1 << (pollInterval -1);
		debugIOLogC("initBuffers");
		//, frameSize %d direction %d mPollInterval %d", frameSize, mDirection, mPollInterval);
		// clear up any existing buffers
		UInt32 inputSize = mInput.maxFrameSize;
		UInt32 outputSize = mOutput.maxFrameSize;
		UInt32	samplesPerFrame = inputSize / mInput.multFactor;
		debugIOLogC("inputSize= %d multFactor= %d", inputSize, mInput.multFactor);
		debugIOLogC("outputSize= %d multFactor= %d", outputSize, mOutput.multFactor);
		
        //	FailIf(samplesPerFrame != outputSize / mOutput.multFactor, Exit); - JH allocated size may be off 1 such as with AC3
		
		// this is total guesswork (AC)
        
        //	UInt32	numSamplesInBuffer = samplesPerFrame * 256;//
		//UInt32 numSamplesInBuffer = samplesPerFrame * 16;
		UInt32	numSamplesInBuffer = PAGE_SIZE * (2 + (sampleRate.whole > 48000) + (sampleRate.whole > 96000));
        
        // HACK notice that PAGE_SIZE is a hardware specific value for 32 bit intel machines.
        // Why is this not related to the number of channels?
        // I propose this should be computed more directly based on the framelist size.


		
		mInput.bufferSize = numSamplesInBuffer * mInput.multFactor;
		mOutput.bufferSize = numSamplesInBuffer * mOutput.multFactor;
		
		// setup the input buffer
		if (NULL != mInput.bufferMemoryDescriptor) {
			mInput.audioStream->setSampleBuffer (NULL, 0);
			setNumSampleFramesPerBuffer (0);
			debugIOLogC("releasing the input sampleBufferMemory descriptor");
			mInput.bufferMemoryDescriptor->complete();
			mInput.bufferMemoryDescriptor->release();
			mInput.bufferMemoryDescriptor = NULL;
			mInput.bufferPtr = NULL;// reset the sample buffer ptr
		}
		readUSBFrameListSize = inputSize * mInput.numUSBFramesPerList;
		debugIOLogC("new bufferSize = %d numSamplesInBuffer = %d\n", mInput.bufferSize, numSamplesInBuffer );
		if (mInput.usbBufferDescriptor) {
			debugIOLogC("disposing the mUSBBufferDescriptor input");
			mInput.usbBufferDescriptor->complete();
			mInput.usbBufferDescriptor->release();
			mInput.usbBufferDescriptor = NULL;
			readBuffer = NULL;
		}
		// read buffer section
		// following is the actual buffer that stuff gets read into
		debugIOLogC("initBuffers readUSBFrameListSize %d numUSBFrameLists %d", readUSBFrameListSize, mInput.numUSBFrameLists);
#ifdef CONTIGUOUS
		mInput.usbBufferDescriptor = IOBufferMemoryDescriptor::withOptions(kIOMemoryPhysicallyContiguous| kIODirectionInOut, mInput.numUSBFrameLists * readUSBFrameListSize, page_size);
#else
		mInput.usbBufferDescriptor = IOBufferMemoryDescriptor::withOptions(kIODirectionInOut, mInput.numUSBFrameLists * readUSBFrameListSize, page_size);
#endif
		FailIf (NULL == mInput.usbBufferDescriptor, Exit);
		mInput.usbBufferDescriptor->prepare();
        // Wouter: this gets direct ptr to the USB buffer memory
		readBuffer = mInput.usbBufferDescriptor->getBytesNoCopy();// get a valid ptr or NULL
		FailIf (NULL == readBuffer, Exit);
		// setup the sub ranges
		FailIf(NULL == mInput.bufferDescriptors, Exit);
		for (UInt32 i = 0; i < mInput.numUSBFrameLists; ++i) {
			if (mInput.bufferDescriptors[i]) {
                //				debugIOLogC("disposing input soundBufferDescriptors[%d]", i);
				mInput.bufferDescriptors[i]->complete();
				mInput.bufferDescriptors[i]->release();
				mInput.bufferDescriptors[i] = NULL;
			}
			mInput.bufferDescriptors[i] = OSTypeAlloc(IOSubMemoryDescriptor);
			bool initResult = mInput.bufferDescriptors[i]->initSubRange(mInput.usbBufferDescriptor, i * readUSBFrameListSize, readUSBFrameListSize, kIODirectionInOut);
            //			debugIOLogC("initSubRange soundBufferDescriptors[%d] %d", i, initResult);
			FailIf (NULL == mInput.bufferDescriptors[i]|| !initResult, Exit);
			result = mInput.bufferDescriptors[i]->prepare();
			FailIf(kIOReturnSuccess != result, Exit);
		}
		// setup the input sample buffer memory descriptor and pointer
		mInput.bufferMemoryDescriptor = IOBufferMemoryDescriptor::withOptions (kIOMemoryPhysicallyContiguous|kIODirectionInOut, mInput.bufferSize, page_size);
		FailIf (NULL == mInput.bufferMemoryDescriptor, Exit);
		mInput.bufferMemoryDescriptor->prepare();
		mInput.bufferPtr = mInput.bufferMemoryDescriptor->getBytesNoCopy();// get a valid pointer or NULL
		FailIf(NULL == mInput.bufferPtr, Exit);
		
        // HACK this is insufficient, we are doing block transfer with much larger blocks
        // note, samplesPerFrame is really fields per frame
        //UInt32	offsetToSet = samplesPerFrame;
        // quarter a framelist is fine but gives 16ms latency.
        // UInt32 offsetToSet = samplesPerFrame * mInput.numUSBFramesPerList / 4;

        // actual jitter on the USB input is 0.01ms. But we have high latency pulses that have 1ms.
        // The clock lock filter spreads out the effect of the pulse over a long time,
        // at the cost of increase in latency to 3ms or so. We need to measure this all exactly.
        UInt32 offsetToSet = sampleRate.whole / 200; // * 0.005 ms
		
		// use samplesPerFrame as our "safety offset" unless explicitly set as a driver property
        // You can set it in the properties .list file in EMUUSBAudioControl04:        SafetyOffset        Number 192
        // BUT, this is not recommended as this safety offset really depends on the settings we make here.
        // maybe we could define something different there. Not now.
		OSNumber *safetyOffsetObj = OSDynamicCast(OSNumber,usbAudioDevice->getProperty("SafetyOffset"));
		if (safetyOffsetObj) {
            debugIOLogT("using externally requested safety offset %d",safetyOffsetObj->unsigned32BitValue());
			setSampleOffset(safetyOffsetObj->unsigned32BitValue());
		} else {
            debugIOLogT("using our own estimated offset %d",offsetToSet);
            // This is the offset for the playback head: min distance the IOAudioEngine keeps from our write head.
            setSampleOffset(offsetToSet);
		}
        // idem for the playback head.
		//setInputSampleLatency(2*samplesPerFrame);
        // HACK. This is the latency from read till end user. Not clear if IOEngine uses it.
        setInputSampleLatency(2* mInput.numUSBFramesPerList * mInput.maxFrameSize / mInput.multFactor);
		mInput.audioStream->setSampleBuffer(mInput.bufferPtr, mInput.bufferSize);
		
		//now the output buffer
		if (mOutput.usbBufferDescriptor) {
			debugIOLogC("disposing the output mUSBBufferDescriptor");
			mOutput.audioStream->setSampleBuffer(NULL, 0);
			setNumSampleFramesPerBuffer(0);
			mOutput.usbBufferDescriptor->complete();
			mOutput.usbBufferDescriptor->release();
			mOutput.usbBufferDescriptor = NULL;
		}
		debugIOLogC("In the out path, making new buffer with size of %d numSamplesInBuffer %d", mOutput.bufferSize, numSamplesInBuffer);
		mOutput.usbBufferDescriptor = IOBufferMemoryDescriptor::withOptions (kIODirectionInOut, mOutput.bufferSize, page_size);
		FailIf (NULL == mOutput.usbBufferDescriptor, Exit);
		mOutput.usbBufferDescriptor->prepare();
		FailIf(NULL == mOutput.bufferDescriptors, Exit);
		for (UInt32 i = 0; i < mOutput.numUSBFrameLists; ++i) {
			if (mOutput.bufferDescriptors[i]) {
                //				debugIOLogC("disposing output soundBufferDescriptors[%d]", i);
				mOutput.bufferDescriptors[i]->complete();
				mOutput.bufferDescriptors[i]->release();
				mOutput.bufferDescriptors[i] = NULL;
			}
			mOutput.bufferDescriptors[i] = OSTypeAlloc (IOSubMemoryDescriptor);
			bool initResult = mOutput.bufferDescriptors[i]->initSubRange (mOutput.usbBufferDescriptor, 0, mOutput.bufferSize, kIODirectionInOut);
			FailIf (NULL == mOutput.bufferDescriptors[i]|| !initResult, Exit);
			result = mOutput.bufferDescriptors[i]->prepare();
			FailIf (kIOReturnSuccess != result, Exit);
		}
		mOutput.bufferPtr = mOutput.usbBufferDescriptor->getBytesNoCopy();
		FailIf (NULL == mOutput.bufferPtr, Exit);
		offsetToSet = samplesPerFrame; // appears to be the best possible
		setOutputSampleLatency(2*samplesPerFrame);
		mOutput.audioStream->setSampleBuffer(mOutput.bufferPtr, mOutput.bufferSize);
        
		setNumSampleFramesPerBuffer(numSamplesInBuffer);
        
		debugIOLogC("completed initBuffers");
		result = kIOReturnSuccess;
	}
Exit:
	return result;
}


#if PREPINPUT // routines to clear out the first kNumFramesToClear frames in the input pipe - currently disabled for now.
void EMUUSBAudioEngine::prepInputPipe() {
	// mInput.pipe will have to exist by this point in time
	mClearInputCompletion->target = (void*) this;
	mClearInputCompletion->action = prepInputHandler;
	mClearInputCompletion->parameter = NULL;
	
	for (UInt32 i = 0; i < kNumFramesToClear; i += 4) {
		mClearIsocFrames[i].frStatus = mClearIsocFrames[i+1].frStatus = mClearIsocFrames[i+2].frStatus = mClearIsocFrames[i+3].frStatus = -1;
		mClearIsocFrames[i].frActCount = mClearIsocFrames[i+1].frActCount = mClearIsocFrames[i+2].frActCount = mClearIsocFrames[i+3].frActCount = 0;
		mClearIsocFrames[i].frReqCount = mClearIsocFrames[i+1].frReqCount = mClearIsocFrames[i+2].frReqCount = mClearIsocFrames[i+3].frReqCount = maxFrameSize;
	}
	IOReturn	result = kIOReturnError;
	result = mInput.pipemOutput.usbBufferDescriptor->Read(soundBufferDescriptors[0], usbFrameToQueueAt, kNumFramesToClear, mClearIsocFrames, /*mClearInputCompletion*/ NULL, 0);
	if (kIOReturnSuccess == result) {
		debugIOLogC("prepInputPipe ok");
		usbFrameToQueueAt += kNumFramesToClear;
	}
	debugIOLogC("prepInputPipe result %x", result);
}
// currently debating whether or not to perform Reads synchronously or asynchronously
void EMUUSBAudioEngine::prepInputHandler(void* object, void* frameListIndex, IOReturn result, IOUSBLowLatencyIsocFrame* pFrames) {
	EMUUSBAudioEngine*	engine = (EMUUSBAudioEngine*) object;
	debugIOLogC("prepInputHandler result %x", result);
}
#endif

//<AC mod>
void EMUUSBAudioEngine::findAudioStreamInterfaces(IOUSBInterface *pAudioControlIfc)
{
    IOUSBDevice *pDevice = pAudioControlIfc->GetDevice();
    
    IOUSBFindInterfaceRequest req;
    
    req.bInterfaceClass = 255; // 255 == vendor-specific interface class
    req.bInterfaceSubClass = 2; // 2 == AudioStream Interface
    req.bInterfaceProtocol = 0;
    req.bAlternateSetting = 0;
    
    IOUSBInterface *pInterface = NULL;
    
    // Create array from stream interfaces, we're expecting 2 of them.
    mStreamInterfaces = OSArray::withCapacity(2);
    
    for (pInterface = pDevice->FindNextInterface(pInterface, &req);
         pInterface;
         pInterface = pDevice->FindNextInterface(pInterface, &req)) {	
        mStreamInterfaces->setObject(pInterface);
    }
}
//</AC mod>


void EMUUSBAudioEngine::setupChannelNames() {
	//for now we'll be sneaky and simply copy the properties from the device (they are definied in Info.plist for the device class)
	OSObject *obj = usbAudioDevice->getProperty("IOAudioEngineChannelNames");
	OSDictionary *dict = OSDynamicCast(OSDictionary,obj);
	if (dict) {
		debugIOLogC("Found IOAudioEngineChannelNames");
		setProperty("IOAudioEngineChannelNames",dict);
	}
	obj = usbAudioDevice->getProperty("IOAudioEngineChannelCategoryNames");
	dict = OSDynamicCast(OSDictionary,obj);
	if (dict) {
		debugIOLogC("Found IOAudioEngineChannelCategoryNames");
		setProperty("IOAudioEngineChannelCategoryNames",dict);
	}	
	obj = usbAudioDevice->getProperty("IOAudioEngineChannelNumberNames");
	dict = OSDynamicCast(OSDictionary,obj);
	if (dict) {
		debugIOLogC("Found IOAudioEngineChannelNumberNames");
		setProperty("IOAudioEngineChannelNumberNames",dict);
	}	
}

void EMUUSBAudioEngine::PushFrameSize(UInt32 frameSize) {
	frameSizeQueue[frameSizeQueueBack++] = frameSize;
	frameSizeQueueBack %= FRAMESIZE_QUEUE_SIZE;	
	//debugIOLogC("queued framesize %d, ptr %d",frameSize,frameSizeQueueBack);
}

UInt32 EMUUSBAudioEngine::PopFrameSize() {
	if (frameSizeQueueFront == frameSizeQueueBack) {
		debugIOLogC("empty framesize queue");
		return 0;
	}
	UInt32 result = frameSizeQueue[frameSizeQueueFront++];
	frameSizeQueueFront %= FRAMESIZE_QUEUE_SIZE;
	//debugIOLogC("dequeued framesize %d, read %d",result,frameSizeQueueFront);
	return result;
}

void EMUUSBAudioEngine::AddToLastFrameSize(SInt32 toAdd) {
    frameSizeQueue[frameSizeQueueFront] += toAdd;
}

void EMUUSBAudioEngine::ClearFrameSizes() {
	frameSizeQueueFront = frameSizeQueueBack = 0;
}
