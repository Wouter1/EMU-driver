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
#ifndef _EMUUSBAUDIOLEVELCONTROL_H
#define _EMUUSBAUDIOLEVELCONTROL_H

#include <sys/cdefs.h>

__BEGIN_DECLS
#include <kern/thread_call.h>
__END_DECLS

#include <libkern/OSByteOrder.h>

#include <IOKit/IOLib.h>
#include <IOKit/IOPlatformExpert.h>

#include <IOKit/usb/USB.h>
#include <IOKit/usb/IOUSBInterface.h>

#include <IOKit/audio/IOAudioLevelControl.h>
#include <IOKit/audio/IOAudioTypes.h>
#include <IOKit/audio/IOAudioDefines.h>

#include "EMUUSBAudioCommon.h"
#include "USBAudioObject.h"
#include "EMUUSBAudioClip.h"

class EMUUSBAudioDevice;
typedef struct call_entry *thread_call_t;

typedef IOReturn (*USBDeviceRequest)(IOUSBDevRequest * request, void * refCon , IOUSBCompletion * completion );
//typedef IOReturn (*USBDeviceRequest)(IOUSBDevRequest * request, void * refCon = 0, IOUSBCompletion * completion = 0);

// PRAM read write values
enum{
	kMaximumPRAMVolume 	= 	7,
	kMinimumPRAMVolume	= 	0,
	KNumPramVolumeSteps	= 	(kMaximumPRAMVolume - kMinimumPRAMVolume + 1),
	kPRamVolumeAddr	= 		8,

	kDefaultVolume	= 0x006E006E,
	kInvalidVolumeMask	= 0xFE00FE00
};

#define kiSubMaxVolume		60
#define kiSubVolumePercent	92

class EMUUSBAudioLevelControl : public IOAudioLevelControl
{
    OSDeclareDefaultStructors(EMUUSBAudioLevelControl);

    UInt8					unitID;
    UInt8					interfaceNumber;
    UInt8					controlSelector;
    UInt8					channelNumber;
    SInt16					offset;
	UInt16					volRes;
    thread_call_t			setValueThreadCall;
    USBDeviceRequest		usbDeviceRequest;
	void *					callerRefCon;
	Boolean					gExpertMode;
    UInt32					fMaxVolume;
    UInt32					fMinVolume;
	Boolean					fShouldUpdatePRAM;

public:
	static EMUUSBAudioLevelControl *create(UInt8 theUnitID, UInt8 theInterfaceNumber, UInt8 theControlSelector, UInt8 theChannelNumber, Boolean shouldUpdatePRAM, USBDeviceRequest theUSBDeviceRequest, void *theCallerRefCon, UInt32 subType, UInt32 usage);

	virtual bool init(UInt8 theUnitID, UInt8 theInterfaceNumber, UInt8 theControlSelector, UInt8 theChannelNumber, Boolean shouldUpdatePRAM, USBDeviceRequest theUSBDeviceRequest, void *theCallerRefCon, UInt32 subType, UInt32 usage, OSDictionary *properties = NULL);
	virtual void free();

	virtual IOReturn performValueChange(OSObject * newValue);
	virtual void updateUSBValue();
	virtual void updateUSBValue(SInt32 newValue);

	static void updateValueCallback(void *arg1, void *arg2);

private:
	IOReturn	GetCurVolume (UInt8 interfaceNumber, UInt8 channelNumber, SInt16 * target);
	IOReturn	GetMaxVolume (UInt8 interfaceNumber, UInt8 channelNumber, SInt16 * target);
	IOReturn	GetMinVolume (UInt8 interfaceNumber, UInt8 channelNumber, SInt16 * target);
	IOReturn	GetVolumeResolution (UInt8 interfaceNumber, UInt8 channelNumber, UInt16 * target);
	IOReturn	SetCurVolume (UInt8 interfaceNumber, UInt8 channelNumber, SInt16 volume);
	IOFixed		ConvertUSBVolumeTodB (SInt16 volume);
//	IORegistryEntry * FindEntryByNameAndProperty (const IORegistryEntry * start, const char * name, const char * key, UInt32 value);
};

////////////////////////////////////////////////////////////////////////////////
///
/// Handles proper software volume control.  
///

class EMUUSBAudioSoftLevelControl : public IOAudioLevelControl
{
    OSDeclareDefaultStructors(EMUUSBAudioSoftLevelControl);

// volumes in gains
	Float32				mVolume;
	Float32				mTargetVolume;
	Float32				mLastVolume;
	
public:
	
	static EMUUSBAudioSoftLevelControl* create(SInt32 initialValue,
                                       SInt32 minValue,
                                       SInt32 maxValue,
                                       IOFixed minDB,
                                       IOFixed maxDB,
                                       UInt32 channelID,
                                       const char *channelName = 0,
                                       UInt32 cntrlID = 0,
                                       UInt32 subType = 0,
                                       UInt32 usage = 0);
	
	Float32	GetVolume(void){ return mVolume; }
	void	SetVolume(Float32 volume){ mVolume = volume; }
	Float32	GetTargetVolume(void){ return mTargetVolume; }
	void	SetTargetVolume(Float32 volume){ mTargetVolume = volume; }
	Float32 GetLastVolume(void){ return	mLastVolume; }
	void	SetLastVolume(Float32 volume){ mLastVolume = volume; }

};

////////////////////////////////////////////////////////////////////////////////
///
/// Handles proper hardware volume control.  
///

class EMUUSBAudioHardLevelControl : public IOAudioLevelControl
{
    OSDeclareDefaultStructors(EMUUSBAudioHardLevelControl);

// volumes in gains
	Float32				mVolume;
	Float32				mTargetVolume;
	Float32				mLastVolume;
	
public:
	
	static EMUUSBAudioHardLevelControl* create(SInt32 initialValue,
                                       SInt32 minValue,
                                       SInt32 maxValue,
                                       IOFixed minDB,
                                       IOFixed maxDB,
                                       UInt32 channelID,
                                       const char *channelName = 0,
                                       UInt32 cntrlID = 0,
                                       UInt32 subType = 0,
                                       UInt32 usage = 0);
	
	Float32	GetVolume(void){ return mVolume; }
	void	SetVolume(Float32 volume){ mVolume = volume; }
	Float32	GetTargetVolume(void){ return mTargetVolume; }
	void	SetTargetVolume(Float32 volume){ mTargetVolume = volume; }
	Float32 GetLastVolume(void){ return	mLastVolume; }
	void	SetLastVolume(Float32 volume){ mLastVolume = volume; }

};

#endif
