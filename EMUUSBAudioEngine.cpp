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
#define DEBUGZEROTIME			FALSE
#define DEBUGLOADING			FALSE
#define DEBUGTIMESTAMPS			FALSE

#include "EMUUSBAudioEngine.h"
#include "EMUUSBAudioPlugin.h"
#include "EMUUSBDeviceDefines.h"
#include "EMUUSBUserClient.h"

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

	debugIOLog("+EMUUSBAudioEngine[%p]::init ()", this);
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
	debugIOLog("EMUUSBAudioEngine[%p]::init ()", this);
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
	
	debugIOLog("ourInterfaceNumber = %u\n", ourInterfaceNumber);*/
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
	debugIOLog("GetUSBAudioConfigObject");
	usbAudio = usbAudioDevice->GetUSBAudioConfigObject ();
	FailIf (NULL == usbAudio, Exit);
	// This will cause the driver to not load on any device that has _only_ a low frequency effect output terminal
	FailIf (usbAudio->GetNumOutputTerminals (0, 0) == 1 && usbAudio->GetIndexedOutputTerminalType (0, 0, 0) == OUTPUT_LOW_FREQUENCY_EFFECTS_SPEAKER, Exit);

	debugIOLog("+IOAudioEngine::start");
	resultCode = super::start (provider, usbAudioDevice);
	usbAudioDevice->release();
	debugIOLog("-IOAudioEngine::start");
	debugIOLog ("-EMUUSBAudioEngine[%p]::start (%p) = %d", this, provider, resultCode);

Exit:    
	return resultCode;
}
    
void EMUUSBAudioEngine::stop (IOService * provider) {
    debugIOLog("+EMUUSBAudioEngine[%p]::stop (%p)", this, provider);

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
		debugIOLog("closing stream");
		mInput.streamInterface->close (this);
		mInput.streamInterface = NULL;
	}
	if (mOutput.streamInterface && mOutput.streamInterface->isOpen()) { 
		debugIOLog("closing stream");
		mOutput.streamInterface->close (this);
		mOutput.streamInterface = NULL;
	}

	super::stop (provider);

	debugIOLog ("-EMUUSBAudioEngine[%p]::stop (%p) - rc=%ld", this, provider, getRetainCount());
}

bool EMUUSBAudioEngine::terminate (IOOptionBits options) {
	debugIOLog("terminating the EMUUSBAudioEngine\n");
	return super::terminate(options);
}

void EMUUSBAudioEngine::detach(IOService *provider) {
	debugIOLog("EMUUSBAudioEngine[%p]: detaching %p",this,provider);
	super::detach(provider);
}

void EMUUSBAudioEngine::close(IOService *forClient, IOOptionBits options) {
	debugIOLog("EMUUSBAudioEngine[%p]: closing for %p",this,forClient);
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
			debugIOLog("direction %d pollInterval %d", direction, pollInterval);
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
	debugIOLog("CheckForAssociatedEndpoint address %d, syncType %d", address, syncType);
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
					debugIOLog("Disposing off current usbCompletion %p", usbCompletion);
					IOFree(usbCompletion, numUSBFrameLists * sizeof(IOUSBLowLatencyIsocCompletion));
					usbCompletion = NULL;
				}
				numUSBFramesPerList = framesUntilRefresh;		// It needs to be updated more often, so run as the device requests.
				mUSBIsocFrames = (IOUSBLowLatencyIsocFrame *)IOMalloc(numUSBFrameLists * numUSBFramesPerList * sizeof(IOUSBLowLatencyIsocFrame));
				debugIOLog ("mUSBIsocFrames is now %p", mUSBIsocFrames);
				FailIf (NULL == mUSBIsocFrames, Exit);
				usbCompletion = (IOUSBLowLatencyIsocCompletion *)IOMalloc(numUSBFrameLists * sizeof(IOUSBLowLatencyIsocCompletion));
				debugIOLog("usbCompletion is %p", usbCompletion);
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
				debugIOLog("allocating neededSampleRateDescriptor");
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
	debugIOLog("assocEndpoint is %d", assocEndpoint);
#endif
	assocReq.type = kUSBIsoc;
	assocReq.direction = kUSBIn;
	assocReq.maxPacketSize = 3 + (kUSBDeviceSpeedHigh == mHubSpeed);
	assocReq.interval = 0xff;
	if (ourInterfaceNumber = mOutput.interfaceNumber) {
		mOutput.associatedPipe = mOutput.streamInterface->FindNextPipe(NULL, &assocReq);
		FailWithAction(NULL == mOutput.associatedPipe, result = kIOReturnError, Exit);
	} else {
		mInput.associatedPipe = mInput.streamInterface->FindNextPipe(NULL, &assocReq);
		FailWithAction(NULL == mInput.associatedPipe, result = kIOReturnError, Exit);
	}	
	framesUntilRefresh = kEMURefreshRate;// hardcoded value
	refreshInterval = 5;
	debugIOLog("framesUntilRefresh %d", framesUntilRefresh);
	debugIOLog("found sync endpoint");
	if (NULL == neededSampleRateDescriptor) {
		debugIOLog("allocating neededSampleRateDescriptor");
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
	IOReturn			result = kIOReturnError;
//	IOLockLock(mFormatLock);

	
	SInt32 offsetFrames = previouslyPreparedBufferOffset / mOutput.multFactor;
	debugIOLog2("clipOutputSamples firstSampleFrame=%u, numSampleFrames=%d, firstSampleFrame-offsetFrames =%d ",firstSampleFrame,numSampleFrames,firstSampleFrame-offsetFrames);	

	if (firstSampleFrame != nextExpectedOutputFrame) {
		debugIOLog("**** Output Hiccup!! firstSampleFrame=%d, nextExpectedOutputFrame=%d",firstSampleFrame,nextExpectedOutputFrame);		
	}
	nextExpectedOutputFrame = firstSampleFrame + numSampleFrames;
	
	if (firstSampleFrame > numSampleFrames) {
		debugIOLog("> offset delta %d",firstSampleFrame-offsetFrames);
	}
	if ((SInt32)firstSampleFrame - offsetFrames > 4000) {
		debugIOLog("****** derail alert!");
	}

	//if (offsetFrames > firstSampleFrame && firstSampleFrame > numSampleFrames) {
	//	debugIOLog("*** offset ahead by %d",offsetFrames-firstSampleFrame);
	//}
	
	if (0 == shouldStop && TRUE != inWriteCompletion) {
		UInt64	curUSBFrameNumber = mBus->GetFrameNumber();
#if 1
		if (mOutput.usbFrameToQueueAt < curUSBFrameNumber) {
			debugIOLog2("***** output usbFrameToQueueAt < curUSBFrameNumber, %lld %lld", mOutput.usbFrameToQueueAt, curUSBFrameNumber);			
			mOutput.usbFrameToQueueAt  = curUSBFrameNumber + mOutput.frameOffset;//kMinimumFrameOffset;
		}
#endif
		UInt64	framesLeftInQueue = mOutput.usbFrameToQueueAt - curUSBFrameNumber;
		SInt64	frameLimit = mOutput.numUSBTimeFrames * (mOutput.numUSBFrameListsToQueue -1);
		if (framesLeftInQueue <  frameLimit) {
			debugIOLog2 ("**** queue a write from clipOutputSamples: framesLeftInQueue = %ld", (UInt32)framesLeftInQueue);
			writeHandler (this, mOutput.usbCompletion[mOutput.currentFrameList].parameter, kIOReturnSuccess, 
							&mOutput.usbIsocFrames[mOutput.currentFrameList * mOutput.numUSBFramesPerList]);
		}
	} else if (TRUE == inWriteCompletion) {
		debugIOLog("**in completion");
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
	
	//debugIOLog("clipOutputSamples: numSampleFrames = %d",numSampleFrames);
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
// routine called by the frameHandler
void EMUUSBAudioEngine::GatherInputSamples(IOUSBLowLatencyIsocFrame * pFrames) {
	AbsoluteTime	time, timeOffset;
	UInt32			numBytesToCopy = 0; // number of bytes to move the dest ptr by each time
	UInt32			frameIndex = 0;
	UInt8*			buffStart = (UInt8*) mInput.bufferPtr;
	/*
#if LOCKING
	IOLockLock(mLock);
#endif
	*/
	if (mInput.bufferSize <= mInput.bufferOffset) // sanity checking to prevent going beyond the end of the allocated dest buffer
		mInput.bufferOffset = 0;
	
	UInt8*			dest = buffStart + mInput.bufferOffset;
	UInt8*			source = (UInt8*) readBuffer + (mInput.currentFrameList * readUSBFrameListSize);
	if (kUSBDeviceSpeedHigh == mHubSpeed) {// USB 2.0 section
		// iterate over 2 frames
		debugIOLog2("GatherInputSamples %d",mInput.bufferOffset/mInput.multFactor);
		while(frameIndex < RECORD_NUM_USB_FRAMES_PER_LIST && kUSBLowLatencyIsochTransferKey != pFrames[frameIndex].frStatus &&
			-1 != pFrames[frameIndex].frStatus) 
		{
			if (mDropStartingFrames <= 0)
			{
				UInt32	byteCount = pFrames[frameIndex].frActCount;
				if (byteCount != lastInputSize) {
					//debugIOLog("Happy Leap Sample!  new size = %d, current size = %d",byteCount,lastInputSize);
					lastInputSize = byteCount;
					lastInputFrames = byteCount / mInput.multFactor;
				}
				
				runningInputCount += lastInputFrames;
			//	debugIOLog("push %d",lastInputFrames);
				// save the # of frames to the framesize queue so that we generate an appropriately sized output packet
				PushFrameSize(lastInputFrames);
				SInt32	numBytesToEnd = mInput.bufferSize - mInput.bufferOffset; // assumes that bufferOffset <= bufferSize
				if (byteCount < numBytesToEnd) { // no wrap
					memcpy(dest, source, byteCount);
					mInput.bufferOffset += byteCount;
					numBytesToCopy = byteCount;// number of bytes the dest ptr will be moved by
				} else { // wrapping around - end up at bufferStart or bufferStart + an offset
					UInt64	stampTime;
					EmuTime systemTime;
					memcpy(dest, source, numBytesToEnd); // copy data into the remaining portion of the dest buffer
					dest = (UInt8*) buffStart;	// wrap around to the start
					UInt32	overflow = byteCount - numBytesToEnd; // the excess
					mInput.bufferOffset = overflow; // remember the location the dest ptr will be set to
					if (overflow)	// copy the remaining bytes into the front of the dest buffer
						memcpy(dest, source + numBytesToEnd, overflow);
					time = generateTimeStamp(frameIndex, numBytesToEnd, byteCount);
					takeTimeStamp (TRUE, &time);
					clock_get_uptime(&systemTime);
					stampTime = AbsoluteTime_to_scalar(&time);
					debugIOLog2("GatherInputSamples frame %d stampTime %llu system time %llu byteCount %d", frameIndex, stampTime, systemTime,byteCount);	
					numBytesToCopy = overflow;
				}
			}
			else if(pFrames[frameIndex].frActCount && mDropStartingFrames > 0) // first frames might have zero length
			{
				mDropStartingFrames--;
			}
			
	//		debugIOLog("GatherInputSamples frameIndex is %d, numBytesToCopy %d", frameIndex, numBytesToCopy);
			++frameIndex;
			source += mInput.maxFrameSize; // each frame's frReqCount is set to maxFrameSize
			dest += numBytesToCopy;
		}
		// why did we leave loop?
#if 1
		if (frameIndex != RECORD_NUM_USB_FRAMES_PER_LIST) {
			if (kUSBLowLatencyIsochTransferKey == pFrames[frameIndex].frStatus || 
				-1  == pFrames[frameIndex].frStatus) {
				debugIOLog2("GatherInputSamples: Bad Frame Status [%d] 0x%x",frameIndex,pFrames[frameIndex].frStatus);
			} 
		}	
#endif
	} else {// usb 1.1 section - identical except for the computation of the time
		while(frameIndex < RECORD_NUM_USB_FRAMES_PER_LIST && kUSBLowLatencyIsochTransferKey != pFrames[frameIndex].frStatus) {
			UInt32	byteCount = pFrames[frameIndex].frActCount;
			SInt32	numBytesToEnd = mInput.bufferSize - mInput.bufferOffset;// assumes that bufferOffset <= bufferSize
			if (byteCount < numBytesToEnd) { // no wrapping
				memcpy(dest, source, byteCount);
				mInput.bufferOffset += byteCount;
				numBytesToCopy = byteCount;
			} else { // wrap around to either bufferStart or bufferStart + an offset
				memcpy(dest, source, numBytesToEnd); // copy data into the tail end of the dest buffer
				dest = (UInt8*) buffStart; // wrap around to the start
				UInt32	overflow = byteCount - numBytesToEnd; // compute the excess
				mInput.bufferOffset = overflow; // remember the position that the dest ptr will be set to
				if (overflow) // copy the remaining data into the front of the dest buffer
					memcpy(dest, source + numBytesToEnd, overflow);
				numBytesToCopy = overflow;
				// old method of generating the time used for time stamping
				UInt64	nanos = 0;
				time = pFrames[frameIndex].frTimeStamp;
				nanos = numBytesToCopy * 1000000 / byteCount;
				nanoseconds_to_absolutetime (nanos, EmuAbsoluteTimePtr(&timeOffset));
				SUB_ABSOLUTETIME (&time, &timeOffset);
				takeTimeStamp (TRUE, &time);
			}
			++frameIndex;
			source += mInput.maxFrameSize;
			dest += numBytesToCopy;
		}
	}
	/*
#if LOCKING
	IOLockUnlock(mLock);
#endif
	*/
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
		debugIOLog("EMUUSBAudioEngine::softwareVolumeChangedHandler output: channel= %d oldValue= %d newValue= %d", audioControl->getChannelID(), oldValue, newValue);
		
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
		debugIOLog("EMUUSBAudioEngine::softwareVolumeChangedHandler input: channel= %d oldValue= %d newValue= %d", audioControl->getChannelID(), oldValue, newValue);
		
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

void EMUUSBAudioEngine::CoalesceInputSamples(SInt32 numBytesToCoalesce, IOUSBLowLatencyIsocFrame * pFrames) {
	// implicit assumptions 
	//	a. currentFrameList never exceeds RECORD_NUM_USB_FRAME_LISTS
	//	b. bufferOffset never exceeds bufferSize
#if LOCKING
	IOLockLock(mLock);
#endif	
	UInt32			frameLimit = (mInput.numUSBFramesPerList * mInput.numUSBFrameLists);// -1
	UInt32			indexBase = mInput.currentFrameList * mInput.numUSBFramesPerList;
	UInt32			usbFrameIndex = 0;
	UInt32			numBytesToCopy = 0;// ultimately dependent on frActCount
	SInt32			numBytesToEnd = 0;
	UInt32			originalBufferOffset = mInput.bufferOffset;
	UInt32			framesChecked = 0;
	SInt32			numBytesLeft = numBytesToCoalesce;
	//debugIOLog ("coalesce from %ld %ld bytes", originalBufferOffset, numBytesToCoalesce);
	debugIOLog2 ("CoalesceInputSamples currentFrameList %ld numBytesToCoalesce %ld", mInput.currentFrameList, numBytesToCoalesce);

	UInt8*	buffStart = (UInt8*) mInput.bufferPtr;
	if (mInput.bufferOffset >=  mInput.bufferSize)	// sanity checking
		mInput.bufferOffset = 0;
	UInt8*	dest = buffStart + mInput.bufferOffset;

	UInt8*	source = (UInt8 *)readBuffer + (mInput.currentFrameList * readUSBFrameListSize);
	bool	looped = false;// flag to ensure we only go through the array of frames just 2x. 

	while (framesChecked < frameLimit && kUSBLowLatencyIsochTransferKey != pFrames[usbFrameIndex].frStatus && 
		-1 != pFrames[usbFrameIndex].frStatus) {
		numBytesToEnd =  (SInt32) mInput.bufferSize - mInput.bufferOffset;
		UInt32	byteCount = pFrames[usbFrameIndex].frActCount;
		UInt32  inputSize = 0;
				// save the # of frames to the framesize queue so that we generate an appropriately sized output packet
		if (byteCount < numBytesToEnd) {// not wrapping
			memcpy(dest, source, byteCount);
			mInput.bufferOffset += byteCount;
			numBytesToCopy = byteCount;
		} else {// at least equal to or greater than numBytesToEnd
			inputSize = numBytesToEnd;
			UInt32	overflow = byteCount - numBytesToEnd;// the excess byte count past the bufferEnd
			memcpy(dest, source, numBytesToEnd);// copy data right up to the buffer end
			dest = (UInt8*) buffStart;// reset the destination to the start of the buffer
			debugIOLog("wrapping!!! overflow is %d", overflow);
			mInput.bufferOffset = overflow;
			if (overflow) // time stamp only when wrapping
			{
				memcpy(dest, source + numBytesToEnd, overflow); // copy the remaining data into the buffer				
			}
			debugIOLog("wrapping in CoalesceInputSamples");
			numBytesToCopy = mInput.bufferOffset;// in sync with the bufferOffset
		}
	
//		debugIOLog("byteCount is %d, numBytesToCopy is %d, framesChecked is %d, usbFrameIndex is %d", byteCount, numBytesToCopy, framesChecked, usbFrameIndex);
		numBytesLeft -= byteCount;
		dest += numBytesToCopy;// advance destination
		source += mInput.maxFrameSize;// @ frReqCount is set to maxFrameSize must never exceed numUSBFrameLists * readUSBFrameListSize
		++usbFrameIndex;
		++framesChecked;
		 
		//debugIOLog("CoalesceInputSamples: numBytesToCopy %d",numBytesToCopy); 
		 
		if (0 >= numBytesLeft)// we're done
			break;
		
		if ((usbFrameIndex + indexBase) >= frameLimit) {
			/*
			if (looped)// make sure we go through the frames just once more.
				break;
			looped = true;
			*/
			pFrames = &mInput.usbIsocFrames[0]; // wrap around the frame list and keep trying to coalesce
			usbFrameIndex = 0;
			source = (UInt8 *)readBuffer;
			debugIOLog ("wrapping coalesce numBytesToCoalesce = %d", numBytesToCoalesce); 
		}
		
	}
	// why did we leave loop?
#if 0
	if (numBytesLeft > 0) {
		debugIOLog("only coalesced %d bytes, left %d bytes uncoalesced :(",numBytesToCoalesce - numBytesLeft, numBytesLeft);
		if (kUSBLowLatencyIsochTransferKey == pFrames[usbFrameIndex].frStatus || 
			-1  == pFrames[usbFrameIndex].frStatus) {
				debugIOLog("Bad Frame Status %d",pFrames[usbFrameIndex].frStatus);
		} else if (framesChecked >= frameLimit) {
			debugIOLog("exceeded frameLimit %d, framesChecked %d",frameLimit,framesChecked);
		}
	}
#endif

	//debugIOLog("bufferOffset is %d originalBufferOffset %d", mInput.bufferOffset, originalBufferOffset);	
	mInput.bufferOffset = originalBufferOffset;
#if LOCKING
	IOLockUnlock(mLock);
#endif
}

IOReturn EMUUSBAudioEngine::convertInputSamples (const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame,
													UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat,
													IOAudioStream *audioStream) {													
	UInt32		lastSampleByte = (firstSampleFrame + numSampleFrames) * mInput.multFactor;// max number of bytes to get
	UInt32		windowStartByte = 0;
	UInt32		windowEndByte = 0;
	IOReturn	result;
	
	debugIOLog2("convertInputSamples firstSampleFrame=%u, numSampleFrames=%d",firstSampleFrame,numSampleFrames);

	if (firstSampleFrame != nextExpectedFrame) {
		debugIOLog("****** HICCUP firstSampleFrame=%d, nextExpectedFrame=%d",firstSampleFrame,nextExpectedFrame);	
	}
	
	if (!shouldStop && !inReadCompletion) {
		UInt64	curInputUSBFrameNumber = mBus->GetFrameNumber();
		SInt64	framesLeftInQueue = 0;
		
		if (mInput.usbFrameToQueueAt > curInputUSBFrameNumber)// Sanity check. Otherwise we'll be in trouble.
		{
			framesLeftInQueue = mInput.usbFrameToQueueAt - curInputUSBFrameNumber;
		}
		
		if (framesLeftInQueue <  (mInput.numUSBTimeFrames * (mInput.numUSBFrameListsToQueue / 2)) / 2) {
			SInt64	frameLimit = mInput.numUSBTimeFrames * (mInput.numUSBFrameListsToQueue -1);
			while (framesLeftInQueue < frameLimit && !shouldStop) {
				debugIOLog2 ("queue a read from convertInputSamples: framesLeftInQueue = %ld", (UInt32)framesLeftInQueue);
				readHandler (this, mInput.usbCompletion[mInput.currentFrameList].parameter, kIOReturnSuccess, 
							&mInput.usbIsocFrames[mInput.currentFrameList * mInput.numUSBFramesPerList]);
				curInputUSBFrameNumber = mBus->GetFrameNumber();
				framesLeftInQueue = mInput.usbFrameToQueueAt - curInputUSBFrameNumber;
			}
		}
	}


	windowStartByte = (mInput.bufferOffset + 1) * ((mInput.bufferOffset + 1) <= mInput.bufferSize);// starting position
	
	windowEndByte = windowStartByte + (mInput.numUSBFrameListsToQueue * readUSBFrameListSize);// end position
	windowEndByte -= mInput.bufferSize  * (windowEndByte > mInput.bufferSize);// MUST always be less than the end
	if ((windowStartByte < lastSampleByte && windowEndByte > lastSampleByte) ||
		(windowEndByte > lastSampleByte && windowStartByte > windowEndByte) ||
		(windowStartByte < lastSampleByte && windowStartByte > windowEndByte && windowEndByte < lastSampleByte)) {

	//if ((windowEndByte > lastSampleByte) && (windowStartByte < lastSampleByte || windowStartByte > windowEndByte) ||
	//	(windowStartByte < lastSampleByte && windowStartByte > windowEndByte && windowEndByte < lastSampleByte)) {
		//debugIOLog ("firstSampleFrame %ld, numSampleFrames %ld, lastSampleByte %ld, currentFrameList %ld, bufferOffset %ld, windowStartByte %ld, windowEndByte %ld",
		//			firstSampleFrame, numSampleFrames, lastSampleByte, mInput.currentFrameList, mInput.bufferOffset, windowStartByte, windowEndByte);

		SInt32	numBytesToCoalesce = lastSampleByte - mInput.bufferOffset;
		numBytesToCoalesce += (mInput.bufferSize * (numBytesToCoalesce < 0));
//		debugIOLog("numBytesToCoalesce %d", numBytesToCoalesce);
		if (numBytesToCoalesce > 0) {
			CoalesceInputSamples(numBytesToCoalesce, &mInput.usbIsocFrames[mInput.currentFrameList * mInput.numUSBFramesPerList]);
		}
	}
		
    result = convertFromEMUUSBAudioInputStreamNoWrap (sampleBuf, destBuf, firstSampleFrame, numSampleFrames, streamFormat);
//debugIOLog("convertInputSamples after convertFromEMUUSBAudiomInput.audioStreamNoWrap %d", result);
	if (mPlugin) 
		mPlugin->pluginProcessInput ((float *)destBuf + (firstSampleFrame * streamFormat->fNumChannels), numSampleFrames, streamFormat->fNumChannels);
#if 0
	lastInputFrames = numSampleFrames;
	PushFrameSize(lastInputFrames);
	runningInputCount += lastInputFrames;
#endif

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

	return result;
}

// NB  byteCount MUST never == 0. If this computation is incorrect, we will encounter audio artifacts
AbsoluteTime EMUUSBAudioEngine::generateTimeStamp(UInt32 usbFrameIndex, UInt32 preWrapBytes, UInt32 byteCount) {
	UInt64			time_nanos = 0;
	AbsoluteTime	time = {0,0};
	AbsoluteTime	refWallTime = {0, 0};
	UInt64			referenceWallTime_nanos = 0;
	UInt64			referenceFrame = 0ull;
	
 	UInt32			usedFrameIndex = usbFrameIndex >> kPollIntervalShift;
 	UInt64			thisFrameNum = mInput.frameQueuedForList[mInput.currentFrameList] + usedFrameIndex;	
	
	byteCount *= kNumberOfFramesPerMillisecond;
	
	debugIOLog2("**** generateTimeStamp usedFrameIndex %d byteCount %d",usedFrameIndex,byteCount);
	
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
			debugIOLog("**** zero byteCount in generateTimeStamp ****");
		}
		absolutetime_to_nanoseconds(EmuAbsoluteTime(refWallTime), &referenceWallTime_nanos);
		if (!referenceAhead)
			time_nanos += referenceWallTime_nanos;
		else
			time_nanos = referenceWallTime_nanos - time_nanos;
		time_nanos += (usbAudioDevice->mWallTimePerUSBCycle / kWallTimeExtraPrecision);
//		debugIOLog("stampTime is %llu", time_nanos);
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
			debugIOLog("can't resolve direction from endpoints, resorting to slimy hack instead");
			// interface 1 is output and interface 2 is input in the current 0202USB and 0404USB devices
			direction = (ourInterfaceNumber == 1) ? kUSBOut : kUSBIn;
		}
		
		debugIOLog("direction of interface %d is %d (In=%d,Out=%d)",ourInterfaceNumber,direction,kUSBIn,kUSBOut);
		
		// reference the appropriate StreamInfo for input or output
		StreamInfo &info = (direction == kUSBIn) ? mInput : mOutput;
		info.interfaceNumber = ourInterfaceNumber;
		info.streamDirection = direction;
		info.streamInterface = streamInterface;
		
		newSampleRate.fraction = 0;
		newSampleRate.whole = usbAudioDevice->getHardwareSampleRate();// get the sample rate the device was set to
		debugIOLog("hardware sample rate is %d", newSampleRate.whole);
		info.numChannels = kChannelCount_10;// try 4 channels first - uh... make that 10... uh... this is stupid.
		mChannelWidth = kBitDepth_24bits;
		UInt32	altChannelWidth = kBitDepth_16bits;
		
		while (!found && info.numChannels >= kChannelCount_STEREO) {// will never be a mono device
			debugIOLog("Finding interface %d numChannels %d sampleRate %d", ourInterfaceNumber, info.numChannels, newSampleRate.whole);
			newAltSettingID = usbAudio->FindAltInterfaceWithSettings (ourInterfaceNumber, info.numChannels, mChannelWidth, newSampleRate.whole);
			if (255 == newAltSettingID) {// try finding 16 bit setting
				newAltSettingID = usbAudio->FindAltInterfaceWithSettings(ourInterfaceNumber, info.numChannels, altChannelWidth, newSampleRate.whole);
				mChannelWidth = altChannelWidth;
			}
			
			if (255 != newAltSettingID) {
#ifdef DEBUGLOGGING
				debugIOLog("newAltSettingID %d", newAltSettingID);
				UInt16	format = usbAudio->GetFormat(ourInterfaceNumber, newAltSettingID);
				debugIOLog("format is %d", format);
#endif

				UInt32	pollInterval = (UInt32) usbAudio->GetEndpointPollInterval(ourInterfaceNumber, newAltSettingID, info.streamDirection);
				mPollInterval = 1 << (pollInterval -1);
				debugIOLog("direction is %d pollInterval is %d", info.streamDirection, mPollInterval);
				if ((1 == mPollInterval) || (8 == mPollInterval)) {
					debugIOLog("found channel count %d", info.numChannels);
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
			debugIOLog("last resort in GetDefaultSettings");
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

OSString * EMUUSBAudioEngine::getGlobalUniqueID () {
    char *				uniqueIDStr = NULL;
    OSString *			uniqueID = NULL;
	OSNumber *			usbLocation = NULL;
	IOReturn			err = kIOReturnSuccess;
    UInt32				uniqueIDSize = 128; // ??? - JH
	UInt32				locationID = 0;
	UInt8				stringIndex = 0;
	UInt8				interfaceNumber = 0;
	char				productString[kStringBufferSize];
	char				manufacturerString[kStringBufferSize];
	char				serialNumberString[kStringBufferSize];
	char				locationIDString[kStringBufferSize];
	char				interfaceNumberString[4];	// biggest string possible is "255"

	uniqueIDSize += strlen ("EMUUSBAudioEngine");

	manufacturerString[0] = 0;
	stringIndex = mInput.streamInterface->GetDevice()->GetManufacturerStringIndex ();
	if (0 != stringIndex)
		mInput.streamInterface->GetDevice()->GetStringDescriptor (stringIndex, manufacturerString, kStringBufferSize);

	if (0 == manufacturerString[0] || kIOReturnSuccess != err)
		strcpy (manufacturerString, "Unknown Manufacturer");
	
	uniqueIDSize += strlen (manufacturerString);

	productString[0] = 0;
	stringIndex = mInput.streamInterface->GetDevice()->GetProductStringIndex ();
	if (0 != stringIndex)
		mInput.streamInterface->GetDevice()->GetStringDescriptor (stringIndex, productString, kStringBufferSize);

	if (0 == productString[0] || kIOReturnSuccess != err) 
		strcpy (productString, "Unknown USB Audio Device");
	
	uniqueIDSize += strlen (productString);

	serialNumberString[0] = 0;
	stringIndex = mInput.streamInterface->GetDevice()->GetSerialNumberStringIndex ();
	stringIndex = 0;
	if (0 != stringIndex) 
		err = mInput.streamInterface->GetDevice()->GetStringDescriptor (stringIndex, serialNumberString, kStringBufferSize);

	if (0 == serialNumberString[0] || kIOReturnSuccess != err) {
		// device doesn't have a serial number, get its location ID
		usbLocation = OSDynamicCast (OSNumber, mInput.streamInterface->GetDevice()->getProperty (kUSBDevicePropertyLocationID));
		if (NULL != usbLocation) {
			locationID = usbLocation->unsigned32BitValue ();
			sprintf (locationIDString, "%x", locationID);
		} else {
			strcpy (locationIDString, "Unknown location");
		}
		uniqueIDSize += strlen (locationIDString);
	} else {
		// device has a serial number that we can use to track it
		debugIOLog ("device has a serial number = %s", serialNumberString);
		uniqueIDSize += strlen (serialNumberString);
	}

	interfaceNumber = mInput.streamInterface->GetInterfaceNumber ();
	sprintf (interfaceNumberString, "%d", interfaceNumber);
	uniqueIDSize += strlen (interfaceNumberString);

	uniqueIDStr = (char *)IOMalloc (uniqueIDSize);

	if (NULL != uniqueIDStr) {
		uniqueIDStr[0] = 0;
	
		if (0 == serialNumberString[0]) {
			sprintf (uniqueIDStr, "EMUUSBAudioEngine:%s:%s:%s:%s", manufacturerString, productString, locationIDString, interfaceNumberString);
		} else {
			sprintf (uniqueIDStr, "EMUUSBAudioEngine:%s:%s:%s:%s", manufacturerString, productString, serialNumberString, interfaceNumberString);
		}
	
		uniqueID = OSString::withCString (uniqueIDStr);
		debugIOLog ("getGlobalUniqueID = %s", uniqueIDStr);
		IOFree (uniqueIDStr, uniqueIDSize);
	}

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
	
    debugIOLog ("+EMUUSBAudioEngine[%p]::initHardware (%p)", this, provider);
	terminatingDriver = FALSE;
#if LOCKING
	mLock = NULL;
	mWriteLock = NULL;
	mFormatLock = NULL;
#endif
    FailIf (FALSE == super::initHardware (provider), Exit);

	FailIf (NULL == usbAudioDevice, Exit); // (AC mod)

    EMUUSBAudioConfigObject*	usbAudio = usbAudioDevice->GetUSBAudioConfigObject();
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

	debugIOLog("input alternateID %d",mInput.alternateSettingID);
	debugIOLog("output alternatID %d",mOutput.alternateSettingID);
		
		
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
	debugIOLog("sampleRate %d", sampleRate.whole);
	
	CalculateSamplesPerFrame (sampleRate.whole, &averageFrameSamples, &additionalSampleFrameFreq);
	// this calcs (frame size, etc.) could probably be simplified, but I'm leaving them this way for now (AC)
	mInput.multFactor = mInput.numChannels * (mChannelWidth / 8);
	mOutput.multFactor = mOutput.numChannels * (mChannelWidth / 8);
	mInput.maxFrameSize = (averageFrameSamples + 1) * mInput.multFactor;
	mOutput.maxFrameSize = (averageFrameSamples + 1) * mOutput.multFactor; 
	debugIOLog("in initHardware about to call initBuffers");
	
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
	debugIOLog("initHardware input NumChannels %d, bitWidth, %d", streamFormat.fNumChannels, streamFormat.fBitWidth);
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
	debugIOLog("opening input stream interface");
	FailIf (FALSE == mInput.streamInterface->open (this), Exit);		// Have to open the interface because calling setFormat will call performFormatChange, which expects the interface to be open.
	debugIOLog("setting input format");
	resultCode = mInput.audioStream->setFormat (&streamFormat);
	//	FailIf (kIOReturnSuccess != resultCode, Exit);
	debugIOLog("adding input audio stream");
	resultCode = addAudioStream (mInput.audioStream);
	FailIf (kIOReturnSuccess != resultCode, Exit);
	
	// now for output
	streamFormat.fNumChannels = usbAudio->GetNumChannels (mOutput.interfaceNumber, mOutput.alternateSettingID);
	streamFormat.fBitDepth = usbAudio->GetSampleSize (mOutput.interfaceNumber, mOutput.alternateSettingID);
	streamFormat.fBitWidth = usbAudio->GetSubframeSize (mOutput.interfaceNumber, mOutput.alternateSettingID) * 8;
	streamFormat.fAlignment = kIOAudioStreamAlignmentLowByte;
	streamFormat.fByteOrder = kIOAudioStreamByteOrderLittleEndian;
	streamFormat.fDriverTag = (mOutput.interfaceNumber << 16) | mOutput.alternateSettingID;
	debugIOLog("initHardware output NumChannels %d, bitWidth, %d", streamFormat.fNumChannels, streamFormat.fBitWidth);
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
	workLoop->addEventSource (startTimer);

	usbAudioDevice->doControlStuff(this, mOutput.interfaceNumber, mOutput.alternateSettingID);
	usbAudioDevice->doControlStuff(this, mInput.interfaceNumber, mInput.alternateSettingID);
	//usbAudioDevice->addCustomAudioControls(this);	// add any custom controls
    usbAudioDevice->activateAudioEngine (this, FALSE);

    resultBool = TRUE;
    
	sprintf (vendorIDCString, "0x%04X", mInput.streamInterface->GetDevice()->GetVendorID ());
	sprintf (productIDCString, "0x%04X", mInput.streamInterface->GetDevice()->GetProductID ());
	
	setupChannelNames();
		
	//setProperty (vendorIDCString, productIDCString);		// Ask for plugin to load (if it exists)
	IOService::registerService ();

Exit:
    debugIOLog("EMUUSBAudioEngine[%p]::initHardware(%p), resultCode = %x, resultBool = %d", this, provider, resultCode, resultBool);
    return resultBool;
}

void EMUUSBAudioEngine::registerPlugin (EMUUSBAudioPlugin * thePlugin) {
	mPlugin = thePlugin;
	mPluginInitThread = thread_call_allocate ((thread_call_func_t)pluginLoaded, (thread_call_param_t)this);
debugIOLog("EMUUSBAudioEngine::registerPlugin");
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

#if defined(ENABLE_TIMEBOMB)
    if (usbAudioDevice->checkForTimebomb())
    {
        return kIOReturnError;
    }
#endif // ENABLE_TIMEBOMB
    
    debugIOLog ("+EMUUSBAudioEngine[%p]::performAudioEngineStart ()", this);
	// Reset timestamping mechanism
	if (mPlugin)  {
		debugIOLog("starting plugin");
		mPlugin->pluginStart ();
	}

    if (!usbStreamRunning) {
		debugIOLog("about to start USB stream(s)");
        resultCode = startUSBStream();
	}

	debugIOLog("++EMUUSBAudioEngine[%p]::performEngineStart result is 0x%x direction %d", this, resultCode, mInput.streamDirection);
    return resultCode;
}

IOReturn EMUUSBAudioEngine::performAudioEngineStop() {
    debugIOLog("+EMUUSBAudioEngine[%p]::performAudioEngineStop ()", this);
	if (mPlugin) 
		mPlugin->pluginStop ();

    if (usbStreamRunning) 
        stopUSBStream ();

    debugIOLog("-EMUUSBAudioEngine[%p]::performAudioEngineStop()", this);
    return kIOReturnSuccess;
}

// This gets called when the HAL wants to select one of the different formats that we made available via mainStream->addAvailableFormat
IOReturn EMUUSBAudioEngine::performFormatChange (IOAudioStream *audioStream, const IOAudioStreamFormat *newFormat, const IOAudioSampleRate *newSampleRate) {
	if (!newFormat) 
		return kIOReturnSuccess;

//	IOLockLock(mFormatLock);
	IOReturn	result = kIOReturnError;
	UInt8			streamDirection;
	bool			needToRestartEngine = false;
	
	if (audioStream == mInput.audioStream) {
		debugIOLog("new format from INPUT");
		streamDirection = mInput.streamDirection;
	} else if (audioStream == mOutput.audioStream) {
		debugIOLog("new format from OUTPUT");
		streamDirection = mOutput.streamDirection;
	} else {
		debugIOLog("unrecognized stream");
		goto Exit;
	}
	debugIOLog ("+EMUUSBAudioEngine::performFormatChange existing  sampleRate is %d direction %d", sampleRate.whole, streamDirection);
	debugIOLog("newFormat->fBitDepth %d, newFormat->fNumChannels %d", newFormat->fBitDepth, newFormat->fNumChannels);
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
		debugIOLog("alternateFrameSize is %d", alternateFrameSize);
		if (mInput.maxFrameSize != alternateFrameSize) {
			debugIOLog("maxFrameSize %d alternateFrameSize %d", mInput.maxFrameSize, alternateFrameSize);
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
		debugIOLog("alternateFrameSize is %d", alternateFrameSize);
		if (mOutput.maxFrameSize != alternateFrameSize) {
			debugIOLog("maxFrameSize %d alternateFrameSize %d", mOutput.maxFrameSize, alternateFrameSize);
			mOutput.maxFrameSize = alternateFrameSize;
			needNewBuffers = true;
			
		}	
		
		// set the format - JH
			mOutput.audioStream->setFormat(newFormat,false);
		}
	
		
		if (needNewBuffers) {
			if (kIOReturnSuccess!= initBuffers()) {
				debugIOLog("problem with initBuffers in performFormatChange");
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
		debugIOLog ("called setSampleOffset with %d", mInput.numUSBFramesPerList);
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

IOReturn EMUUSBAudioEngine::PrepareWriteFrameList (UInt32 arrayIndex) {
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
		
//		debugIOLog("PrepareWriteFrameList stockSamplesInFrame %d numUSBFramesPerList %d", stockSamplesInFrame, numUSBFramesPerList);
		for (UInt32 numUSBFramesPrepared = 0; numUSBFramesPrepared < mOutput.numUSBFramesPerList; ++numUSBFramesPrepared) {
			integerSamplesInFrame = stockSamplesInFrame;// init to this stock value each time around
			//integerSamplesInFrame = lastInputFrames; // init to # of input frames;
			fractionalSamplesRemaining += averageSampleRate - (integerSamplesInFrame * 1000);
	//		debugIOLog("PrepareWriteFrameList fractionalSamplesRemaining %d", fractionalSamplesRemaining);
			if (fractionalSamplesRemaining >= 1000) {
				++integerSamplesInFrame;
	//			debugIOLog("inc integerSamplesInFrame");
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
			//debugIOLog("thisFrameSize %d",thisFrameSize);
			thisFrameSize *= mOutput.multFactor;
			//debugIOLog("PrepareWriteFrameList fractionalSamplesRemaining %d thisFrameSize %d", fractionalSamplesRemaining, thisFrameSize);
			if (thisFrameSize >= numBytesToBufferEnd) {
	//		debugIOLog("param has something %d", numUSBFramesPrepared);

#if 0				
				//perform zero test
				bool allZeroes = true;
				for (int i = lastPreparedByte; i < lastPreparedByte + numBytesToBufferEnd; ++i) {
				  //debugIOLog("%d",((Byte *) mOutput.bufferPtr)[i]);
				  if (((Byte *) mOutput.bufferPtr)[i] != 1) {
					allZeroes = false;
					break;
				  }
				}
				if (allZeroes) {
					++contiguousZeroes;
					//debugIOLog("*** detected all zeroes in write buffer %d ",numUSBFramesPrepared);
				} else { 
					contiguousZeroes = 0;
					//debugIOLog("*** non-zero buffer %d",mOutput.usbFrameToQueueAt+numUSBFramesPrepared);
					if (mOutput.usbFrameToQueueAt+numUSBFramesPrepared - lastNonZeroFrame > 1) {
						debugIOLog("*** non-zero discontinuity at %d",mOutput.usbFrameToQueueAt+numUSBFramesPrepared);
					}
					lastNonZeroFrame = mOutput.usbFrameToQueueAt+numUSBFramesPrepared;
				}
#endif
	
				lastPreparedByte = thisFrameSize - numBytesToBufferEnd;
				mOutput.usbCompletion[arrayIndex].parameter = (void *)(((numUSBFramesPrepared + 1) << 16) | lastPreparedByte);
				theWrapDescriptors[0]->initSubRange (mOutput.usbBufferDescriptor, previouslyPreparedBufferOffset, sampleBufferSize - previouslyPreparedBufferOffset, kIODirectionInOut);
				numBytesToBufferEnd = sampleBufferSize - lastPreparedByte;// reset
				haveWrapped = true;
			} else {
#if 0
			    //perform zero test
				bool allZeroes = true;
				for (int i = lastPreparedByte; i < lastPreparedByte + thisFrameSize; ++i) {
				  //debugIOLog("%d",((Byte *) mOutput.bufferPtr)[i]);
				  if (((Byte *) mOutput.bufferPtr)[i] != 1) {
					allZeroes = false;
					break;
				  }
				}
				if (allZeroes) {
					++contiguousZeroes;				
					//debugIOLog("*** detected all zeroes in write buffer %d ",numUSBFramesPrepared);
				} else {
					contiguousZeroes = 0;
					//debugIOLog("*** non-zero buffer %d",mOutput.usbFrameToQueueAt+numUSBFramesPrepared);
					if (mOutput.usbFrameToQueueAt+numUSBFramesPrepared - lastNonZeroFrame > 1) {
						debugIOLog("*** non-zero discontinuity at %d --> %d",lastNonZeroFrame,mOutput.usbFrameToQueueAt+numUSBFramesPrepared);
					}
					lastNonZeroFrame = mOutput.usbFrameToQueueAt+numUSBFramesPrepared;
				}
#endif 
				thisFrameListSize += thisFrameSize;
				lastPreparedByte += thisFrameSize;
				numBytesToBufferEnd -= thisFrameSize;
	//			debugIOLog("no param");
			}
			mOutput.usbIsocFrames[firstFrame + numUSBFramesPrepared].frStatus = -1;
			mOutput.usbIsocFrames[firstFrame + numUSBFramesPrepared].frActCount = 0;
			mOutput.usbIsocFrames[firstFrame + numUSBFramesPrepared].frReqCount = thisFrameSize;
		}
	//	debugIOLog("Done with the numUSBFrames loop");
		//debugIOLog("num actual data frames in list %d",mOutput.numUSBFramesPerList - contiguousZeroes);
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

		debugIOLog2("PrepareWriteFrameList: lastPrepareFrame %d safeToEraseTo %d",lastPreparedByte / mOutput.multFactor, safeToEraseTo / mOutput.multFactor);

		safeToEraseTo = lastSafeErasePoint;
		lastSafeErasePoint = previouslyPreparedBufferOffset;
		previouslyPreparedBufferOffset = lastPreparedByte;
		result = kIOReturnSuccess;
	}
Exit:
	return result;
}

IOReturn EMUUSBAudioEngine::readFrameList (UInt32 frameListNum) {// frameListNum must be in the valid range 0 - 126
	IOReturn	result = kIOReturnError;
	if (mInput.pipe) {
		UInt32		firstFrame = frameListNum * mInput.numUSBFramesPerList;
		mInput.usbCompletion[frameListNum].target = (void*) this;
		mInput.usbCompletion[frameListNum].action = readHandler;
		mInput.usbCompletion[frameListNum].parameter = (void*) frameListNum; // remember the frameListNum

#if 0
		// unrolled for loop - numUSBFramesPerList is 4 (i have no idea why this was done [AC])
		mInput.usbIsocFrames[firstFrame].frStatus = mInput.usbIsocFrames[firstFrame + 1].frStatus = 
		mInput.usbIsocFrames[firstFrame + 2].frStatus = mInput.usbIsocFrames[firstFrame + 3].frStatus = -1;
		mInput.usbIsocFrames[firstFrame].frActCount = mInput.usbIsocFrames[firstFrame + 1].frActCount = 
		mInput.usbIsocFrames[firstFrame + 2].frActCount = mInput.usbIsocFrames[firstFrame + 3].frActCount = 0;
		mInput.usbIsocFrames[firstFrame].frReqCount = mInput.usbIsocFrames[firstFrame + 1].frReqCount = 
		mInput.usbIsocFrames[firstFrame + 2].frReqCount = mInput.usbIsocFrames[firstFrame + 3].frReqCount = mInput.maxFrameSize;
		*(UInt64 *)(&(mInput.usbIsocFrames[firstFrame].frTimeStamp)) = *(UInt64 *)(&(mInput.usbIsocFrames[firstFrame + 1].frTimeStamp)) = 
		*(UInt64 *)(&(mInput.usbIsocFrames[firstFrame + 2].frTimeStamp)) = *(UInt64 *)(&(mInput.usbIsocFrames[firstFrame + 3].frTimeStamp)) = 0ul;
#else
		// un-unrolled loop
		for (int i = 0; i < mInput.numUSBFramesPerList; ++i) {
			mInput.usbIsocFrames[firstFrame+i].frStatus = -1;
			mInput.usbIsocFrames[firstFrame+i].frActCount = 0;
			mInput.usbIsocFrames[firstFrame+i].frReqCount = mInput.maxFrameSize;
			*(UInt64 *)(&(mInput.usbIsocFrames[firstFrame + i].frTimeStamp)) = 	0ul;
		}
#endif
		result = mInput.pipe->Read(mInput.bufferDescriptors[frameListNum], mInput.usbFrameToQueueAt, mInput.numUSBFramesPerList, &mInput.usbIsocFrames[firstFrame], &mInput.usbCompletion[frameListNum], 1);//mPollInterval);
		if (mInput.frameQueuedForList)
			mInput.frameQueuedForList[frameListNum] = mInput.usbFrameToQueueAt;
			
			
		mInput.usbFrameToQueueAt += mInput.numUSBTimeFrames;
	}
	return result;
}

void EMUUSBAudioEngine::readHandler (void * object, void * parameter, IOReturn result, IOUSBLowLatencyIsocFrame * pFrames) {
	EMUUSBAudioEngine *			engine = (EMUUSBAudioEngine *)object;
#if LOCKING
	IOLockLock(engine->mLock);
#endif

	FailIf (TRUE == engine->inReadCompletion, Exit);
	engine->inReadCompletion = TRUE;
	
	FailIf (NULL == engine->mInput.streamInterface, Exit);

	//debugIOLog("+ readhandler");	

	if (TRUE == engine->startingEngine) 
		engine->startingEngine = FALSE;	// The engine is fully started (it's no longer starting... it's running)
				
	UInt64	currentUSBFrameNumber = engine->mBus->GetFrameNumber();

	if (0 == engine->shouldStop && (SInt32)(engine->mInput.usbFrameToQueueAt - currentUSBFrameNumber) > (SInt32)(engine->mInput.numUSBTimeFrames * (engine->mInput.numUSBFrameListsToQueue- 1))) {
		// The frame list that this would have queued has already been queued by convertInputSamples
//#if DEBUGLOADING
		debugIOLog2 ("Not queuing a frame list in readHandler (%ld)", (SInt32)(engine->mInput.usbFrameToQueueAt - currentUSBFrameNumber));
//#endif
		goto Exit;
	}
	if (kIOReturnSuccess != result && kIOReturnAborted != result) {
		// skip ahead and see if that helps
		if (engine->mInput.usbFrameToQueueAt <= currentUSBFrameNumber) {// ensure that the usbFrameToQueueAt is at least > currentUSBFrameNumber
			engine->mInput.usbFrameToQueueAt = currentUSBFrameNumber + engine->mInput.frameOffset;//kMinimumFrameOffset;
			debugIOLog2("try adjusting the input frame number");
		}
	}

	if (kIOReturnAborted != result) {
		engine->GatherInputSamples(&engine->mInput.usbIsocFrames[engine->mInput.currentFrameList * engine->mInput.numUSBFramesPerList]);
	}
	
	if (!engine->shouldStop) {
		// keep incrementing until limit of numUSBFrameLists - 1 is reached
		engine->mInput.currentFrameList = (engine->mInput.currentFrameList + 1) * (engine->mInput.currentFrameList < RECORD_FRAME_LISTS_LIMIT);
		UInt32	frameListToRead = engine->mInput.currentFrameList - 1 + engine->mInput.numUSBFrameListsToQueue;
		frameListToRead -= engine->mInput.numUSBFrameLists * (frameListToRead >= engine->mInput.numUSBFrameLists);// crop the number of framesToRead
		
		engine->readFrameList(frameListToRead);
		
		//call the write handler (hey, this might actually work!)
		//engine->writeHandler(object,parameter,result,engine->mOutput.usbIsocFrames);
		
	} else  {// stop issued
		debugIOLog("++EMUUSBAudioEngine::readHandler() - stopping: %d", engine->shouldStop);
		++engine->shouldStop;
		if (engine->shouldStop == (engine->mInput.numUSBFrameListsToQueue + 1) && TRUE == engine->terminatingDriver) {
			engine->mInput.streamInterface->close(engine);
			engine->mInput.streamInterface = NULL;
		}
	}

Exit:
	engine->inReadCompletion = FALSE;
	//debugIOLog("- readhandler");	
#if LOCKING
	IOLockUnlock(engine->mLock);
#endif

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
	debugIOLog("averageFrameSamples=%d",averageFrameSamples);

	// bit width should be the same for both input and output
//	FailIf(inputFormat->fBitWidth != outputFormat->fBitWidth, Exit);
	mChannelWidth = inputFormat->fBitWidth;
	
	mDropStartingFrames = kNumberOfStartingFramesToDrop;
	
	UInt32	newInputMultFactor = (inputFormat->fBitWidth / 8) * inputFormat->fNumChannels;
	UInt32	newOutputMultFactor = (outputFormat->fBitWidth / 8) * outputFormat->fNumChannels;
	
	UInt32	altFrameSampleSize = averageFrameSamples + 1;
	mInput.currentFrameList = mOutput.currentFrameList = 0;
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
	debugIOLog("mInput.frameQueuedList");
	if (mInput.frameQueuedForList) {
		bzero(mInput.frameQueuedForList, sizeof(UInt64) * mInput.numUSBFrameLists);
	}
	if (mOutput.frameQueuedForList) {
		bzero(mOutput.frameQueuedForList, sizeof(UInt64) * mOutput.numUSBFrameLists);
	}

	debugIOLog("Isoc Frames / usbCompletions");	
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
			debugIOLog("startUSBStream: about to re-init buffers, input factor=%d (now %d), output factor=%d (now %d)",
					   mInput.multFactor,newInputMultFactor,mOutput.multFactor,newOutputMultFactor); 
			mInput.maxFrameSize = altFrameSampleSize * newInputMultFactor;
			mOutput.maxFrameSize = altFrameSampleSize * newOutputMultFactor; 
			mInput.multFactor = newInputMultFactor;
			mOutput.multFactor = newOutputMultFactor;
			debugIOLog("pre initBuffers");
			beginConfigurationChange();
			initBuffers();
			completeConfigurationChange();
			debugIOLog("post initBuffers");
	}
		
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
		debugIOLog("createInputPipe");
		mInput.pipe = mInput.streamInterface->FindNextPipe (NULL, &audioIsochEndpoint);
		FailIf (NULL == mInput.pipe, Exit);
		mInput.pipe->retain ();
	}
	
	UInt8	address = usbAudio->GetIsocEndpointAddress(mInput.interfaceNumber, mInput.alternateSettingID, mInput.streamDirection);
	UInt32	maxPacketSize = usbAudio->GetEndpointMaxPacketSize(mInput.interfaceNumber, mInput.alternateSettingID, address);
		
				
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
															//	debugIOLog("usbFrameToQueueAt is %x\n", usbFrameToQueueAt);
	
	//IOLog("the buffers are %x and %x\n", buffers[0], buffers[1]);
	
	*(UInt64 *) (&(mInput.usbIsocFrames[0].frTimeStamp)) = 0xFFFFFFFFFFFFFFFFull;
		
	if (NULL != mInput.associatedPipe) {
		nextSynchReadFrame = mInput.usbFrameToQueueAt;
		debugIOLog("read from associated input pipe");
		resultCode = mInput.associatedPipe->Read(neededSampleRateDescriptor, nextSynchReadFrame, 1,
													&(mSampleRateFrame), &sampleRateCompletion);
	}
#if PREPINPUT
	prepInputPipe();
#endif
	for (UInt32 frameListNum = mInput.currentFrameList; frameListNum < mInput.numUSBFrameListsToQueue; ++frameListNum) {
		debugIOLog("read frame list %d",frameListNum);
		readFrameList(frameListNum);
	}
#endif
	
	// and now the output
	
	resultCode = mOutput.streamInterface->SetAlternateInterface (this, mOutput.alternateSettingID);
	FailIf (kIOReturnSuccess != resultCode, Exit);
	
	debugIOLog("create output pipe");
	bzero(&audioIsochEndpoint,sizeof(audioIsochEndpoint));
	audioIsochEndpoint.type = kUSBIsoc;
	audioIsochEndpoint.direction = mOutput.streamDirection;	
	mOutput.pipe = mOutput.streamInterface->FindNextPipe (NULL, &audioIsochEndpoint);
	FailIf (NULL == mOutput.pipe, Exit);
	mOutput.pipe->retain ();
	debugIOLog("check for associated endpoint");
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
		debugIOLog("read from associated output pipe");
		resultCode = mOutput.associatedPipe->Read(neededSampleRateDescriptor, nextSynchReadFrame, 1,
												&(mSampleRateFrame), &sampleRateCompletion);
	}*/
		
	for (UInt32 frameListNum = mOutput.currentFrameList; frameListNum < mOutput.numUSBFrameListsToQueue; ++frameListNum)  {
		debugIOLog("write frame list %d",frameListNum);
		writeFrameList(frameListNum);		
	}

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
		//debugIOLog("getAnchor: frame %llu time %llu",theFrame,theTime);
	}
	return result;
}

// time off of read stream (??) [AC]
void EMUUSBAudioEngine::waitForFirstUSBFrameCompletion (OSObject * owner, IOTimerEventSource * sender) {
	EMUUSBAudioEngine *				usbAudioEngineObject  = OSDynamicCast (EMUUSBAudioEngine, owner);
//	AbsoluteTime						timeOffset;
	static UInt32					timeout = 60;

	FailIf ((NULL == usbAudioEngineObject) || (usbAudioEngineObject->isInactive()) || (0 != usbAudioEngineObject->shouldStop), Exit);
	if (0 == timeout || ( * (UInt64 *) ( & (usbAudioEngineObject->mInput.usbIsocFrames[0].frTimeStamp)) & 0xFFFFFFFF) != 0xFFFFFFFF) {
		AbsoluteTime	timeOffset, uptime;
	//	UInt64			systemTime;
		AbsoluteTime	systemTime;
		nanoseconds_to_absolutetime (kMinimumFrameOffset * kWallTimeConstant,EmuAbsoluteTimePtr(&timeOffset));
		clock_get_uptime(EmuAbsoluteTimePtr(&uptime));
		SUB_ABSOLUTETIME(&uptime, &timeOffset);
		usbAudioEngineObject->takeTimeStamp(FALSE, &uptime);
		clock_get_uptime(EmuAbsoluteTimePtr(&systemTime));
		debugIOLog("first time stamp %llu systemTime %llu", uptime, systemTime);

		usbAudioEngineObject->startingEngine = FALSE;			// It's started now.
		usbAudioEngineObject->startTimer->cancelTimeout();
		usbAudioEngineObject->startTimer->disable ();
	} else {
		// Requeue timer to check for the first frame completing
		usbAudioEngineObject->startTimer->setTimeoutUS (50);		// will call us back in 50 micoseconds
		--timeout;
	//	debugIOLog ("audio not running, requeuing timer (%ld)", timeout);
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
			debugIOLog("In writehandler Exit");
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

		//debugIOLog("writehandler: frame %d, parameter %u",curUSBFrameNumber,(UInt32) parameter);

		if (0 != parameter) {
			// Take a timestamp 
			AbsoluteTime systemTime;
			unsigned long long	/*systemTime,*/ stampTime;
			UInt32	byteOffset = (UInt32)parameter & 0x00FF;
			unsigned long	frameIndex = ((UInt32) parameter >>16) - 1;
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
			debugIOLog("write frameIndex %d stampTime %llu system time %llu \n", frameIndex, stampTime, systemTime);
			//self->takeTimeStamp(TRUE, &time);
			int delta = self->runningInputCount - self->runningOutputCount;
			int drift = self->lastDelta - delta;
			self->lastDelta = delta;
			debugIOLog("running counts: input %lu, output %lu, delta %ld, drift %ld",self->runningInputCount,self->runningOutputCount,delta, drift);
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
			debugIOLog("writeHandler: should stop");
			if (self->shouldStop == (self->mOutput.numUSBFrameListsToQueue + 1) && TRUE == self->terminatingDriver) {
				self->mOutput.streamInterface->close (self);
				self->mOutput.streamInterface = NULL;
			}
		} 
//		IOLockUnlock(self->mWriteLock);		
	} else {
		debugIOLog("*** Already in write completion!");
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

IOReturn EMUUSBAudioEngine::hardwareSampleRateChangedAux(const IOAudioSampleRate *newSampleRate, EMUUSBAudioEngine::StreamInfo &info) {
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
		debugIOLog("initBuffers");
		//, frameSize %d direction %d mPollInterval %d", frameSize, mDirection, mPollInterval);
		// clear up any existing buffers
		UInt32 inputSize = mInput.maxFrameSize;
		UInt32 outputSize = mOutput.maxFrameSize;
		UInt32	samplesPerFrame = inputSize / mInput.multFactor;
		debugIOLog("inputSize= %d multFactor= %d", inputSize, mInput.multFactor);
		debugIOLog("outputSize= %d multFactor= %d", outputSize, mOutput.multFactor);
		
	//	FailIf(samplesPerFrame != outputSize / mOutput.multFactor, Exit); - JH allocated size may be off 1 such as with AC3
		
		// this is total guesswork (AC)
	//	UInt32	numSamplesInBuffer = samplesPerFrame * 256;//
		UInt32	numSamplesInBuffer = PAGE_SIZE * (2 + (sampleRate.whole > 48000) + (sampleRate.whole > 96000));
		//UInt32 numSamplesInBuffer = samplesPerFrame * 16;
		
		
		mInput.bufferSize = numSamplesInBuffer * mInput.multFactor;
		mOutput.bufferSize = numSamplesInBuffer * mOutput.multFactor;
		
		// setup the input buffer
		if (NULL != mInput.bufferMemoryDescriptor) {
			mInput.audioStream->setSampleBuffer (NULL, 0);
			setNumSampleFramesPerBuffer (0);
			debugIOLog("releasing the input sampleBufferMemory descriptor");
			mInput.bufferMemoryDescriptor->complete();
			mInput.bufferMemoryDescriptor->release();
			mInput.bufferMemoryDescriptor = NULL;
			mInput.bufferPtr = NULL;// reset the sample buffer ptr
		}
		readUSBFrameListSize = inputSize * mInput.numUSBFramesPerList;
		debugIOLog("new bufferSize = %d numSamplesInBuffer = %d\n", mInput.bufferSize, numSamplesInBuffer );
		if (mInput.usbBufferDescriptor) {
			debugIOLog("disposing the mUSBBufferDescriptor input");
			mInput.usbBufferDescriptor->complete();
			mInput.usbBufferDescriptor->release();
			mInput.usbBufferDescriptor = NULL;
			readBuffer = NULL;
		}
		// read buffer section
		// following is the actual buffer that stuff gets read into
		debugIOLog("initBuffers readUSBFrameListSize %d numUSBFrameLists %d", readUSBFrameListSize, mInput.numUSBFrameLists);
#ifdef CONTIGUOUS
		mInput.usbBufferDescriptor = IOBufferMemoryDescriptor::withOptions(kIOMemoryPhysicallyContiguous| kIODirectionInOut, mInput.numUSBFrameLists * readUSBFrameListSize, page_size);
#else
		mInput.usbBufferDescriptor = IOBufferMemoryDescriptor::withOptions(kIODirectionInOut, mInput.numUSBFrameLists * readUSBFrameListSize, page_size);
#endif
		FailIf (NULL == mInput.usbBufferDescriptor, Exit);
		mInput.usbBufferDescriptor->prepare();
		readBuffer = mInput.usbBufferDescriptor->getBytesNoCopy();// get a valid ptr or NULL
		FailIf (NULL == readBuffer, Exit);
		// setup the sub ranges
		FailIf(NULL == mInput.bufferDescriptors, Exit);
		for (UInt32 i = 0; i < mInput.numUSBFrameLists; ++i) {
			if (mInput.bufferDescriptors[i]) {
//				debugIOLog("disposing input soundBufferDescriptors[%d]", i);
				mInput.bufferDescriptors[i]->complete();
				mInput.bufferDescriptors[i]->release();
				mInput.bufferDescriptors[i] = NULL;
			}
			mInput.bufferDescriptors[i] = OSTypeAlloc(IOSubMemoryDescriptor);
			bool initResult = mInput.bufferDescriptors[i]->initSubRange(mInput.usbBufferDescriptor, i * readUSBFrameListSize, readUSBFrameListSize, kIODirectionInOut);
//			debugIOLog("initSubRange soundBufferDescriptors[%d] %d", i, initResult);
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
		UInt32	offsetToSet = samplesPerFrame;
		
		// use samplesPerFrame as our "safety offset" unless explicitly set as a driver property
		OSNumber *safetyOffsetObj = OSDynamicCast(OSNumber,usbAudioDevice->getProperty("SafetyOffset"));
		if (safetyOffsetObj) {
			setSampleOffset(safetyOffsetObj->unsigned32BitValue());
		} else {
			setSampleOffset(2*offsetToSet);
		}
		setInputSampleLatency(2*samplesPerFrame);
		mInput.audioStream->setSampleBuffer(mInput.bufferPtr, mInput.bufferSize);
		
		//now the output buffer
		if (mOutput.usbBufferDescriptor) {
			debugIOLog("disposing the output mUSBBufferDescriptor");
			mOutput.audioStream->setSampleBuffer(NULL, 0);
			setNumSampleFramesPerBuffer(0);
			mOutput.usbBufferDescriptor->complete();
			mOutput.usbBufferDescriptor->release();
			mOutput.usbBufferDescriptor = NULL;
		}
		debugIOLog("In the out path, making new buffer with size of %d numSamplesInBuffer %d", mOutput.bufferSize, numSamplesInBuffer);
		mOutput.usbBufferDescriptor = IOBufferMemoryDescriptor::withOptions (kIODirectionInOut, mOutput.bufferSize, page_size);
		FailIf (NULL == mOutput.usbBufferDescriptor, Exit);
		mOutput.usbBufferDescriptor->prepare();
		FailIf(NULL == mOutput.bufferDescriptors, Exit);
		for (UInt32 i = 0; i < mOutput.numUSBFrameLists; ++i) {
			if (mOutput.bufferDescriptors[i]) {
//				debugIOLog("disposing output soundBufferDescriptors[%d]", i);
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

		debugIOLog("completed initBuffers");
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
		debugIOLog("prepInputPipe ok");
		usbFrameToQueueAt += kNumFramesToClear;
	}
	debugIOLog("prepInputPipe result %x", result);
}
// currently debating whether or not to perform Reads synchronously or asynchronously
void EMUUSBAudioEngine::prepInputHandler(void* object, void* frameListIndex, IOReturn result, IOUSBLowLatencyIsocFrame* pFrames) {
	EMUUSBAudioEngine*	engine = (EMUUSBAudioEngine*) object;
	debugIOLog("prepInputHandler result %x", result);
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
		debugIOLog("Found IOAudioEngineChannelNames");
		setProperty("IOAudioEngineChannelNames",dict);
	}
	obj = usbAudioDevice->getProperty("IOAudioEngineChannelCategoryNames");
	dict = OSDynamicCast(OSDictionary,obj);
	if (dict) {
		debugIOLog("Found IOAudioEngineChannelCategoryNames");
		setProperty("IOAudioEngineChannelCategoryNames",dict);
	}	
	obj = usbAudioDevice->getProperty("IOAudioEngineChannelNumberNames");
	dict = OSDynamicCast(OSDictionary,obj);
	if (dict) {
		debugIOLog("Found IOAudioEngineChannelNumberNames");
		setProperty("IOAudioEngineChannelNumberNames",dict);
	}	
}

void EMUUSBAudioEngine::PushFrameSize(UInt32 frameSize) {
	frameSizeQueue[frameSizeQueueBack++] = frameSize;
	frameSizeQueueBack %= FRAMESIZE_QUEUE_SIZE;	
	//debugIOLog("queued framesize %d, ptr %d",frameSize,frameSizeQueueBack);
}

UInt32 EMUUSBAudioEngine::PopFrameSize() {
	if (frameSizeQueueFront == frameSizeQueueBack) {
		debugIOLog("empty framesize queue");
		return 0;
	}
	UInt32 result = frameSizeQueue[frameSizeQueueFront++];
	frameSizeQueueFront %= FRAMESIZE_QUEUE_SIZE;
	//debugIOLog("dequeued framesize %d, read %d",result,frameSizeQueueFront);
	return result;
}

void EMUUSBAudioEngine::AddToLastFrameSize(SInt32 toAdd) {
   frameSizeQueue[frameSizeQueueFront] += toAdd;
}

void EMUUSBAudioEngine::ClearFrameSizes() {
	frameSizeQueueFront = frameSizeQueueBack = 0;
}
