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

