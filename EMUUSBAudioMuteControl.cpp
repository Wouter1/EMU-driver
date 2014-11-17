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
#include "EMUUSBAudioMuteControl.h"

#define super IOAudioToggleControl
OSDefineMetaClassAndStructors(EMUUSBAudioMuteControl, IOAudioToggleControl)

EMUUSBAudioMuteControl *EMUUSBAudioMuteControl::create (UInt8 theUnitID, UInt8 theInterfaceNumber, UInt8 theChannelNumber, USBDeviceRequest theUSBDeviceRequest, void *theCallerRefCon, UInt32 usage, UInt32 subType, UInt32 controlID) {
    EMUUSBAudioMuteControl 			*control;

    debugIOLog ("+EMUUSBAudioMuteControl::create (%d, %d, %d, 0x%x, %lX, %lX)", theUnitID, theInterfaceNumber, theChannelNumber, theUSBDeviceRequest, usage, controlID);
    control = new EMUUSBAudioMuteControl;

    if (control) {
        if (FALSE == control->init (theUnitID, theInterfaceNumber, theChannelNumber, theUSBDeviceRequest, theCallerRefCon, usage, subType, controlID)) {
            control->release ();
            control = NULL;
        }
    }

    debugIOLog ("-EMUUSBAudioMuteControl[0x%x]::create (%d, %d, %d, 0x%x, %lX, %lX)", theUnitID, theInterfaceNumber, theChannelNumber, theUSBDeviceRequest, usage, controlID);
    return control;
}

bool EMUUSBAudioMuteControl::init (UInt8 theUnitID, UInt8 theInterfaceNumber, UInt8 theChannelNumber, USBDeviceRequest theUSBDeviceRequest, void *theCallerRefCon, UInt32 usage, UInt32 subType, UInt32 controlID, OSDictionary *properties) {
    const char *						channelName = NULL;
    UInt8								currentValue;
    IOReturn							ret;
    Boolean								result;
    UInt8								theValue;
    
    debugIOLog ("+EMUUSBAudioMuteControl[0x%x]::init (%d, %d, %d, 0x%x, 0x%x, %lX)", this, theUnitID, theInterfaceNumber, theChannelNumber, theUSBDeviceRequest, usage, properties);
    result = FALSE;
    FailIf (NULL == theUSBDeviceRequest, Exit);

    setValueThreadCall = thread_call_allocate ((thread_call_func_t)updateValueCallback, this);
    FailIf (NULL == setValueThreadCall, Exit);

    unitID = theUnitID;
    interfaceNumber = theInterfaceNumber;
    channelNumber = theChannelNumber;
	callerRefCon = theCallerRefCon;
    usbDeviceRequest = theUSBDeviceRequest;

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
            debugIOLog ("++EMUUSBAudioMuteControl: Does not support channel number 0xff.");
            return FALSE;
        default:
            channelName = "Unknown";
            break;
    }
	
	if (kIOAudioToggleControlSubTypeLFEMute == subType) {
		theValue = 0; // 0 = un-mute
		ret = SetCurMute (interfaceNumber, channelNumber, theValue);
	}

	currentValue = GetCurMute (interfaceNumber, channelNumber, &ret);
    FailIf (kIOReturnSuccess != ret, Exit);
    FailIf (FALSE == super::init (currentValue, theChannelNumber, channelName, controlID, subType, usage), Exit);

    result = TRUE;
    
Exit:
    debugIOLog ("-EMUUSBAudioMuteControl[0x%x]::init (%d, %d, %d, 0x%x, 0x%x, %lX)", this, theUnitID, theInterfaceNumber, theChannelNumber, theUSBDeviceRequest, usage, properties);
    return result;
}

void EMUUSBAudioMuteControl::free () {
    debugIOLog ("+EMUUSBAudioMuteControl[0x%x]::free ()", this);

    if (setValueThreadCall) {
        thread_call_free (setValueThreadCall);
        setValueThreadCall = NULL;
    }

    debugIOLog ("-EMUUSBAudioMuteControl[0x%x]::free ()", this);
    super::free ();
}

IOReturn EMUUSBAudioMuteControl::performValueChange (OSObject * newValue) {
	OSNumber *					newValueAsNumber;
	SInt32						newValueAsSInt32;

    debugIOLog ("+EMUUSBAudioMuteControl[0x%x]::performValueChange (%d)", this, newValue); 

	newValueAsNumber = OSDynamicCast (OSNumber, newValue);
	FailIf (NULL == newValueAsNumber, Exit);
	newValueAsSInt32 = newValueAsNumber->unsigned32BitValue ();
	debugIOLog ("++EMUUSBAudioMuteControl[0x%x]::performValueChange (%ld)", this, newValueAsSInt32);

    // updateUSBValue ();
    // We should just be able to make an asynchronous deviceRequest, but for some reason that doesn't want to work
    // we get pipe stall errors and the change is never made

    assert (setValueThreadCall);
    thread_call_enter1 (setValueThreadCall, (thread_call_param_t)newValueAsSInt32);
    
    debugIOLog ("-EMUUSBAudioMuteControl::performValueChange (%d)", newValueAsSInt32);

Exit:
    return kIOReturnSuccess;
}

UInt8 EMUUSBAudioMuteControl::GetCurMute (UInt8 interfaceNumber, UInt8 channelNumber, IOReturn * error) {
    IOUSBDevRequest				devReq;
    UInt8						theMuteState;

    devReq.bmRequestType = USBmakebmRequestType (kUSBIn, kUSBClass, kUSBInterface);
    devReq.bRequest = GET_CUR;
    devReq.wValue = (MUTE_CONTROL << 8) | channelNumber;
    devReq.wIndex = (unitID << 8) | interfaceNumber;
    devReq.wLength = 1;
    devReq.pData = &theMuteState;

	*error = usbDeviceRequest (&devReq, callerRefCon, 0);
    FailIf (kIOReturnSuccess != *error, Error);

Exit:
    return theMuteState;
Error:
	theMuteState = 0;
	goto Exit;
}

IOReturn EMUUSBAudioMuteControl::SetCurMute (UInt8 interfaceNumber, UInt8 channelNumber, UInt8 theMuteState) {
    IOUSBDevRequest				devReq;
	IOReturn					error;

    devReq.bmRequestType = USBmakebmRequestType (kUSBOut, kUSBClass, kUSBInterface);
    devReq.bRequest = SET_CUR;
    devReq.wValue = (MUTE_CONTROL << 8) | channelNumber;
    devReq.wIndex = (unitID << 8) | interfaceNumber;
    devReq.wLength = 1;
    devReq.pData = &theMuteState;

	FailIf ((TRUE == isInactive()), DeviceInactive);	// In case we've been unplugged during sleep
	error = usbDeviceRequest (&devReq, callerRefCon, 0);
    FailIf (kIOReturnSuccess != error, Exit);

Exit:
    return error;

DeviceInactive:
	debugIOLog("EMUUSBAudioMuteControl::SetCurMute ERROR attempt to send a device request to and inactive device");
	error = kIOReturnError;
	goto Exit;
}

void EMUUSBAudioMuteControl::updateUSBValue () {
    updateUSBValue (getIntValue ());
}

void EMUUSBAudioMuteControl::updateUSBValue (SInt32 newValue) {
    UInt8						theValue;
    IOReturn					ret;

    debugIOLog ("+EMUUSBAudioMuteControl::updateUSBValue (%d)", newValue);

    theValue = (newValue != 0);
	ret = SetCurMute (interfaceNumber, channelNumber, theValue);

    debugIOLog ("-EMUUSBAudioMuteControl::updateUSBValue (%d)", newValue);
}

void EMUUSBAudioMuteControl::updateValueCallback (void *arg1, void *arg2) {
    EMUUSBAudioMuteControl 	*muteControl;
    UInt32						value;

    debugIOLog ("+EMUUSBAudioMuteControl::updateValueCallback (%d, %d)", (UInt32*)arg1, (UInt32*)arg2);
    muteControl = (EMUUSBAudioMuteControl *)arg1;
    value = (UInt32)arg2;

    if (muteControl && OSDynamicCast (EMUUSBAudioMuteControl, muteControl)) {
        muteControl->updateUSBValue (value);
    }

    debugIOLog ("-EMUUSBAudioMuteControl::updateValueCallback (%d, %d)", (UInt32*)arg1, (UInt32*)arg2);
}

