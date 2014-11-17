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
#include "EMUUSBAudioLevelControl.h"

#define super IOAudioLevelControl

OSDefineMetaClassAndStructors(EMUUSBAudioLevelControl, IOAudioLevelControl)

EMUUSBAudioLevelControl *EMUUSBAudioLevelControl::create (UInt8 theUnitID, UInt8 theInterfaceNumber, UInt8 theControlSelector, UInt8 theChannelNumber, Boolean shouldUpdatePRAM, USBDeviceRequest theUSBDeviceRequest, void *theCallerRefCon, UInt32 subType, UInt32 usage) {
    EMUUSBAudioLevelControl *			control;

    debugIOLog ("+EMUUSBAudioLevelControl::create (%d, %d, %d, %d, %p, %lX, %lX)", theUnitID, theInterfaceNumber, theControlSelector, theChannelNumber, theUSBDeviceRequest, subType, usage);
    control = new EMUUSBAudioLevelControl;
    FailIf (NULL == control, Exit);

    if (FALSE == control->init (theUnitID, theInterfaceNumber, theControlSelector, theChannelNumber, shouldUpdatePRAM, theUSBDeviceRequest, theCallerRefCon, subType, usage)) {
        control->release ();
        control = NULL;
    }

Exit:
    debugIOLog ("-EMUUSBAudioLevelControl::create(%d, %d, %d, %d, %p, %lX, %lX)", theUnitID, theInterfaceNumber, theControlSelector, theChannelNumber, theUSBDeviceRequest, subType, usage);
    return control;
}

bool EMUUSBAudioLevelControl::init (UInt8 theUnitID, UInt8 theInterfaceNumber, UInt8 theControlSelector, UInt8 theChannelNumber, Boolean shouldUpdatePRAM, USBDeviceRequest theUSBDeviceRequest, void *theCallerRefCon, UInt32 subType, UInt32 usage, OSDictionary *properties) {
	const char *				channelName = NULL;
	SInt16						currentValue;
	SInt16						deviceMin;
	SInt16						deviceMax;
	IOFixed						deviceMinDB;
	IOFixed						deviceMaxDB;
	IOFixed						resolutionDB;
	IOReturn					ret;
	Boolean						result;
	Boolean						arePRAMControl;

	debugIOLog ("+EMUUSBAudioLevelControl[%p]::init (%d, %d, %d, %d, %p, %p)", this, theUnitID, theInterfaceNumber, theControlSelector, theChannelNumber, theUSBDeviceRequest, properties);
	result = FALSE;
	arePRAMControl = FALSE;

	FailIf (NULL == theUSBDeviceRequest, Exit);
	setValueThreadCall = thread_call_allocate ((thread_call_func_t)updateValueCallback, this);
	FailIf (NULL == setValueThreadCall, Exit);

	// If this control is supposed to be a pram volume control, pretend we're just a regular volume control
	// so that we can get the min and max dB from the device as we init
	if (theControlSelector == 0xff) {
		theControlSelector = VOLUME_CONTROL;
		arePRAMControl = TRUE;
	}

	if (kIOAudioLevelControlSubTypeLFEVolume == subType) {
		// The iSub controls are on channels 1 and 2, but we want to make them look like they're on channel 0 for the HAL
		// set it to 1 here so we can query the iSub
		theChannelNumber = 1;
	}

	unitID = theUnitID;
	interfaceNumber = theInterfaceNumber;
	controlSelector = theControlSelector;
	channelNumber = theChannelNumber;
	callerRefCon = theCallerRefCon;
	usbDeviceRequest = theUSBDeviceRequest;
	fShouldUpdatePRAM = shouldUpdatePRAM;

	switch (channelNumber) {
		case kIOAudioControlChannelIDAll:
			channelName = kIOAudioControlChannelNameAll;
			break;
		case kIOAudioControlChannelIDDefaultLeft:
			channelName = kIOAudioControlChannelNameLeft;
			break;
		case kIOAudioControlChannelIDDefaultRight:
			channelName = kIOAudioControlChannelNameRight;
			break;
		case 0xff:
			debugIOLog ("++EMUUSBAudioLevelControl: Does not support channel number 0xff.");
			return FALSE;
		default:
			channelName = "Unknown";
			break;
	}

	ret = GetCurVolume (interfaceNumber, channelNumber, &currentValue);
	FailIf (kIOReturnSuccess != ret, Exit);
	debugIOLog ("channelNumber %d, currentValue = 0x%X, ", channelNumber, currentValue);
	ret = GetVolumeResolution (interfaceNumber, channelNumber, &volRes);
	FailIf (kIOReturnSuccess != ret, Exit);
	debugIOLog ("vol res = %d, ", volRes);
	ret = GetMinVolume (interfaceNumber, channelNumber, &deviceMin);
	FailIf (kIOReturnSuccess != ret, Exit);
	debugIOLog ("deviceMin = 0x%X, ", deviceMin);
	ret = GetMaxVolume (interfaceNumber, channelNumber, &deviceMax);
	FailIf (kIOReturnSuccess != ret, Exit);
	debugIOLog ("deviceMax = 0x%X", deviceMax);

	// Having the device say that it does -infinity dB messes up our math, so set the min at -127.9961dB instead.
	if ((SInt16)0x8000 == deviceMin) {
		deviceMin = (SInt16)0x8001;
		debugIOLog ("deviceMin adjusted to = %d", deviceMin);
	}

	deviceMinDB = ConvertUSBVolumeTodB (deviceMin);
	deviceMaxDB = ConvertUSBVolumeTodB (deviceMax);
	resolutionDB = ConvertUSBVolumeTodB (volRes);		// The volume is incremented in units of this many dB, represented as 1/256 dB (eg 256 == 1dB of control)

	offset = -deviceMin;
	debugIOLog ("offset = %d", offset);

	currentValue = (currentValue + offset) / volRes;
	if (deviceMin < 0 && deviceMax > 0) {
		deviceMax += volRes;
		debugIOLog ("deviceMax adjusted to = 0x%X", deviceMax);
	}
	deviceMax = ((deviceMin + offset) + (deviceMax + offset)) / volRes;

	if (kIOAudioLevelControlSubTypeLFEVolume == subType) {
		currentValue = currentValue / 2;
		//updateUSBValue (currentValue);
	}

	// Set values needed to compute proper PRAM boot beep volume setting
	fMaxVolume = deviceMax;
	fMinVolume = deviceMin + offset;
	deviceMin = -1;

	if (arePRAMControl) {
		// If this is a 'pram' control then there is no need to call the hardware.
		UInt8 						curPRAMVol;
		IODTPlatformExpert * 		platform = NULL;

		curPRAMVol = 0;
		platform = OSDynamicCast (IODTPlatformExpert, getPlatform ());
		if (NULL != platform) {
			platform->readXPRAM ((IOByteCount)kPRamVolumeAddr, &curPRAMVol, (IOByteCount)1);
			curPRAMVol = (curPRAMVol & 0xF8);
		}
		currentValue = curPRAMVol;
		deviceMin = 0;
		deviceMax = 7;
		subType = kIOAudioLevelControlSubTypePRAMVolume;
		channelName = kIOAudioControlChannelNameAll;
		theChannelNumber = 0;	// force it to the master channel even though we're piggy backing off of the channel 1 control
	}

	if (kIOAudioLevelControlSubTypeLFEVolume == subType) {
		// The iSub controls are on channels 1 and 2, but we want to make them look like they're on channel 0 for the HAL
		theChannelNumber = 0;
		channelName = kIOAudioControlChannelNameAll;
	}

	debugIOLog ("min = %d, max = %d, current = %d", deviceMin, deviceMax, currentValue);

	FailIf (FALSE == super::init (currentValue, deviceMin, deviceMax, deviceMinDB, deviceMaxDB, theChannelNumber, channelName, 0, subType, usage), Exit);

	if (kIOAudioLevelControlSubTypeLFEVolume == subType) {
		updateUSBValue (currentValue);
	}

	result = TRUE;

Exit:
	debugIOLog ("-EMUUSBAudioLevelControl[%p]::init (%d, %d, %d, %d, %p, %p)", this, theUnitID, theInterfaceNumber, theControlSelector, theChannelNumber, theUSBDeviceRequest, properties);
	return result;
}

void EMUUSBAudioLevelControl::free () {
    debugIOLog ("+EMUUSBAudioLevelControl[%p]::free ()", this);

    if (setValueThreadCall) {
        thread_call_free (setValueThreadCall);
        setValueThreadCall = NULL;
    }

    super::free ();
    debugIOLog ("-EMUUSBAudioLevelControl[%p]::free ()", this);
}

IOReturn EMUUSBAudioLevelControl::performValueChange (OSObject * newValue) {
	OSNumber *		newValueAsNumber = OSDynamicCast (OSNumber, newValue);

    debugIOLog ("+EMUUSBAudioLevelControl[%p]::performValueChange (%d)", this, newValue); 

	FailIf (NULL == newValueAsNumber, Exit);
	SInt32		newValueAsSInt32 = newValueAsNumber->unsigned32BitValue ();
	debugIOLog ("++EMUUSBAudioLevelControl[%p]::performValueChange (%ld)", this, newValueAsSInt32);

    if (NULL != setValueThreadCall) {
        thread_call_enter1 (setValueThreadCall, (thread_call_param_t)newValueAsSInt32);
    }

    debugIOLog ("-EMUUSBAudioLevelControl[%p]::performValueChange (%d)", this, newValueAsSInt32);

Exit:
    return kIOReturnSuccess;
}

void EMUUSBAudioLevelControl::updateUSBValue () {
    updateUSBValue (getIntValue ());
}

void EMUUSBAudioLevelControl::updateUSBValue (SInt32 newValue) {
//	SInt32						newiSubVolume;
    SInt16						theValue;
	SInt16						newVolume = 0x8000;
    IOReturn					ret;

    debugIOLog ("+EMUUSBAudioLevelControl[%p]::updateUSBValue (%d)", this, newValue);

	newVolume = ((newValue - (1 * (newValue > 0))) * volRes) - offset;

	debugIOLog ("volume value is 0x%X", newVolume);
    theValue = HostToUSBWord (newVolume);
	debugIOLog ("setting volume to 0x%X (little endian)", newVolume);
	ret = SetCurVolume (interfaceNumber, channelNumber, theValue);

	if (getSubType () == kIOAudioLevelControlSubTypeLFEVolume) {
		// We set the iSub's left volume (on channel 1 above), now set it on channel 2 to mimic having only a master volume control
		ret = SetCurVolume (interfaceNumber, 2, theValue);
	}


    debugIOLog ("-EMUUSBAudioLevelControl[%p]::updateUSBValue (%d)", this, newValue);
}

IOReturn EMUUSBAudioLevelControl::GetCurVolume (UInt8 interfaceNumber, UInt8 channelNumber, SInt16 * target) {
    IOUSBDevRequest				devReq;
    SInt16						theVolume = 0;
	IOReturn					result = kIOReturnError;
	FailIf ( NULL == target, Exit );
	
    devReq.bmRequestType = USBmakebmRequestType (kUSBIn, kUSBClass, kUSBInterface);
    devReq.bRequest = GET_CUR;
    devReq.wValue = (controlSelector << 8) | channelNumber;
    devReq.wIndex = (unitID << 8) | interfaceNumber;
    devReq.wLength = 2;
    devReq.pData = &theVolume;

	result = usbDeviceRequest (&devReq, callerRefCon, 0);
    FailIf (kIOReturnSuccess != result, Exit);

Exit:
	if (NULL != target) {
		* target = USBToHostWord (theVolume);
		}
    return result;
}

IOReturn EMUUSBAudioLevelControl::GetMaxVolume (UInt8 interfaceNumber, UInt8 channelNumber, SInt16 * target) {
    IOReturn					result;
	IOUSBDevRequest				devReq;
    SInt16						theVolume;

	result = kIOReturnError;
	theVolume = 0;
	FailIf (NULL == target, Exit);
	
    devReq.bmRequestType = USBmakebmRequestType (kUSBIn, kUSBClass, kUSBInterface);
    devReq.bRequest = GET_MAX;
    devReq.wValue = (controlSelector << 8) | channelNumber;
    devReq.wIndex = (unitID << 8) | interfaceNumber;
    devReq.wLength = 2;
    devReq.pData = &theVolume;

	result = usbDeviceRequest (&devReq, callerRefCon, 0);
    FailIf (kIOReturnSuccess != result, Exit);

Exit:
	if (NULL != target) {
		* target = USBToHostWord (theVolume);
	}
    return result;
}

IOReturn EMUUSBAudioLevelControl::GetMinVolume (UInt8 interfaceNumber, UInt8 channelNumber, SInt16 * target) {
    IOReturn					result = kIOReturnError;
	IOUSBDevRequest				devReq;
    SInt16						theVolume = 0;

	FailIf (NULL == target, Exit);
	
	devReq.bmRequestType = USBmakebmRequestType (kUSBIn, kUSBClass, kUSBInterface);
    devReq.bRequest = GET_MIN;
    devReq.wValue = (controlSelector << 8) | channelNumber;
    devReq.wIndex = (unitID << 8) | interfaceNumber;
    devReq.wLength = 2;
    devReq.pData = &theVolume;

	result = usbDeviceRequest (&devReq, callerRefCon, 0);
    FailIf (kIOReturnSuccess != result, Exit);

Exit:
	if (NULL != target) {
		* target = USBToHostWord (theVolume);
	}
    return result;
}

IOReturn EMUUSBAudioLevelControl::GetVolumeResolution (UInt8 interfaceNumber, UInt8 channelNumber, UInt16 * target) {
    IOReturn					result;
	IOUSBDevRequest				devReq;
    UInt16						theResolution;

	result = kIOReturnError;
	theResolution = 0;
	FailIf (NULL == target, Exit);
	
    devReq.bmRequestType = USBmakebmRequestType (kUSBIn, kUSBClass, kUSBInterface);
    devReq.bRequest = GET_RES;
    devReq.wValue = (controlSelector << 8) | channelNumber;
    devReq.wIndex = (unitID << 8) | interfaceNumber;
    devReq.wLength = 2;
    devReq.pData = &theResolution;

	result = usbDeviceRequest (&devReq, callerRefCon, 0);
    FailIf (kIOReturnSuccess != result, Exit);

Exit:
	if (NULL != target) {
		* target = USBToHostWord (theResolution);
	}
    return result;
}

IOReturn EMUUSBAudioLevelControl::SetCurVolume (UInt8 interfaceNumber, UInt8 channelNumber, SInt16 volume) {
    IOUSBDevRequest				devReq;
	IOReturn					error;

    devReq.bmRequestType = USBmakebmRequestType (kUSBOut, kUSBClass, kUSBInterface);
    devReq.bRequest = SET_CUR;
    devReq.wValue = (controlSelector << 8) | channelNumber;
    devReq.wIndex = (unitID << 8) | interfaceNumber;
    devReq.wLength = 2;
    devReq.pData = &volume;

	FailIf ((TRUE == isInactive()), DeviceInactive);  	// In case we've been unplugged during sleep
	error = usbDeviceRequest (&devReq, callerRefCon, 0);
    FailIf (kIOReturnSuccess != error, Exit);

Exit:
    return error;
	
DeviceInactive:
	debugIOLog("EMUUSBAudioLevelControl::SetCurVolume ERROR attempt to send a device request to and inactive device");
	error = kIOReturnError;
	goto Exit;
}

void EMUUSBAudioLevelControl::updateValueCallback (void *arg1, void *arg2) {
    EMUUSBAudioLevelControl 		*levelControl = OSDynamicCast (EMUUSBAudioLevelControl, (OSObject*)arg1);
    SInt32							value = (SInt32)arg2;

    debugIOLog ("+EMUUSBAudioLevelControl::updateValueCallback (%p, %p)", (UInt32*)arg1, (UInt32*)arg2);

    if (levelControl) {
		UInt32	subType = levelControl->getSubType ();
		
		if (kIOAudioLevelControlSubTypePRAMVolume == subType) {
			UInt8 						curPRAMVol;
			IODTPlatformExpert * 		platform = NULL;

			platform = OSDynamicCast (IODTPlatformExpert, getPlatform ());
			if (NULL != platform) {
				platform->readXPRAM ((IOByteCount)kPRamVolumeAddr, &curPRAMVol, (IOByteCount)1);
				curPRAMVol = (curPRAMVol & 0xF8) | value;
				platform->writeXPRAM ((IOByteCount)kPRamVolumeAddr, &curPRAMVol, (IOByteCount)1);
			}
		} else {
			levelControl->updateUSBValue (value);
		}
    }

    debugIOLog ("-EMUUSBAudioLevelControl::updateValueCallback (%p, %p)", (UInt32*)arg1, (UInt32*)arg2);
}

// This is how the thing is defined in the USB Audio spec (section 5.2.2.4.3.2 for the curious).
// The volume setting of a device is described in 1/256 dB increments using a number that goes from
// a max of 0x7fff (127.9961 dB) down to 0x8001 (-127.9961 dB) using standard signed math, but 0x8000
// is actually negative infinity (not -128 dB), so I have to special case it.
IOFixed EMUUSBAudioLevelControl::ConvertUSBVolumeTodB (SInt16 volume) {
	IOFixed							dBVolumeFixed;

	if (volume == (SInt16)0x8000) {
		dBVolumeFixed = ((SInt16)0x8000 * 256) << 8;	// really is negative infinity
	} else {
		dBVolumeFixed = volume * 256;
	}

	debugIOLog ("volume = %d, dBVolumeFixed = 0x%X", volume, dBVolumeFixed);

	return dBVolumeFixed;
}

/////////////////////////////////////////////////////////////////////////////
///
/// EMUUSBAudioSoftLevelControl class
///

OSDefineMetaClassAndStructors(EMUUSBAudioSoftLevelControl, IOAudioLevelControl)

EMUUSBAudioSoftLevelControl* EMUUSBAudioSoftLevelControl::create(SInt32 initialValue,
                                       SInt32 minValue,
                                       SInt32 maxValue,
                                       IOFixed minDB,
                                       IOFixed maxDB,
                                       UInt32 channelID,
                                       const char *channelName,
                                       UInt32 cntrlID,
                                       UInt32 subType,
                                       UInt32 usage)
{
	EMUUSBAudioSoftLevelControl* control = NULL;

    control = new EMUUSBAudioSoftLevelControl;
	
	if (control)
	{
		control->init(initialValue,
                       minValue,
                       maxValue,
                       minDB,
                       maxDB,
                       channelID,
					   channelName,
                       cntrlID,
                       subType,
                       usage);
					   
		control->SetVolume(1.0);
		control->SetTargetVolume(1.0);
		control->SetLastVolume(1.0);
	}
	
	return control;
}

/////////////////////////////////////////////////////////////////////////////
///
/// EMUUSBAudioHardLevelControl class
///

OSDefineMetaClassAndStructors(EMUUSBAudioHardLevelControl, IOAudioLevelControl)

EMUUSBAudioHardLevelControl* EMUUSBAudioHardLevelControl::create(SInt32 initialValue,
                                       SInt32 minValue,
                                       SInt32 maxValue,
                                       IOFixed minDB,
                                       IOFixed maxDB,
                                       UInt32 channelID,
                                       const char *channelName,
                                       UInt32 cntrlID,
                                       UInt32 subType,
                                       UInt32 usage)
{
	EMUUSBAudioHardLevelControl* control = NULL;

    control = new EMUUSBAudioHardLevelControl;
	
	if (control)
	{
		control->init(initialValue,
                       minValue,
                       maxValue,
                       minDB,
                       maxDB,
                       channelID,
					   channelName,
                       cntrlID,
                       subType,
                       usage);
					   
		control->SetVolume(1.0);
		control->SetTargetVolume(1.0);
		control->SetLastVolume(1.0);
	}
	
	return control;
}

