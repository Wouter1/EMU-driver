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
// $Revision: 1.4 $  $Date: 2007/08/08 17:31:30 $  
// 
////////////////////////////////////////////////////////////////////////////////

#ifndef _EMUUSBUSERCLIENT_H
#define _EMUUSBUSERCLIENT_H

#include <mach/mach_types.h>
#include <IOKit/IOUserClient.h>

#include "EMUUSBAudioDevice.h"
#include "EMUUSBPlatform.h"

typedef enum EMU_CLIENT_EVENT {
	EMU_VOLUME_EVENT = 0,
	EMU_MUTE_EVENT,
	EMU_MAX_CLIENT_EVENTS
} EMU_CLIENT_EVENT;

typedef struct EMU_REGISTER_CLIENT{
	void* hDeviceEvents[EMU_MAX_CLIENT_EVENTS];
	void* pRefCon; //for use with Mac only!
};

class EMUUSBUserClient: public IOUserClient
{
		OSDeclareDefaultStructors (EMUUSBUserClient)
		
		task_t m_fTask;
		mach_port_t m_hNotificationPort;
		bool m_notificationRegistered;
		OSAsyncReference m_EventCallbackAsyncRef[EMU_MAX_CLIENT_EVENTS];
		bool m_EventCallbackSet[EMU_MAX_CLIENT_EVENTS];
		
		EMUUSBAudioDevice*	mDevice;
		
		UInt8	mMetersID;
		UInt8	mMixerID;
		UInt8	mProcessingUnitID;
 
public:
		static const long kMaxNumberOfUnits = 8;
		
		virtual bool initWithTask(task_t owningTask, void * security_id, UInt32 type);
		virtual bool start(IOService *provider);
		virtual void free();

		virtual IOExternalMethod * getTargetAndMethodForIndex(IOService ** target, UInt32 index);
		virtual IOReturn clientClose(void);

		virtual IOReturn clientMemoryForType (UInt32 memoryAddressToMap, IOOptionBits *pOptions, IOMemoryDescriptor **ppMemory);
		virtual IOReturn registerNotificationPort (mach_port_t port, UInt32 type, UInt32 refCon);
		virtual void SendEventNotification (EMU_CLIENT_EVENT eventType);
		virtual IOReturn RegisterClient(EMU_REGISTER_CLIENT* pRegisterClient, IOByteCount inStructSize);

private:
		UInt8	mInputFeatureUnits[kMaxNumberOfUnits];
		UInt8	mNumberOfInputFeatureUnits;
		UInt8	mOutputFeatureUnits[kMaxNumberOfUnits];
		UInt8	mNumberOfOutputFeatureUnits;
		
		bool	GetVolumeID(tDirection direction,UInt8& volumeID);

		IOReturn GetInterfaceVersion (unsigned long* pulVersion);
		IOReturn SetNickName (PEMU_SET_NICK_NAME pInDiceSetNickName, PEMU_SET_NICK_NAME pOutDiceSetNickName, IOByteCount inStructSize, IOByteCount *pOutStructSize);
		IOReturn GetDriverVersion (PDRIVER_VERSION pDriverVersion, IOByteCount *pOutStructSize);
		IOReturn GetClipData (PEMU_METER_DATA pInMeterData, PEMU_METER_DATA pOutMeterData, IOByteCount inStructSize, IOByteCount *pOutStructSize);
		IOReturn GetMeterData (PEMU_METER_DATA pInMeterData, PEMU_METER_DATA pOutMeterData, IOByteCount inStructSize, IOByteCount *pOutStructSize);

		IOReturn GetAnalogPad(PEMU_ANALOG_PAD_DATA pInAnalogPadData, PEMU_ANALOG_PAD_DATA pOutAnalogPadData, IOByteCount inStructSize, 
							  IOByteCount* pOutStructSize);
		IOReturn SetAnalogPad(PEMU_ANALOG_PAD_DATA pInAnalogPadData, IOByteCount inStructSize);

		IOReturn GetPhantomPower(PEMU_PHANTOM_POWER_DATA pInPhantomData, PEMU_PHANTOM_POWER_DATA pOutPhantomData, IOByteCount inStructSize, 
								 IOByteCount* pOutStructSize);
		IOReturn SetPhantomPower(PEMU_PHANTOM_POWER_DATA pInPhantomData, IOByteCount inStructSize);
		
		IOReturn GetHeadPhoneSource(PEMU_HEADPHONE_DATA pInHeadPhoneData, PEMU_HEADPHONE_DATA pOutHeadPhoneData, IOByteCount inStructSize, 
									IOByteCount* pOutStructSize);
		IOReturn SetHeadPhoneSource(PEMU_HEADPHONE_DATA pInHeadPhoneData, IOByteCount inStructSize);
		
		IOReturn GetMixerValue(PEMU_MIXER_VALUE pInMixerValue, PEMU_MIXER_VALUE pOutMixerValue, IOByteCount inStructSize, 
									IOByteCount* pOutStructSize);
		IOReturn SetMixerValue(PEMU_MIXER_VALUE pInMixerValue, IOByteCount inStructSize);
		
		IOReturn GetVolumeValue(PEMU_VOLUME_VALUE pInVolumeValue, PEMU_VOLUME_VALUE pOutVolumeValue, IOByteCount inStructSize, 
									IOByteCount* pOutStructSize);
		IOReturn SetVolumeValue(PEMU_VOLUME_VALUE pInVolumeValue, IOByteCount inStructSize);
		
		IOReturn GetMuteValue(PEMU_MUTE_VALUE pInMuteValue, PEMU_MUTE_VALUE pOutMuteValue, IOByteCount inStructSize, 
									IOByteCount* pOutStructSize);
		IOReturn SetMuteValue(PEMU_MUTE_VALUE pInMuteValue, IOByteCount inStructSize);
		
		
};


#endif _EMUUSBUSERCLIENT_H

