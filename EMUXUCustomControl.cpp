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
//	File:		EMUXUCustomControl.cpp
//	Contains:	Custom IOAudioControl class code for the notification control exposed through CoreAudio
//	Written by:	David Tay 2006.
//--------------------------------------------------------------------------------
#include "EMUXUCustomControl.h"
#include "EMUUSBAudioDevice.h"
#include "EMUUSBAudioCommon.h"
#define super IOAudioControl
OSDefineMetaClassAndStructors(EMUXUCustomControl, IOAudioControl)

EMUXUCustomControl* EMUXUCustomControl::create(UInt32 initialValue, UInt32 channelID, const char *channelName, UInt32 cntrlID, UInt32 subType, UInt32 usage) {
debugIOLog("EMUXUCustomControl create initialValue %d", initialValue);
	EMUXUCustomControl*	control = new EMUXUCustomControl;
	if (control) {
		OSNumber	*number;
		number  = OSNumber::withNumber(initialValue, 32);// 32 bits
		if (number && !control->init(kEMUXUControl, number, channelID, channelName, cntrlID, subType, usage, NULL)) {
			control->release();
			control = NULL;
		}
		if (number)
			number->release();
	}
	return control;
}

bool EMUXUCustomControl::init(UInt32 type, OSObject *initialVal, UInt32 channelID, const char *channelName, UInt32 controlID, UInt32 subType, UInt32 usage, OSDictionary *properties) {
	bool result = false;
	if (super::init(type, initialVal, channelID, channelName, controlID, subType, usage)) {
		result = true;
	}
	return result;
}

void EMUXUCustomControl::free() {// place holder in case future versions have slots that require disposal 
	super::free();
}

IOReturn EMUXUCustomControl::hardwareValueChanged(OSObject *newValue) {
	IOReturn	result = kIOReturnBadArgument;
	if (newValue) {
#ifdef DEBUGLOGGING
		// just for debugging purposes
		OSNumber*	number;
		number = OSDynamicCast(OSNumber, newValue);
		if (number) {
			UInt32	theVal = number->unsigned32BitValue();
			debugIOLog("EMUXUCustomControl::hardwareValueChanged value %d", theVal);
		}
#endif
		result = super::hardwareValueChanged(newValue);
	}
	return result;
}

// The following functions are here simply to assist in debugging and for possible future additional functionality 
OSObject* EMUXUCustomControl::getValue() {
debugIOLog("EMUXUCustomControl::getValue");
		return super::getValue();
	
}

IOReturn EMUXUCustomControl::setValue(OSObject *newValue) {
	debugIOLog("EMUXUCustomControl::setValue");
	return super::setValue(newValue);
}

void EMUXUCustomControl::setValueChangeHandler(IntValueChangeHandler intValueChangeHandler, OSObject *target) {
	debugIOLog("EMUXUCustomControl::setValueChangedHandler");
	super::setValueChangeHandler(intValueChangeHandler, target);
}

IOReturn EMUXUCustomControl::addUserClient(IOAudioControlUserClient *newUserClient) {
	IOReturn result =  super::addUserClient(newUserClient);
	debugIOLog("EMUXUCustomControl::addUserClient %p, %d", newUserClient, result);
	return result;
}
