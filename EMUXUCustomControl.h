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
//	File:		EMUXUCustomControl.h
//	Contains:	Custom IOAudioControl class definition for the notification control exposed through CoreAudio
//	Written by:	David Tay 2006.
//--------------------------------------------------------------------------------
#ifndef	_EMUXUCUSTOMCONTROL
#define _EMUXUCUSTOMCONTROL
#include <IOKit/audio/IOAudioControl.h>


class EMUXUCustomControl:public IOAudioControl {
	OSDeclareDefaultStructors(EMUXUCustomControl);

	public:
		static EMUXUCustomControl	*create(UInt32 initialVal, UInt32 channelID, const char *channelName, UInt32 cntrlID = 0, UInt32 subType = 0, UInt32 usage = 0);
		virtual bool init(UInt32 type, OSObject *initialVal, UInt32 channelID, const char *channelName, UInt32 controlID, UInt32 subType, UInt32 usage, OSDictionary *properties = 0);
		virtual void free();
		virtual IOReturn hardwareValueChanged(OSObject* newValue);
		virtual OSObject* getValue();
		virtual IOReturn setValue(OSObject *newValue);
		virtual void setValueChangeHandler(IntValueChangeHandler intValueChangeHandler, OSObject *target);
		virtual IOReturn addUserClient(IOAudioControlUserClient *newUserClient);
};

#endif