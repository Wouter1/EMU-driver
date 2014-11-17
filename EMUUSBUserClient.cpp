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
// $Revision: 1.5 $  $Date: 2007/08/08 17:31:30 $  
// 
////////////////////////////////////////////////////////////////////////////////

#include <IOKit/audio/IOAudioEngine.h>

#include "EMUUSBUserClient.h"
#include "EMUUSBAudioCommon.h"
#include "EMUUSBDeviceDefines.h"
#include "USBAudioObject.h"


#define PARENTCLASS IOUserClient
OSDefineMetaClassAndStructors(EMUUSBUserClient, IOUserClient)

/*------------------------------------------------------------
*	EMUUSBUserClient::getTargetAndMethodForIndex
*
*	Parameters:  
*		See documentation for IOUserClient
*
*	Globals Used:
*		None
*
*	Description:
*		This routine returns information for the requested method into
*		the user client.
*
*	Returns:
*		NULL on failture, a pointer to IOExternalMethod on success.
*
*------------------------------------------------------------*/



IOExternalMethod *EMUUSBUserClient::getTargetAndMethodForIndex(IOService ** target, UInt32 index)
{
    static const IOExternalMethod sMethods[kNumberOfMethods] = 
    {
        {   // kGetInterfaceVersion
            NULL,						 // The IOService * will be determined at runtime below.
            (IOMethod) &EMUUSBUserClient::GetInterfaceVersion,	 // Method pointer.
            kIOUCScalarIScalarO,		       		 // Scalar Input, Scalar Output.
            0,						 // scalar input values.
            1							 // scalar output values
        }

		,{ //kSetNickName
            NULL,						 // The IOService * will be determined at runtime below.
            (IOMethod) &EMUUSBUserClient::SetNickName,	 // Method pointer.
            kIOUCStructIStructO,								// struct Input, Struct Output.
            sizeof(EMU_SET_NICK_NAME),					// struct input values.
            sizeof(EMU_SET_NICK_NAME)					// struct output values
		}

		,{ //kGetDriverVersion
            NULL,						 // The IOService * will be determined at runtime below.
            (IOMethod) &EMUUSBUserClient::GetDriverVersion,	 // Method pointer.
            kIOUCScalarIStructO,								// Scalar Input, Struct Output.
            0,																	// number of inputs
            sizeof(DRIVER_VERSION),																	// size of output struct
		}

		,{ //kGetMeterData
            NULL,						 // The IOService * will be determined at runtime below.
            (IOMethod) &EMUUSBUserClient::GetMeterData,	 // Method pointer.
            kIOUCStructIStructO,								// struct Input, Struct Output.
            sizeof(EMU_METER_DATA),					// struct input values.
            sizeof(EMU_METER_DATA)					// struct output values
		}

		,{ //kGetClipData
            NULL,						 // The IOService * will be determined at runtime below.
            (IOMethod) &EMUUSBUserClient::GetClipData,	 // Method pointer.
            kIOUCStructIStructO,								// struct Input, Struct Output.
            sizeof(EMU_METER_DATA),					// struct input values.
            sizeof(EMU_METER_DATA)					// struct output values
		}
		
		,{	// kGetAnalogPad
			NULL,						 // The IOService * will be determined at runtime below.
			(IOMethod) &EMUUSBUserClient::GetAnalogPad,	 // Method pointer.
			kIOUCStructIStructO,								// Struct Input, Struct Output.
			sizeof(EMU_ANALOG_PAD_DATA),						// size of input struct
			sizeof(EMU_ANALOG_PAD_DATA),					// size of output struct
		}
		
		,{	// kSetAnalogPad
			NULL,						 // The IOService * will be determined at runtime below.
			(IOMethod) &EMUUSBUserClient::SetAnalogPad,	 // Method pointer.
			kIOUCStructIStructO,								// Struct Input, Struct Output.
			sizeof(EMU_ANALOG_PAD_DATA),						// size of input struct
			0,													// size of output struct
		}
		
		,{	// kGetPhantomPower
			NULL,						 // The IOService * will be determined at runtime below.
			(IOMethod) &EMUUSBUserClient::GetPhantomPower,	 // Method pointer.
			kIOUCStructIStructO,								// Struct Input, Struct Output.
			sizeof(EMU_PHANTOM_POWER_DATA),						// size of input struct
			sizeof(EMU_PHANTOM_POWER_DATA),					// size of output struct
		}
		
		,{	// kSetPhantomPower
			NULL,						 // The IOService * will be determined at runtime below.
			(IOMethod) &EMUUSBUserClient::SetPhantomPower,	 // Method pointer.
			kIOUCStructIStructO,								// Struct Input, Struct Output.
			sizeof(EMU_PHANTOM_POWER_DATA),						// size of input struct
			0,													// size of output struct
		}
		
		,{	// kGetHeadphoneSource
			NULL,						 // The IOService * will be determined at runtime below.
			(IOMethod) &EMUUSBUserClient::GetHeadPhoneSource,	 // Method pointer.
			kIOUCStructIStructO,								// Struct Input, Struct Output.
			sizeof(EMU_HEADPHONE_DATA),						// size of input struct
			sizeof(EMU_HEADPHONE_DATA),					// size of output struct
		}
		
		,{	// kSetHeadPhoneSource
			NULL,						 // The IOService * will be determined at runtime below.
			(IOMethod) &EMUUSBUserClient::SetHeadPhoneSource,	 // Method pointer.
			kIOUCStructIStructO,								// Struct Input, Struct Output.
			sizeof(EMU_HEADPHONE_DATA),						// size of input struct
			0,													// size of output struct
		}
		
		,{	// kGetMixerValue
			NULL,						// The IOService * will be determined at runtime below.
			(IOMethod) &EMUUSBUserClient::GetMixerValue,	 // Method pointer.
			kIOUCStructIStructO,						// Struct Input, Struct Output.
			sizeof(EMU_MIXER_VALUE),						// size of input struct
			sizeof(EMU_MIXER_VALUE),					// size of output struct
		}
		
		,{	// kSetMixerValue
			NULL,						// The IOService * will be determined at runtime below.
			(IOMethod) &EMUUSBUserClient::SetMixerValue,	 // Method pointer.
			kIOUCStructIStructO,								// Struct Input, Struct Output.
			sizeof(EMU_MIXER_VALUE),						// size of input struct
			0,													// size of output struct
		}
		
		,{	// kGeVolumeValue
			NULL,						// The IOService * will be determined at runtime below.
			(IOMethod) &EMUUSBUserClient::GetVolumeValue,	 // Method pointer.
			kIOUCStructIStructO,						// Struct Input, Struct Output.
			sizeof(EMU_VOLUME_VALUE),						// size of input struct
			sizeof(EMU_VOLUME_VALUE),					// size of output struct
		}
		
		,{	// kSetVolumeValue
			NULL,						// The IOService * will be determined at runtime below.
			(IOMethod) &EMUUSBUserClient::SetVolumeValue,	 // Method pointer.
			kIOUCStructIStructO,								// Struct Input, Struct Output.
			sizeof(EMU_VOLUME_VALUE),						// size of input struct
			0,													// size of output struct
		}

		,{	// kGeMuteValue
			NULL,						// The IOService * will be determined at runtime below.
			(IOMethod) &EMUUSBUserClient::GetMuteValue,	 // Method pointer.
			kIOUCStructIStructO,						// Struct Input, Struct Output.
			sizeof(EMU_MUTE_VALUE),						// size of input struct
			sizeof(EMU_MUTE_VALUE),					// size of output struct
		}
		
		,{	// kSetMuteValue
			NULL,						// The IOService * will be determined at runtime below.
			(IOMethod) &EMUUSBUserClient::SetMuteValue,	 // Method pointer.
			kIOUCStructIStructO,								// Struct Input, Struct Output.
			sizeof(EMU_MUTE_VALUE),						// size of input struct
			0,													// size of output struct
		}
    };

    
		//D3 (("Dice1394: GetTargetMethodForIndex: %d, sizeof (EMU_CHANNEL_NAMES_INFO = %d)\n", (int)index, sizeof(EMU_CHANNEL_NAMES_INFO) ));
		
    // Make sure that the index of the function we're calling actually exists in the function table.
    if (index < (UInt32)kNumberOfMethods)
	{
				// These methods exist in EMUUSBUserClient, so return a pointer to Dice1394UserClient.
        *target = this;
        return (IOExternalMethod *) &sMethods[index];
      }
        
    return NULL;
}



/*------------------------------------------------------------
*	EMUUSBUserClient::initWithTask
*
*	Parameters:  
*		See documentation for IOUserClient
*
*	Globals Used:
*		None
*
*	Description:
*		We use this mainly to get hold of the owningTask - which can be used for
*		a number of purposes later on.
*
*	Returns:
*		true on success, false an failure.
*
*------------------------------------------------------------*/
bool EMUUSBUserClient::initWithTask(task_t owningTask, void *security_id , UInt32 type)
{
    debugIOLog("EMUUSBUserClient:initWithTask");
    
    if (!PARENTCLASS::initWithTask(owningTask, security_id , type))
        return false;
    
    if (!owningTask)
			return false;
	
    m_fTask = owningTask;
//    m_pDiceInterfaceDevice = NULL;
	//	BlockZero (&m_NotificationQuadlets, sizeof(m_NotificationQuadlets));
        
	 debugIOLog("-EMUUSBUserClient:initWithTask");
	 
    return true;
}

 

/*------------------------------------------------------------
*	EMUUSBUserClient::start
*
*	Parameters:  
*		See documentation for IOUserClient
*
*	Globals Used:
*		None
*
*	Description:
*		We use this mainly to get hold of provider - which
*		is our DiceInterfaceDevice object.
*
*	Returns:
*		true on success, false on failure.
*
*------------------------------------------------------------*/
bool EMUUSBUserClient::start(IOService *provider)
{
	debugIOLog ("EMUUSBUserClient[%p]::start",provider);
			
    if (!PARENTCLASS::start(provider)) {
			return false;
	}

	mDevice = OSDynamicCast (EMUUSBAudioDevice, provider);
	if (mDevice == NULL) { return false; }
	
	mDevice->SetUserClient(this);
	
	EMUUSBAudioConfigObject* usbConfig = mDevice->GetUSBAudioConfigObject();
	
	mMetersID = usbConfig->FindExtensionUnitID(mDevice->mInterfaceNum, kMetering);
	
	mMixerID = 0;
	mProcessingUnitID = 0;
	mNumberOfInputFeatureUnits = 0;
	mNumberOfOutputFeatureUnits = 0;
		
	IOReturn result = mDevice->GetVariousUnits(mInputFeatureUnits,
												mNumberOfInputFeatureUnits,
												mOutputFeatureUnits,
												mNumberOfOutputFeatureUnits,
												mMixerID,
												mProcessingUnitID);
												
	for (long i=0;i<mNumberOfInputFeatureUnits;i++) 
	{
		debugIOLog ("EMUUSBUserClient:: mInputFeatureUnits[%d] = %d",i,mInputFeatureUnits[i]);
	}
	
	for (long i=0;i<mNumberOfOutputFeatureUnits;i++) 
	{
		debugIOLog ("EMUUSBUserClient:: mOutputFeatureUnits[%d] = %d",i,mOutputFeatureUnits[i]);
	}
	
	debugIOLog ("EMUUSBUserClient::mMixerID = %d",mMixerID);
	debugIOLog ("EMUUSBUserClient::mProcessingUnitID = %d",mProcessingUnitID);
		
	debugIOLog ("-EMUUSBUserClient[%p]::mDevice",mDevice);
		
	return true;
		
}


/*------------------------------------------------------------
*	EMUUSBUserClient::free
*
*	Parameters:  
*		None.
*
*	Globals Used:
*		None
*
*	Description:
*		Not doing anything really!
*
*	Returns:
*
*
*------------------------------------------------------------*/
void EMUUSBUserClient::free()
{
	//Free it now.
	PARENTCLASS::free ();
	debugIOLog ("EMUUSBUserClient::free");
}



/*------------------------------------------------------------
*	EMUUSBUserClient::clientClose
*
*	Parameters:  
*		None
*
*	Globals Used:
*		None
*
*	Description:
*		We use this to find out when a client has gone away.
*
*	Returns:
*		kIOReturnSuccess always.
*
*------------------------------------------------------------*/
IOReturn EMUUSBUserClient::clientClose(void)
{
    debugIOLog("EMUUSBUserClient::clientClose");
    
    if (m_fTask) {
		m_fTask = NULL;
	}
	
	if (mDevice)
	{
		mDevice->SetUserClient(NULL);
	}
		
    terminate();
    
    //DONT call PARENTCALSS::clientClose, which just returns notSupported
    
    return kIOReturnSuccess;
}

/*------------------------------------------------------------
*	EMUUSBUserClient::clientMemoryForType
*
*	Parameters:  
*		See documentation for IOUserClient
*
*	Globals Used:
*		None
*
*	Description:
*		This routine maps driver/kernel memory for use with a user client.
*
*	Returns:
*		kIOReturnNotOpen on failure, kIOReturnSuccess on success.
*
*------------------------------------------------------------*/
IOReturn EMUUSBUserClient::clientMemoryForType (UInt32 memoryAddressToMap, IOOptionBits *pOptions, IOMemoryDescriptor **ppMemory)
{
	debugIOLog("EMUUSBUserClient::clientMemoryForType");
	
	return PARENTCLASS::clientMemoryForType(memoryAddressToMap, pOptions, ppMemory);
	   
	   /*
	   
	IOReturn retVal = kIOReturnNoMemory;
	IOMemoryDescriptor *mem = NULL;
	
//	if (!m_pDiceInterfaceDevice)
//		goto Exit;

	
//	Find out which memory address is to be mapped and obtain a map, store it in mem
	
	
	//Which address to map
	switch (memoryAddressToMap)
	  {
			case kMIDIBuffers:
				mem = IOMemoryDescriptor::withAddress ((void *)&m_pDiceInterfaceDevice->m_MIDIBuffers, (IOByteCount)sizeof (EMU_MIDI_BUFFERS), kIODirectionOutIn);
				break;
			case kChannelNamesTable:
				mem = IOMemoryDescriptor::withAddress ((void *)&m_ChannelNamesTable, (IOByteCount)sizeof (EMU_CHANNEL_NAMES_TABLE), kIODirectionOutIn);
				break;
		}

	
	Exit:
	if (mem)
		{
			mem->retain ();
			*ppMemory = mem;
			//Return Success
			retVal = kIOReturnSuccess;
		}

	return retVal;*/
}

/*------------------------------------------------------------
*	EMUUSBUserClient::registerNotificationPort
*
*	Parameters:  
*	port		-> Client specified port
*	type		-> Client specified Type
*	refcon	-> Client specified refcon
*
*	Globals Used:
*		None
*
*	Description:
*		It simply stores client specified port for further use.
*
*	Returns:
*		kIOReturnNotOpen on failure, kIOReturnSuccess on success.
*
*------------------------------------------------------------*/
IOReturn EMUUSBUserClient::registerNotificationPort(
	mach_port_t port, 
	UInt32 type, 
	UInt32 refCon)
{
	IOReturn result = kIOReturnSuccess;

	//Retain reference to mach notification port here.
	m_hNotificationPort = port;

	//We have registered notification here so update it with TRUE.
	m_notificationRegistered = TRUE;
		
	return result;
}

/*------------------------------------------------------------
*	EMUUSBUserClient::RegisterClient
*
*	Parameters:  
*		None
*
*	Globals Used:
*		None
*
*	Description:
*		This routine will setup things to provide notifications to the client applications.
*
*	Returns:
*		kIOReturnNotOpen on failure, kIOReturnSuccess on success.
*
*------------------------------------------------------------*/
IOReturn EMUUSBUserClient::RegisterClient(
	EMU_REGISTER_CLIENT* pRegisterClient,
	IOByteCount inStructSize)
{
	for (int currentEvent = EMU_VOLUME_EVENT; currentEvent < EMU_MAX_CLIENT_EVENTS; currentEvent++)
	{
		if (pRegisterClient->hDeviceEvents[currentEvent])
		{
			//The setAsyncReference method initializes the OSAsyncReference array with the following 
			//constants (in this order): kIOAsyncReservedIndex, kIOAsyncCalloutFuncIndex, and kIOAsyncCalloutRefconIndex. 
			IOUserClient::setAsyncReference (m_EventCallbackAsyncRef[currentEvent], m_hNotificationPort, pRegisterClient->hDeviceEvents[currentEvent], pRegisterClient->pRefCon);
			
			//Set flag that we've successfully set callback.
			m_EventCallbackSet[currentEvent] = true;
		}
	}
	
	return kIOReturnSuccess;
}
	
/*------------------------------------------------------------
*	EMUUSBUserClient::SendNotification
*
*	Description:
*		It simply sends notification to client app.
*
*	Returns:
*		kIOReturnSuccess on success kIOReturnNotOpen on failure.
*
*------------------------------------------------------------*/
void EMUUSBUserClient::SendEventNotification(
	EMU_CLIENT_EVENT eventType)
{
	//Check if notification is registered and callback is set
	if ((m_notificationRegistered) && (m_EventCallbackSet[eventType]))
	{
		//Notify user application from here.
		//m_ConfigChangedCallbackAsyncRef - We have updated it in our IOCTL and we want to use it now.
		//It contains information about port, refcon and callback
		sendAsyncResult (m_EventCallbackAsyncRef[eventType], kIOReturnSuccess, NULL, 0);
	}
}

/*------------------------------------------------------------
*	EMUUSBUserClient::GetDriverVersion
*
*	Parameters:  
*		pDriverVersion - pointer to a structure which will receive driver version information.
*		pOutStructSize - pointer to store the amount of data being sent back.
*
*	Globals Used:
*		None
*
*	Description:
*		This routine returns the driver version (in the form "1.2d2")
*
*	Returns:
*		kIOReturnNotOpen on failure, kIOReturnSuccess on success.
*
*------------------------------------------------------------*/
IOReturn EMUUSBUserClient::GetInterfaceVersion (unsigned long* pulVersion)
{
		//IOLog ("GetInterfaceVersion -> Welcome\n");
	//	D2 (("Dice1394AudioUserClient::GetInterfaceVersion \n"));
	//	*pulVersion = EMU1394_KEXT_INTERFACE_VERSION;
	return kIOReturnSuccess;
}
	
IOReturn EMUUSBUserClient::GetDriverVersion (PDRIVER_VERSION pDriverVersion, IOByteCount *pOutStructSize)
{
	/*	if (m_pDiceInterfaceDevice)
			{
				*pOutStructSize = sizeof (EMU_DRIVER_VERSION);
				strncpy (pDriverVersion->driverVersionString, DRIVER_VERSION_NUMBER, MAX_DRIVER_VERSION_LENGTH);
				return kIOReturnSuccess;
			}*/
			
	return kIOReturnSuccess;
  //  return kIOReturnNotOpen;
}


IOReturn EMUUSBUserClient::SetNickName(
	PEMU_SET_NICK_NAME pInDiceSetNickName, 
	PEMU_SET_NICK_NAME pOutDiceSetNickName, 
	IOByteCount inStructSize, 
	IOByteCount *pOutStructSize)
{
	return kIOReturnSuccess;
}

IOReturn EMUUSBUserClient::GetClipData(
	PEMU_METER_DATA pInMeterData, 
	PEMU_METER_DATA pOutMeterData, 
	IOByteCount inStructSize, 
	IOByteCount *pOutStructSize)
{
	return kIOReturnSuccess;
}

/*------------------------------------------------------------
*	EMUUSBUserClient::GetMeterData
*
*	Parameters:  
*		pInMeterData - pointer to a structure specifying parameters for reading Meter Data.
*		pOutMeterData - pointer to a structure where the data read will be stored.
*		inStructSize - amount of data being sent with pInAsyncRead (sizeof the structure)
*		pOutStructSize - pointer to received the actual amount of data being sent back.
*
*	Globals Used:
*		None
*
*	Description:
*		This routine calls on the Dice1394AudioDevice to read/clear the data
*
*	Returns:
*		kIOReturnNotOpen on failure, kIOReturnSuccess on success.
*
*------------------------------------------------------------*/
IOReturn EMUUSBUserClient::GetMeterData(PEMU_METER_DATA pInMeterData, PEMU_METER_DATA pOutMeterData, IOByteCount inStructSize, IOByteCount *pOutStructSize)
{
	debugIOLog("+EMUUSBUserClient::GetMeterData");
	
	if (pOutMeterData == NULL) {
		return kIOReturnBadArgument;
	}

	const long kControlSelectorMeterRead = 0x3;
	
	if (mDevice) 
	{
		mDevice->getExtensionUnitSetting(mMetersID, kControlSelectorMeterRead, pOutMeterData->meter_data, sizeof(unsigned short) * MAX_NUMBER_METERS);
	}
	else {
		return kIOReturnError;
	}
			
    return kIOReturnSuccess;
}


IOReturn EMUUSBUserClient::GetPhantomPower(
	PEMU_PHANTOM_POWER_DATA pInPhantomData, 
	PEMU_PHANTOM_POWER_DATA pOutPhantomData, 
	IOByteCount inStructSize, 
	IOByteCount* pOutStructSize)
{
/*	if (pInPhantomData == NULL || pInPhantomData == NULL || m_pDiceInterfaceDevice == NULL) {
		return kIOReturnBadArgument;
	}
	
	if (pInPhantomData->deviceIndex > m_pDiceInterfaceDevice->m_DeviceCount) {
		return kIOReturnBadArgument;
	}
	
	EMU_DEVICE_STRUCT* pDevice = &(m_pDiceInterfaceDevice->m_DiceDevices[pInPhantomData->deviceIndex]);
	
	DeviceInfo* pDevInfo = &pDevice->pDice1394Device->m_deviceInfo;
	
	tStatus theStatus = ioMixerQueryPhantomPower( pDevInfo,
												pInPhantomData->channelIndex,
												(unsigned int*) &pOutPhantomData->status );
							   
	if (theStatus == IOMIXER_FAIL) {
		return kIOReturnError;
	}*/

	return kIOReturnSuccess;
}
		
		
IOReturn EMUUSBUserClient::SetPhantomPower(
	PEMU_PHANTOM_POWER_DATA pInPhantomData, 
	IOByteCount inStructSize)
{
/*	if (pInPhantomData == NULL || m_pDiceInterfaceDevice == NULL) {
		return kIOReturnBadArgument;
	}
	
	if (pInPhantomData->deviceIndex > m_pDiceInterfaceDevice->m_DeviceCount) {
		return kIOReturnBadArgument;
	}
	
	EMU_DEVICE_STRUCT* pDevice = &(m_pDiceInterfaceDevice->m_DiceDevices[pInPhantomData->deviceIndex]);
	
	DeviceInfo* pDevInfo = &pDevice->pDice1394Device->m_deviceInfo;
	
	tStatus theStatus = ioMixerPhantomPower( pDevInfo,
                               pInPhantomData->channelIndex,
                               pInPhantomData->status );
							   
	if (theStatus == IOMIXER_FAIL) {
		return kIOReturnError;
	}*/

	return kIOReturnSuccess;
}

IOReturn EMUUSBUserClient::GetAnalogPad(
	PEMU_ANALOG_PAD_DATA pInAnalogPadData, 
	PEMU_ANALOG_PAD_DATA pOutAnalogPadData, 
	IOByteCount inStructSize, 
	IOByteCount* pOutStructSize)
{
/*	if (pInAnalogPadData == NULL || pOutAnalogPadData == NULL || m_pDiceInterfaceDevice == NULL) {
		return kIOReturnBadArgument;
	}
	
	if (pInAnalogPadData->deviceIndex > m_pDiceInterfaceDevice->m_DeviceCount) {
		return kIOReturnBadArgument;
	}
	
	EMU_DEVICE_STRUCT* pDevice = &(m_pDiceInterfaceDevice->m_DiceDevices[pInAnalogPadData->deviceIndex]);
	
	DeviceInfo* pDevInfo = &pDevice->pDice1394Device->m_deviceInfo;
	
	tStatus theStatus = ioMixerQueryAnalogPad( pDevInfo,
								pInAnalogPadData->direction,
								pInAnalogPadData->channelIndex,
								(unsigned int*) &pOutAnalogPadData->status );
							   
	if (theStatus == IOMIXER_FAIL) {
		return kIOReturnError;
	}*/

	return kIOReturnSuccess;
}

IOReturn EMUUSBUserClient::SetAnalogPad(
	PEMU_ANALOG_PAD_DATA pInAnalogPadData, 
	IOByteCount inStructSize)
{
/*	if (pInAnalogPadData == NULL || m_pDiceInterfaceDevice == NULL) {
		return kIOReturnBadArgument;
	}
	
	if (pInAnalogPadData->deviceIndex > m_pDiceInterfaceDevice->m_DeviceCount) {
		return kIOReturnBadArgument;
	}
	
	EMU_DEVICE_STRUCT* pDevice = &(m_pDiceInterfaceDevice->m_DiceDevices[pInAnalogPadData->deviceIndex]);
	
	DeviceInfo* pDevInfo = &pDevice->pDice1394Device->m_deviceInfo;
	
	tStatus theStatus = ioMixerAnalogPad( pDevInfo,
										pInAnalogPadData->direction,
										pInAnalogPadData->channelIndex,
										pInAnalogPadData->status );
							   
	if (theStatus == IOMIXER_FAIL) {
		return kIOReturnError;
	}*/

	return kIOReturnSuccess;
}

		
IOReturn EMUUSBUserClient::GetHeadPhoneSource(
	PEMU_HEADPHONE_DATA pInHeadPhoneData, 
	PEMU_HEADPHONE_DATA pOutHeadPhoneData, 
	IOByteCount inStructSize, 
	IOByteCount* pOutStructSize)
{
	debugIOLog("EMUUSBUserClient::GetHeadPhoneSource");
	
	if (pOutHeadPhoneData) {
		return kIOReturnBadArgument;
	}
		
	if (mDevice && mProcessingUnitID) 
	{
		UInt16 headphoneSource = 0;
				
		// use this extension unit API to send a processing unit request
		mDevice->getExtensionUnitSetting(mProcessingUnitID,
										0x2, // UD_MODE_SELECT_CONTROL 
										&headphoneSource, 
										sizeof(UInt16));
										
		debugIOLog("EMUUSBUserClient::GetHeadPhoneSource: headphoneSource %d",headphoneSource);
										
		// Then set the current source
		pOutHeadPhoneData->headphoneSource = headphoneSource;
	}
	else {
		return kIOReturnError;
	}
	
	return kIOReturnSuccess;
}

IOReturn EMUUSBUserClient::SetHeadPhoneSource(
	PEMU_HEADPHONE_DATA pInHeadPhoneData, 
	IOByteCount inStructSize)
{
	debugIOLog("+EMUUSBUserClient::SetHeadPhoneSource");
	
	if (pInHeadPhoneData == NULL) {
		return kIOReturnBadArgument;
	}
	
	UInt16 headphoneSource = HostToUSBWord(pInHeadPhoneData->headphoneSource);
	
	debugIOLog("EMUUSBUserClient::SetHeadPhoneSource: headphoneSource %d",headphoneSource);
	
	if (mDevice && mProcessingUnitID) 
	{
		// use this extension unit API to send a processing unit request
		mDevice->setExtensionUnitSetting(mProcessingUnitID,
										0x2, // UD_MODE_SELECT_CONTROL 
										&headphoneSource, 
										sizeof(UInt16));
	}
	else {
		debugIOLog("ERROR: EMUUSBUserClient::SetHeadPhoneSource: mProcessingUnitID %d mDevice %d",mProcessingUnitID, mDevice);
		
		return kIOReturnError;
	}

	return kIOReturnSuccess;
}

IOReturn EMUUSBUserClient::GetMixerValue(
	PEMU_MIXER_VALUE pInMixerValue, 
	PEMU_MIXER_VALUE pOutMixerValue, 
	IOByteCount inStructSize, 
	IOByteCount* pOutStructSize)
{
	debugIOLog("EMUUSBUserClient::GetMixerValue");
	
	if (pInMixerValue == NULL || pOutMixerValue) {
		return kIOReturnBadArgument;
	}
		
	if (mDevice && mMixerID) 
	{
		// Set all the values to be the same
		*pOutMixerValue = *pInMixerValue;
		
		UInt16 volume = 0;
				
		// use this feature unit API to send a mixer unit request
		mDevice->getFeatureUnitSetting((UInt8)pInMixerValue->inputChannelIndex, 
										mMixerID,
										(UInt8)pInMixerValue->outputChannelIndex, 
										GET_CUR, 
										(SInt16*)&volume);
										
		debugIOLog("EMUUSBUserClient::GetVolumeValue: inputChannelIndex %d outputChannelIndex %d volume %x",
				(UInt8)pOutMixerValue->inputChannelIndex,(UInt8)pOutMixerValue->outputChannelIndex,volume);
										
		// Then set the current volume
		pOutMixerValue->volume = volume;
	}

	return kIOReturnSuccess;
}

IOReturn EMUUSBUserClient::SetMixerValue(
	PEMU_MIXER_VALUE pInMixerValue, 
	IOByteCount inStructSize)
{
	debugIOLog("+EMUUSBUserClient::SetMixerValue");
	
	if (pInMixerValue == NULL) {
		return kIOReturnBadArgument;
	}
	
	UInt16 volume = pInMixerValue->volume;
	
	debugIOLog("EMUUSBUserClient::SetMixerValue: inputChannelIndex %d outputChannelIndex %d volume %d",
				(UInt8)pInMixerValue->inputChannelIndex,(UInt8)pInMixerValue->outputChannelIndex,volume);
	
	if (mDevice && mMixerID) 
	{
		// use this feature unit API to send a mixer unit request
		mDevice->setFeatureUnitSetting((UInt8)pInMixerValue->inputChannelIndex, 
										mMixerID,
										(UInt8)pInMixerValue->outputChannelIndex, 
										SET_CUR, 
										HostToUSBWord(volume), 
										sizeof(UInt16));
	}
	else {
		return kIOReturnError;
	}

	return kIOReturnSuccess;
}

bool EMUUSBUserClient::GetVolumeID(
	tDirection direction,
	UInt8& volumeID)
{
	bool foundID = true;
	
	/// ITEY SPECIFIC
	switch(direction)
	{
	case EMU_INPUT:
		volumeID = mInputFeatureUnits[0];
	break;
	case EMU_OUTPUT:
		volumeID = mOutputFeatureUnits[0];
	break;
	case EMU_HOST_OUTPUT:
		volumeID = mOutputFeatureUnits[1];
	break;
	default:
		foundID = false;
		break;
	}
	/// ITEY SPECIFIC
	
	return foundID;
}

IOReturn EMUUSBUserClient::GetVolumeValue(
	PEMU_VOLUME_VALUE pInVolumeValue, 
	PEMU_VOLUME_VALUE pOutVolumeValue, 
	IOByteCount inStructSize, 
	IOByteCount* pOutStructSize)
{
	debugIOLog("EMUUSBUserClient::GetVolumeValue");
	
	if (pInVolumeValue == NULL || pOutVolumeValue) {
		return kIOReturnBadArgument;
	}
	
	UInt16 volume = 0;
	UInt8 volumeID = 0;
	
	bool foundID = GetVolumeID((tDirection)pInVolumeValue->direction, volumeID);
		
	if (mDevice && foundID) 
	{
		// Set all the values to be the same
		*pOutVolumeValue = *pInVolumeValue;
				
		mDevice->getFeatureUnitSetting(VOLUME_CONTROL, 
										volumeID,
										(UInt8)pInVolumeValue->channelIndex, 
										GET_CUR, 
										(SInt16*)&volume);
										
		debugIOLog("EMUUSBUserClient::GetVolumeValue: direction %d channelIndex %d volume %x volumeID %d",
				(UInt8)pInVolumeValue->direction,(UInt8)pInVolumeValue->channelIndex,volume,volumeID);
										
		// Then set the current volume
		pOutVolumeValue->volume = volume;
	}

	return kIOReturnSuccess;
}

IOReturn EMUUSBUserClient::SetVolumeValue(
	PEMU_VOLUME_VALUE pInVolumeValue, 
	IOByteCount inStructSize)
{
	debugIOLog("EMUUSBUserClient::SetVolumeValue");
	
	if (pInVolumeValue == NULL) {
		return kIOReturnBadArgument;
	}
	
	UInt16 volume = pInVolumeValue->volume;
	UInt8 volumeID = 0;
	
	bool foundID = GetVolumeID((tDirection)pInVolumeValue->direction, volumeID);
		
	if (mDevice && foundID) 
	{
		debugIOLog("EMUUSBUserClient::SetVolumeValue: direction %d channelIndex %d volume %x volumeID %d",
				(UInt8)pInVolumeValue->direction,(UInt8)pInVolumeValue->channelIndex,volume,volumeID);
				
		mDevice->setFeatureUnitSetting(VOLUME_CONTROL, 
										volumeID,
										(UInt8)pInVolumeValue->channelIndex, 
										SET_CUR, 
										HostToUSBWord(volume), 
										sizeof(UInt16));
	}

	return kIOReturnSuccess;
}

IOReturn EMUUSBUserClient::GetMuteValue(
	PEMU_MUTE_VALUE pInMuteValue, 
	PEMU_MUTE_VALUE pOutMuteValue, 
	IOByteCount inStructSize, 
	IOByteCount* pOutStructSize)
{

	return kIOReturnSuccess;
}

IOReturn EMUUSBUserClient::SetMuteValue(
	PEMU_MUTE_VALUE pInMuteValue, 
	IOByteCount inStructSize)
{

	return kIOReturnSuccess;
}

	


