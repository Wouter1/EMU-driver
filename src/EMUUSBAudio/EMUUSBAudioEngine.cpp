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


#define super IOAudioEngine

const SInt32 kSoftVolumeLookupRange = 7200;
const SInt32 kSoftDBRange = -72;

OSDefineMetaClassAndStructors(EMUUSBAudioEngine, IOAudioEngine)

#pragma mark -IOKit Routines-

void EMUUSBAudioEngine::free () {
	debugIOLog ("+EMUUSBAudioEngine[%p]::free ()", this);
    
//	if (NULL != startTimer) {
//		startTimer->cancelTimeout ();
//		startTimer->release ();
//		startTimer = NULL;
//	}
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
    
    frameSizeQueue.free();
//	if (NULL != mOutput.frameQueuedForList) {
//		delete [] mOutput.frameQueuedForList;
//		mOutput.frameQueuedForList = NULL;
//	}
    if (NULL != buf) {
        IOFree(buf, usbInputStream.bufferSize);
        buf=NULL;
    }
    
	if (neededSampleRateDescriptor) {
		neededSampleRateDescriptor->complete();
		neededSampleRateDescriptor->release();
		neededSampleRateDescriptor = NULL;
	}
	RELEASEOBJ(usbInputStream.pipe);
	RELEASEOBJ(mOutput.pipe);
	RELEASEOBJ(usbInputStream.associatedPipe);
	RELEASEOBJ(mOutput.associatedPipe);
	
	if (aveSampleRateBuf) {
		IOFree (aveSampleRateBuf, sizeof(UInt32));// expt try aveSampleBuf extended to account for larger read buf
		aveSampleRateBuf = NULL;
	}
    
	if (mSyncer) {
		mSyncer->release ();
		mSyncer = NULL;
	}
	
	if (usbInputStream.usbBufferDescriptor) {
		usbInputStream.usbBufferDescriptor->complete();
		usbInputStream.usbBufferDescriptor->release();
		usbInputStream.usbBufferDescriptor= NULL;
	}
	if (mOutput.usbBufferDescriptor) {
		mOutput.usbBufferDescriptor->complete();
		mOutput.usbBufferDescriptor->release();
		mOutput.usbBufferDescriptor= NULL;
	}
	usbInputStream.readBuffer = NULL;

    mOutput.free();
    
	if (mOutput.bufferMemoryDescriptor) {
		mOutput.bufferMemoryDescriptor->complete();
		mOutput.bufferMemoryDescriptor->release();
		mOutput.bufferMemoryDescriptor = NULL;
	}
	if (usbInputStream.bufferMemoryDescriptor) {
		usbInputStream.bufferMemoryDescriptor->complete();
		usbInputStream.bufferMemoryDescriptor->release();
		usbInputStream.bufferMemoryDescriptor = NULL;
	}
	usbInputStream.bufferPtr = NULL;
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
	if (NULL != usbInputStream.bufferDescriptors) {
		for (UInt32 i = 0; i < usbInputStream.numUSBFrameLists; ++i) {
			if (NULL != usbInputStream.bufferDescriptors[i]) {
				usbInputStream.bufferDescriptors[i]->complete();
				usbInputStream.bufferDescriptors[i]->release();
				usbInputStream.bufferDescriptors[i] = NULL;
			}
		}
		IOFree (usbInputStream.bufferDescriptors, usbInputStream.numUSBFrameLists * sizeof (IOSubMemoryDescriptor *));
		usbInputStream.bufferDescriptors = NULL;
	}
	
	if (NULL != usbInputStream.usbIsocFrames) {
		IOFree (usbInputStream.usbIsocFrames, usbInputStream.numUSBFrameLists * usbInputStream.numUSBFramesPerList * sizeof (IOUSBLowLatencyIsocFrame));
		usbInputStream.usbIsocFrames = NULL;
	}
	if (NULL != mOutput.usbIsocFrames) {
		IOFree (mOutput.usbIsocFrames, mOutput.numUSBFrameLists * mOutput.numUSBFramesPerList * sizeof (IOUSBLowLatencyIsocFrame));
		mOutput.usbIsocFrames = NULL;
	}
	
	if (NULL != usbInputStream.usbCompletion) {
		IOFree (usbInputStream.usbCompletion, usbInputStream.numUSBFrameLists * sizeof (IOUSBLowLatencyIsocCompletion));
		usbInputStream.usbCompletion = NULL;
	}
	if (NULL != mOutput.usbCompletion) {
		IOFree (mOutput.usbCompletion, mOutput.numUSBFrameLists * sizeof (IOUSBLowLatencyIsocCompletion));
		mOutput.usbCompletion = NULL;
	}
    
	RELEASEOBJ(usbInputStream.audioStream);
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
	usbInputStream.readBuffer = usbInputStream.bufferPtr = mOutput.bufferPtr = NULL;// initialize both the read, input and output to NULL
	mSyncer = IOSyncer::create (FALSE);
	result = TRUE;
    mPlugin = NULL;
	usbInputStream.associatedPipe = mOutput.associatedPipe = NULL; // (AC mod)
	neededSampleRateDescriptor = NULL;
	usbInputStream.usbCompletion = mOutput.usbCompletion= NULL;
	usbInputStream.usbIsocFrames = mOutput.usbIsocFrames = NULL;
    
Exit:
	debugIOLogC("EMUUSBAudioEngine[%p]::init ()", this);
	return result;
}

bool EMUUSBAudioEngine::requestTerminate (IOService * provider, IOOptionBits options) {
	debugIOLog ("-EMUUSBAudioEngine[%p]::requestTerminate (%p, %x)", this, provider, options);
	return (usbAudioDevice == provider || usbInputStream.streamInterface == provider || mOutput.streamInterface == provider);
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
	if (usbInputStream.streamInterface && usbInputStream.streamInterface->isOpen()) {
		debugIOLogC("closing stream");
		usbInputStream.streamInterface->close (this);
		usbInputStream.streamInterface = NULL;
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
	IOAudioStream *audioStream = (ourInterfaceNumber == usbInputStream.interfaceNumber) ? usbInputStream.audioStream : mOutput.audioStream;
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
			//if ((1 == pollInterval) || (8 == pollInterval)) {// exclude all interfaces that use microframes for now
            if (pollInterval == 4) 
            {
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
                        // What??
//						if (kIOAudioStreamSampleFormatLinearPCM == streamFormat.fSampleFormat) {
//							streamFormat.fIsMixable = FALSE;
//							audioStream->addAvailableFormat (&streamFormat, &streamFormatExtension, &lowSampleRate, &lowSampleRate);
//							streamFormat.fIsMixable = TRUE;		// set it back to TRUE for next time through the loop
//						} 
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
	UInt32		subFrameDivisor = 1000 * 8 / mPollInterval;
	UInt32		divisor = inSampleRate % subFrameDivisor;
    
	*averageFrameSamples = inSampleRate / subFrameDivisor;
	*additionalSampleFrameFreq = 0;
	if (divisor) {
		*additionalSampleFrameFreq = subFrameDivisor / divisor;
    }
    
    debugIOLogC("averageFrameSamples=%d",*averageFrameSamples);
}


IOReturn EMUUSBAudioEngine::clipOutputSamples (const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream) {
    
	IOReturn			result = kIOReturnError;
    
    //	IOLockLock(mFormatLock);
    

	//SInt32 offsetFrames = mOutput.previouslyPreparedBufferOffset / mOutput.multFactor;
	debugIOLogW("clipOutputSamples firstSampleFrame=%u, numSampleFrames=%d, currentHead =%d ",firstSampleFrame,numSampleFrames,getCurrentSampleFrame(0));
    
	if (firstSampleFrame != nextExpectedOutputFrame) {
		debugIOLog("**** Output Hiccup!! firstSampleFrame=%d, nextExpectedOutputFrame=%d bufsize=%d",firstSampleFrame,nextExpectedOutputFrame,mOutput.bufferSize);
	}
    UInt32 samplesInBuffer = mOutput.bufferSize /mOutput.multFactor;
	nextExpectedOutputFrame = (firstSampleFrame + numSampleFrames) % samplesInBuffer;

    
	
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
	
	//debugIOLogW("clipOutputSamples: numSampleFrames = %d",numSampleFrames);
	if (TRUE == streamFormat->fIsMixable) {
		if (mPlugin) {
			mPlugin->pluginProcess ((Float32*)mixBuf + (firstSampleFrame * streamFormat->fNumChannels), numSampleFrames, streamFormat->fNumChannels);
        }
        
		result = clipEMUUSBAudioToOutputStream (mixBuf, sampleBuf, firstSampleFrame, numSampleFrames, streamFormat);
	} else {
		UInt32	offset = firstSampleFrame * mOutput.multFactor;
        
		memcpy ((UInt8 *)sampleBuf + offset, (UInt8 *)mixBuf, numSampleFrames * mOutput.multFactor);
		result = kIOReturnSuccess;
	}
    //debugIOLogC("-clipOutput %d to %d estcur= %d", firstSampleFrame,firstSampleFrame+numSampleFrames, getCurrentSampleFrame(0l));
    //	IOLockUnlock(mFormatLock);
	return result;
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



IOReturn EMUUSBAudioEngine::convertInputSamples (const void *sampleBufNull, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat,
                                                 IOAudioStream *audioStream) {
    // Since we don't tell IOAudioEngine about our sample buffer, we get null for sampleBufNull.
    
	IOReturn	result;
    
    // debugIOLogRD("+convertInputSamples firstSampleFrame=%u, numSampleFrames=%d byteorder=%d bitWidth=%d numchannels=%d latency= %d",firstSampleFrame,numSampleFrames,streamFormat->fByteOrder,streamFormat->fBitWidth,streamFormat->fNumChannels, usbInputRing.available());
    
    if (!usbInputStream.isRunning()) {
        return kIOReturnNotReady;
    }

    // #40 check if coreaudio keeps the specified offset
    //debugIOLogC("+convertInputSamples coreaudio distance %d", firstSampleFrame + numSampleFrames - getCurrentSampleFrame(0));
    
    
    usbInputStream.update();
    //debugIOLogRD("+convertInputSamples firstSampleFrame=%u, numSampleFrames=%d byteorder=%d bitWidth=%d numchannels=%d latency= %d",firstSampleFrame,numSampleFrames,streamFormat->fByteOrder,streamFormat->fBitWidth,streamFormat->fNumChannels, usbInputRing.available());

    //#48 check if estimated and actual buffer pos match
    //debugIOLogC("+convertInputSamples act est %d %d", (SInt32)usbInputRing.currentWritePosition()/usbInputStream.multFactor  , getCurrentSampleFrame(0));


    // at this point, usbInputRing should contain sufficient info to handle the request.
    // of course this all depends on proper timing of this call.
    // It's the caller that "ensures" this using
    // "sophisticated techniques and extremely accurate timing mechanisms".
    // I don't like this black box approach but we have to live with it.
    
    if (usbInputRing.seek(firstSampleFrame * usbInputStream.multFactor) == kIOReturnUnderrun)  {
        debugIOLog("EMUUSBAudioEngine::convertInputSamples READ HICKUP");
    }
    
    IOReturn res = usbInputRing.pop(buf, numSampleFrames * usbInputStream.multFactor);
    if (res != kIOReturnSuccess) {
        debugIOLog("EMUUSBAudioEngine::convertInputSamples err reading ring: %x",res);
        // Not sure what to do. For now, go on and feed the noise this will give.
    }
    
    result = convertFromEMUUSBAudioInputStreamNoWrap (buf, destBuf, 0, numSampleFrames, streamFormat);

	if (mPlugin) {
		mPlugin->pluginProcessInput ((float *)destBuf + (firstSampleFrame * streamFormat->fNumChannels), numSampleFrames, streamFormat->fNumChannels);
    }
    
    // software volume
	if(mInputVolume)
	{
		UInt32 usedNumberOfSamples = numSampleFrames * streamFormat->fNumChannels;
		
		if (mDidInputVolumeChange)
		{
			mDidInputVolumeChange = false;
            
			SmoothVolume(((Float32*)destBuf), mInputVolume->GetTargetVolume(),
                         mInputVolume->GetLastVolume(), 0, numSampleFrames,
                         usedNumberOfSamples, streamFormat->fNumChannels);
            
			mInputVolume->SetLastVolume(mInputVolume->GetTargetVolume());
		}
		else
		{
			Volume(((Float32*)destBuf), mInputVolume->GetTargetVolume(), 0, usedNumberOfSamples);
		}
	}
    debugIOLogRD("-convertInputSamples ");
    
	return result;
}



IOReturn EMUUSBAudioEngine::eraseOutputSamples(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream)
{
    // apparently we must implement but we leave it to the super class.
	super::eraseOutputSamples (mixBuf, sampleBuf, firstSampleFrame, numSampleFrames, streamFormat, audioStream);
    //debugIOLogC("-erase first= %d num= %d estcur= %d", firstSampleFrame,numSampleFrames, getCurrentSampleFrame(0));
	
	return kIOReturnSuccess;
}

UInt32 EMUUSBAudioEngine::getCurrentSampleFrame() {
    // compute the safe erase point for the playback.
    // actual transfer can happen up to 1ms after estimated head pos
    // because of jitter in the transmission times.
    // 1.5ms seems to work in most cases but not at 192k and 44k
    // for 44k we need 2ms
    // for 192k we need
    return getCurrentSampleFrame(-4000000);
}

UInt32 EMUUSBAudioEngine::getCurrentSampleFrame(SInt64 offsetns) {

    UInt32 max = usbInputStream.bufferSize / usbInputStream.multFactor - 1;

    double pos = usbInputRing.estimatePositionAt(offsetns);

    //debugIOLog("get current sampleframe %lld", (UInt64)(pos*1000000000));

    // modulo 1, but for float. CHECK maybe getting the mantisse would work?
    if (pos > 1) {
        pos = pos-1;
    } else if (pos < 0) {
        pos = pos+1;
    }
    if (pos < 0 || pos > 1) {
        debugIOLog("warning way-out ring wrap position");
        // safety catch. Should not happen in normal playback.
        return 0;
    }
    
    UInt32 framepos =  (UInt32)(pos * (double)usbInputStream.bufferSize) / usbInputStream.multFactor;
    
    if (framepos > max) {
        return max;
    }

    return framepos;
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
		StreamInfo *info;// = (direction == kUSBIn) ? &usbInputStream : &mOutput;
        if (direction == kUSBIn) {
            info = &usbInputStream;
        } else {
            info = &mOutput;
        }
		info->interfaceNumber = ourInterfaceNumber;
		info->streamDirection = direction;
		info->streamInterface = streamInterface;
		
		newSampleRate.fraction = 0;
		newSampleRate.whole = usbAudioDevice->getHardwareSampleRate();// get the sample rate the device was set to
		debugIOLogC("hardware sample rate is %d", newSampleRate.whole);
		info->numChannels = kChannelCount_10;// try 4 channels first - uh... make that 10... uh... this is stupid.
		mChannelWidth = kBitDepth_24bits;
		UInt32	altChannelWidth = kBitDepth_16bits;
		
        // for info->numChannels = 10 down to 2 step -2
		while (!found && info->numChannels >= kChannelCount_STEREO) {// will never be a mono device
			debugIOLogC("Finding interface %d numChannels %d sampleRate %d", ourInterfaceNumber, info->numChannels, newSampleRate.whole);
			newAltSettingID = usbAudio->FindAltInterfaceWithSettings (ourInterfaceNumber, info->numChannels, mChannelWidth, newSampleRate.whole);
			if (255 == newAltSettingID) {// try finding 16 bit setting
				newAltSettingID = usbAudio->FindAltInterfaceWithSettings(ourInterfaceNumber, info->numChannels, altChannelWidth, newSampleRate.whole);
				mChannelWidth = altChannelWidth;
			}
			
			if (255 != newAltSettingID) {
#ifdef DEBUGLOGGING
				debugIOLogC("newAltSettingID %d", newAltSettingID);
				UInt16	format = usbAudio->GetFormat(ourInterfaceNumber, newAltSettingID);
				debugIOLogC("format is %d", format);
#endif
                
				UInt32	pollInterval = (UInt32) usbAudio->GetEndpointPollInterval(ourInterfaceNumber, newAltSettingID, info->streamDirection);
				mPollInterval = 1 << (pollInterval -1);
				debugIOLogC("direction is %d pollInterval is %d", info->streamDirection, mPollInterval);
				if ((1 == mPollInterval) || (8 == mPollInterval)) {
					debugIOLogC("found channel count %d", info->numChannels);
					info->multFactor = 3 * info->numChannels;
					found = true;
					break;
				}
			}
			info->numChannels -= 2;
			mChannelWidth = kBitDepth_24bits;// restore to 24 bit setting
		}
		if (!found) {// last resort - get anything
			mChannelWidth = kBitDepth_16bits;
			debugIOLogC("last resort in GetDefaultSettings");
			info->numChannels = kChannelCount_STEREO;
			newAltSettingID = usbAudio->FindAltInterfaceWithSettings (ourInterfaceNumber, info->numChannels, mChannelWidth, newSampleRate.whole);
			info->multFactor = kChannelCount_STEREO * 2;
			if (255 == newAltSettingID) {
				//	try for a stereo 16-bit interface with any sample rate
				newAltSettingID = usbAudio->FindAltInterfaceWithSettings (ourInterfaceNumber, info->numChannels, mChannelWidth);
				newSampleRate.whole = usbAudio->GetHighestSampleRate (ourInterfaceNumber, newAltSettingID);			// we'll run at the highest sample rate that the device has
			}
		}
		debugIOLog ("Default sample rate is %d", newSampleRate.whole);
		debugIOLog ("Default alternate setting ID is %d", newAltSettingID);
		if(newSampleRate.whole) {
			*defaultSampleRate = newSampleRate;
			info->alternateSettingID = newAltSettingID;
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
    IOUSBDevice *usbDevice =usbInputStream.streamInterface->GetDevice();
    
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
    
    IOUSBDevice *usbDevice = usbInputStream.streamInterface->GetDevice();
    
    getDescriptorString(manufacturerString, usbDevice->GetManufacturerStringIndex ());
    getDescriptorString(productString,usbDevice->GetProductStringIndex ());
	interfaceNumber = usbInputStream.streamInterface->GetInterfaceNumber ();
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
    // TODO lot of cleanup required in this code. Move parts to StreamInfo and EMUUSBInputStream/OutputStream
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
	mWriteLock = NULL;
	mFormatLock = NULL;
    FailIf (FALSE == super::initHardware (provider), Exit);
    
	FailIf (NULL == usbAudioDevice, Exit); // (AC mod)
    
    usbAudio = usbAudioDevice->GetUSBAudioConfigObject();
    FailIf (NULL == usbAudio, Exit);
    
	usbInputStream.audioStream = OSTypeAlloc(IOAudioStream); // new IOAudioStream
	FailIf(NULL == usbInputStream.audioStream, Exit);
	mOutput.audioStream = OSTypeAlloc(IOAudioStream); // new IOAudioStream
	if (NULL == mOutput.audioStream) {
		usbInputStream.audioStream->release();
		usbInputStream.audioStream = NULL;
		return resultBool;
	}
	// Iterate through our assocated streams and perform initialization on each
	// This will also sort out which stream is input and which is output
    for (int i = 0; i < mStreamInterfaces->getCount(); ++i) {
		IOUSBInterface *streamInterface = OSDynamicCast(IOUSBInterface,mStreamInterfaces->getObject(i));
		FailIf(NULL == streamInterface, Exit);
		if (kIOReturnSuccess != GetDefaultSettings(streamInterface, &sampleRate)) {
			usbInputStream.audioStream->release();
			usbInputStream.audioStream = NULL;
			mOutput.audioStream->release();
			mOutput.audioStream = NULL;
			return resultBool;
		}
	}
    
	debugIOLogC("input alternateID %d",usbInputStream.alternateSettingID);
	debugIOLogC("output alternatID %d",mOutput.alternateSettingID);
    
    
	// setup input stream (AC)
	if (!usbInputStream.audioStream->initWithAudioEngine (this, (IOAudioStreamDirection) usbInputStream.streamDirection, 1)) {
		usbInputStream.audioStream->release();
		return resultBool;
	}
	// look for a streaming output terminal that's connected to a non-streaming input terminal
	//debugIOLog ("This is an input type endpoint (mic, etc.)");
	index = 0;
	do {
		terminalType = usbAudio->GetIndexedInputTerminalType (usbAudioDevice->mInterfaceNum, 0, index++);		// Change this to not use mControlInterface
	} while (terminalType == INPUT_UNDEFINED && index < 256);
    
	usbInputStream.audioStream->setTerminalType (terminalType);
	
	usbInputStream.numUSBFrameLists = RECORD_NUM_USB_FRAME_LISTS;
	usbInputStream.numUSBFramesPerList = RECORD_NUM_USB_FRAMES_PER_LIST;
	usbInputStream.numUSBFrameListsToQueue = RECORD_NUM_USB_FRAME_LISTS_TO_QUEUE;
	
	//usbInputStream.numUSBTimeFrames = usbInputStream.numUSBFramesPerList / kNumberOfFramesPerMillisecond;
	
	mWriteLock = IOLockAlloc();
	FailIf(!mWriteLock, Exit);
	mFormatLock = IOLockAlloc();
	FailIf(!mFormatLock, Exit);
    
	// alloc memory required to clear the input pipe
	usbInputStream.usbIsocFrames = (IOUSBLowLatencyIsocFrame *)IOMalloc (usbInputStream.numUSBFrameLists * usbInputStream.numUSBFramesPerList * sizeof (IOUSBLowLatencyIsocFrame));
	FailIf (NULL == usbInputStream.usbIsocFrames, Exit);
	usbInputStream.usbCompletion = (IOUSBLowLatencyIsocCompletion *)IOMalloc (usbInputStream.numUSBFrameLists * sizeof (IOUSBLowLatencyIsocCompletion));
	FailIf (NULL == usbInputStream.usbCompletion, Exit);
	bzero(usbInputStream.usbCompletion, usbInputStream.numUSBFrameLists * sizeof(IOUSBLowLatencyIsocCompletion));
	usbInputStream.bufferDescriptors = (IOSubMemoryDescriptor **)IOMalloc (usbInputStream.numUSBFrameLists * sizeof (IOSubMemoryDescriptor *));
	FailIf (NULL == usbInputStream.bufferDescriptors, Exit);
	bzero (usbInputStream.bufferDescriptors, usbInputStream.numUSBFrameLists * sizeof (IOSubMemoryDescriptor *));
    
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
    
	//mOutput.numUSBTimeFrames = mOutput.numUSBFramesPerList / kNumberOfFramesPerMillisecond;
    
	// Get the hub speed
	mHubSpeed = usbAudioDevice->getHubSpeed();
	
	//mOutput.frameQueuedForList = NULL;
	
//	if (kUSBDeviceSpeedHigh == mHubSpeed) {
//		// Allocate frame list time stamp array
//		mOutput.frameQueuedForList = new UInt64[mOutput.numUSBFrameLists];
//		FailIf (NULL == mOutput.frameQueuedForList, Exit);
//	}
    
	mOutput.usbIsocFrames = (IOUSBLowLatencyIsocFrame *)IOMalloc (mOutput.numUSBFrameLists * mOutput.numUSBFramesPerList * sizeof (IOUSBLowLatencyIsocFrame));
	FailIf (NULL == mOutput.usbIsocFrames, Exit);
    
	
	mOutput.usbCompletion = (IOUSBLowLatencyIsocCompletion *)IOMalloc (mOutput.numUSBFrameLists * sizeof (IOUSBLowLatencyIsocCompletion));
	FailIf (NULL == mOutput.usbCompletion, Exit);
	bzero(mOutput.usbCompletion, mOutput.numUSBFrameLists * sizeof(IOUSBLowLatencyIsocCompletion));
    
	
	mOutput.bufferDescriptors = (IOSubMemoryDescriptor **)IOMalloc (mOutput.numUSBFrameLists * sizeof (IOSubMemoryDescriptor *));
	FailIf (NULL == mOutput.bufferDescriptors, Exit);
	bzero (mOutput.bufferDescriptors, mOutput.numUSBFrameLists * sizeof (IOSubMemoryDescriptor *));
    
	//needed for output (AC)
    FailIf(mOutput.init(this) != kIOReturnSuccess, Exit);
    
	FailIf (kIOReturnSuccess != AddAvailableFormatsFromDevice (usbAudio,usbInputStream.interfaceNumber), Exit);
	FailIf (kIOReturnSuccess != AddAvailableFormatsFromDevice (usbAudio,mOutput.interfaceNumber), Exit);
	beginConfigurationChange();
	debugIOLogC("sampleRate %d", sampleRate.whole);
	
	CalculateSamplesPerFrame (sampleRate.whole, &averageFrameSamples, &additionalSampleFrameFreq);
	// this calcs (frame size, etc.) could probably be simplified, but I'm leaving them this way for now (AC)
	usbInputStream.multFactor = usbInputStream.numChannels * (mChannelWidth / 8);
	mOutput.multFactor = mOutput.numChannels * (mChannelWidth / 8);
	usbInputStream.maxFrameSize = (averageFrameSamples + 1) * usbInputStream.multFactor;
	mOutput.maxFrameSize = (averageFrameSamples + 1) * mOutput.multFactor;
	debugIOLogC("in initHardware about to call initBuffers");
	
	initBuffers();
	setSampleRate(&sampleRate);
	completeConfigurationChange();
    
    
	// Tell the IOAudioFamily what format we are going to be running in.
	// first, for input
	streamFormat.fNumChannels = usbAudio->GetNumChannels (usbInputStream.interfaceNumber, usbInputStream.alternateSettingID);
	streamFormat.fBitDepth = usbAudio->GetSampleSize (usbInputStream.interfaceNumber, usbInputStream.alternateSettingID);
	streamFormat.fBitWidth = usbAudio->GetSubframeSize (usbInputStream.interfaceNumber, usbInputStream.alternateSettingID) * 8;
	streamFormat.fAlignment = kIOAudioStreamAlignmentLowByte;
	streamFormat.fByteOrder = kIOAudioStreamByteOrderLittleEndian;
	streamFormat.fDriverTag = (usbInputStream.interfaceNumber << 16) | usbInputStream.alternateSettingID;
	debugIOLogC("initHardware input NumChannels %d, bitWidth, %d", streamFormat.fNumChannels, streamFormat.fBitWidth);
	switch (usbAudio->GetFormat (usbInputStream.interfaceNumber, usbInputStream.alternateSettingID)) {
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
	FailIf (FALSE == usbInputStream.streamInterface->open (this), Exit);		// Have to open the interface because calling setFormat will call performFormatChange, which expects the interface to be open.
	debugIOLogC("setting input format");
	resultCode = usbInputStream.audioStream->setFormat (&streamFormat);
	//	FailIf (kIOReturnSuccess != resultCode, Exit);
	debugIOLogC("adding input audio stream");
	resultCode = addAudioStream (usbInputStream.audioStream);
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
    FailIf(kUSBAudioClass != usbAudio->GetInterfaceClass (usbInputStream.interfaceNumber, usbInputStream.alternateSettingID), Exit);
#else
	FailIf(VENDOR_SPECIFIC != usbAudio->GetInterfaceClass(usbInputStream.interfaceNumber, usbInputStream.alternateSettingID), Exit);
#endif
    FailIf (kUSBAudioStreamInterfaceSubclass != usbAudio->GetInterfaceSubClass (usbInputStream.interfaceNumber, usbInputStream.alternateSettingID), Exit);
    
    
	usbAudioDevice->doControlStuff(this, mOutput.interfaceNumber, mOutput.alternateSettingID);
	usbAudioDevice->doControlStuff(this, usbInputStream.interfaceNumber, usbInputStream.alternateSettingID);
	//usbAudioDevice->addCustomAudioControls(this);	// add any custom controls
    usbAudioDevice->activateAudioEngine (this, FALSE);
    
    resultBool = TRUE;
    
	snprintf (vendorIDCString, sizeof(vendorIDCString),"0x%04X", usbInputStream.streamInterface->GetDevice()->GetVendorID ());
	snprintf (productIDCString, sizeof(productIDCString),"0x%04X", usbInputStream.streamInterface->GetDevice()->GetProductID ());
	
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
        
		IOReturn result = usbAudioEngineObject->mPlugin->pluginInit (usbAudioEngineObject, usbAudioEngineObject->usbInputStream.streamInterface->GetDevice()->GetVendorID (), usbAudioEngineObject->usbInputStream.streamInterface->GetDevice()->GetProductID ());
		if (result == kIOReturnSuccess) {
			debugIOLog ("success initing the plugin");
			usbAudioEngineObject->mPlugin->pluginSetDirection ((IOAudioStreamDirection) usbAudioEngineObject->usbInputStream.streamDirection);
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
    
    
	if (mPlugin)  {
		debugIOLogC("starting plugin");
		mPlugin->pluginStart ();
	}
    
    if (!usbStreamRunning) {
		debugIOLogC("about to start USB stream(s)");
        resultCode = startUSBStream();
	}
    
	debugIOLogC("++EMUUSBAudioEngine[%p]::performEngineStart result is 0x%x direction %d", this, resultCode, usbInputStream.streamDirection);
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
	
	if (audioStream == usbInputStream.audioStream) {
		debugIOLogC("new format from INPUT");
		streamDirection = usbInputStream.streamDirection;
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
	
    result = performFormatChangeInternal(audioStream, newFormat, newSampleRate, streamDirection);

    if (needToRestartEngine) {
		performAudioEngineStart();
	}

Exit:
    //	IOLockUnlock(mFormatLock);
	debugIOLog ("-EMUUSBAudioEngine::performFormatChange, result = %x", result);
    return result;
}


IOReturn EMUUSBAudioEngine::performFormatChangeInternal (IOAudioStream *audioStream, const IOAudioStreamFormat *newFormat, const IOAudioSampleRate *newSampleRate, UInt8 streamDirection)
{
    EMUUSBAudioConfigObject*	usbAudio = usbAudioDevice->GetUSBAudioConfigObject();
    UInt16					alternateFrameSize = 0;
    //UInt8					ourInterfaceNumber = (UInt8)(newFormat->fDriverTag >> 16);
    UInt8					newAlternateSettingID = (UInt8)(newFormat->fDriverTag);
    bool					needToChangeChannels = false;// default
    bool					sampleRateChanged = false;
    
    debugIOLogC("+performFormatChangeInternal");
    ReturnIf(NULL == usbAudio, kIOReturnError);
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
        // special hack to ensure input channel count stays in sync w/ output when changing sample rate
        // at least from AMS, sample-rate changes always come from output stream (go figure)
        // Wouter: this is NOT true for EMU0404, there "new format from INPUT" can come first.
        // However, the input format often does not arrive at all. #15
        // CHECK I have not seen INPUT first for a long time.
        
        if (newFormat->fNumChannels != usbInputStream.audioStream->format.fNumChannels
            && audioStream != usbInputStream.audioStream) {
            needToChangeChannels = true;
            debugIOLog ("Need to adjust input number of channels, cur = %d, new = %d", usbInputStream.audioStream->format.fNumChannels, newFormat->fNumChannels);
            // FIXME this gives errors when switching to 4 channels. Maybe remove it?
            // The error may be because beginConfigurationChange() was not yet called.
            // this may be related to disabled code at end of this function?
            // usbInputStream.audioStream->setFormat(newFormat,false);
        }
        sampleRate = *newSampleRate;
        sampleRateChanged = true;
    }
    
    if (mPlugin) {
        mPlugin->pluginSetFormat (newFormat, &sampleRate);
    }
    
    
    if (audioStream == usbInputStream.audioStream || sampleRateChanged)
    {
        UInt8	newDirection = usbAudio->GetIsocEndpointDirection (usbInputStream.interfaceNumber, newAlternateSettingID);
        if (FALSE == usbAudio->VerifySampleRateIsSupported(usbInputStream.interfaceNumber, newAlternateSettingID, sampleRate.whole)) {
            // find an alternative if requested rate not supported.
            newAlternateSettingID = usbAudio->FindAltInterfaceWithSettings (usbInputStream.interfaceNumber, newFormat->fNumChannels, newFormat->fBitDepth, sampleRate.whole);
            mPollInterval = (UInt32) (1 << ((UInt32) usbAudio->GetEndpointPollInterval(usbInputStream.interfaceNumber, newAlternateSettingID, newDirection) -1));
            // Wouter: following test is broken, it always will succeed!
            if ((1 != mPollInterval) || (8 != mPollInterval)) {// disallow selection of endpoints with sub ms polling interval NB - assumes that sub ms device will not use a poll interval of 1 (every microframe)
                newAlternateSettingID = 255;
            }
            debugIOLog ("%d channel %d bit @ %d Hz is not supported. Suggesting alternate setting %d", newFormat->fNumChannels,
                        newFormat->fBitDepth, sampleRate.whole, newAlternateSettingID);
            
            ReturnIf (255 == newAlternateSettingID, kIOReturnError);
        }
        
        ReturnIf (newDirection != usbInputStream.streamDirection, kIOReturnError);
        debugIOLog ("++about to set input : ourInterfaceNumber = %d & newAlternateSettingID = %d", usbInputStream.interfaceNumber, newAlternateSettingID);
        beginConfigurationChange();
        
        UInt8	address = usbAudio->GetIsocEndpointAddress(usbInputStream.interfaceNumber, newAlternateSettingID, usbInputStream.streamDirection);
        alternateFrameSize = usbAudio->GetEndpointMaxPacketSize(usbInputStream.interfaceNumber, newAlternateSettingID, address);
        mOutput.averageSampleRate = sampleRate.whole;	// Set this as the default until we are told otherwise
        debugIOLog ("averageSampleRate = %d", mOutput.averageSampleRate);
        
        //if (streamDirection == usbInputStream.streamDirection) {
        usbInputStream.numChannels = newFormat->fNumChannels;
        //}
        usbInputStream.alternateSettingID = newAlternateSettingID;
        mChannelWidth = newFormat->fBitWidth;
        usbInputStream.multFactor = usbInputStream.numChannels * (mChannelWidth / 8);
        debugIOLogC("alternateFrameSize is %d", alternateFrameSize);
        if (usbInputStream.maxFrameSize != alternateFrameSize) {
            debugIOLogC("maxFrameSize %d alternateFrameSize %d", usbInputStream.maxFrameSize, alternateFrameSize);
            usbInputStream.maxFrameSize = alternateFrameSize;
            needNewBuffers = true;
        }
        
        // set the format - JH
        usbInputStream.audioStream->setFormat(newFormat,false);
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
            
            ReturnIf (255 == newAlternateSettingID, kIOReturnError);
        }
        ReturnIf (newDirection != mOutput.streamDirection, kIOReturnError);
        
        
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

    // #30 update the poll interval.
    mPollInterval = (UInt32) (1 << ((UInt32) usbAudio->GetEndpointPollInterval(usbInputStream.interfaceNumber, usbInputStream.alternateSettingID, usbInputStream.streamDirection) -1));

    
    if (needNewBuffers) {
        if (kIOReturnSuccess!= initBuffers()) {
            debugIOLogC("problem with initBuffers in performFormatChange");
            return kIOReturnError;
        }
    }
    
    

    
    // Set the sampling rate on the device
    SetSampleRate(usbAudio, sampleRate.whole);		// no need to check the error
    
    // I think this code is not needed, as we have no controls needing to be changed
    // Wouter: maybe we do. See comments inside if(newSampleRate) above .
#if 0
    // #6 this code is broken, don't enable.
    if (needToChangeChannels) {
        removeAllDefaultAudioControls();
        usbAudioDevice->createControlsForInterface(this, usbInputStream.interfaceNumber, newAlternateSettingID);
        usbAudioDevice->createControlsForInterface(this, mOutput.interfaceNumber, newAlternateSettingID);
        //usbAudioDevice->createControlsForInterface(this, ourInterfaceNumber, newAlternateSettingID);
    }
#endif
    
    
    debugIOLog ("called setNumSampleFramesPerBuffer with %d", usbInputStream.bufferSize / usbInputStream.multFactor);
    debugIOLog ("newFormat->fNumChannels = %d, newFormat->fBitWidth %d", newFormat->fNumChannels, newFormat->fBitWidth);
    // debugIOLog ("called setSampleOffset with %d", usbInputStream.numUSBFramesPerList);
    completeConfigurationChange();
    
    debugIOLogC("-performFormatChangeInternal");
    return kIOReturnSuccess;
}







void EMUUSBAudioEngine::resetClipPosition (IOAudioStream *audioStream, UInt32 clipSampleFrame) {
	if (mPlugin)
		mPlugin->pluginReset();
}


IOReturn EMUUSBAudioEngine::SetSampleRate (EMUUSBAudioConfigObject *usbAudio, UInt32 inSampleRate) {
	IOReturn				result = kIOReturnError;
    
    debugIOLogC("EMUUSBAudioEngine::SetSampleRate %d", inSampleRate);
	
    if (usbAudio->IsocEndpointHasSampleFreqControl (mOutput.interfaceNumber, mOutput.alternateSettingID)) {
        debugIOLogC("EMUUSBAudioEngine::SetSampleRate has IsocEndpointHasSampleFreqControl");
    }
    
    if (usbAudioDevice && usbAudioDevice->hasSampleRateXU()) {// try using the XU method to set the sample rate before using the default
        debugIOLogC("using SampleRateXU");

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
            debugIOLogC("set result  = %x",result);
            if (kIOReturnSuccess == result) {// the engines are tied together
                usbAudioDevice->setHardwareSampleRate(inSampleRate);
                usbAudioDevice->setOtherEngineSampleRate(this, inSampleRate);
            } else {
                result = kIOReturnSuccess;
            }
        }
    } else if (usbAudio->IsocEndpointHasSampleFreqControl (mOutput.interfaceNumber, mOutput.alternateSettingID)) {// use the conventional method
        debugIOLogC("using DeviceRequest");

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
	const IOAudioStreamFormat *			inputFormat = usbInputStream.audioStream->getFormat();
	const IOAudioStreamFormat *			outputFormat = mOutput.audioStream->getFormat();
	
	IOReturn							resultCode = kIOReturnError;
	IOUSBFindEndpointRequest			audioIsochEndpoint;
	EMUUSBAudioConfigObject *			usbAudio = usbAudioDevice->GetUSBAudioConfigObject();
    /*! usual number of stereo(quad)samples per frame. (the average is a little higher) */
	UInt16								averageFrameSamples = 0;
	UInt16								additionalSampleFrameFreq = 0;
    UInt8	address;
    UInt32	maxPacketSize;
    UInt64 startFrameNr;


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
		
	UInt32	newInputMultFactor = (inputFormat->fBitWidth / 8) * inputFormat->fNumChannels;
	UInt32	newOutputMultFactor = (outputFormat->fBitWidth / 8) * outputFormat->fNumChannels;
	
	UInt32	altFrameSampleSize = averageFrameSamples + 1;
    
	usbInputStream.bufferOffset = 0;
    

    
	debugIOLogC("Isoc Frames / usbCompletions");
	bzero(usbInputStream.usbIsocFrames, usbInputStream.numUSBFrameLists * usbInputStream.numUSBFramesPerList * sizeof(IOUSBLowLatencyIsocFrame));
	bzero(usbInputStream.usbCompletion, usbInputStream.numUSBFrameLists * sizeof(IOUSBLowLatencyIsocCompletion));
    FailIf ((usbInputStream.numUSBFrameLists < usbInputStream.numUSBFrameListsToQueue), Exit);

	mOutput.currentFrameList = 0;
    mOutput.bufferOffset = 0;
	mOutput.previouslyPreparedBufferOffset = 0;		// Start playing from the start of the buffer
	bzero(mOutput.usbIsocFrames, mOutput.numUSBFrameLists * mOutput.numUSBFramesPerList * sizeof(IOUSBLowLatencyIsocFrame));
	bzero(mOutput.usbCompletion, mOutput.numUSBFrameLists * sizeof(IOUSBLowLatencyIsocCompletion));
    FailIf ((mOutput.numUSBFrameLists < mOutput.numUSBFrameListsToQueue), Exit);
    
	SetSampleRate(usbAudio, sampleRate.whole);
	
	
	// if our buffer characteristics have changed (or they don't yet exist), initialize buffers now
	
	if (newInputMultFactor > usbInputStream.multFactor || newOutputMultFactor > mOutput.multFactor
		|| !usbInputStream.usbBufferDescriptor || !mOutput.usbBufferDescriptor) {
        debugIOLogC("startUSBStream: about to re-init buffers, input factor=%d (now %d), output factor=%d (now %d)",
                    usbInputStream.multFactor,newInputMultFactor,mOutput.multFactor,newOutputMultFactor);
        usbInputStream.maxFrameSize = altFrameSampleSize * newInputMultFactor;
        mOutput.maxFrameSize = altFrameSampleSize * newOutputMultFactor;
        usbInputStream.multFactor = newInputMultFactor;
        mOutput.multFactor = newOutputMultFactor;
        debugIOLogC("pre initBuffers");
        beginConfigurationChange();
        initBuffers();
        completeConfigurationChange();
        debugIOLogC("post initBuffers");
	}
    
    
	//first do the input
    
	// Allocate the pipe now so that we don't keep it open when we're not streaming audio to the device.
	FailIf (NULL == usbInputStream.streamInterface, Exit);
    
	resultCode = usbInputStream.streamInterface->SetAlternateInterface (this, usbInputStream.alternateSettingID);
	FailIf (kIOReturnSuccess != resultCode, Exit);
	
	// Acquire a PIPE for the isochronous stream.
	audioIsochEndpoint.type = kUSBIsoc;
	audioIsochEndpoint.direction = usbInputStream.streamDirection;
	{
		debugIOLogC("createInputPipe");
		usbInputStream.pipe = usbInputStream.streamInterface->FindNextPipe (NULL, &audioIsochEndpoint);
		FailIf (NULL == usbInputStream.pipe, Exit);
		usbInputStream.pipe->retain ();
	}
	
	address = usbAudio->GetIsocEndpointAddress(usbInputStream.interfaceNumber, usbInputStream.alternateSettingID, usbInputStream.streamDirection);
	maxPacketSize = usbAudio->GetEndpointMaxPacketSize(usbInputStream.interfaceNumber, usbInputStream.alternateSettingID, address);
    
	usbInputStream.maxFrameSize = altFrameSampleSize * usbInputStream.multFactor;
	if (usbInputStream.maxFrameSize != maxPacketSize)
		usbInputStream.maxFrameSize = maxPacketSize;
	//mBus = usbInputStream.streamInterface->GetDevice()->GetBus();// this will not change
    // Other possible variations - use a static variable that holds the offset number.
    // The var is set depending on the hub speed and whether the first write/ read failed with a late error.
    // When a late error is encountered (USB 2.0), increment the var until a max of 16 frames is reached.
    // NB - From testing and observation this work around does not help and has therefore been deleted.
	usbInputStream.frameOffset = kMinimumFrameOffset + ((kUSBDeviceSpeedHigh == mHubSpeed) * kUSB2FrameOffset);
	
	*(UInt64 *) (&(usbInputStream.usbIsocFrames[0].frTimeStamp)) = 0xFFFFFFFFFFFFFFFFull;
    
    
    resultCode =usbInputRing.init(usbInputStream.bufferSize, this, sampleRate.whole * usbInputStream.multFactor);
    FailIf( kIOReturnSuccess != resultCode, Exit);
    FailIf( kIOReturnSuccess != frameSizeQueue.init(FRAMESIZE_QUEUE_SIZE,"frameSizeQueue"), Exit);
    
    usbInputStream.init(this, &usbInputRing, &frameSizeQueue);
    
    // delay actual start() till very end to get all start at once
    
	
	// and now the output
	
	resultCode = mOutput.streamInterface->SetAlternateInterface (this, mOutput.alternateSettingID);
	FailIf (kIOReturnSuccess != resultCode, Exit);
    
    debugIOLog("create output pipe ");
	bzero(&audioIsochEndpoint,sizeof(audioIsochEndpoint));
	audioIsochEndpoint.type = kUSBIsoc;
	audioIsochEndpoint.direction = mOutput.streamDirection;
	mOutput.pipe = mOutput.streamInterface->FindNextPipe (NULL, &audioIsochEndpoint);
	FailIf (NULL == mOutput.pipe, Exit);
	mOutput.pipe->retain ();
	debugIOLog("check for associated endpoint");
	//CheckForAssociatedEndpoint (usbAudio,mOutput.interfaceNumber,mOutput.alternateSettingID);// result is ignored
    
	address = usbAudio->GetIsocEndpointAddress(mOutput.interfaceNumber, mOutput.alternateSettingID, mOutput.streamDirection);
	maxPacketSize = usbAudio->GetEndpointMaxPacketSize(mOutput.interfaceNumber, mOutput.alternateSettingID, address);
    
    
	mOutput.maxFrameSize = altFrameSampleSize * mOutput.multFactor;
	if (mOutput.maxFrameSize != maxPacketSize)
		mOutput.maxFrameSize = maxPacketSize;
	//mBus = mOutput.streamInterface->GetDevice()->GetBus();// this will not change
    // Other possible variations - use a static variable that holds the offset number.
    // The var is set depending on the hub speed and whether the first write/ read failed with a late error.
    // When a late error is encountered (USB 2.0), increment the var until a max of 16 frames is reached.
    // NB - From testing and observation this work around does not help and has therefore been deleted.
	mOutput.frameOffset = 8; // HACK kMinimumFrameOffset + ((kUSBDeviceSpeedHigh == mHubSpeed) * kUSB2FrameOffset);
	//mOutput.usbFrameToQueueAt = 0; // INIT as late as possible. mBus->GetFrameNumber() + mOutput.frameOffset;	// start on an offset usb frame
	*(UInt64 *) (&(mOutput.usbIsocFrames[0].frTimeStamp)) = 0xFFFFFFFFFFFFFFFFull;
    
    setRunEraseHead(true); // need it to avoid stutter at start&end and to allow multiple simultaneous playback.
    
    // Ok, all set, go!
    // plan startFrameNr well in the future, so that we have time to start both streams before that point.
    startFrameNr = usbInputStream.streamInterface->GetDevice()->GetBus()->GetFrameNumber() + 64;
    resultCode = usbInputStream.start(startFrameNr);
    FailIf (kIOReturnSuccess != resultCode, Exit)

    resultCode = mOutput.start(&frameSizeQueue, startFrameNr, averageFrameSamples);
    FailIf (kIOReturnSuccess != resultCode, Exit)

    
    // It's actually not well defined what "stable" means.
    // Therefore it is not clear what to do here.
    // The EMU clock is very stable.
    // Our PLL will take some time to exactly sync with the EMU clock.
    // But it will be very stable too because of our low pass filter to
    // filter out USB timing inaccuracy. The USB timing inaccuracy is
    // in the order of 1ms because that's the max update rate we can request.
    // This directly limits our sync accuracy. 
	//setClockIsStable(FALSE);
    
    usbStreamRunning = TRUE;
    resultCode = kIOReturnSuccess;
    
Exit: // FAILURE EXIT
	if (kIOReturnSuccess != resultCode) {
        usbInputStream.stop();
        mOutput.stop();
        IOSleep(1000); // HACK give mInput time to stop. Callback is tricky at this point.
        usbInputRing.free();
		RELEASEOBJ(usbInputStream.pipe);
		RELEASEOBJ(mOutput.pipe);
		RELEASEOBJ(usbInputStream.associatedPipe);
		RELEASEOBJ(mOutput.associatedPipe);
	}
	
    return resultCode;
}

IOReturn EMUUSBAudioEngine::stopUSBStream () {
	debugIOLog ("+EMUUSBAudioEngine[%p]::stopUSBStream ()", this);
	usbStreamRunning = FALSE;
    usbInputStream.stop();
    mOutput.stop();
    // HACK give time to the input channel to stop.
    // move code to notifyClosed()?  Or consider the sleep as a time-out?
    IOSleep(1000);
	if (NULL != mOutput.pipe) {
		if (FALSE == terminatingDriver)
			mOutput.pipe->SetPipePolicy (0, 0);// don't call USB to avoid deadlock
		
		// Have to close the current pipe so we can open a new one because changing the alternate interface will tear down the current pipe
		RELEASEOBJ(mOutput.pipe);
	}
	RELEASEOBJ(mOutput.associatedPipe);
	if (NULL != usbInputStream.pipe) {
		if (FALSE == terminatingDriver)
			usbInputStream.pipe->SetPipePolicy (0, 0);// don't call USB to avoid deadlock
		
		// Have to close the current pipe so we can open a new one because changing the alternate interface will tear down the current pipe
		RELEASEOBJ(usbInputStream.pipe);
	}
	RELEASEOBJ(usbInputStream.associatedPipe);
    
    
	if (FALSE == terminatingDriver) {
		// Don't call USB if we are being terminated because we could deadlock their workloop.
	      	if (NULL != usbInputStream.streamInterface) // if we don't have an interface, message() got called and we are being terminated
			usbInputStream.streamInterface->SetAlternateInterface (this, kRootAlternateSetting);
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
    // Why is this done so complex? What makes this different from
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


bool EMUUSBAudioEngine::willTerminate (IOService * provider, IOOptionBits options) {
    
	if (usbInputStream.streamInterface == provider) {
		terminatingDriver = TRUE;
		if (FALSE == usbStreamRunning) {
			// Close our stream interface and go away because we're not running.
			usbInputStream.streamInterface->close (this);
			usbInputStream.streamInterface = NULL;
		} else {
			// Have the write completion routine clean everything up because we are running.
            usbInputStream.stop();
		}
	} else if (mOutput.streamInterface == provider) {
		terminatingDriver = TRUE;
		if (FALSE == usbStreamRunning) {
			// Close our stream interface and go away because we're not running.
			mOutput.streamInterface->close (this);
			mOutput.streamInterface = NULL;
		} else {
			// Have the write completion routine clean everything up because we are running.
            mOutput.stop();
//			if (0 == usbInputStream.shouldStop) {
//				usbInputStream.shouldStop++;
//			}
		}
	}
    
	debugIOLog ("-EMUUSBAudioEngine[%p]::willTerminate", this);
    
	return super::willTerminate (provider, options);
}




IOReturn EMUUSBAudioEngine::hardwareSampleRateChanged(const IOAudioSampleRate *newSampleRate) {
	IOReturn	result = kIOReturnError;
    debugIOLogC("received hardewareSampleRateChanged notification");
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


IOReturn EMUUSBAudioEngine::initBuffers() {
	IOReturn						result = kIOReturnError;
	if (usbAudioDevice) {
        // poll interval should have been set when this is called.
		debugIOLogC("initBuffers mPollInterval=%d",mPollInterval);

		// clear up any existing buffers
		UInt32 inputSize = usbInputStream.maxFrameSize;
		UInt32 outputSize = mOutput.maxFrameSize;
		UInt32	samplesPerFrame = inputSize / usbInputStream.multFactor;
		debugIOLogC("inputSize= %d multFactor= %d", inputSize, usbInputStream.multFactor);
		debugIOLogC("outputSize= %d multFactor= %d", outputSize, mOutput.multFactor);
		
        //	FailIf(samplesPerFrame != outputSize / mOutput.multFactor, Exit); - JH allocated size may be off 1 such as with AC3
		
		// this is total guesswork (AC)
        
        // Wouter: it seems that 0.1s buffer size is working ok.
        // I guess PAGE_SIZE helps to align buffer in memory.
		UInt32	numSamplesInBuffer = PAGE_SIZE * (2 + (sampleRate.whole > 48000) + 3 * (sampleRate.whole > 96000) );
        
		usbInputStream.bufferSize = numSamplesInBuffer * usbInputStream.multFactor;
		mOutput.bufferSize = numSamplesInBuffer * mOutput.multFactor;
        
        buf = (UInt8 *) IOMalloc(usbInputStream.bufferSize);
        FailIf(NULL == buf, Exit);
        
		// setup the input buffer
		if (NULL != usbInputStream.bufferMemoryDescriptor) {
			usbInputStream.audioStream->setSampleBuffer (NULL, 0);
			setNumSampleFramesPerBuffer (0);
			debugIOLogC("releasing the input sampleBufferMemory descriptor");
			usbInputStream.bufferMemoryDescriptor->complete();
			usbInputStream.bufferMemoryDescriptor->release();
			usbInputStream.bufferMemoryDescriptor = NULL;
			usbInputStream.bufferPtr = NULL;// reset the sample buffer ptr
		}
		usbInputStream.readUSBFrameListSize = inputSize * usbInputStream.numUSBFramesPerList;
		debugIOLogC("new bufferSize = %d numSamplesInBuffer = %d\n", usbInputStream.bufferSize, numSamplesInBuffer );
		if (usbInputStream.usbBufferDescriptor) {
			debugIOLogC("disposing the mUSBBufferDescriptor input");
			usbInputStream.usbBufferDescriptor->complete();
			usbInputStream.usbBufferDescriptor->release();
			usbInputStream.usbBufferDescriptor = NULL;
			usbInputStream.readBuffer = NULL;
		}
        
		// read buffer section
		// following is the actual buffer that stuff gets read into
		debugIOLogC("initBuffers numUSBFrameLists %d", usbInputStream.numUSBFrameLists);
#ifdef CONTIGUOUS
		usbInputStream.usbBufferDescriptor = IOBufferMemoryDescriptor::withOptions(kIOMemoryPhysicallyContiguous| kIODirectionInOut, usbInputStream.numUSBFrameLists * usbInputStream.readUSBFrameListSize, page_size);
#else
		usbInputStream.usbBufferDescriptor = IOBufferMemoryDescriptor::withOptions(kIODirectionInOut, usbInputStream.numUSBFrameLists * readUSBFrameListSize, page_size);
#endif
		FailIf (NULL == usbInputStream.usbBufferDescriptor, Exit);
		usbInputStream.usbBufferDescriptor->prepare();
        // Wouter: this gets direct ptr to the USB buffer memory
		usbInputStream.readBuffer = usbInputStream.usbBufferDescriptor->getBytesNoCopy();// get a valid ptr or NULL
		FailIf (NULL == usbInputStream.readBuffer, Exit);
		// setup the sub ranges
		FailIf(NULL == usbInputStream.bufferDescriptors, Exit);
		for (UInt32 i = 0; i < usbInputStream.numUSBFrameLists; ++i) {
			if (usbInputStream.bufferDescriptors[i]) {
                //				debugIOLogC("disposing input soundBufferDescriptors[%d]", i);
				usbInputStream.bufferDescriptors[i]->complete();
				usbInputStream.bufferDescriptors[i]->release();
				usbInputStream.bufferDescriptors[i] = NULL;
			}
			usbInputStream.bufferDescriptors[i] = OSTypeAlloc(IOSubMemoryDescriptor);
			bool initResult = usbInputStream.bufferDescriptors[i]->initSubRange(usbInputStream.usbBufferDescriptor, i * usbInputStream.readUSBFrameListSize, usbInputStream.readUSBFrameListSize, kIODirectionInOut);
            //			debugIOLogC("initSubRange soundBufferDescriptors[%d] %d", i, initResult);
			FailIf (NULL == usbInputStream.bufferDescriptors[i]|| !initResult, Exit);
			result = usbInputStream.bufferDescriptors[i]->prepare();
			FailIf(kIOReturnSuccess != result, Exit);
		}
		// setup the input sample buffer memory descriptor and pointer
		usbInputStream.bufferMemoryDescriptor = IOBufferMemoryDescriptor::withOptions (kIOMemoryPhysicallyContiguous|kIODirectionInOut, usbInputStream.bufferSize, page_size);
		FailIf (NULL == usbInputStream.bufferMemoryDescriptor, Exit);
		usbInputStream.bufferMemoryDescriptor->prepare();
		usbInputStream.bufferPtr = usbInputStream.bufferMemoryDescriptor->getBytesNoCopy();// get a valid pointer or NULL
		FailIf(NULL == usbInputStream.bufferPtr, Exit);
		
        
        // actual jitter on the USB input is 0.01ms. But we have high latency pulses that have 1ms,
        // a few even higher latency pulses at 2ms, and very rare ones at 3ms from expected.
        // Probably 4ms happens but extremely rare.
        
        // Our PLL (time filter) filters out those, but the buffer may have an additional latency
        // when this happens. This is where the offset comes in.
        

		/* You can set another offset using the SafetyOffsetMicroSec in the plist
          You can set it in the properties .list file in
          EMUUSBAudioControl04:        SafetyOffsetMicroSec        Number 4200
          2222 us seems good choice for low latency applications. 4200 is the reliable value.
        
          CoreAudio  converts the value in setSampleOffset to a time offset.
          Then it estimates the input read position from the wrap timestamps.
          Then it plans the calls to convertInputSamples such that the LAST read sample meets the time offset.
        */
        
        UInt32 offsetMicros = getPListNumber("SafetyOffsetMicroSec", 4200);
        UInt64 offsetToSet = sampleRate.whole * offsetMicros / 1000000;
        setSampleOffset((UInt32)offsetToSet);
        debugIOLogC("sample offset %d samples",(UInt32)offsetToSet);

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

        // These numbers were estimated from measurements and then broken down according to DAC and ADC specs
        UInt32 dacLatency = getPListNumber("latencyDAC", 15);
        UInt32 adcLatency = getPListNumber("latencyADC", 53);
        // just half of the measured roundtrip. Can we measure one-way directly?
        UInt32 emuInternaloneWay = (sampleRate.whole / 591 ) / 2;
        setInputSampleLatency((UInt32)offsetToSet + adcLatency + emuInternaloneWay);
		setOutputSampleLatency((UInt32)offsetToSet + dacLatency + emuInternaloneWay);
        
		mOutput.audioStream->setSampleBuffer(mOutput.bufferPtr, mOutput.bufferSize);
        
		setNumSampleFramesPerBuffer(numSamplesInBuffer);
        
		debugIOLogC("completed initBuffers");
		result = kIOReturnSuccess;
	}
Exit:
	return result;
}


UInt32 EMUUSBAudioEngine::getPListNumber( const char *field, UInt32 defaultValue) {
    OSNumber *numberobj = OSDynamicCast(OSNumber,usbAudioDevice->getProperty(field));
    if (numberobj) {
        UInt32 val = numberobj->unsigned32BitValue();
        debugIOLogC("using plist value %s = %d",field,val);
        return val;
    }
    debugIOLogC("using plist %s DEFAULT %d",field,defaultValue);
    return defaultValue;

}



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

/*********************************************/
// UsbInputRing code


IOReturn UsbInputRing::init(UInt32 newSize, IOAudioEngine *engine, UInt32 expected_byte_rate) {
    debugIOLogC("+UsbInputRing::init bytesize=%d byterate=%d", newSize,expected_byte_rate);
    theEngine = engine;
    isFirstWrap = true;
    
    previousfrTimestampNs = 0;
    goodWraps = 0;
    
    expected_wrap_time = 1000000000ull *  newSize / expected_byte_rate;

    debugIOLogC("-UsbInputRing::init %lld", expected_wrap_time);
    
    return RingBufferDefault<UInt8>::init(newSize,"USBInputRing");
}

void UsbInputRing::free() {
    RingBufferDefault<UInt8>::free();
    theEngine = NULL;
}


void UsbInputRing::notifyWrap(AbsoluteTime wt) {
    UInt64 wrapTimeNs;
    
    absolutetime_to_nanoseconds(wt,&wrapTimeNs);
    // the timestamp that USB gives us apparently is more accurate than expected from a 1ms poll rate.
    // There seem to be no consistent  offset on the timestamps.
    
    if (goodWraps >= 5) {
        // regular operation after initial wraps. Enable debug line to check timestamping
        //debugIOLogC("UsbInputRing::notifyWrap %lld",wrapTimeNs);
        takeTimeStampNs(lpfilter.filter(wrapTimeNs),TRUE);
    } else {
        debugIOLogC("UsbInputRing::notifyWrap %d",goodWraps);
        // setting up the timer. Find good wraps.
        if (goodWraps == 0) {
            goodWraps++;
        } else {
            // check if previous wrap had correct spacing deltaT.
            SInt64 deltaT = wrapTimeNs - previousfrTimestampNs - expected_wrap_time;
            UInt64 errorT = abs( deltaT );
            // since we check every ms for completion,
            // we have floor(expected_wrap_time_ms) and ceil(expected_wrap_time_ms) as possibilities.
            if (errorT < 10000000) { // 1ms = max deviation from expected wraptime.
                goodWraps ++;
                if (goodWraps == 5) {
                    lpfilter.init(wrapTimeNs,expected_wrap_time);
                    takeTimeStampNs(wrapTimeNs,FALSE);
                    doLog("USB timer started");
                }
            } else {
                goodWraps = 0;
                doLog("USB hick (expected %lld, got %lld, error=%lld). timer re-syncing.",
                      expected_wrap_time, wrapTimeNs - previousfrTimestampNs, errorT);
            }
        }
    }
    previousfrTimestampNs = wrapTimeNs;
}


void UsbInputRing::takeTimeStampNs(UInt64 timeStampNs, Boolean increment) {
    AbsoluteTime t;
    
    nanoseconds_to_absolutetime(timeStampNs, &t);
    theEngine->takeTimeStamp(increment, &t) ;
}

double UsbInputRing::estimatePositionAt(SInt64 offset) {
    UInt64 now;
    
    absolutetime_to_nanoseconds(mach_absolute_time(), &now);

    return lpfilter.getRelativeDist(now + offset);

}

/*********************************************/
// OurUSBInputStream code


void EMUUSBAudioEngine::OurUSBInputStream::init(EMUUSBAudioEngine * engine,
                                                UsbInputRing * ring, FrameSizeQueue * frameQueue) {
    EMUUSBInputStream::init(ring, frameQueue);
    theEngine = engine;
}



void EMUUSBAudioEngine::OurUSBInputStream::notifyClosed() {
    if (!theEngine) {
        doLog("BUG! EMUUSBAudioEngine not initialized");
        return;
    }
    debugIOLogC("+EMUUSBAudioEngine::OurUSBInputStream::notifyClosed.");
    
    theEngine->usbInputRing.free();
    
    if (theEngine->terminatingDriver) {
        IOReturn res = theEngine->usbInputStream.free();
        if (res != kIOReturnSuccess) {
            doLog("problem freeing input stream:%x",res);
        }
        // this one call is why we need this class
        theEngine->usbInputStream.streamInterface->close(theEngine);
        theEngine->usbInputStream.streamInterface = NULL;
    }
}


/*********************************************/
// OurUSBOutputStream code


IOReturn EMUUSBAudioEngine::OurUSBOutputStream::init(EMUUSBAudioEngine * engine) {
    theEngine = engine;
    return EMUUSBOutputStream::init();
}



void EMUUSBAudioEngine::OurUSBOutputStream::notifyClosed() {
    if (!theEngine)    {
        doLog("BUG! EMUUSBAudioEngine not initialized");
        return;
    }
    debugIOLogC("+EMUUSBAudioEngine::OurUSBOutputStream::notifyClosed.");

    if (theEngine->terminatingDriver) {
        theEngine->mOutput.streamInterface->close (theEngine);
        theEngine->mOutput.streamInterface = NULL;
    }
}
