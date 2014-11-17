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
////////////////////////////////////////////////////////////////////////////////
// Revision:
// $Author: jaimeh $
// $Revision: 1.3 $  $Date: 2007/06/28 22:16:55 $  
// 
////////////////////////////////////////////////////////////////////////////////

#ifndef _EMUUSBPLATFORM_H
#define _EMUUSBPLATFORM_H

#define MAX_DEV_NICK_NAME 64
#define MAX_NUMBER_METERS 64

typedef enum _tDirection
{
  EMU_INPUT,
  EMU_OUTPUT,
  EMU_HOST_OUTPUT
} tDirection;

typedef struct _DRIVER_VERSION{
	unsigned long version;
} DRIVER_VERSION, *PDRIVER_VERSION;

typedef struct _EMU_SET_NICK_NAME{
	char devNickName[MAX_DEV_NICK_NAME];
} EMU_SET_NICK_NAME, *PEMU_SET_NICK_NAME;

typedef struct _EMU_METER_DATA{
	unsigned short meter_data[MAX_NUMBER_METERS];
}	EMU_METER_DATA, *PEMU_METER_DATA;

typedef struct _EMU_HEADPHONE_DATA{
	unsigned long headphoneSource;
} EMU_HEADPHONE_DATA, *PEMU_HEADPHONE_DATA;

typedef struct _EMU_ANALOG_PAD_DATA{
	unsigned long channelIndex;
	unsigned long direction; // use tDirection enum
	unsigned long status;   
} EMU_ANALOG_PAD_DATA, *PEMU_ANALOG_PAD_DATA;

typedef struct _EMU_PHANTOM_POWER_DATA{
	unsigned long  channelIndex;
	unsigned long  status;  
} EMU_PHANTOM_POWER_DATA, *PEMU_PHANTOM_POWER_DATA;

typedef struct _EMU_MIXER_VALUE{
	unsigned long inputChannelIndex;
	unsigned long outputChannelIndex;  
	short volume;
} EMU_MIXER_VALUE, *PEMU_MIXER_VALUE;

typedef struct _EMU_VOLUME_VALUE{
	unsigned long channelIndex;
	unsigned long direction; // use tDirection enum
	short volume;   
} EMU_VOLUME_VALUE, *PEMU_VOLUME_VALUE;
  
typedef struct _EMU_MUTE_VALUE{
	unsigned long channelIndex;
	unsigned long direction; // use tDirection enum
	unsigned long mute;   
} EMU_MUTE_VALUE, *PEMU_MUTE_VALUE;


#ifdef _HULA_MACOSX_
enum
{
    kGetInterfaceVersion,
	kSetNickName,
	kGetDriverVersion,
	kGetMeterData,
	kGetClipData,
	kGetAnalogPad,
	kSetAnalogPad,
	kGetPhantomPower,
	kSetPhantomPower,
	kGetHeadphoneSource,	
	kSetHeadPhoneSource,
	kGetMixerValue,	
	kSetMixerValue,
	kGetVolumeValue,	
	kSetVolumeValue,
	kGetMuteValue,	
	kSetMuteValue,
    kNumberOfMethods
};
#endif


#endif _EMUUSBUSERCLIENT_H

