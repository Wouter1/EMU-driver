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
//	File:		EMUUSBAudioDevice.cpp
//
//	Contains:	Support for the USB Audio Class Control Interface.
//			This includes support for exporting device controls
//			to the Audio HAL such as Volume, Bass, Treble and
//			Mute.
//
//			Future support will include parsing of the device
//			topology and exporting of all appropriate device
//			control functions through the Audio HAL.
//
//	Technology:	OS X
//
//--------------------------------------------------------------------------------
#include <libkern/c++/OSCollectionIterator.h>

#include <IOKit/IOLocks.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IORegistryEntry.h>
#include <IOKit/IOMessage.h>
#include <UserNotification/KUNCUserNotifications.h>
#include <kern/clock.h>

#include "EMUUSBAudioDevice.h"
#include "EMUUSBAudioDefines.h"
#include "EMUUSBDeviceDefines.h"
#include "EMUUSBUserClient.h"

#define super IOAudioDevice
#define	ENABLEHARDCONTROLS	0	// disable exposing hardware volume sliders to the OS
#define	ENABLESOFTCONTROLS	0	// disable software volume

OSDefineMetaClassAndStructors(EMUUSBAudioDevice, super)

void EMUUSBAudioDevice::free() {
    debugIOLog("+EMUUSBAudioDevice[%p]::free()", this);

    if(mInterfaceLock) {
        IORecursiveLockFree(mInterfaceLock);
        mInterfaceLock = NULL;
    }
	if (mInitHardwareThread) {
		thread_call_free(mInitHardwareThread);
		mInitHardwareThread = NULL;
	}
    RELEASEOBJ(mUSBAudioConfig);
	RELEASEOBJ(mRegisteredEngines);
	RELEASEOBJ(mControlGraph);
	if (mStatusBufferDesc) {
		mStatusBufferDesc->complete();
		mStatusBufferDesc->release();
		mStatusBufferDesc = NULL;
	}
	if (mDeviceStatusBuffer) {
		IOFree(mDeviceStatusBuffer, sizeof(UInt16));
		mDeviceStatusBuffer = NULL;
	}
	
	RELEASEOBJ(mStatusPipe);
	// release all allocated XU controls
	RELEASEOBJ(mXUChanged);
	RELEASEOBJ(mClockSelector);
	RELEASEOBJ(mDigitalIOStatus);
	RELEASEOBJ(mDigitalIOSyncSrc);
	RELEASEOBJ(mDigitalIOAsyncSrc);
	RELEASEOBJ(mDigitalIOSPDIF);
	RELEASEOBJ(mDevOptionCtrl);
#if DIRECTMONITOR
	RELEASEOBJ(mDirectMonStereo);
	RELEASEOBJ(mDirectMonitor);
	RELEASEOBJ(mMonitorInput);
	RELEASEOBJ(mMonitorOutput);
	RELEASEOBJ(mMonitorGain);
	RELEASEOBJ(mMonitorPan);
#endif
	RELEASEOBJ(mMonoControlsArray);
	super::free();
    debugIOLog("-EMUUSBAudioDevice[%p]::free()", this);
}

bool EMUUSBAudioDevice::ControlsStreamNumber(UInt8 streamNumber) {
	bool	doesControl = FALSE;

	if(mUSBAudioConfig) {
		UInt8*	streamNumbers;
		UInt8	numStreams;		
		mUSBAudioConfig->GetControlledStreamNumbers(&streamNumbers, &numStreams);
		for(UInt8 index = 0; index < numStreams; ++index) {
			debugIOLog("Checking stream %d against controled stream %d", streamNumber, streamNumbers[index]);
			if(streamNumber == streamNumbers[index]) {
				return true;// exit the for loop
			}
		}
	}

	return doesControl;
}

bool EMUUSBAudioDevice::start(IOService * provider) {
	bool			result = FALSE;
	
    debugIOLog("+ EMUUSBAudioDevice[%p]::start(%p)", this, provider);
	mControlInterface = OSDynamicCast(IOUSBInterface, provider);
	FailIf(FALSE == mControlInterface->open(this), Exit);
	mCurSampleRate = mNumEngines = 0;
	mInitHardwareThread = thread_call_allocate((thread_call_func_t)EMUUSBAudioDevice::initHardwareThread,(thread_call_param_t)this);
	FailIf(NULL == mInitHardwareThread, Exit);

    debugIOLog("- EMUUSBAudioDevice[%p]::start(%p)", this, provider);

	result = super::start(provider);				// Causes our initHardware routine to be called.
	debugIOLog("EMUUSBAudioDevice started about to registerService result is %d\n", result);
Exit:
	return result;
}

bool EMUUSBAudioDevice::initHardware(IOService * provider) {
	bool		result = FALSE;
	mStatusPipe = NULL;
	FailIf(NULL == mInitHardwareThread, Exit);
	thread_call_enter1(mInitHardwareThread,(void *)provider);

	result = TRUE;

Exit:
	return result;
}

void EMUUSBAudioDevice::initHardwareThread(EMUUSBAudioDevice * aua, void * provider) {
	if (aua) {
		IOCommandGate*	cg = aua->getCommandGate();
		if(cg) 
			cg->runAction(aua->initHardwareThreadAction, provider);// no use for the return result
	}
}

IOReturn EMUUSBAudioDevice::initHardwareThreadAction(OSObject * owner, void * provider, void * arg2, void * arg3, void * arg4) {
	IOReturn	result = kIOReturnError;

	if (owner) {
		EMUUSBAudioDevice*	aua = (EMUUSBAudioDevice*) owner;
		result = aua->protectedInitHardware((IOService *)provider);
	}
	return result;
}

IOReturn EMUUSBAudioDevice::protectedInitHardware(IOService * provider) {
	char							string[kStringBufferSize];
	UInt8							stringIndex;
    IOReturn						resultCode = kIOReturnError;
	UInt8 *							streamNumbers;
	UInt8							numStreams;
	debugIOLog("+EMUUSBAudioDevice[%p]::protectedInitHardware(%p)", this, provider);
	mTerminatingDriver = FALSE;
	if (mControlInterface) {//FailIf(NULL == mControlInterface, Exit);
		checkUHCI();// see whether we're attached via a UHCI controller
		mInterfaceNum = mControlInterface->GetInterfaceNumber();
		debugIOLog("There are %d configurations on this device", mControlInterface->GetDevice()->GetNumConfigurations());
		debugIOLog("Our control interface number is %d", mInterfaceNum);
		mUSBAudioConfig = EMUUSBAudioConfigObject::create(mControlInterface->GetDevice()->GetFullConfigurationDescriptor(0), mInterfaceNum);
		FailIf(NULL == mUSBAudioConfig, Exit);
		mQueryXU = 0;// initialized 
		mXUChanged = mClockSelector = mDigitalIOStatus = mDigitalIOSyncSrc = mDigitalIOAsyncSrc = mDigitalIOSPDIF = mDevOptionCtrl = NULL;
		mControlGraph = BuildConnectionGraph(mInterfaceNum);
		FailIf(NULL == mControlGraph, Exit);

		// Check to make sure that the control interface we loaded against has audio streaming interfaces and not just MIDI.
		mUSBAudioConfig->GetControlledStreamNumbers(&streamNumbers, &numStreams);
		debugIOLog("Num streams controlled = %d", numStreams);
		debugIOLog("GetNumStreamInterfaces = %d", mUSBAudioConfig->GetNumStreamInterfaces());
		FailIf(0 == numStreams, Exit);

		// figure out the number of available extension units 
		mAvailXUs = mUSBAudioConfig->GetNumExtensionUnits(mInterfaceNum, 0);
		
		if (mAvailXUs) {	// get the sample rate remembered by the hardware
			
			mClockRateXU = mUSBAudioConfig->FindExtensionUnitID(mInterfaceNum, kClockRate);
			if (mClockRateXU)
				getExtensionUnitSetting(mClockRateXU, kClockRateSelector, &mCurSampleRate, kStdDataLen);
		}

		string[0] = 0;
		stringIndex = mControlInterface->GetInterfaceStringIndex(); // try getting this first
		IOUSBDevice*	device = mControlInterface->GetDevice();	// this must always work
		mBus = device->GetBus();// get the bus
		if (!stringIndex)
			stringIndex = device->GetProductStringIndex();
			
		if(0 != stringIndex) {
			UInt32	i = 0;
			while (i < 2 && (kIOReturnSuccess != resultCode)) {
				if(kIOReturnSuccess != (resultCode = device->GetStringDescriptor(stringIndex, string, kStringBufferSize))) {// try again
					debugIOLog(" ++EMUUSBAudioDevice[%p]::protectedInitHardware - couldn't get string descriptor. Retrying ...", this);
					if (kIOReturnSuccess != (resultCode = device->GetStringDescriptor(stringIndex, string, kStringBufferSize)) && (i< 1)) {
						device->ResetDevice();
						IOSleep(50);
						debugIOLog("protectedInitHardware last retry\n");
					}
				}
				++i;
			}
		} 

		if(0 == string[0] || kIOReturnSuccess != resultCode) 
			strcpy(string, "Unknown USB Audio Device");
		
		setDeviceName(string);
		resultCode = kIOReturnError;// re-initialize
		string[0] = 0;
		stringIndex = device->GetManufacturerStringIndex();
		if(0 != stringIndex) 
			resultCode = device->GetStringDescriptor(stringIndex, string, kStringBufferSize);
		
		if(0 == string[0] || kIOReturnSuccess != resultCode) 
			strcpy(string, "Unknown Manufacturer");
		
		setManufacturerName(string);
		
		// USB transport unless TransportTypeOverride is set to nonzero (AC)

		OSNumber *ttOverride = OSDynamicCast(OSNumber,getProperty("TransportTypeOverride"));
		if (ttOverride && ttOverride->unsigned32BitValue()) {
			setDeviceTransportType(kIOAudioDeviceTransportTypeOther);		
		} else {
			setDeviceTransportType(kIOAudioDeviceTransportTypeUSB);
		}

		mInterfaceLock = IORecursiveLockAlloc();
		FailIf(NULL == mInterfaceLock, Exit);
		
		// try locating the interrupt status endpoint
		setupStatusFeedback();
		if (hasSampleRateXU()) {
			UInt8	theSampleRate = 0;// the default
			UInt32	newSampleRate = 44100;	// default 
			UInt32	count = 0;
			bool	done = false;
			while (count < kMaxTryCount && !done) {
				resultCode = getExtensionUnitSettings(kClockRate, kClockRateSelector,(void*) &theSampleRate, kStdDataLen);
				if (kIOReturnSuccess == resultCode) {
					done = true;
					switch (theSampleRate) {
							case sr_44kHz:
								newSampleRate = 44100;
								break;
							case sr_48kHz:
								newSampleRate = 48000;
								break;
							case sr_88kHz:
								newSampleRate = 88200;
								break;
							case sr_96kHz:
								newSampleRate = 96000;
								break;
							case sr_176kHz:
								newSampleRate = 176400;
								break;
							case sr_192kHz:
								newSampleRate = 192000;				
								break;
						}
				}
				++count;
			}
			debugIOLog("default sample rate is %d", newSampleRate);
			setHardwareSampleRate(newSampleRate);
		}
		resultCode = super::initHardware(provider);
		mWallTimePerUSBCycle =  1000000ull * kWallTimeExtraPrecision;
		mLastWallTimeNanos = mLastUSBFrame = mNewReferenceUSBFrame = 0ull;
		*((UInt64 *) &mReferenceWallTime) = 0ull;
		mAnchorResetCount = kRefreshCount;
		mUpdateTimer = IOTimerEventSource::timerEventSource(this, TimerAction);
		FailIf(NULL == mUpdateTimer, Exit);
		workLoop->addEventSource(mUpdateTimer);
		TimerAction(this, mUpdateTimer);
		setProperty(kIOAudioEngineCoreAudioPlugInKey, "EMUUSBAudio.kext/Contents/Plugins/EMUHALPlugin.bundle");	//"EMUUSBAudio.kext/Contents/Plugins/
		IOService::registerService();
		//<AC mod>
		// init engine - see what happens
#if 1
		// need to make sure we're not crossing paths with a terminating driver (hey, it happens)
		if (FALSE == mTerminatingDriver) {
			EMUUSBAudioEngine *audioEngine = NULL;
			audioEngine = new EMUUSBAudioEngine;
			FailIf(NULL == audioEngine->init(NULL), Exit);
			debugIOLog("activate audio engine");
			if (activateAudioEngine(audioEngine)) {
				debugIOLog("failed to activate audio engine");
				resultCode = kIOReturnError;
				goto Exit;
			}
			debugIOLog("done activating engine");
			audioEngine->release();
			mAudioEngine = audioEngine; //used for releasing at end
		}
#endif
		//</AC mod>
	}
Exit:
	debugIOLog("-EMUUSBAudioDevice[%p]::start(%p)", this, provider);
	if (mInitHardwareThread) {
		thread_call_free(mInitHardwareThread);
		mInitHardwareThread = NULL;
	}
	return resultCode;
}

void EMUUSBAudioDevice::checkUHCI() {
	const IORegistryPlane*	svcPlane = getPlane(kIOServicePlane);
	IOUSBDevice*			device = OSDynamicCast(IOUSBDevice, mControlInterface->GetDevice());
	mUHCI = false;
	if (device) {
		IORegistryEntry*	parent = device->getParentEntry(svcPlane);
		if (parent) {// only anticipate iterating one level up - with the current IOKit
			IOService*	currentEntry = OSDynamicCast(IOService, parent);
			debugIOLog("checkUHCI currentEntry %x", currentEntry);
			if (currentEntry) {
				mUHCI = !strcmp(currentEntry->getName(svcPlane), "AppleUSBUHCI");
			}
			debugIOLog("UHCI connection is %d", mUHCI);
		}
	}
}

#if defined(ENABLE_TIMEBOMB)
#warning "NOTE: Timebomb is enabled in the driver."
// This method returns true if the driver has expired and
// should be disabled.
//
bool
EMUUSBAudioDevice::checkForTimebomb()
{
    uint32_t seconds, nanosecs;
    clock_get_calendar_nanotime(&seconds, &nanosecs);
    
    // TIMEBOMB_TIME is specified in seconds since the beginning of the Unix
    // Epoch (Jan 1, 1970).   
    if (seconds > TIMEBOMB_TIME) 
    {
        // NOTE: This notice won't be visible if the user isn't
        // logged in when this function is called.
        KUNCUserNotificationDisplayNotice( 0,
            0,
            NULL,
            NULL,
            NULL,
            "E-MU USB Driver Beta period has expired.",
            "Please download a more recent version of the driver from www.emu.com.",
            "OK" );

        return true;
    }
    else
        return false;
}
#endif // ENABLE_TIMEBOMB

// possible ways to do this
// start a read of the interrupt endpoint
// in the completion routine, schedule the next read or start the timer
// OR - add a timer that will fire off
// and start the checking as done below

void EMUUSBAudioDevice::setupStatusFeedback() {// find the feedback endpoint & start the feedback cycle going
	IOUSBFindEndpointRequest	statusEndpoint;
	
	statusEndpoint.type = kUSBInterrupt;
	statusEndpoint.direction = kUSBIn;
	statusEndpoint.maxPacketSize = kStatusPacketSize;
	statusEndpoint.interval = 0xFF;

	mStatusPipe = mControlInterface->FindNextPipe(NULL, &statusEndpoint);
	if (mStatusPipe) {// the endpoint exists
		mStatusPipe->retain();// retain until tear down
		mDeviceStatusBuffer = (UInt16*) IOMalloc(sizeof(UInt16));
		if (mDeviceStatusBuffer) {
			mStatusBufferDesc = IOMemoryDescriptor::withAddress(mDeviceStatusBuffer, kStatusPacketSize, kIODirectionIn);
			if (mStatusBufferDesc) {
				mStatusBufferDesc->prepare();
				mStatusCheckCompletion.target = (void*) this;
				mStatusCheckCompletion.action = statusHandler;
				mStatusCheckCompletion.parameter = 0;
				mStatusCheckTimer = IOTimerEventSource::timerEventSource(this, StatusAction);
				if (mStatusCheckTimer) {
					workLoop->addEventSource(mStatusCheckTimer);// add timer action to the workloop
					StatusAction(this, mStatusCheckTimer);// fire off
				}
			}
		}
	}
}

IOReturn EMUUSBAudioDevice::performPowerStateChange(IOAudioDevicePowerState oldPowerState, IOAudioDevicePowerState newPowerState, UInt32 *microSecsUntilComplete) 
{
	IOReturn	result = super::performPowerStateChange(oldPowerState, newPowerState, microSecsUntilComplete);

	debugIOLog("+EMUUSBAudioDevice[%p]::performPowerStateChange(%d, %d, %p)", this, oldPowerState, newPowerState, microSecsUntilComplete);
	

	// Ripped of from Apple USB reference (hey, it might actually work) [AC]
	// We need to stop the time stamp rate timer now
	if	(		(mUpdateTimer)
			&&	(kIOAudioDeviceSleep == newPowerState))
	{
		// Stop the timer and reset the anchor.
		debugIOLog ("? AppleUSBAudioDevice[%p]::performPowerStateChange () - Going to sleep - stopping the rate timer.", this);
		mUpdateTimer->cancelTimeout ();
		// mUpdateTimer->disable();
		
		// The frame/time correlation isn't preserved across sleep/wake
		mNewReferenceUSBFrame = 0ull;		
		mLastUSBFrame = 0ull;
		( * (UInt64 *) &mNewReferenceWallTime) = 0ull;
		mLastWallTimeNanos = 0ull;
	}

	if(oldPowerState == kIOAudioDeviceSleep) {
	
		// Ripped of from Apple USB reference (hey, it might actually work) [AC]
		// A new anchor should be taken at the first opportunity. The timer action will handle this with the following instruction.
		mAnchorResetCount = kRefreshCount;
		
		// [rdar://4380545] We need to reset the wall time per USB cycle because the frame number could become invalid entering sleep.
		mWallTimePerUSBCycle = 1000000ull * kWallTimeExtraPrecision;
		
		// [rdar://4234453] Reset the device after waking from sleep just to be safe.
		FailIf (NULL == mControlInterface, Exit);
		debugIOLog ("? AppleUSBAudioDevice[%p]::performPowerStateChange () - Resetting port after wake from sleep ...", this);
		mControlInterface->GetDevice()->ResetDevice();
		IOSleep (10);
		
		// We need to restart the time stamp rate timer now
		debugIOLog ("? AppleUSBAudioDevice[%p]::performPowerStateChange () - Waking from sleep - restarting the rate timer.", this);
		TimerAction ( this, mUpdateTimer);


		//debugIOLog("Waking from sleep - flushing controls to the device.");
	//	-JH commented out cause it causes a crash when waking from sleep on 0202.  Theory is that it powers down during sleep which causes this to flail.
//		flushAudioControls(); 
	}

Exit:
	return result;
}

void EMUUSBAudioDevice::stop(IOService *provider) {
	debugIOLog("+EMUUSBAudioDevice[%p]::stop(%p) - audioEngines = %p - rc=%d", this, provider, audioEngines, getRetainCount());
	if (mStatusCheckTimer) {
		debugIOLog("releasing the statusCheckTimer");
		mStatusCheckTimer->cancelTimeout();
		RELEASEOBJ(mStatusCheckTimer);
	}
	if (mUpdateTimer) {
		debugIOLog("releasing the updateTimer");
		mUpdateTimer->cancelTimeout();
		RELEASEOBJ(mUpdateTimer);
	}
	if (mStatusPipe) {
		debugIOLog("releasing the status pipe");
		RELEASEOBJ(mStatusPipe);
	}

	super::stop(provider);  // call the IOAudioDevice generic stop routine

	debugIOLog("Terminating the engine");
	if (mAudioEngine && !mAudioEngine->isInactive()) {
		mAudioEngine->terminate();
	}
		
	debugIOLog("about to close the control interface");
	if(mControlInterface) {
		debugIOLog("closing control Interface");
        mControlInterface->close(this);
        mControlInterface = NULL;
    }
	
	debugIOLog("Release the XU controls");
	// release all allocated XU controls
	RELEASEOBJ(mXUChanged);
	RELEASEOBJ(mClockSelector);
	RELEASEOBJ(mDigitalIOStatus);
	RELEASEOBJ(mDigitalIOSyncSrc);
	RELEASEOBJ(mDigitalIOAsyncSrc);
	RELEASEOBJ(mDigitalIOSPDIF);
	RELEASEOBJ(mDevOptionCtrl);
	
	debugIOLog("Release the AudioConfig object");
	RELEASEOBJ(mUSBAudioConfig);
		
	debugIOLog("-EMUUSBAudioDevice[%p]::stop()", this);
}

bool EMUUSBAudioDevice::terminate(IOOptionBits options) {
	debugIOLog("terminating the EMUUSBAudioDevice");
	return super::terminate(options);
}

void EMUUSBAudioDevice::detach(IOService *provider) {
	debugIOLog("+EMUUSBAudioDevice[%p]: detaching %p, rc=%d",this,provider,getRetainCount());
	super::detach(provider);
	debugIOLog("-EMUUSBAudioDevice[%p]: detaching %p, rc=%d",this,provider,getRetainCount());
	
}

void EMUUSBAudioDevice::close(IOService *forClient, IOOptionBits options) {
	debugIOLog("EMUUSBAudioDevice[%p]: closing for %p",this,forClient);
	super::close(forClient,options);
}


IOReturn EMUUSBAudioDevice::message(UInt32 type, IOService * provider, void * arg) {
	debugIOLog("+EMUUSBAudioDevice[%p]::message(0x%x, %p) - rc=%d", this, type, provider, getRetainCount());

	if ((kIOMessageServiceIsTerminated == type) || (kIOMessageServiceIsRequestingClose == type)) {
		if (mStatusCheckTimer) {
			debugIOLog("message releasing mStatusCheckTimer");
			mStatusCheckTimer->cancelTimeout();
			RELEASEOBJ(mStatusCheckTimer);
		}
		if (mUpdateTimer) {
			debugIOLog("releasing the updateTimer");
			mUpdateTimer->cancelTimeout();
			RELEASEOBJ(mUpdateTimer);
		}
		
		if(mControlInterface != NULL && mControlInterface == provider) {
			mControlInterface->close(this);
			mControlInterface = NULL;
		}
	}
	return kIOReturnSuccess;
}

UInt8 EMUUSBAudioDevice::getHubSpeed() {
	UInt8	speed = kUSBDeviceSpeedFull;
	
	if (mControlInterface) {
		IOUSBDevice*			usbDevice = OSDynamicCast(IOUSBDevice, mControlInterface->GetDevice());
		IORegistryEntry*		currentEntry = OSDynamicCast(IORegistryEntry, usbDevice);
		const IORegistryPlane*	usbPlane = getPlane(kIOUSBPlane);
		while(currentEntry && usbDevice) {
			speed = usbDevice->GetSpeed();// look for 1st high speed hub
			if(kUSBDeviceSpeedHigh == speed) {
				// Must be connected via USB 2.0 hub
				debugIOLog(" ++EMUUSBAudioDevice::getHubSpeed() = kUSBDeviceSpeedHigh");
				break;// found the high speed device now break out of the loop
			} else {
				// Get parent in USB plane
				currentEntry = OSDynamicCast(IORegistryEntry, currentEntry->getParentEntry(usbPlane));
				// If the current registry entry is not a device, this will make usbDevice NULL and exit the loop
				usbDevice = OSDynamicCast(IOUSBDevice, currentEntry);
			}
		} // end while
	}
	return speed;
}

SInt32 EMUUSBAudioDevice::getEngineInfoIndex(EMUUSBAudioEngine * inAudioEngine) {
	if(mRegisteredEngines) {
		UInt32		engineIndex = 0;
		while (engineIndex < mNumEngines) {
			OSDictionary*	engineInfo = OSDynamicCast(OSDictionary, mRegisteredEngines->getObject(engineIndex));
			if (engineInfo) {
				EMUUSBAudioEngine*	usbAudioEngine = OSDynamicCast(EMUUSBAudioEngine, engineInfo->getObject(kEngine));
				if (usbAudioEngine == inAudioEngine)
					return engineIndex;// found the engine
			}
			++engineIndex;// keep looking
		}
	}

	return -1;// nothing found
}

IOReturn EMUUSBAudioDevice::GetVariousUnits(
	UInt8* inputFeatureUnitIDs,
	UInt8& numberOfInputFeatureUnits,
	UInt8* outputFeatureUnitIDs,
	UInt8& numberOfOutputFeatureUnits,
	UInt8& mixerUnitID,
	UInt8& processingUnitID) 
{
	OSArray *					arrayOfPathsFromOutputTerminal = NULL;
	OSArray *					aPath = NULL;
	OSNumber *					theUnitIDNum = NULL;
	IOReturn					result = kIOReturnError;
	UInt32						numOutputTerminals = 0;
	UInt32						pathsToOutputTerminalN = 0;

	UInt8						outputTerminalID = 0;

	debugIOLog("+EMUUSBAudioDevice::GetVariousUnits()");

    FailIf(NULL == mControlInterface, Exit);

	numOutputTerminals = mUSBAudioConfig->GetNumOutputTerminals(mInterfaceNum, 0);

	numberOfOutputFeatureUnits = 0;
	numberOfInputFeatureUnits = 0;
	
	for(UInt32 otIndex = 0; otIndex < numOutputTerminals; ++otIndex) 
	{
		if(mUSBAudioConfig->GetIndexedOutputTerminalType(mInterfaceNum, 0, otIndex) != 0x101) 
		{
			outputTerminalID = mUSBAudioConfig->GetIndexedOutputTerminalID(mInterfaceNum, 0, otIndex);

			UInt32 numOutputTerminalArrays = mControlGraph->getCount();
						
			for(UInt32 pathsToOutputTerminalN = 0; pathsToOutputTerminalN < numOutputTerminalArrays; ++pathsToOutputTerminalN) 
			{
				arrayOfPathsFromOutputTerminal = OSDynamicCast(OSArray, mControlGraph->getObject(pathsToOutputTerminalN));
				FailIf(NULL == arrayOfPathsFromOutputTerminal, Exit);
				aPath = OSDynamicCast(OSArray, arrayOfPathsFromOutputTerminal->getObject(0));
				FailIf(NULL == aPath, Exit);
				theUnitIDNum = OSDynamicCast(OSNumber, aPath->getObject(0));
				FailIf(NULL == theUnitIDNum, Exit);
				UInt8 unitID = theUnitIDNum->unsigned8BitValue();

	
				if(unitID == outputTerminalID) 
				{
					UInt32 numUnitsInPath = aPath->getCount();
					
					// Find the feature unit closest to the output terminal.
					for(UInt8 unitIndex = 1; unitIndex < numUnitsInPath; ++unitIndex) 
					{
						OSNumber* unitIDNum = OSDynamicCast(OSNumber, aPath->getObject(unitIndex));
						
						if(NULL != unitIDNum) 
						{
							UInt8 unitID = unitIDNum->unsigned8BitValue();
							UInt8 subtype = mUSBAudioConfig->GetSubType(mInterfaceNum, 0, unitID); 
				
							switch(subtype)
							{
								case FEATURE_UNIT: 
								{
									bool addFeatureUnit = true;
									for(long i = 0; i < numberOfOutputFeatureUnits; i++)
									{
										if(outputFeatureUnitIDs[i] == unitID) {
											addFeatureUnit = false;
										}
									}
									
									if (addFeatureUnit)
									{
										debugIOLog("EMUUSBAudioDevice::GetVariousUnits : output feature unit = %d", unitID);
									
										outputFeatureUnitIDs[numberOfOutputFeatureUnits++] = unitID;
									}
								}
								break;
								
								case MIXER_UNIT:
									mixerUnitID = unitID;
									break;
									
								case PROCESSING_UNIT:
									processingUnitID = unitID;
									break;
							
							}
						}
					}
				}
			}
		}
	
		for(UInt32 otIndx = 0; otIndx < numOutputTerminals; ++otIndx) {
			if(mUSBAudioConfig->GetIndexedOutputTerminalType(mInterfaceNum, 0, otIndx) == 0x101) {
				outputTerminalID = mUSBAudioConfig->GetIndexedOutputTerminalID(mInterfaceNum, 0, otIndx);
				break;		// Found the(hopefully only) streaming output terminal we're looking for
			}
		}

		UInt32 numOutputTerminalArrays = mControlGraph->getCount();
		for(pathsToOutputTerminalN = 0; pathsToOutputTerminalN < numOutputTerminalArrays; ++pathsToOutputTerminalN) 
		{
			arrayOfPathsFromOutputTerminal = OSDynamicCast(OSArray, mControlGraph->getObject(pathsToOutputTerminalN));
			FailIf(NULL == arrayOfPathsFromOutputTerminal, Exit);
			aPath = OSDynamicCast(OSArray, arrayOfPathsFromOutputTerminal->getObject(0));
			FailIf(NULL == aPath, Exit);
			theUnitIDNum = OSDynamicCast(OSNumber, aPath->getObject(0));
			FailIf(NULL == theUnitIDNum, Exit);
			UInt8 unitID = theUnitIDNum->unsigned8BitValue();

			if(unitID == outputTerminalID) 
			{
				UInt32 numUnitsInPath = aPath->getCount();
				
				// Find the feature unit closest to the output terminal.
				for(UInt8 unitIndex = 1; unitIndex < numUnitsInPath; ++unitIndex) 
				{
					OSNumber* unitIDNum = OSDynamicCast(OSNumber, aPath->getObject(unitIndex));
					
					if(NULL != unitIDNum) 
					{
						UInt8 unitID = unitIDNum->unsigned8BitValue();
						UInt8 subtype = mUSBAudioConfig->GetSubType(mInterfaceNum, 0, unitID); 
			
						if(FEATURE_UNIT == subtype) 
						{
							bool addFeatureUnit = true;
							for(long i = 0; i < numberOfInputFeatureUnits; i++)
							{
								if(inputFeatureUnitIDs[i] == unitID) {
									addFeatureUnit = false;
								}
							}
							
							if (addFeatureUnit)
							{
								debugIOLog("EMUUSBAudioDevice::GetVariousUnits : input feature unit = %d", unitID);
							
								inputFeatureUnitIDs[numberOfInputFeatureUnits++] = unitID;
							}
						}
					}
				}
			}
		}
	}
	
	result = kIOReturnSuccess;

Exit:
	debugIOLog("-EMUUSBAudioDevice::GetVariousUnits()");
	return result;
}

IOReturn EMUUSBAudioDevice::doControlStuff(IOAudioEngine *audioEngine, UInt8 interfaceNum, UInt8 altSettingNum) {
	EMUUSBAudioEngine *			usbAudioEngine = OSDynamicCast(EMUUSBAudioEngine, audioEngine);
#if ENABLEHARDCONTROLS
	IOAudioSelectorControl *	inputSelector = NULL;
#endif
	OSArray *					arrayOfPathsFromOutputTerminal = NULL;
	OSArray *					aPath = NULL;
	OSArray *					playThroughPaths = NULL;
	OSNumber *					theUnitIDNum = NULL;
	OSNumber *					number = NULL;
	OSDictionary *				engineInfo = NULL;
	IOReturn					result = kIOReturnError;
	UInt32						numOutputTerminals = 0;
	UInt32						pathsToOutputTerminalN = 0;
#if ENABLEHARDCONTROLS
	UInt32						numPathsFromOutputTerminal = 0;
	UInt32						selection = 0;
	SInt32						engineIndex = 0;
	UInt8						selectorUnitID = 0;
	UInt8						featureUnitID = 0;
	Boolean						done = FALSE;
#endif
	UInt8						outputTerminalID = 0;

	debugIOLog("+EMUUSBAudioDevice::doControlStuff(0x%x, %d, %d)", audioEngine, interfaceNum, altSettingNum);

    FailIf(NULL == usbAudioEngine || NULL == mControlInterface, Exit);
	debugIOLog("this usbAudioEngine = %p", usbAudioEngine);

	if(NULL == mRegisteredEngines) {
		mRegisteredEngines = OSArray::withCapacity(1);
		FailIf(NULL == mRegisteredEngines, Exit);
	}
	engineInfo = OSDictionary::withCapacity(1);
	FailIf(NULL == engineInfo, Exit);
	engineInfo->setObject(kEngine, usbAudioEngine);
	number = OSNumber::withNumber(interfaceNum, 8);
	engineInfo->setObject(kInterface, number);
	number->release();
	number = OSNumber::withNumber(altSettingNum, 8);
	engineInfo->setObject(kAltSetting, number);
	number->release();
	// try finding the engine info before putting anything in
	SInt32	engineInfoIndex = getEngineInfoIndex(usbAudioEngine);
	OSDictionary*	storedEngineInfo = OSDynamicCast(OSDictionary, mRegisteredEngines->getObject(engineInfoIndex));
	if (!storedEngineInfo) {
		mRegisteredEngines->setObject(engineInfo);
		++mNumEngines;
	}
	engineInfo->release();

	numOutputTerminals = mUSBAudioConfig->GetNumOutputTerminals(mInterfaceNum, 0);
	
#if ENABLESOFTCONTROLS
	usbAudioEngine->addSoftVolumeControls(audioEngine);
#endif

	addHardVolumeControls(audioEngine);

#if ENABLEHARDCONTROLS// disable all this since we don't have any input controls
		for(UInt32 otIndex = 0; otIndex < numOutputTerminals && FALSE == done; ++otIndex) {
			if(mUSBAudioConfig->GetIndexedOutputTerminalType(mInterfaceNum, 0, otIndex) != 0x101) {
				outputTerminalID = mUSBAudioConfig->GetIndexedOutputTerminalID(mInterfaceNum, 0, otIndex);

				UInt32 numOutputTerminalArrays = mControlGraph->getCount();
				for(UInt32 pathsToOutputTerminalN = 0; pathsToOutputTerminalN < numOutputTerminalArrays; ++pathsToOutputTerminalN) {
					arrayOfPathsFromOutputTerminal = OSDynamicCast(OSArray, mControlGraph->getObject(pathsToOutputTerminalN));
					FailIf(NULL == arrayOfPathsFromOutputTerminal, Exit);
					aPath = OSDynamicCast(OSArray, arrayOfPathsFromOutputTerminal->getObject(0));
					FailIf(NULL == aPath, Exit);
					theUnitIDNum = OSDynamicCast(OSNumber, aPath->getObject(0));
					FailIf(NULL == theUnitIDNum, Exit);
					UInt8 unitID = theUnitIDNum->unsigned8BitValue();
		
					if(unitID == outputTerminalID) {
						featureUnitID = getBestFeatureUnitInPath(aPath, kIOAudioControlUsageOutput, interfaceNum, altSettingNum, kVolumeControl);
						if(featureUnitID) {
							debugIOLog("----- Creating Output Gain Controls -----");// Create the output gain controls
							addVolumeControls(usbAudioEngine, featureUnitID, interfaceNum, altSettingNum, kIOAudioControlUsageOutput);
						}
						featureUnitID = getBestFeatureUnitInPath(aPath, kIOAudioControlUsageOutput, interfaceNum, altSettingNum, kMuteControl);
						if(featureUnitID) {// add mute control
							addMuteControl(usbAudioEngine, featureUnitID, interfaceNum, altSettingNum, kIOAudioControlUsageOutput);
							done = TRUE;
						}
					}
				}
			}
		}
#endif
	
		for(UInt32 otIndx = 0; otIndx < numOutputTerminals; ++otIndx) {
			if(mUSBAudioConfig->GetIndexedOutputTerminalType(mInterfaceNum, 0, otIndx) == 0x101) {
				outputTerminalID = mUSBAudioConfig->GetIndexedOutputTerminalID(mInterfaceNum, 0, otIndx);
				break;		// Found the(hopefully only) streaming output terminal we're looking for
			}
		}

		UInt32 numOutputTerminalArrays = mControlGraph->getCount();
		for(pathsToOutputTerminalN = 0; pathsToOutputTerminalN < numOutputTerminalArrays; ++pathsToOutputTerminalN) {
			arrayOfPathsFromOutputTerminal = OSDynamicCast(OSArray, mControlGraph->getObject(pathsToOutputTerminalN));
			FailIf(NULL == arrayOfPathsFromOutputTerminal, Exit);
			aPath = OSDynamicCast(OSArray, arrayOfPathsFromOutputTerminal->getObject(0));
			FailIf(NULL == aPath, Exit);
			theUnitIDNum = OSDynamicCast(OSNumber, aPath->getObject(0));
			FailIf(NULL == theUnitIDNum, Exit);
			UInt8 unitID = theUnitIDNum->unsigned8BitValue();

			if(unitID == outputTerminalID) {
				// Check for a playthrough path that would require a playthrough control
				theUnitIDNum = OSDynamicCast(OSNumber, aPath->getLastObject());
				FailIf(NULL == theUnitIDNum, Exit);
				unitID = theUnitIDNum->unsigned8BitValue();
				playThroughPaths = getPlaythroughPaths();
#if ENABLEHARDCONTROLS
				if(playThroughPaths) {
					doPlaythroughSetup(usbAudioEngine, playThroughPaths, interfaceNum, altSettingNum);
					playThroughPaths->release();
				}
				numPathsFromOutputTerminal = arrayOfPathsFromOutputTerminal->getCount();
				if(numPathsFromOutputTerminal > 1 && mUSBAudioConfig->GetNumSelectorUnits(mInterfaceNum, 0)) {
					// Found the array of paths that lead to our streaming output terminal
					UInt32 numUnitsInPath = aPath->getCount();
					for(UInt32 unitIndexInPath = 1; unitIndexInPath < numUnitsInPath; ++unitIndexInPath) {
						theUnitIDNum = OSDynamicCast(OSNumber, aPath->getObject(unitIndexInPath));
						FailIf(NULL == theUnitIDNum, Exit);
						UInt8 unitID = theUnitIDNum->unsigned8BitValue();
						if(SELECTOR_UNIT == mUSBAudioConfig->GetSubType(mInterfaceNum, 0, unitID)) {
							if(kIOReturnSuccess == setSelectorSetting(unitID, 1)) {
								selectorUnitID = unitID;
								engineIndex = getEngineInfoIndex(usbAudioEngine);
								if(-1 != engineIndex) {
									selection =(0xFF000000 &(pathsToOutputTerminalN << 24)) |(0x00FF0000 &(0 << 16)) |(0x0000FF00 &(selectorUnitID << 8)) | 1;
									inputSelector = IOAudioSelectorControl::createInputSelector(selection, kIOAudioControlChannelIDAll, 0, engineIndex);
									FailIf(NULL == inputSelector, Exit);
									inputSelector->setValueChangeHandler(controlChangedHandler, this);
									usbAudioEngine->addDefaultAudioControl(inputSelector);
									featureUnitID = getBestFeatureUnitInPath(aPath, kIOAudioC«öontrolUsageInput, interfaceNum, altSettingNum, kVolumeControl);
									if(featureUnitID) {
										// Create the input gain controls
										debugIOLog("----- Creating Intput Gain Controls -----");
										addVolumeControls(usbAudioEngine, featureUnitID, interfaceNum, altSettingNum, kIOAudioControlUsageInput);
									}
									featureUnitID = getBestFeatureUnitInPath(aPath, kIOAudioControlUsageInput, interfaceNum, altSettingNum, kMuteControl);
									if(featureUnitID) 
										addMuteControl(usbAudioEngine, featureUnitID, interfaceNum, altSettingNum, kIOAudioControlUsageInput);
								}
							}
							if (NULL != inputSelector) {
								addSelectorSourcesToSelectorControl(inputSelector, arrayOfPathsFromOutputTerminal, pathsToOutputTerminalN, unitIndexInPath);
								inputSelector->release();
							} else {// no programmable selectors. Find the feature unit
								featureUnitID = getBestFeatureUnitInPath(aPath, kIOAudioControlUsageInput, interfaceNum, altSettingNum, kVolumeControl);
								if (featureUnitID) 
									addVolumeControls(usbAudioEngine, featureUnitID, interfaceNum, altSettingNum, kIOAudioControlUsageInput);
							}
							break;		// Get out of unitIndexInPath for loop
						}
					}

				}/* else {
					// There are no selectors, so just find the one feature unit, if it exists.
					featureUnitID = getBestFeatureUnitInPath(aPath, kIOAudioControlUsageInput, interfaceNum, altSettingNum, kVolumeControl);
					if(featureUnitID) // Create playthrough volume controls
						addVolumeControls(usbAudioEngine, featureUnitID, interfaceNum, altSettingNum, kIOAudioControlUsageInput);
				}*/
#endif
				break;		// Get out of pathsToOutputTerminalN for loop
			}
		}
		
		
	
	// add a clock-selector control.  it's fun and easy!
	
	if (interfaceNum == 2) {
		mRealClockSelector = IOAudioSelectorControl::create(0,
															kIOAudioControlChannelIDAll,
															kIOAudioControlChannelNameAll,
															0,
															kIOAudioSelectorControlSubTypeClockSource,
															kIOAudioControlUsageInput);
	
		// We probably want to put the strings somewhere else, but this will do for now (AC)
		mRealClockSelector->addAvailableSelection(0,"Internal");
		//check if we really have SPDIF clock before adding it to the list (bad things happen if we don't)
		if (getProperty("bHasSPDIFClock")) {
			mRealClockSelector->addAvailableSelection(1,"External SPDIF");
		}
		
		mRealClockSelector->setValueChangeHandler(controlChangedHandler,this);
#if 0
		//sanity check												
		if (mRealClockSelector->valueExists(kClockSourceInternal)) {
			debugIOLog("clockSource Internal value OK");
		}
		if (mRealClockSelector->valueExists(kClockSourceSpdifExternal)) {
			debugIOLog("clockSource External SPDIF value OK");
		}
#endif
																				
		usbAudioEngine->addDefaultAudioControl(mRealClockSelector);
		
	// note in addCustomAudioControls where the default value is sent to the device (mClockSrcXU, kClockSourceSelector XU).
		mRealClockSelector->setProperty(kIOAudioSelectorControlClockSourceKey,"0");

		// we only add the custom controls on the output interface, so we don't get two copies of each
		addCustomAudioControls(usbAudioEngine);

	}
	
	result = kIOReturnSuccess;
		

Exit:
	debugIOLog("-EMUUSBAudioDevice::doControlStuff(0x%x, %d, %d)", audioEngine, interfaceNum, altSettingNum);
	return result;
}

IOReturn EMUUSBAudioDevice::doPlaythroughSetup(EMUUSBAudioEngine * usbAudioEngine, OSArray * playThroughPaths, UInt8 interfaceNum, UInt8 altSettingNum) {
	IOReturn		result = kIOReturnError;
	if (mControlInterface) {
		OSArray *					aPath = NULL;
		OSNumber *					theUnitIDNum = NULL;
		OSString *					nameString = NULL;
		IOAudioSelectorControl*		playThroughSelector = NULL;
		OSDictionary *				engineInfo = NULL;
		UInt32						numPlayThroughPaths = 0;
		SInt32						engineInfoIndex = getEngineInfoIndex(usbAudioEngine);
		UInt8						featureUnitID = 0;
		UInt8						inputTerminalID = 0;

		FailIf(-1 == engineInfoIndex, Exit);
		engineInfo = OSDynamicCast(OSDictionary, mRegisteredEngines->getObject(engineInfoIndex));
		FailIf(NULL == engineInfo, Exit);
		engineInfo->setObject(kPassThruPathsArray, playThroughPaths);

		numPlayThroughPaths = playThroughPaths->getCount();
		if(numPlayThroughPaths > 1) {
			// Create a virtual selector to manipulate the mutes on the feature units to toggle through playthrough sources.
			playThroughSelector = IOAudioSelectorControl::create(0, kIOAudioControlChannelIDAll, 0, engineInfoIndex, kIOAudioSelectorControlSubTypeInput, kIOAudioControlUsagePassThru);
			FailIf(NULL == playThroughSelector, Exit);
			playThroughSelector->setValueChangeHandler(controlChangedHandler, this);
			usbAudioEngine->addDefaultAudioControl(playThroughSelector);
			aPath = OSDynamicCast(OSArray, playThroughPaths->getObject(0));
			if (aPath) {
				featureUnitID = getBestFeatureUnitInPath( aPath, kIOAudioControlUsageInput, interfaceNum, altSettingNum, kVolumeControl);
				if (featureUnitID) {
					theUnitIDNum = OSDynamicCast(OSNumber, aPath->getLastObject());
					if (theUnitIDNum) {
						inputTerminalID = theUnitIDNum->unsigned8BitValue();
						nameString = OSString::withCString(TerminalTypeString(mUSBAudioConfig->GetInputTerminalType(mInterfaceNum, 0, inputTerminalID)));
						FailIf(NULL == nameString, Exit);
						playThroughSelector->addAvailableSelection(0, nameString);
						nameString->release();
						UInt8	total = mUSBAudioConfig->GetNumControls(mInterfaceNum, 0, featureUnitID);
						for(UInt8 channelNum = 0; channelNum < total; ++channelNum) {
							setCurVolume(featureUnitID, channelNum, 0);
							setCurMute(featureUnitID, channelNum, 0);
						}
					}
					addVolumeControls(usbAudioEngine, featureUnitID, interfaceNum, altSettingNum, kIOAudioControlUsagePassThru);
				}
				featureUnitID = getBestFeatureUnitInPath(aPath, kIOAudioControlUsageInput, interfaceNum, altSettingNum, kMuteControl);
				if (featureUnitID)
					addMuteControl(usbAudioEngine, featureUnitID, interfaceNum, altSettingNum, kIOAudioControlUsagePassThru);
			}
			for(UInt32 pathIndex = 1; pathIndex < numPlayThroughPaths; ++pathIndex) {
				aPath = OSDynamicCast(OSArray, playThroughPaths->getObject(pathIndex));
				FailIf(NULL == aPath, Exit);
				featureUnitID = getBestFeatureUnitInPath(aPath, kIOAudioControlUsageInput, interfaceNum, altSettingNum, kVolumeControl);
				if(featureUnitID) {
					theUnitIDNum = OSDynamicCast(OSNumber, aPath->getLastObject());
					if (theUnitIDNum) {
						inputTerminalID = theUnitIDNum->unsigned8BitValue();
						nameString = OSString::withCString(TerminalTypeString(mUSBAudioConfig->GetInputTerminalType(mInterfaceNum, 0, inputTerminalID)));
						FailIf(NULL == nameString, Exit);
						playThroughSelector->addAvailableSelection(pathIndex, nameString);
						nameString->release();
						UInt8	total = mUSBAudioConfig->GetNumControls(mInterfaceNum, 0, featureUnitID);
						for(UInt8 channelNum = 0; channelNum < total; ++channelNum) {
							setCurVolume(featureUnitID, channelNum, 0);
							setCurMute(featureUnitID, channelNum, 0);
						}
					}
				}
			}
			result = kIOReturnSuccess;
		} else {
			// Only one playthrough path, so just publish its volume and mute controls.
			aPath = OSDynamicCast(OSArray, playThroughPaths->getObject(0));
			FailIf(NULL == aPath, Exit);
			featureUnitID = getBestFeatureUnitInPath(aPath, kIOAudioControlUsagePassThru, interfaceNum, altSettingNum, kVolumeControl);
			if(featureUnitID) 
				addVolumeControls(usbAudioEngine, featureUnitID, interfaceNum, altSettingNum, kIOAudioControlUsagePassThru);// create playthrough volume controls
			
			featureUnitID = getBestFeatureUnitInPath(aPath, kIOAudioControlUsagePassThru, interfaceNum, altSettingNum, kMuteControl);
			if(featureUnitID) 
				addMuteControl(usbAudioEngine, featureUnitID, interfaceNum, altSettingNum, kIOAudioControlUsagePassThru);
			
			result = kIOReturnSuccess;
		}
	}
Exit:
	return result;
}

IOReturn EMUUSBAudioDevice::addSelectorSourcesToSelectorControl(IOAudioSelectorControl * theSelectorControl, OSArray * arrayOfPathsFromOutputTerminal, UInt32 pathsToOutputTerminalN, UInt8 selectorIndex) {
	IOReturn			result = kIOReturnError;
	if (mControlInterface) {
		OSArray *			aPath = OSDynamicCast(OSArray, arrayOfPathsFromOutputTerminal->getObject(0));
		OSNumber *			theUnitIDNum = NULL;
		OSString *			nameString = NULL;
		UInt32				pathIndex = 0;

		FailIf(NULL == aPath, Exit);
		theUnitIDNum = OSDynamicCast(OSNumber, aPath->getObject(selectorIndex));
		FailIf(NULL == theUnitIDNum, Exit);
		UInt8	selectorID = theUnitIDNum->unsigned8BitValue();
		UInt8	numSelectorSources = mUSBAudioConfig->GetNumSources(mInterfaceNum, 0, selectorID);
		for(UInt8 selectorSourceIndex = 0; selectorSourceIndex < numSelectorSources; ++selectorSourceIndex) {
			nameString = getNameForPath(arrayOfPathsFromOutputTerminal, &pathIndex, selectorIndex + 1);
			if(NULL != nameString) {
				UInt32 selection =(0xFF000000 &(pathsToOutputTerminalN << 24)) |(0x00FF0000 &((pathIndex - 1) << 16)) |(0x0000FF00 &(selectorID << 8)) |(0x000000FF &(selectorSourceIndex + 1));
				theSelectorControl->addAvailableSelection(selection, nameString);
				nameString->release();
			}
		}
		result = kIOReturnSuccess;// if we reach this point things are ok
	}
Exit:
	return result;
}

// Starting point is the array index of the element after the selector unit.
OSString * EMUUSBAudioDevice::getNameForPath(OSArray * arrayOfPathsFromOutputTerminal, UInt32 * pathIndex, UInt8 startingPoint) {
	OSString *			theString = NULL;
	OSArray *			aPath = NULL;

	FailIf(NULL == mControlInterface, Exit);
	aPath = OSDynamicCast(OSArray, arrayOfPathsFromOutputTerminal->getObject(*pathIndex));
	FailIf(NULL == aPath, Exit);

	UInt32	numElementsInPath = aPath->getCount();
	UInt32	elementIndex = (UInt32) startingPoint;
	while( elementIndex < numElementsInPath) {
		OSNumber* theUnitIDNum = OSDynamicCast(OSNumber, aPath->getObject(elementIndex));
		FailIf(NULL == theUnitIDNum, Exit);
		UInt8	unitID = theUnitIDNum->unsigned8BitValue();
		UInt8	subType = mUSBAudioConfig->GetSubType(mInterfaceNum, 0, unitID);
		if (INPUT_TERMINAL == subType) {
			OSString*	tempStr = OSString::withCString(TerminalTypeString(mUSBAudioConfig->GetInputTerminalType(mInterfaceNum, 0, unitID)));
			if (NULL != tempStr) {
				if (!tempStr->isEqualTo("USB streaming")) 
					theString = OSString::withString(tempStr);
				tempStr->release();
				(*pathIndex)++;
			}
		} else if (MIXER_UNIT == subType) {
			theString = getNameForMixerPath(arrayOfPathsFromOutputTerminal, pathIndex, elementIndex);
			break;
		}
		++elementIndex;
	}

Exit:
	return theString;
}

// Starting point is the array index of the mixer unit.
OSString * EMUUSBAudioDevice::getNameForMixerPath(OSArray * arrayOfPathsFromOutputTerminal, UInt32 * pathIndex, UInt8 startingPoint) {
	char					string[255];
	OSString *				theString = NULL;
	OSNumber *				theUnitIDNum = NULL;
	OSArray *				aPath = NULL;
	UInt32					mixerSourceIndex = *pathIndex;

	string[0] = 0;
	FailIf(NULL == mControlInterface, Exit);
	aPath = OSDynamicCast(OSArray, arrayOfPathsFromOutputTerminal->getObject(*pathIndex));
	FailIf(NULL == aPath, Exit);
	theUnitIDNum = OSDynamicCast(OSNumber, aPath->getObject(startingPoint));
	FailIf(NULL == theUnitIDNum, Exit);

	UInt32	numElementsInPath = aPath->getCount();
	UInt8	numMixerSources = mUSBAudioConfig->GetNumSources(mInterfaceNum, 0, theUnitIDNum->unsigned8BitValue());
	while (mixerSourceIndex < *pathIndex + numMixerSources) {
		for(UInt8 elementIndex = startingPoint + 1; elementIndex < numElementsInPath; elementIndex++) {
			theUnitIDNum = OSDynamicCast(OSNumber, aPath->getObject(elementIndex));
			FailIf(NULL == theUnitIDNum, Exit);
			UInt8	unitID = theUnitIDNum->unsigned8BitValue();
			UInt8	subType = mUSBAudioConfig->GetSubType(mInterfaceNum, 0, unitID);
			if (INPUT_TERMINAL == subType) {
				OSString*	tempString = getNameForPath(arrayOfPathsFromOutputTerminal, &mixerSourceIndex, elementIndex);
				if (tempString) {
					strcat(string, tempString->getCStringNoCopy());
					strcat(string, " & ");
					tempString->release();
				}
			} else if (MIXER_UNIT == subType) {
				OSString*	tempString = getNameForMixerPath(arrayOfPathsFromOutputTerminal, &mixerSourceIndex, elementIndex);
				if (tempString) {
					strcat(string, tempString->getCStringNoCopy());
					tempString->release();
				}
			}
		}
	}
	*pathIndex = mixerSourceIndex;

	if(strlen(string) > 3) 
		string[strlen(string) - 3] = 0;

Exit:
	theString = OSString::withCString(string);
	return theString;
}

void EMUUSBAudioDevice::addVolumeControls(EMUUSBAudioEngine * usbAudioEngine, UInt8 featureUnitID, UInt8 interfaceNum, UInt8 altSettingNum, UInt32 usage) 
{
	debugIOLog("EMUUSBAudioDevice::addVolumeControls(0x%x, %d, %d, %d, 0x%x)", usbAudioEngine, featureUnitID, interfaceNum, altSettingNum, usage);
	
#pragma unused (interfaceNum, altSettingNum)
	SInt32		engineInfoIndex = getEngineInfoIndex(usbAudioEngine);
	if (-1 != engineInfoIndex && (NULL != mControlInterface)) {
		OSDictionary*	engineInfo = OSDynamicCast(OSDictionary, mRegisteredEngines->getObject(engineInfoIndex));
		if (engineInfo) {
			OSArray*		inputGainControlArray = NULL;
			OSArray*		passThruVolControlArray = NULL;
			OSArray*		outputVolControlArray = NULL;
			
			if((kIOAudioControlUsageOutput == usage) && (NULL != mMonoControlsArray)) {
				mMonoControlsArray->release();
				mMonoControlsArray = NULL;
			}
			UInt8		total = mUSBAudioConfig->GetNumControls(mInterfaceNum, 0, featureUnitID);
			UInt8		channelNum = 0;
			IOReturn	result = kIOReturnSuccess;
			SInt16		deviceMax = 0;
			SInt16		deviceMin = 0;
			SInt16		deviceCur = 0;
			SInt16		offset = 0;
			UInt16		volRes = 1; // prevent divde by 0
	
			while (channelNum <= total) {
				IOAudioLevelControl*	levelControl = NULL;
					if(mUSBAudioConfig->ChannelHasVolumeControl(mInterfaceNum, 0, featureUnitID, channelNum)) {
					// must have the requisite volume controls - current, min, max
					FailIf(((kIOReturnSuccess != (result = getCurVolume(featureUnitID, channelNum, &deviceCur)))
							|| (kIOReturnSuccess != (result = getMinVolume(featureUnitID, channelNum, &deviceMin)))
							|| (kIOReturnSuccess != (result = getMaxVolume(featureUnitID, channelNum, &deviceMax)))), Error);
					getVolumeResolution(featureUnitID, channelNum, &volRes);
					if (kIOReturnSuccess != (result = setCurVolume(featureUnitID, channelNum, HostToUSBWord(deviceCur)))) {
						break;
					}
					if((SInt16) kNegativeInfinity == deviceMin) { // do not set to negative infinity
						deviceMin = (SInt16) 0x8001;
					}
					// Definition in the USB Audio spec (section 5.2.2.4.3.2). The volume setting is in 1/256 db increments
					// from a max of 0x7fff (127.9961 dB) to 0x8001 (-127.9961 dB) in standard signed math. 0x8000 is 
					// is negative infinity(not -128 dB), so -128 dB value must be handled as follows ((SInt16) 0x8000 * 256) << 8
					// or we could avoid that by assigning 0x8001 instead.
					
					debugIOLog("************  original deviceMin is %d and original deviceMax is %d ***************\n", deviceMin, deviceMax);
			
					IOFixed	deviceMinDB = deviceMin * 256;
					IOFixed	deviceMaxDB = deviceMax * 256;					
					
					offset = -deviceMin;
					deviceCur = (deviceCur + offset) / volRes;
					deviceMax += volRes * ((deviceMin < 0) && (deviceMax > 0));
					
					// Range will be something like -128..-32768.  Add 1 to make the range 0..-32768 - jh
					deviceMax = (((deviceMin + offset) + (deviceMax + offset)) / volRes);
					deviceMin = -1;
					
					debugIOLog("************  deviceMin is %d and deviceMax is %d volRes= %d channelNum= %d  ***************\n", deviceMin, deviceMax,volRes,channelNum);
					debugIOLog("************  deviceMinDB is %d and deviceMaxDB is %d ***************\n", deviceMinDB, deviceMaxDB);
			
					levelControl = IOAudioLevelControl::createVolumeControl(deviceCur, deviceMin, deviceMax, deviceMinDB,
										deviceMaxDB, channelNum, 0, featureUnitID, usage);
				
					if (levelControl) {
						levelControl->setValueChangeHandler(controlChangedHandler, this);
						usbAudioEngine->addDefaultAudioControl(levelControl);
						if (kIOAudioControlUsageInput == usage) {
							if (NULL == inputGainControlArray)
								inputGainControlArray = OSArray::withObjects((const OSObject **)&levelControl, 1);
							else
								inputGainControlArray->setObject(levelControl);
						} else if (kIOAudioControlUsagePassThru == usage) {
							if (NULL == passThruVolControlArray)
								passThruVolControlArray = OSArray::withObjects((const OSObject **)&levelControl, 1);
							else
								passThruVolControlArray->setObject(levelControl);
						} else if (kIOAudioControlUsageOutput == usage) {
							if (NULL == outputVolControlArray)
								outputVolControlArray = OSArray::withObjects((const OSObject **)&levelControl, 1);
							else
								outputVolControlArray->setObject(levelControl);
							if (mDeviceIsInMonoMode) {// in-case device is in mono mode
								OSNumber*	number = OSNumber::withNumber(channelNum, 8);
								if (NULL == mMonoControlsArray)
									mMonoControlsArray = OSArray::withObjects((const OSObject **) &number, 1);
								else
									mMonoControlsArray->setObject(number);
								number->release();
							}
						}
						levelControl->release();
					}
				}
				++channelNum;
			}
Error:
			if (inputGainControlArray) {// assign the input gain controls and release
				engineInfo->setObject(kInputGainControls, inputGainControlArray);
				inputGainControlArray->release();
			}
			if (passThruVolControlArray) {
				engineInfo->setObject(kPassThruVolControls, passThruVolControlArray);
				passThruVolControlArray->release();
			}
			if (outputVolControlArray) {
				engineInfo->setObject(kOutputVolControls, outputVolControlArray);
				outputVolControlArray->release();
			}
		}
	}
}

void EMUUSBAudioDevice::addMuteControl(EMUUSBAudioEngine * usbAudioEngine, UInt8 featureUnitID, UInt8 interfaceNum, UInt8 altSettingNum, UInt32 usage) {
	SInt32		engineInfoIndex = getEngineInfoIndex(usbAudioEngine);
	
	if (-1 != engineInfoIndex && (NULL != mControlInterface)) {
		OSDictionary*	engineInfo = OSDynamicCast(OSDictionary, mRegisteredEngines->getObject(engineInfoIndex));
		if (engineInfo) {
			OSArray*	inputMuteControlArray = NULL;
			UInt8		total = mUSBAudioConfig->GetNumControls(mInterfaceNum, 0, featureUnitID); 
			for (UInt8 channelNum = 0; channelNum <= total; ++channelNum) {
				if(mUSBAudioConfig->ChannelHasMuteControl(mInterfaceNum, 0, featureUnitID, channelNum)) {
					SInt16	deviceCur;
					getCurMute(featureUnitID, channelNum, &deviceCur);// don't care about the return result
					IOAudioToggleControl*	muteControl = IOAudioToggleControl::createMuteControl(deviceCur, channelNum, 0, featureUnitID, usage);
					setCurMute(featureUnitID, channelNum, HostToUSBWord(deviceCur));
					if (muteControl) {
						muteControl->setValueChangeHandler(controlChangedHandler, this);
						usbAudioEngine->addDefaultAudioControl(muteControl);
						if (kIOAudioControlUsageInput == usage) {
							if (!inputMuteControlArray)
								inputMuteControlArray = OSArray::withObjects((const OSObject **)&muteControl, 1);
							else
								inputMuteControlArray->setObject(muteControl);
						}
						muteControl->release();
					}
				}
			}
			if (inputMuteControlArray) {
				engineInfo->setObject(kInputMuteControls, inputMuteControlArray);
				inputMuteControlArray->release();
			}
		}
	}
}

// routine to return the extension unit unitID
UInt8 EMUUSBAudioDevice::getExtensionUnitID(UInt16 extCode) {
	UInt8	unitID = 0;
	switch (extCode) {
		case kClockRate:// sample rate 
			unitID = mClockRateXU;
			break;
		case kClockSource:// clock source unit - specifies either internal or external
			unitID = mClockSrcXU;
			break;
		case kDigitalIOStatus:// the status of the digital IO if used
			unitID = mDigitalIOXU;
			break;
		case kDeviceOptions:// what options are in use
			unitID = mDeviceOptionsXU;
			break;
#if DIRECTMONITOR
		case kDirectMonitoring:// direct monitor settings
			unitID = mDirectMonXU;
			break;
#endif
		default:// should never come here
			break;
	}
	return unitID;
}

IOReturn EMUUSBAudioDevice::getExtensionUnitSettings(UInt16 extCode, UInt8 controlSelector, void* setting, UInt32 length) {
	IOReturn	result = kIOReturnError;
	UInt8		unitID = getExtensionUnitID(extCode);
	// find the extensionUnit that corresponds to the particular extension code and interfaceNum
	if(unitID) 
		result = getExtensionUnitSetting(unitID, controlSelector, setting, length);
	return result;
}

IOReturn EMUUSBAudioDevice::setExtensionUnitSettings(UInt16 extCode, UInt8 controlSelector, void* setting, UInt32 length) {
	IOReturn		result	= kIOReturnError;
	// Find the XU corresponding to the extension code and interfaceNum

	UInt8	unitID = getExtensionUnitID(extCode);
	if(unitID)
		result = setExtensionUnitSetting(unitID, controlSelector, setting, length);

	return result;
}

IOReturn EMUUSBAudioDevice::getExtensionUnitSetting(UInt8 unitID,  UInt8 controlSelector, void* setting, UInt32 length) {
	IOReturn					result = kIOReturnError;// default value
	IOBufferMemoryDescriptor *	settingDesc = IOBufferMemoryDescriptor::withOptions(kIODirectionIn, length);
	
	if(NULL != settingDesc) {// set up the request for the ExtensionUnit in question
		IOUSBDevRequestDesc			devReq;
		devReq.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBClass, kUSBInterface);// the request type
		devReq.bRequest = GET_CUR;
		devReq.wValue = controlSelector << 8;
		devReq.wIndex =(unitID << 8 | mInterfaceNum);
		devReq.wLength = length;// keep this for now
		devReq.pData = settingDesc;
		FailIf((TRUE == isInactive()), Exit);
		result = deviceRequest(&devReq);
		// do something with the values returned
		FailIf(kIOReturnSuccess != result, Exit);
		memmove(setting, settingDesc->getBytesNoCopy(), length);
		
	}
Exit:
	if(NULL != settingDesc)
		settingDesc->release();
	return result;
}

IOReturn EMUUSBAudioDevice::setExtensionUnitSetting(UInt8 unitID, UInt8 controlSelector, void* settings, UInt32 length) {
	IOReturn					result = kIOReturnError;
	IOBufferMemoryDescriptor*	settingDesc = IOBufferMemoryDescriptor::withOptions(kIODirectionOut, length);
	if(NULL != settingDesc) {
		IOUSBDevRequestDesc		devReq;
		settingDesc->initWithBytes(settings, length, kIODirectionOut);
		devReq.bmRequestType = USBmakebmRequestType(kUSBOut, kUSBClass, kUSBInterface);
		devReq.bRequest = SET_CUR;
		devReq.wValue = controlSelector << 8;
		devReq.wIndex =(unitID << 8 | mInterfaceNum);
		devReq.wLength = length;
		devReq.pData = settingDesc;
		FailIf((TRUE == isInactive()), Exit);
		result = deviceRequest(&devReq);
	}
Exit:
	if(NULL != settingDesc)
		settingDesc->release();
	return result;
}

IOReturn EMUUSBAudioDevice::getFeatureUnitSetting(UInt8 controlSelector, UInt8 unitID, UInt8 channelNumber, UInt8 requestType, SInt16 * target) {
    IOReturn	result = kIOReturnError;
	if (target && mControlInterface) {
		IOUSBDevRequestDesc			devReq;
		UInt16						theSetting = 0;
		IOBufferMemoryDescriptor*	settingDesc = NULL;
		UInt8						length = 0;
		if (MUTE_CONTROL == controlSelector)
			length = 1;
		else if (VOLUME_CONTROL == controlSelector)
			length = 2;

		settingDesc = IOBufferMemoryDescriptor::withOptions(kIODirectionIn, length);
		if (settingDesc) {
			devReq.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBClass, kUSBInterface);
			devReq.bRequest = requestType;
			devReq.wValue =(controlSelector << 8) | channelNumber;
			devReq.wIndex =(0xFF00 &(unitID << 8)) |(0x00FF & mInterfaceNum);
			devReq.wLength = length;
			devReq.pData = settingDesc;

			result = deviceRequest(&devReq);
			if (kIOReturnSuccess == result) {
				memcpy(&theSetting, settingDesc->getBytesNoCopy(), length);
				*target = USBToHostWord(theSetting);
			}
			settingDesc->release();
		}
	}
	return result;
}

IOReturn EMUUSBAudioDevice::setFeatureUnitSetting(UInt8 controlSelector, UInt8 unitID, UInt8 channelNumber, UInt8 requestType, UInt16 newValue, UInt16 newValueLen) {
	IOReturn		result = kIOReturnError;
	if (mControlInterface) {
		IOUSBDevRequestDesc			devReq;
		IOBufferMemoryDescriptor*	settingDesc = NULL;
		settingDesc = OSTypeAlloc(IOBufferMemoryDescriptor);
		if (settingDesc) {
			settingDesc->initWithBytes(&newValue, newValueLen, kIODirectionIn);

			devReq.bmRequestType = USBmakebmRequestType(kUSBOut, kUSBClass, kUSBInterface);
			devReq.bRequest = requestType;
			devReq.wValue =(controlSelector << 8) | channelNumber;
			devReq.wIndex =(0xFF00 &(unitID << 8)) |(0x00FF & mInterfaceNum);
			devReq.wLength = newValueLen;
			devReq.pData = settingDesc;

			if (TRUE != isInactive())  	// In case we've been unplugged during sleep
				result = deviceRequest(&devReq);
			settingDesc->release();
		}
	}
	return result;	
}

IOReturn EMUUSBAudioDevice::getCurMute(UInt8 unitID, UInt8 channelNumber, SInt16 * target) {
	return getFeatureUnitSetting(MUTE_CONTROL, unitID, channelNumber, GET_CUR, target);
}

IOReturn EMUUSBAudioDevice::getCurVolume(UInt8 unitID, UInt8 channelNumber, SInt16 * target) {
	return getFeatureUnitSetting(VOLUME_CONTROL, unitID, channelNumber, GET_CUR, target);
}

IOReturn EMUUSBAudioDevice::getMaxVolume(UInt8 unitID, UInt8 channelNumber, SInt16 * target) {
	return getFeatureUnitSetting(VOLUME_CONTROL, unitID, channelNumber, GET_MAX, target);
}

IOReturn EMUUSBAudioDevice::getMinVolume(UInt8 unitID, UInt8 channelNumber, SInt16 * target) {
	return getFeatureUnitSetting(VOLUME_CONTROL, unitID, channelNumber, GET_MIN, target);
}

IOReturn EMUUSBAudioDevice::getVolumeResolution(UInt8 unitID, UInt8 channelNumber, UInt16 * target) {
	return getFeatureUnitSetting(VOLUME_CONTROL, unitID, channelNumber, GET_RES,(SInt16 *) target);
}

IOReturn EMUUSBAudioDevice::setCurVolume(UInt8 unitID, UInt8 channelNumber, SInt16 volume) // volume is negative
{	
	return setFeatureUnitSetting(VOLUME_CONTROL, unitID, channelNumber, SET_CUR, volume, 2);
}

IOReturn EMUUSBAudioDevice::setCurMute(UInt8 unitID, UInt8 channelNumber, SInt16 mute) 
{
	debugIOLog("+EMUUSBAudioDevice::setCurMute: unitID= %d mute= %d channelNumber= %d", unitID, mute, channelNumber);
	
	return setFeatureUnitSetting(MUTE_CONTROL, unitID, channelNumber, SET_CUR, mute, 1);
}

const SInt32 kHardVolumeMaxDb = 12;
const SInt32 kHardVolumeMinDb = -127;
const UInt8 kMasterVolumeIndex = 1;

void EMUUSBAudioDevice::addHardVolumeControls(
	IOAudioEngine* audioEngine) 
{
	SInt32		maxValue = kHardVolumeMaxDb * 256;
	SInt32		minValue = kHardVolumeMinDb * 256;
	IOFixed		minDB = kHardVolumeMinDb << 16; // 16.16 fixed
	IOFixed		maxDB = kHardVolumeMaxDb << 16;
	UInt32		cntrlID = 0;
	const char *channelName = 0;
	
	UInt8	ignoreMixerID = 0;
	UInt8	ignoreProcessingUnitID = 0;
	UInt8	numberOfInputFeatureUnits = 0;
	UInt8	numberOfOutputFeatureUnits = 0;
	UInt8	ignoreInputFeatureUnits[EMUUSBUserClient::kMaxNumberOfUnits];
	UInt8	outputFeatureUnits[EMUUSBUserClient::kMaxNumberOfUnits];
	memset(outputFeatureUnits, 0, sizeof(UInt8) * EMUUSBUserClient::kMaxNumberOfUnits);
		
	IOReturn result = GetVariousUnits(ignoreInputFeatureUnits,
										numberOfInputFeatureUnits,
										outputFeatureUnits,
										numberOfOutputFeatureUnits,
										ignoreMixerID,
										ignoreProcessingUnitID);
												
					
	SInt16 currentVolumeValue = 0;
	if (result == kIOReturnSuccess) 
	{
		mHardwareOutputVolumeID = outputFeatureUnits[0]; // EMU_OUTPUT - first feature unit from the output
		
		getFeatureUnitSetting(VOLUME_CONTROL, mHardwareOutputVolumeID, kMasterVolumeIndex, GET_CUR, &currentVolumeValue);
	}
	
	SInt32 initialValue = (SInt32)currentVolumeValue;
	
// output
	mHardwareOutputVolume = EMUUSBAudioHardLevelControl::create(initialValue,
																minValue,
																maxValue,
																minDB,
																maxDB,
																kIOAudioControlChannelIDAll,
																channelName,
																cntrlID,
																kIOAudioLevelControlSubTypeVolume,
																kIOAudioControlUsageOutput);
										
	if (mHardwareOutputVolume) 
	{
		mHardwareOutputVolume->setLinearScale(true); // here, false is true and true is false
		
		mHardwareOutputVolume->setValueChangeHandler(hardwareVolumeChangedHandler, this);
		audioEngine->addDefaultAudioControl(mHardwareOutputVolume);
		mHardwareOutputVolume->release();
	}
	
	mOuputMuteControl = IOAudioToggleControl::createMuteControl(false, 
															kIOAudioControlChannelIDAll, 
															0, 
															cntrlID, 
															kIOAudioControlUsageOutput);

	if (mOuputMuteControl) 
	{
		mOuputMuteControl->setValueChangeHandler(hardwareMuteChangedHandler, this);
		audioEngine->addDefaultAudioControl(mOuputMuteControl);
		mOuputMuteControl->release();
	}
}

IOReturn EMUUSBAudioDevice::hardwareVolumeChangedHandler(OSObject * target, IOAudioControl * audioControl, SInt32 oldValue, SInt32 newValue) 
{
    IOReturn				result = kIOReturnSuccess;
	EMUUSBAudioDevice *		device = OSDynamicCast(EMUUSBAudioDevice, target);
	
	if (audioControl->getUsage() == kIOAudioControlUsageOutput) 
	{
		debugIOLog("EMUUSBAudioDevice::hardwareVolumeChangedHandler output %d: channel= %d oldValue= %d newValue= %d",
					device->mHardwareOutputVolumeID, 
					audioControl->getChannelID(), oldValue, newValue);
		
		device->setFeatureUnitSetting(VOLUME_CONTROL, device->mHardwareOutputVolumeID, kMasterVolumeIndex, SET_CUR, HostToUSBWord((SInt16)newValue), sizeof(UInt16));
		device->setFeatureUnitSetting(VOLUME_CONTROL, device->mHardwareOutputVolumeID, kMasterVolumeIndex + 1, SET_CUR, HostToUSBWord((SInt16)newValue), sizeof(UInt16));
		
		// send notice up to client
		if (device->mUserClient)
		{
			device->mUserClient->SendEventNotification(EMU_VOLUME_EVENT);
		}
	}
	
	return result;
}

IOReturn EMUUSBAudioDevice::hardwareMuteChangedHandler(OSObject * target, IOAudioControl * audioControl, SInt32 oldValue, SInt32 newValue) 
{
	EMUUSBAudioDevice* device = OSDynamicCast(EMUUSBAudioDevice, target); 
		
	if (audioControl->getUsage() == kIOAudioControlUsageOutput) 
	{
		device->setFeatureUnitSetting(MUTE_CONTROL, device->mHardwareOutputVolumeID, kMasterVolumeIndex, SET_CUR, HostToUSBWord((SInt16)newValue), sizeof(UInt16));
		
		// send notice up to client
		if (device->mUserClient)
		{
			device->mUserClient->SendEventNotification(EMU_MUTE_EVENT);
		}
	}

	return kIOReturnSuccess;
}

IOReturn EMUUSBAudioDevice::controlChangedHandler(OSObject * target, IOAudioControl * audioControl, SInt32 oldValue, SInt32 newValue) 
{
    IOReturn				result = kIOReturnError;
	EMUUSBAudioDevice *		device = NULL;
	
	if (target) {
		device = OSDynamicCast(EMUUSBAudioDevice, target);
	}
	
	debugIOLog("in controlChangeHandler");
	if (device && audioControl)
	 {
		debugIOLog("controlChangeHandler %d",newValue);
		result = device->protectedControlChangedHandler(audioControl, oldValue, newValue);
    }
	return result;
}

// public handler for the various XUs
IOReturn EMUUSBAudioDevice::deviceXUChangeHandler(OSObject *target, IOAudioControl *audioControl, SInt32 oldValue, SInt32 newValue) {
	IOReturn				result = kIOReturnError;
	EMUUSBAudioDevice *		device = OSDynamicCast(EMUUSBAudioDevice, target);
	debugIOLog("in deviceXUChangeHandler");
	if (device) {
		debugIOLog("deviceXUChangeHandler %d", newValue);
		result = device->protectedXUChangeHandler(audioControl, oldValue, newValue);
	}

	return result;
}

IOReturn EMUUSBAudioDevice::protectedControlChangedHandler(IOAudioControl * audioControl, SInt32 oldValue, SInt32 newValue) {
    IOReturn	result = kIOReturnError;
    UInt32		type = audioControl->getType();

    if (kIOAudioControlTypeLevel == type) 
    	result = doVolumeControlChange(audioControl, oldValue, newValue);
    else if (kIOAudioControlTypeToggle == type)
    	result = doToggleControlChange(audioControl, oldValue, newValue);
    else if (kIOAudioControlTypeSelector == type)
    	result = doSelectorControlChange(audioControl, oldValue, newValue);

	return result;
}

IOReturn EMUUSBAudioDevice::doSelectorControlChange(IOAudioControl * audioControl, SInt32 oldValue, SInt32 newValue) {
    IOReturn	result = kIOReturnError;
	
	if (audioControl)
	{
		UInt32		usage = audioControl->getUsage();
		if (kIOAudioControlUsageInput == usage) {// kIOAudioControlUsageOutput not handled
			// have to distinguish between input selector and clock selector by subtype
			UInt32 subtype = audioControl->getSubType();
			if (subtype == kIOAudioSelectorControlSubTypeClockSource) {
				result = doClockSourceSelectorChange(audioControl, oldValue, newValue);
			} else {
				result = doInputSelectorChange(audioControl, oldValue, newValue);
			}
		} else if (kIOAudioControlUsagePassThru == usage) {
			result = doPassThruSelectorChange(audioControl, oldValue, newValue);
		}
	}
	
	return result;
}

IOReturn EMUUSBAudioDevice::doVolumeControlChange(IOAudioControl * audioControl, SInt32 oldValue, SInt32 newValue) {
	IOReturn		result = kIOReturnError;
	SInt16			newVolume = kNegativeInfinity;// default init value
	SInt16			deviceMin = 0;
	UInt16			volRes = 0;
	UInt8			unitID = audioControl->getControlID();
	UInt8			channelNum = audioControl->getChannelID();

	if((kIOAudioControlUsageInput == audioControl->getUsage())
	     ||(FALSE == mDeviceIsInMonoMode)) {
		getMinVolume(unitID, channelNum, &deviceMin);
		SInt16	offset = -deviceMin;
		if (0 <= newValue) {// for all values greater than or equal to 0
			getVolumeResolution(unitID, channelNum, &volRes);
			newVolume = ((newValue - (newValue > 0)) * volRes) - offset;
		}

		debugIOLog("doVolumeControlChange: setting volume to 0x%x", newVolume);
		result = setCurVolume(unitID, channelNum, HostToUSBWord(newVolume));
	
	} else {
		FailIf(NULL == mMonoControlsArray, Exit);
		debugIOLog("doVolumeControlChange: performing mono volume control change");
		UInt8	total = mMonoControlsArray->getCount();
		for(UInt8 i = 0; i < total; ++i) {
			channelNum =((OSNumber *) mMonoControlsArray->getObject(i))->unsigned8BitValue();
			getMinVolume(unitID, channelNum, &deviceMin);
			SInt16	offset = -deviceMin;
		
			newVolume = kNegativeInfinity;// default init value
			if (0 <= newValue) {// for values greater than or equal to 0
				getVolumeResolution(unitID, channelNum, &volRes);
				newVolume = ((newValue - (newValue > 0)) * volRes) - offset;
			}
			result = setCurVolume(unitID, channelNum, HostToUSBWord(newVolume));
			debugIOLog("doVolumeControlChange: set volume for channel %d to 0x%x = %d", channelNum, newVolume, result);
		}
	}
	
Exit:
	return result;
}

IOReturn EMUUSBAudioDevice::doToggleControlChange(IOAudioControl * audioControl, SInt32 oldValue, SInt32 newValue) 
{
	if (audioControl)
	{
		setCurMute(audioControl->getControlID(), audioControl->getChannelID(), HostToUSBWord(newValue));
	}
	
	return kIOReturnSuccess;
}

IOReturn EMUUSBAudioDevice::doPassThruSelectorChange(IOAudioControl * audioControl, SInt32 oldValue, SInt32 newValue) {
	EMUUSBAudioEngine *			usbAudioEngine = NULL;
	OSArray *					playThroughPaths = NULL;
	OSArray *					passThruVolControlsArray = NULL;
	OSArray *					thePath = NULL;
	OSDictionary *				engineInfo = OSDynamicCast(OSDictionary, mRegisteredEngines->getObject(audioControl->getControlID()));
	OSNumber *					number = OSDynamicCast(OSNumber, engineInfo->getObject(kInterface));
	UInt32						numPassThruControls;
	UInt8						interfaceNum = 0;
	UInt8						altSetting = 0;
	UInt8						featureUnitID = 0;
	UInt8						pathIndex = newValue & 0x000000FF;

	FailIf(NULL == engineInfo, Exit);
	usbAudioEngine = OSDynamicCast(EMUUSBAudioEngine, engineInfo->getObject(kEngine));
	FailIf((!usbAudioEngine || !number), Exit);
	interfaceNum = number->unsigned8BitValue();
	number = OSDynamicCast(OSNumber, engineInfo->getObject(kAltSetting));
	FailIf(NULL == number, Exit);
	altSetting = number->unsigned8BitValue();
	passThruVolControlsArray = OSDynamicCast(OSArray, engineInfo->getObject(kPassThruVolControls));
	FailIf(NULL == passThruVolControlsArray, Exit);
	numPassThruControls = passThruVolControlsArray->getCount();

	usbAudioEngine->pauseAudioEngine();
	usbAudioEngine->beginConfigurationChange();
	for(UInt32 i = 0; i < numPassThruControls; ++i) 
		usbAudioEngine->removeDefaultAudioControl((IOAudioLevelControl *)passThruVolControlsArray->getObject(i));
	
	passThruVolControlsArray->flushCollection();
	playThroughPaths = OSDynamicCast(OSArray, engineInfo->getObject(kPassThruPathsArray));
	FailIf(NULL == playThroughPaths, Exit);
	thePath = OSDynamicCast(OSArray, playThroughPaths->getObject(pathIndex));
	FailIf(NULL == thePath, Exit);
	featureUnitID = getBestFeatureUnitInPath(thePath, kIOAudioControlUsagePassThru, interfaceNum, altSetting, kVolumeControl);
	addVolumeControls(usbAudioEngine, featureUnitID, interfaceNum, altSetting, kIOAudioControlUsagePassThru);
	usbAudioEngine->completeConfigurationChange();
	usbAudioEngine->resumeAudioEngine();

Exit:
	return kIOReturnSuccess;
}

IOReturn EMUUSBAudioDevice::doInputSelectorChange(IOAudioControl * audioControl, SInt32 oldValue, SInt32 newValue) {
	EMUUSBAudioEngine *		usbAudioEngine = NULL;
	OSArray *				inputGainControlsArray = NULL;
	OSArray *				arrayOfPathsFromOutputTerminal = NULL;
	OSArray *				thePath = NULL;
	OSNumber *				number = NULL;
	OSDictionary *			engineInfo = OSDynamicCast(OSDictionary, mRegisteredEngines->getObject(audioControl->getControlID()));
    IOReturn				result= kIOReturnError;
	UInt32					numGainControls = 0;
	UInt8					interfaceNum = 0;
	UInt8					altSetting = 0;
	UInt8					featureUnitID = 0;
	UInt8					selectorUnitID = (newValue & 0x0000FF00) >> 8;
	UInt8					selectorPosition = newValue & 0x000000FF;
	UInt8					pathsToOutputTerminal = (newValue & 0xFF000000) >> 24;
	UInt8					pathIndex = (newValue & 0x00FF0000) >> 16;

	result = setSelectorSetting(selectorUnitID, selectorPosition);
	FailIf((kIOReturnSuccess != result || !engineInfo), Exit);
	usbAudioEngine = OSDynamicCast(EMUUSBAudioEngine, engineInfo->getObject(kEngine));
	FailIf(NULL == usbAudioEngine, Exit);
	number = OSDynamicCast(OSNumber, engineInfo->getObject(kInterface));
	FailIf(NULL == number, Exit);
	interfaceNum = number->unsigned8BitValue();
	number = OSDynamicCast(OSNumber, engineInfo->getObject(kAltSetting));
	FailIf(NULL == number, Exit);
	altSetting = number->unsigned8BitValue();
	inputGainControlsArray = OSDynamicCast(OSArray, engineInfo->getObject(kInputGainControls));
	FailIf(NULL == inputGainControlsArray, Exit);
	numGainControls = inputGainControlsArray->getCount();

	usbAudioEngine->pauseAudioEngine();
	usbAudioEngine->beginConfigurationChange();
	for(UInt32 i = 0; i < numGainControls; ++i) 
		usbAudioEngine->removeDefaultAudioControl((IOAudioLevelControl *)inputGainControlsArray->getObject(i));
	
	inputGainControlsArray->flushCollection();
	arrayOfPathsFromOutputTerminal = OSDynamicCast(OSArray, mControlGraph->getObject(pathsToOutputTerminal));
	FailIf(NULL == arrayOfPathsFromOutputTerminal, Exit);
	thePath = OSDynamicCast(OSArray, arrayOfPathsFromOutputTerminal->getObject(pathIndex));
	FailIf(NULL == thePath, Exit);
	featureUnitID = getBestFeatureUnitInPath(thePath, kIOAudioControlUsageInput, interfaceNum, altSetting, kVolumeControl);
	addVolumeControls(usbAudioEngine, featureUnitID, interfaceNum, altSetting, kIOAudioControlUsageInput);
	usbAudioEngine->completeConfigurationChange();
	usbAudioEngine->resumeAudioEngine();

Exit:
	return result;
}

IOReturn EMUUSBAudioDevice::doClockSourceSelectorChange(IOAudioControl * audioControl, SInt32 oldValue, SInt32 newValue) 
{
	if (audioControl)
	{
		// change the custom clock-source control to equal the standard clock source control
		OSNumber  *stupid = OSNumber::withNumber(newValue,8);
		
		if (stupid && mClockSelector)
		{
			mClockSelector->setValue(stupid);
			stupid->release();
		}
	}
	
	return kIOReturnSuccess;
}

// This should detect a playthrough path; which is a non-streaming input terminal connected to a non-streaming output terminal.
OSArray * EMUUSBAudioDevice::getPlaythroughPaths() {
	OSArray *				playThroughPaths = NULL;
	OSArray *				aPath = NULL;
	OSNumber *				theUnitIDNum = NULL;
	UInt32					numOutputTerminalArrays = mControlGraph->getCount();
	UInt32					numPathsFromOutputTerminal = 0;

	FailIf(NULL == mControlInterface, Exit);

	for(UInt32 pathsToOTerminalN = 0; pathsToOTerminalN < numOutputTerminalArrays; ++pathsToOTerminalN) {
		OSArray*	arrayOfPathsFromOutputTerminal = OSDynamicCast(OSArray, mControlGraph->getObject(pathsToOTerminalN));
		FailIf(NULL == arrayOfPathsFromOutputTerminal, Exit);
		aPath = OSDynamicCast(OSArray, arrayOfPathsFromOutputTerminal->getObject(0));
		FailIf(NULL == aPath, Exit);
		theUnitIDNum = OSDynamicCast(OSNumber, aPath->getObject(0));
		FailIf(NULL == theUnitIDNum, Exit);
		UInt8 unitID = theUnitIDNum->unsigned8BitValue();
		if(mUSBAudioConfig->GetOutputTerminalType(mInterfaceNum, 0, unitID) == 0x101)
			continue;		// only looking for non-streaming outputs

		numPathsFromOutputTerminal = arrayOfPathsFromOutputTerminal->getCount();
		for(UInt32 pathNumber = 0; pathNumber < numPathsFromOutputTerminal; ++pathNumber) {
			aPath = OSDynamicCast(OSArray, arrayOfPathsFromOutputTerminal->getObject(pathNumber));
			FailIf(NULL == aPath, Exit);
			OSNumber*	theUnitIDNum = OSDynamicCast(OSNumber, aPath->getLastObject());
			if (theUnitIDNum) { 
				UInt8	unitID = theUnitIDNum->unsigned8BitValue();
				UInt16	terminalType = mUSBAudioConfig->GetInputTerminalType(mInterfaceNum, 0, unitID);
				if(terminalType != 0x101) {
					if(NULL == playThroughPaths) 
						playThroughPaths = OSArray::withObjects((const OSObject **)&aPath, 1);
					 else 
						playThroughPaths->setObject(aPath);
				}
			}
		}
	}

Exit:
	return playThroughPaths;
}

// This finds the feature unit closest to the input terminal.
UInt8 EMUUSBAudioDevice::getBestFeatureUnitInPath(OSArray * thePath, UInt32 direction, UInt8 interfaceNum, UInt8 altSettingNum, UInt32 controlTypeWanted) {
	UInt8	featureUnitID = 0;
	if (mControlInterface) {
		UInt32		numUnitsInPath = thePath->getCount();
		Boolean		found = FALSE;

		switch(direction) {
			case kIOAudioControlUsageInput:
			case kIOAudioControlUsagePassThru:
			// Find the feature unit closest to the input terminal.
				for(UInt8 unitIndex = numUnitsInPath - 2; unitIndex > 0; --unitIndex) {
					OSNumber* unitIDNum = OSDynamicCast(OSNumber, thePath->getObject(unitIndex));
					if(NULL != unitIDNum) {
						UInt8	unitID = unitIDNum->unsigned8BitValue();
						if(FEATURE_UNIT == mUSBAudioConfig->GetSubType(mInterfaceNum, 0, unitID)) {
							UInt8	total = mUSBAudioConfig->GetNumChannels(interfaceNum, altSettingNum);
							for(UInt8 channelNum = 0; channelNum <= total; ++channelNum) {
								if (((kVolumeControl == controlTypeWanted) && (mUSBAudioConfig->ChannelHasVolumeControl(mInterfaceNum, 0, unitID, channelNum)))
										|| ((kMuteControl == controlTypeWanted) && (mUSBAudioConfig->ChannelHasMuteControl(mInterfaceNum, 0, unitID, channelNum)))) {
									featureUnitID = unitID;
									found = TRUE;
								//	break;// get out of the inner loop when found -- check if this is ok
								}
							}
						}
					}
				}
				break;
			case kIOAudioControlUsageOutput:
			default:
				// Find the feature unit closest to the output terminal.
				for(UInt8 unitIndex = 1; unitIndex < numUnitsInPath && !found; ++unitIndex) {
					OSNumber* unitIDNum = OSDynamicCast(OSNumber, thePath->getObject(unitIndex));
					if(NULL != unitIDNum) {
						UInt8	unitID = unitIDNum->unsigned8BitValue();
						if(FEATURE_UNIT == mUSBAudioConfig->GetSubType(mInterfaceNum, 0, unitID)) {
							UInt8	total = mUSBAudioConfig->GetNumChannels(interfaceNum, altSettingNum);
							for(UInt8 channelNum = 0; channelNum <= total; ++channelNum) {
								if (((kVolumeControl == controlTypeWanted) && (mUSBAudioConfig->ChannelHasVolumeControl(mInterfaceNum, 0, unitID, channelNum)))
										|| ((kMuteControl == controlTypeWanted) && (mUSBAudioConfig->ChannelHasMuteControl(mInterfaceNum, 0, unitID, channelNum)))) {
									featureUnitID = unitID;
									found = TRUE;
									//break;// get out of the inner loop 
								}
							}
						}
					}
				}
				break;
		}
	}
	return featureUnitID;
}

UInt8 EMUUSBAudioDevice::getSelectorSetting(UInt8 selectorID) {
	UInt8	setting = 0;

	if (mControlInterface) {
		IOBufferMemoryDescriptor*	settingDesc = IOBufferMemoryDescriptor::withOptions(kIODirectionIn, 1);
		if (settingDesc) {
			IOUSBDevRequestDesc		devReq;
			devReq.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBClass, kUSBInterface);
			devReq.bRequest = GET_CUR;
			devReq.wValue = 0;
			devReq.wIndex =(0xFF00 &(selectorID << 8)) |(0x00FF & mInterfaceNum);
			devReq.wLength = 1;
			devReq.pData = settingDesc;

			if (kIOReturnSuccess == deviceRequest(&devReq))
				memcpy(&setting, settingDesc->getBytesNoCopy(), 1);
			settingDesc->release();
		}
		
	}
	return setting;
}

IOReturn EMUUSBAudioDevice::setSelectorSetting(UInt8 selectorID, UInt8 setting) {
	IOReturn	result = kIOReturnError;
	if (mControlInterface) {
		IOBufferMemoryDescriptor*	settingDesc = OSTypeAlloc(IOBufferMemoryDescriptor);
		if (settingDesc) {
			IOUSBDevRequestDesc		devReq;
			settingDesc->initWithBytes(&setting, 1, kIODirectionIn);
			devReq.bmRequestType = USBmakebmRequestType(kUSBOut, kUSBClass, kUSBInterface);
			devReq.bRequest = SET_CUR;
			devReq.wValue = 0;
			devReq.wIndex =(0xFF00 &(selectorID << 8)) |(0x00FF & mInterfaceNum);
			devReq.wLength = 1;
			devReq.pData = settingDesc;

			result = deviceRequest(&devReq);
			settingDesc->release();
		}
	}
	return result;
}

void EMUUSBAudioDevice::setMonoState(Boolean state) {
	mDeviceIsInMonoMode = state;
}

IOReturn EMUUSBAudioDevice::createControlsForInterface(IOAudioEngine *audioEngine, UInt8 interfaceNum, UInt8 altSettingNum) {
    IOReturn		result = kIOReturnError;
    debugIOLog("+EMUUSBAudioDevice[%p]::createControlsForInterface %d %d", this, interfaceNum, altSettingNum);
	mTerminatingDriver = FALSE;
	if (audioEngine)
		result = doControlStuff(audioEngine, interfaceNum, altSettingNum);
	
	return result;
}

OSArray * EMUUSBAudioDevice::BuildConnectionGraph(UInt8 controlInterfaceNum) {
	OSArray *		allOutputTerminalPaths = OSArray::withCapacity(1);
	OSArray *		pathsFromOutputTerminalN = OSArray::withCapacity(1);
	OSArray *		thisPath = NULL;
	UInt8			numTerminals = mUSBAudioConfig->GetNumOutputTerminals(controlInterfaceNum, 0);

	if (allOutputTerminalPaths && pathsFromOutputTerminalN) {
		for(UInt8 terminalIndex = 0; terminalIndex < numTerminals; ++terminalIndex) {
			BuildPath(controlInterfaceNum, mUSBAudioConfig->GetIndexedOutputTerminalID(controlInterfaceNum, 0, terminalIndex), pathsFromOutputTerminalN, thisPath);
			allOutputTerminalPaths->setObject(pathsFromOutputTerminalN);
			pathsFromOutputTerminalN->release();
			pathsFromOutputTerminalN = OSArray::withCapacity(1);
			FailIf(NULL == pathsFromOutputTerminalN, Exit);
		}
		pathsFromOutputTerminalN->release();
	}
Exit:
	return allOutputTerminalPaths;
}

OSArray * EMUUSBAudioDevice::BuildPath(UInt8 controlInterfaceNum, UInt8 startingUnitID, OSArray * allPaths, OSArray * startingPath) {
	OSArray *				curPath = NULL;
	OSNumber *				thisUnitIDNum = OSNumber::withNumber(startingUnitID, 8);
	UInt8 *					sourceIDs;
	UInt8					thisUnitID = startingUnitID;
	UInt8					sourceID;
	UInt8					subType = mUSBAudioConfig->GetSubType(controlInterfaceNum, 0, thisUnitID);

	FailIf(NULL == thisUnitIDNum, Exit);
	if(NULL != startingPath) 
		curPath = OSArray::withArray(startingPath);
	if(NULL == curPath) 
		curPath = OSArray::withObjects((const OSObject **)&thisUnitIDNum, 1);
	 else 
		curPath->setObject(thisUnitIDNum);
	
	thisUnitIDNum->release();
	thisUnitIDNum = NULL;
	while(INPUT_TERMINAL != subType && subType != 0) {
		switch(subType) {
			case MIXER_UNIT:
			case SELECTOR_UNIT:
			case EXTENSION_UNIT:
			case PROCESSING_UNIT:
				{
					OSArray*	tempPath = OSArray::withArray(curPath);// curPath is not Null at this point
					UInt8 numSources = mUSBAudioConfig->GetNumSources(controlInterfaceNum, 0, thisUnitID);
					sourceIDs = mUSBAudioConfig->GetSourceIDs(controlInterfaceNum, 0, thisUnitID);
					tempPath = OSArray::withArray(curPath);
					for(UInt8 i = 0; i < numSources; ++i) {
						if(NULL == curPath)
							curPath = OSArray::withCapacity(1);
						
						FailIf(NULL == curPath, Exit);
						curPath = BuildPath(controlInterfaceNum, sourceIDs[i], allPaths, tempPath);// recurse
						if(curPath && curPath->getCount()) {
							thisUnitIDNum = OSDynamicCast(OSNumber, curPath->getLastObject());
							if(thisUnitIDNum) { //DT a FailIf at this point will cause a leak
								UInt8	unitID = thisUnitIDNum->unsigned8BitValue();
								if(unitID && mUSBAudioConfig->GetSubType(controlInterfaceNum, 0, unitID) == INPUT_TERMINAL) 
									allPaths->setObject(curPath);
							}
						}
						if(curPath) {
							curPath->release();
							curPath = NULL;
						}
						FailIf(NULL == thisUnitIDNum, Exit);// fail here instead of earlier
					}
					tempPath->release();// release the tempPath 
					return curPath;// originally set the subtype to 0 to stop 
				}
				break;
			case OUTPUT_TERMINAL:
			case FEATURE_UNIT:
			default:
				sourceID = mUSBAudioConfig->GetSourceID(controlInterfaceNum, 0, thisUnitID);
				thisUnitID = sourceID;
				thisUnitIDNum = OSNumber::withNumber(thisUnitID, 8);
				if(NULL != thisUnitIDNum) {
					curPath->setObject(thisUnitIDNum);
					thisUnitIDNum->release();
					thisUnitIDNum = NULL;
				}
				subType = mUSBAudioConfig->GetSubType(controlInterfaceNum, 0, thisUnitID);
				if(subType == INPUT_TERMINAL && mUSBAudioConfig->GetSubType(controlInterfaceNum, 0, startingUnitID) == OUTPUT_TERMINAL) {
					allPaths->setObject(curPath);
				}
				break;
		}
	}

Exit:
	return curPath;
}

char * EMUUSBAudioDevice::TerminalTypeString(UInt16 terminalType) {
	char *					terminalTypeString;

	switch(terminalType) {
		case 0x101:											terminalTypeString = "USB streaming";									break;
		case INPUT_UNDEFINED:								terminalTypeString = "InputUndefined";									break;
		case INPUT_MICROPHONE:								terminalTypeString = "Microphone";										break;
		case INPUT_DESKTOP_MICROPHONE:						terminalTypeString = "Desktop Microphone";								break;
		case INPUT_PERSONAL_MICROPHONE:						terminalTypeString = "Personal Microphone";								break;
		case INPUT_OMNIDIRECTIONAL_MICROPHONE:				terminalTypeString = "Omnidirectional Microphone";						break;
		case INPUT_MICROPHONE_ARRAY:						terminalTypeString = "Microphone Array";								break;
		case INPUT_PROCESSING_MICROPHONE_ARRAY:				terminalTypeString = "Processing Microphone Array";						break;
		case INPUT_MODEM_AUDIO:								terminalTypeString = "Modem Audio";										break;
		case OUTPUT_UNDEFINED:								terminalTypeString = "Output Undefined";								break;
		case OUTPUT_SPEAKER:								terminalTypeString = "Speaker";											break;
		case OUTPUT_HEADPHONES:								terminalTypeString = "Headphones";										break;
		case OUTPUT_HEAD_MOUNTED_DISPLAY_AUDIO:				terminalTypeString = "Head Mounted Display Audio";						break;
		case OUTPUT_DESKTOP_SPEAKER:						terminalTypeString = "Desktop Speaker";									break;
		case OUTPUT_ROOM_SPEAKER:							terminalTypeString = "Room Speaker";									break;
		case OUTPUT_COMMUNICATION_SPEAKER:					terminalTypeString = "Communication Speaker";							break;
		case OUTPUT_LOW_FREQUENCY_EFFECTS_SPEAKER:			terminalTypeString = "Low Frequency Effects Speaker";					break;
		case BIDIRECTIONAL_UNDEFINED:						terminalTypeString = "Bidirectional Undefined";							break;
		case BIDIRECTIONAL_HANDSET:							terminalTypeString = "Bidirectional Handset";							break;
		case BIDIRECTIONAL_HEADSET:							terminalTypeString = "Bidirectional Headset";							break;
		case BIDIRECTIONAL_SPEAKERPHONE_NO_ECHO_REDX:		terminalTypeString = "Bidirectional Speakerphone No Echo Redx";			break;
		case BIDIRECTIONAL_ECHO_SUPPRESSING_SPEAKERPHONE:	terminalTypeString = "Bidirectional Echo Suppressing Speakerphone";		break;
		case BIDIRECTIONAL_ECHO_CANCELING_SPEAKERPHONE:		terminalTypeString = "Bidirectional Echo Canceling Speakerphone";		break;
		case TELEPHONY_UNDEFINED:							terminalTypeString = "Telephone Undefined";								break;
		case TELEPHONY_PHONE_LINE:							terminalTypeString = "Telephone Line";									break;
		case TELEPHONY_TELEPHONE:							terminalTypeString = "Telephone";										break;
		case TELEPHONY_DOWN_LINE_PHONE:						terminalTypeString = "Down Line Phone";									break;
		case EXTERNAL_UNDEFINED:							terminalTypeString = "External Undefined";								break;
		case EXTERNAL_ANALOG_CONNECTOR:						terminalTypeString = "External Analog Connector";						break;
		case EXTERNAL_DIGITAL_AUDIO_INTERFACE:				terminalTypeString = "External Digital Audio Interface";				break;
		case EXTERNAL_LINE_CONNECTOR:						terminalTypeString = "External Line Connector";							break;
		case EXTERNAL_LEGACY_AUDIO_CONNECTOR:				terminalTypeString = "External Legacy Audio Connector";					break;
		case EXTERNAL_SPDIF_INTERFACE:						terminalTypeString = "External SPDIF Interface";						break;
		case EXTERNAL_1394_DA_STREAM:						terminalTypeString = "External 1394 DA Stream";							break;
		case EXTERNAL_1394_DV_STREAM_SOUNDTRACK:			terminalTypeString = "External 1394 DV Stream Soundtrack";				break;
		case EMBEDDED_UNDEFINED:							terminalTypeString = "Embedded Undefined";								break;
		case EMBEDDED_LEVEL_CALIBRATION_NOISE_SOURCE:		terminalTypeString = "Embedded Level Calibration Noise Source";			break;
		case EMBEDDED_EQUALIZATION_NOISE:					terminalTypeString = "Embedded Equalization Noise";						break;
		case EMBEDDED_CD_PLAYER:							terminalTypeString = "Embedded CD Player";								break;
		case EMBEDDED_DAT:									terminalTypeString = "Embedded DAT";									break;
		case EMBEDDED_DCC:									terminalTypeString = "Embedded DCC";									break;
		case EMBEDDED_MINIDISK:								terminalTypeString = "Embedded Mini Disc";								break;
		case EMBEDDED_ANALOG_TAPE:							terminalTypeString = "Embedded Analog Tape";							break;
		case EMBEDDED_PHONOGRAPH:							terminalTypeString = "Embedded Phonograph";								break;
		case EMBEDDED_VCR_AUDIO:							terminalTypeString = "Embedded VCR Audio";								break;
		case EMBEDDED_VIDEO_DISC_AUDIO:						terminalTypeString = "Embedded Video Disc Audio";						break;
		case EMBEDDED_DVD_AUDIO:							terminalTypeString = "Embedded DVD Audio";								break;
		case EMBEDDED_TV_TUNER_AUDIO:						terminalTypeString = "Embedded TV Tuner Audio";							break;
		case EMBEDDED_SATELLITE_RECEIVER_AUDIO:				terminalTypeString = "Embedded Satellite Receiver Audio";				break;
		case EMBEDDED_CABLE_TUNER_AUDIO:					terminalTypeString = "Embedded Cable Tuner Audio";						break;
		case EMBEDDED_DSS_AUDIO:							terminalTypeString = "Embedded DSS Audio";								break;
		case EMBEDDED_RADIO_RECEIVER:						terminalTypeString = "Embedded Radio Receiver";							break;
		case EMBEDDED_RADIO_TRANSMITTER:					terminalTypeString = "Embedded Radio Transmitter";						break;
		case EMBEDDED_MULTITRACK_RECORDER:					terminalTypeString = "Embedded Multitrack Recorder";					break;
		case EMBEDDED_SYNTHESIZER:							terminalTypeString = "Embedded Synthesizer";							break;
		default:											terminalTypeString = "Unknown";											break;
	}

	return terminalTypeString;
}

IOReturn EMUUSBAudioDevice::deviceRequest(IOUSBDevRequestDesc * request, IOUSBCompletion * completion) {
	IOReturn		result = kIOReturnSuccess;

	if (mInterfaceLock) {
		IORecursiveLockLock(mInterfaceLock);

		if(FALSE == mTerminatingDriver) {
			UInt32	timeout = 5;
			while(timeout && mControlInterface) {
				result = mControlInterface->DeviceRequest(request, completion);
				if(result != kIOReturnSuccess) {
					if (kIOUSBPipeStalled == result) {
						IOUSBPipe*	pipe = mControlInterface->GetPipeObj(0);
						if (pipe) {
#ifdef DEBUGLOGGING
							IOReturn pipeResult = pipe->ClearPipeStall(true);
							debugIOLog("clearing pipe stall result %x", pipeResult);
#else
							pipe->ClearPipeStall(true);
#endif
							break;
						}
					}
					--timeout;
					IOSleep(1);
				} else {
					break;// out of time and there is something wrong with the device
				}
			}
		}
		IORecursiveLockUnlock(mInterfaceLock);
	}
	debugIOLog("++EMUUSBAudioDevice[%p]::deviceRequest(%p, %p) = %lx", this, request, completion, result);
	return result;
}

IOReturn EMUUSBAudioDevice::deviceRequest(IOUSBDevRequest * request, IOUSBCompletion * completion) {
	IOReturn	result = kIOReturnSuccess;
	if (mInterfaceLock) {
		IORecursiveLockLock(mInterfaceLock);
		if(FALSE == mTerminatingDriver) {
			UInt32	timeout = 5;
			while(timeout && mControlInterface) {
				result = mControlInterface->DeviceRequest(request, completion);
				if(result != kIOReturnSuccess) {
					--timeout;
					IOSleep(1);
				} else {
					break;// simply break out of the while loop
				}
			}
		}
		IORecursiveLockUnlock(mInterfaceLock);

		debugIOLog("++EMUUSBAudioDevice[%p]::deviceRequest(%p, %p) = %lx", this, request, completion, result);
	}
	return result;
}

IOReturn EMUUSBAudioDevice::deviceRequest(IOUSBDevRequest *request, EMUUSBAudioDevice * self, IOUSBCompletion *completion) {
	IOReturn		result = kIOReturnSuccess;

	if (self->mInterfaceLock) {
		IORecursiveLockLock(self->mInterfaceLock);
		if(FALSE == self->mTerminatingDriver) {
			UInt32	timeout = 5;
			while(timeout && self->mControlInterface) {
				result = self->mControlInterface->DeviceRequest(request, completion);
				if(result != kIOReturnSuccess) {
					--timeout;
					IOSleep(1);
				} else {
					break;// out of time and there is something wrong with the device
				}
			}
		}
		IORecursiveLockUnlock(self->mInterfaceLock);
	}
	debugIOLog("++EMUUSBAudioDevice[%p]::deviceRequest(%p, %p) = %lx", self, request, completion, result);

	return result;
}

bool EMUUSBAudioDevice::willTerminate(IOService * provider, IOOptionBits options) {
	debugIOLog("+EMUUSBAudioDevice[%p]::willTerminate(%p)", this, provider);
#if 0
	if (mStatusCheckTimer) {
		mStatusCheckTimer->cancelTimeout();
		mStatusCheckTimer->disable();
	}
	if (mUpdateTimer) {
		mUpdateTimer->cancelTimeout();
		mUpdateTimer->disable();
	}
#endif
	if(mControlInterface == provider)
		mTerminatingDriver = TRUE;

	debugIOLog("-EMUUSBAudioDevice[%p]::willTerminate", this);

	return super::willTerminate(provider, options);
}

void EMUUSBAudioDevice::setConfigurationApp(const char *bundleID) {// set configuration application - usually AMS
	setConfigurationApplicationBundle(bundleID);
}

bool EMUUSBAudioDevice::matchPropertyTable(OSDictionary * table, SInt32 *score) {
	bool		returnValue = false;// default when Device has no name
	
	// look at the dictionary (for now)
	OSCollectionIterator *iter = OSCollectionIterator::withCollection(table);
	if (iter != NULL) {
		const OSSymbol * dictionaryEntry = NULL;
		while (NULL != (dictionaryEntry = (const OSSymbol *)iter->getNextObject())) {
			const char *str = dictionaryEntry->getCStringNoCopy();	
			debugIOLog("table entry: %s",str);
		}
	}
	
	OSObject *	deviceName = table->getObject(kIOAudioDeviceNameKey);
	if(deviceName) {
		if(getProperty(kIOAudioDeviceNameKey)) // name exists
			returnValue = true;		
	} else {
		// This is our standard matchPropertyTable implementation
		returnValue = super::matchPropertyTable(table, score);
	}
	
	debugIOLog("++EMUUSBAudioDevice[%p]::matchPropertyTable(%p, %p) = %d(custom dictionary match)", 
					this, table, score, returnValue);
	
	return returnValue;
}


// routines to check the device status
// mStatusPipe is assumed to exist before any of these routines are to be called
void EMUUSBAudioDevice::StatusAction(OSObject *owner, IOTimerEventSource *sender) {
	if (owner) {
		EMUUSBAudioDevice*	device = (EMUUSBAudioDevice*) owner;
		if (device && !device->mTerminatingDriver) {
			device->doStatusCheck(sender);
			device->queryXU();
		}
	}
}

void EMUUSBAudioDevice::doStatusCheck(IOTimerEventSource *timer) {// routine to read the interrupt
	// read the status from the interrupt pipe. If there is something to look out for
	if (mStatusPipe)
		mStatusPipe->Read(mStatusBufferDesc,  &mStatusCheckCompletion);
}

void EMUUSBAudioDevice::queryXU() {
	UInt32	setting = 0;
	UInt32	dataLen = kStdDataLen;	// default standard data length
	UInt8	selector = 0;
	bool	clockSourceChange = (mQueryXU == mClockSrcXU);// change to clockSource XU
	bool	devOptionsChange = (mQueryXU == mDeviceOptionsXU);// change to deviceOptions XU
	bool	digitalChange = (mQueryXU == mDigitalIOXU);	// change to digitalIOXU
	// set up the various parameters
	debugIOLog("+EMUUSBAudioDevice[%p]::QueryXU",this);

	if (digitalChange) {
		if (getProperty("bHasSPDIFClock")) {
			debugIOLog("DigitalIOStatus changed");
			selector = kDigSampRateSel;
			dataLen = kDigIOSampleRateLen;
			debugIOLog("Digital IO SampleRate");
			IOReturn	result = getExtensionUnitSetting(mQueryXU, selector, &setting, dataLen);
			if (kIOReturnSuccess == result) {
				setting = USBToHostLong(setting);
				OSNumber*	change = OSNumber::withNumber(setting, 32);
				if (change && mDigitalIOStatus) {				
					mDigitalIOStatus->hardwareValueChanged(change);// signal the DigitalIOstatus control that something changed
					change->release();
				}
			}
	#if 0
			selector = kDigitalFormat;
			dataLen = kStdDataLen;
			setting = 0;
			debugIOLog("Digital IO Format");
			result = getExtensionUnitSetting(mQueryXU, selector, &setting, dataLen);
			if (kIOReturnSuccess == result) {
				//setting = USBToHostLong(setting);
				OSNumber*	change = OSNumber::withNumber(setting, 32);
				if (change && mDigitalIOSPDIF) {				
					mDigitalIOSPDIF->hardwareValueChanged(change);// signal the DigitalIOSPDIF control that something changed
					change->release();
				}
			}		
			debugIOLog("DigitalIOStatus changed");
	#endif
		}
	} else if (clockSourceChange) {
		selector = kClockRateSelector;
		setting = 3;
		IOReturn	result = getExtensionUnitSetting(mQueryXU, selector, &setting, dataLen);
		if (kIOReturnSuccess == result) {
			setting = USBToHostLong(setting);
			OSNumber*	change = OSNumber::withNumber(setting, 32);
			debugIOLog("Clock Source Changed");
			if (change && mClockSelector) {
				mClockSelector->hardwareValueChanged(change);
				change->release();
			}
		}
	}
	if (digitalChange || clockSourceChange) {
		if (getProperty("bHasSPDIFClock")) {
			// get any changes to the digital sync lock state
			debugIOLog("DigitalIO Sync Lock");
			dataLen = kStdDataLen;
			setting = 0;
			IOReturn	result = getExtensionUnitSetting(mDigitalIOXU, kDigitalSyncLock, &setting, dataLen);
			if (kIOReturnSuccess == result) {
				setting = USBToHostLong(setting);
				OSNumber*	change = OSNumber::withNumber(setting, 32);
				if (change && mDigitalIOSyncSrc) {
					mDigitalIOSyncSrc->hardwareValueChanged(change);
					change->release();
				}
			}
		}
	}
	debugIOLog("-EMUUSBAudioDevice[%p]::QueryXU",this);
}


EMUUSBAudioEngine*	EMUUSBAudioDevice::getOtherEngine(EMUUSBAudioEngine* curEngine) {
	EMUUSBAudioEngine*	otherEngine = NULL;
	if (mRegisteredEngines) {
		UInt32	index = 0;
		while (index < mNumEngines) {
			OSDictionary*	engineInfo = OSDynamicCast(OSDictionary, mRegisteredEngines->getObject(index));
			if (engineInfo) {
				otherEngine = OSDynamicCast(EMUUSBAudioEngine, engineInfo->getObject(kEngine));
				if (otherEngine && (curEngine != otherEngine)) {
					debugIOLog("curEngine %x other engine %x", curEngine, otherEngine);
					break;
				}
			}
			++index;	// look at the next 
		}
	}
	return otherEngine;
}
void EMUUSBAudioDevice::setOtherEngineSampleRate(EMUUSBAudioEngine* curEngine, UInt32 newSampleRate) {
	if (mRegisteredEngines) {
		UInt32	index = 0;
		while (index < mNumEngines) {
			OSDictionary*	engineInfo = OSDynamicCast(OSDictionary, mRegisteredEngines->getObject(index));
			if (engineInfo) {
				EMUUSBAudioEngine *engine = OSDynamicCast(EMUUSBAudioEngine, engineInfo->getObject(kEngine));
				if (engine && (engine != curEngine)) {
					IOAudioSampleRate	newEngineSampleRate;
					newEngineSampleRate.whole = newSampleRate;
					newEngineSampleRate.fraction = 0;// always set this to zero
					debugIOLog("setOtherEngineSampleRate");
					engine->hardwareSampleRateChanged(&newEngineSampleRate);// signal the change
					break;
				}
			}
			++index;
		}
	}
}

void EMUUSBAudioDevice::statusHandler(void* target, void* parameter, IOReturn result, UInt32 bytesLeft) {
	if (target) {
		EMUUSBAudioDevice*	device = (EMUUSBAudioDevice*) target;
		if (device) {
#if (__BIG_ENDIAN__)
			UInt8 unitID = (*device->mDeviceStatusBuffer) & 0xff;
#else
			UInt8 unitID = ((*device->mDeviceStatusBuffer) & 0xff00) >> 8;
#endif
			debugIOLog("unitID %d", unitID);
			if (unitID && (kIOReturnSuccess == result))  {
				device->mQueryXU = unitID;
				OSNumber* change = NULL;
				if (device->mDigitalIOXU == unitID) {
					change = OSNumber::withNumber(kDigIOSyncSrcController, 32);
					debugIOLog("mDigitalIOXU");
				} else if (device->mClockSrcXU == unitID) {
					change = OSNumber::withNumber(kClockSourceController, 32);
					debugIOLog("mClockSrcXU");
				}
#if 0
				debugIOLog("Meow!");
				if (change) {
					debugIOLog("about to call hardware value changed");
					IOReturn myAns = device->mXUChanged->hardwareValueChanged(change);
					debugIOLog("hardwareValueChanged %x", myAns);
					change->release();					
				}
#endif
			}
			IOTimerEventSource *timer =	device->mStatusCheckTimer;
			if (timer)
				timer->setTimeoutMS(kStatusCheckInterval);
		}
	}
}

IOReturn EMUUSBAudioDevice::getAnchorFrameAndTimeStamp(UInt64 *frame, AbsoluteTime *time) 
{
	EmuTime		finishTime;
	EmuTime		offset;
	EmuTime		curTime;
	
	UInt64				thisFrame;
	IOReturn			result = kIOReturnError;
//debugIOLog("getAnchorFrameAndTimeStamp");
	FailIf (NULL == mControlInterface, Exit);
	nanoseconds_to_absolutetime(1100000, &offset);
	clock_get_uptime(&finishTime);
	ADD_ABSOLUTETIME(&finishTime, &offset);	// finishTime is when we timeout
	thisFrame = mControlInterface->GetDevice()->GetBus()->GetFrameNumber(); 
	// spin until the frame changes
	do {
		clock_get_uptime (&curTime);
	} while ((thisFrame == mControlInterface->GetDevice()->GetBus()->GetFrameNumber ()) 
			  && (CMP_ABSOLUTETIME (&finishTime, &curTime) > 0));

	FailIf (CMP_ABSOLUTETIME (&finishTime, &curTime) < 0, Exit);		// if we timed out

	*frame = ++thisFrame;
	
	clock_get_uptime(EmuAbsoluteTimePtr(time));

	result = kIOReturnSuccess;
Exit:
	return result;
}

IOReturn EMUUSBAudioDevice::getFrameAndTimeStamp(UInt64 *frame, AbsoluteTime *time) {
	IOReturn	result = kIOReturnError;
	if (mControlInterface) {
		do {
			*frame = mControlInterface->GetDevice()->GetBus()->GetFrameNumber();
			clock_get_uptime (EmuAbsoluteTimePtr(time));
		} while (*frame != mControlInterface->GetDevice()->GetBus()->GetFrameNumber());// get the frame number from the bus
		result = kIOReturnSuccess;
	}
	return result;
}

UInt64 EMUUSBAudioDevice::jitterFilter(UInt64 prev, UInt64 curr) {
	UInt64 filteredValue = curr + kInvariantCoeffM1 * prev;
			
	// Execute a low pass filter on the new rate
	filteredValue += kInvariantCoeffDiv2;
	filteredValue /= kInvariantCoeff;
//	debugIOLog ("filtered value () = %llu", filteredValue);
	return filteredValue;
}

bool EMUUSBAudioDevice::updateWallTimePerUSBCycle() {
	UInt64			currentUSBFrame = 0;
	UInt64			newWallTimePerUSBCycle = 0;
	AbsoluteTime	time;
	UInt64			time_nanos = 0;
	bool			result = false;
//	debugIOLog("In EMUUSBAudioDevice::updateWallTimePerUSBCycle");
	
	// Get wall time for the current USB frame
	FailIf (kIOReturnSuccess != getFrameAndTimeStamp (&currentUSBFrame, &time), Exit);
	
	if (0ull == mReferenceUSBFrame) {
		mReferenceUSBFrame = currentUSBFrame;
		mReferenceWallTime = time;
//		debugIOLog ("++NOTICE: reference frame = %llu", mReferenceUSBFrame);
//		debugIOLog ("++NOTICE: reference wall time = %llu", * ((UInt64 *) &mReferenceWallTime));
	} else {
		// Convert current time to nanoseconds
		absolutetime_to_nanoseconds (EmuAbsoluteTime(time), &time_nanos);
	}
	
	if (0ull == mLastUSBFrame) {
		// Initialize last reference frame and time
		debugIOLog ("initializing last USB frame and last wall time");
		mLastUSBFrame = mReferenceUSBFrame;
		absolutetime_to_nanoseconds (EmuAbsoluteTime(mReferenceWallTime), &mLastWallTimeNanos);
	} else {
		// Compute new slope
		newWallTimePerUSBCycle = (time_nanos - mLastWallTimeNanos) * kWallTimeExtraPrecision / (currentUSBFrame - mLastUSBFrame);
//		debugIOLog ("mWallTimePerUSBCycle = %llu, newWallTimePerUSBCycle = %llu", mWallTimePerUSBCycle, newWallTimePerUSBCycle);
		
		if (0ull != mWallTimePerUSBCycle) 
			mWallTimePerUSBCycle = jitterFilter(mWallTimePerUSBCycle, newWallTimePerUSBCycle);
		 else 
			// This is our first estimate. Just assign it
			mWallTimePerUSBCycle = newWallTimePerUSBCycle;
		
		// Update last values
		mLastUSBFrame = currentUSBFrame;
		mLastWallTimeNanos = time_nanos;
	}
	result = true;
//	debugIOLog ("EMUUSBAudioDevice::updateWallTimePerUSBCycle () = %llu", mWallTimePerUSBCycle);
Exit:
	return result;
}

void EMUUSBAudioDevice::TimerAction(OSObject * owner, IOTimerEventSource * sender) {
	if (owner) {
		EMUUSBAudioDevice *	device = (EMUUSBAudioDevice *) owner;
		if (device)
			device->doTimerAction(sender);
	}
}

void EMUUSBAudioDevice::doTimerAction(IOTimerEventSource * timer) {
//	debugIOLog("doTimerAction timer is %x\n", timer);
	if (timer) {
		if (updateWallTimePerUSBCycle()) {
			++mAnchorResetCount;
			if (mAnchorResetCount >= kRefreshCount) {
				// re-anchor our reference frame and time
				FailIf (NULL == mControlInterface, Exit);
				getAnchorFrameAndTimeStamp(&mNewReferenceUSBFrame, &mNewReferenceWallTime);
				// reset the counter
				mAnchorResetCount = 0;
			}
			// Schedule the next anchor frame and time update
			timer->setTimeoutMS(kRefreshInterval);
		}		
	}
Exit:
	return;
}

IOReturn EMUUSBAudioDevice::protectedXUChangeHandler(IOAudioControl *audioControl, SInt32 oldValue, SInt32 newValue) {
	IOReturn		result = kIOReturnError;
debugIOLog("++protectedXUChangeHandler");
	UInt32		controlID = audioControl->getSubType();
	UInt8		xuUnitID = (UInt8) (controlID & 0xFF);// must never be zero
	UInt16		xuSelector =(UInt16) (controlID >> 16);// the value extracted from the control ID must never be zero
	if (xuUnitID && xuSelector) {
		// compute the length of data to be sent based on the XU - either 1 or 4 
		UInt32	len = kStdDataLen + ((kDigIOSampleRateLen - 1) * (mDigitalIOXU == xuUnitID));
		//((kDirectMonitorLen - 1) * (mDirectMonXU == xuUnitID));// currently no support for direct monitoring
		
		if (kStdDataLen == len) {
			UInt8	theValue = (UInt8) newValue;
			result = setExtensionUnitSetting(xuUnitID, xuSelector, (void*) &theValue, len);
		} else if (kDigIOSampleRateLen == len) {
			UInt32	theVal = HostToUSBLong(newValue);
			result = setExtensionUnitSetting(xuUnitID, xuSelector, (void*) &theVal, len);
		}
		if (xuUnitID == mClockSrcXU){// clock source changed
			UInt32	setting = 0;
			len = kDigIOSampleRateLen;
			// get the digital sample rate
			result = getExtensionUnitSetting(mDigitalIOXU, kDigSampRateSel, &setting, len);
			if (kIOReturnSuccess == result) {
				setting = USBToHostLong(setting);
				OSNumber*	change = OSNumber::withNumber(setting, 32);
				if (change) {
					mDigitalIOStatus->hardwareValueChanged(change);
					//mDigitalIOSyncSrc->flushValue();
					change->release();
				}
			}
		}
		 
		// is the sample rate locked?
		UInt32	setting = 0;
		len = kStdDataLen;
		result = getExtensionUnitSetting(mDigitalIOXU, kDigitalSyncLock, &setting, len);
		if (kIOReturnSuccess == result) {
			setting = USBToHostLong(setting);
			OSNumber* change = OSNumber::withNumber(newValue, 32);
			if (change) 
			{
				mDigitalIOSyncSrc->hardwareValueChanged(change);
				
				if (mRealClockSelector)
				{
					mRealClockSelector->hardwareValueChanged(change);
				}
				
				change->release();
			}
		}
	}
	debugIOLog("protectedXUChangeHandler result %x", result);
	return result;
}

// Create and add the custom controls each time the default controls are removed.
// This works better than the previous scheme where the custom controls were created just once
// and added each time when the default controls were removed.
void EMUUSBAudioDevice::addCustomAudioControls(IOAudioEngine* engine) {
	if (0 < mAvailXUs) {
		mClockSrcXU = mUSBAudioConfig->FindExtensionUnitID(mInterfaceNum, kClockSource);
		if (mClockSrcXU) {
			UInt8	setting = 0;
			if (kIOReturnSuccess == getExtensionUnitSetting(mClockSrcXU, kClockSourceSelector, &setting, kStdDataLen)) 
				setting = USBToHostLong(setting);
			RELEASEOBJ(mClockSelector);
			debugIOLog("ClockSelector created");
			mClockSelector = EMUXUCustomControl::create(setting, kIOAudioControlChannelIDAll, kIOAudioControlChannelNameAll,
											kClockSourceController, (kClockSourceSelector << 16 | mClockSrcXU), kCtrlUsage);
			if (mClockSelector) {
				mClockSelector->setValueChangeHandler((EMUXUCustomControl::IntValueChangeHandler) deviceXUChangeHandler, this);
				engine->addDefaultAudioControl(mClockSelector);
			}
			
		// mRealClockSelector is set to default to internal, so we need to do that here too.
			UInt8 theValue = (UInt8) 0;
			IOReturn result = setExtensionUnitSetting(mClockSrcXU, kClockSourceSelector, (void*) &theValue, kStdDataLen);
		}
		
		mDigitalIOXU = mUSBAudioConfig->FindExtensionUnitID(mInterfaceNum, kDigitalIOStatus);
		if (mDigitalIOXU) {
			UInt32	setting = 0;
			if (kIOReturnSuccess == getExtensionUnitSetting(mDigitalIOXU, kDigSampRateSel, &setting, kDigIOSampleRateLen)) // should succeed
				setting = USBToHostLong(setting);
			RELEASEOBJ(mDigitalIOStatus);
			mDigitalIOStatus = EMUXUCustomControl::create(setting, kIOAudioControlChannelIDAll, kIOAudioControlChannelNameAll,
											kDigIOSampleRateController, (kDigSampRateSel << 16 | mDigitalIOXU), kCtrlUsage);
			if (mDigitalIOStatus) {
				debugIOLog("DigitalIOStatus created");
				mDigitalIOStatus->setValueChangeHandler((EMUXUCustomControl::IntValueChangeHandler)deviceXUChangeHandler, this);
				engine->addDefaultAudioControl(mDigitalIOStatus);
			}
			setting = 0;
			if (kIOReturnSuccess == getExtensionUnitSetting(mDigitalIOXU, kDigitalSyncLock, &setting, kStdDataLen)) 
				setting = USBToHostLong(setting);
			RELEASEOBJ(mDigitalIOSyncSrc);
			mDigitalIOSyncSrc = EMUXUCustomControl::create(setting, kIOAudioControlChannelIDAll, kIOAudioControlChannelNameAll,
										kDigIOSyncSrcController, (kDigitalSyncLock << 16 | mDigitalIOXU), kCtrlUsage);
			if (mDigitalIOSyncSrc) {
				debugIOLog("made kDigIOSyncSrcController");
				mDigitalIOSyncSrc->setValueChangeHandler((EMUXUCustomControl::IntValueChangeHandler)deviceXUChangeHandler, this);
				engine->addDefaultAudioControl(mDigitalIOSyncSrc);
			}
			setting = 0;
			if (kIOReturnSuccess == getExtensionUnitSetting(mDigitalIOXU, kDigitalSRC, &setting, kStdDataLen)) 
				setting = USBToHostLong(setting);
			RELEASEOBJ(mDigitalIOAsyncSrc);
			mDigitalIOAsyncSrc = EMUXUCustomControl::create(setting, kIOAudioControlChannelIDAll, kIOAudioControlChannelNameAll,
											kDigIOAsyncSrcController, (kDigitalSRC << 16 | mDigitalIOXU), kCtrlUsage);
			if (mDigitalIOAsyncSrc) {
				debugIOLog("made kDigIOAsyncSrcController");
				mDigitalIOAsyncSrc->setValueChangeHandler((EMUXUCustomControl::IntValueChangeHandler)deviceXUChangeHandler, this);
				engine->addDefaultAudioControl(mDigitalIOAsyncSrc);
			}

			// SPDIF format control 
			setting = 0;
			if (kIOReturnSuccess == getExtensionUnitSetting(mDigitalIOXU, kDigitalFormat, &setting, kStdDataLen)) 
				setting = USBToHostLong(setting);
			RELEASEOBJ(mDigitalIOSPDIF);
			mDigitalIOSPDIF = EMUXUCustomControl::create(kSPDIFNone, kIOAudioControlChannelIDAll, kIOAudioControlChannelNameAll,
										kDigIOSPDIFController, (kDigitalFormat << 16 | mDigitalIOXU), kCtrlUsage);
			if (mDigitalIOSPDIF) {
				debugIOLog("made kDigSPDIFController");
				mDigitalIOSPDIF->setValueChangeHandler((EMUXUCustomControl::IntValueChangeHandler) deviceXUChangeHandler, this);
				engine->addDefaultAudioControl(mDigitalIOSPDIF);
			}
		}
		mDeviceOptionsXU = mUSBAudioConfig->FindExtensionUnitID(mInterfaceNum, kDeviceOptions);
		if (mDeviceOptionsXU) {
			UInt32 devOptSetting = 0;
			debugIOLog("attempting to get the softlimit control");
			if (kIOReturnSuccess == getExtensionUnitSetting(mDeviceOptionsXU, kSoftLimitSelector, &devOptSetting, kStdDataLen)) 
				devOptSetting = USBToHostLong(devOptSetting);
			RELEASEOBJ(mDevOptionCtrl);
			mDevOptionCtrl = EMUXUCustomControl::create(devOptSetting, kIOAudioControlChannelIDAll, kIOAudioControlChannelNameAll,
												kDevSoftLimitController, (kSoftLimitSelector << 16 | mDeviceOptionsXU), kCtrlUsage);
			if (mDevOptionCtrl) {
				debugIOLog("made the softlimit control");
				mDevOptionCtrl->setValueChangeHandler((EMUXUCustomControl::IntValueChangeHandler)deviceXUChangeHandler, this);
				engine->addDefaultAudioControl(mDevOptionCtrl);
			}
		}
	}
}


void EMUUSBAudioDevice::RegisterHALCallback(void * toRegister) {
	debugIOLog("+RegisterHALCallback: %p",toRegister);
	propertyChangedProc = toRegister;
}