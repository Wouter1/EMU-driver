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
#ifndef _EMUUSBAUDIOMUTECONTROL_H
#define _EMUUSBAUDIOMUTECONTROL_H

#include <sys/cdefs.h>

__BEGIN_DECLS
#include <kern/thread_call.h>
__END_DECLS

#include <libkern/OSByteOrder.h>

#include <IOKit/IOLib.h>

#include <IOKit/usb/USB.h>
#include <IOKit/usb/IOUSBInterface.h>

#include <IOKit/audio/IOAudioDefines.h>
#include <IOKit/audio/IOAudioTypes.h>
#include <IOKit/audio/IOAudioToggleControl.h>

#include "EMUUSBAudioCommon.h"
#include "USBAudioObject.h"

class EMUUSBAudioDevice;
typedef struct call_entry *thread_call_t;

typedef IOReturn (*USBDeviceRequest)(IOUSBDevRequest * request, void * refCon , IOUSBCompletion * completion );
//typedef IOReturn (*USBDeviceRequest)(IOUSBDevRequest * request, void * refCon = 0, IOUSBCompletion * completion = 0);

class EMUUSBAudioMuteControl : public IOAudioToggleControl
{
    OSDeclareDefaultStructors (EMUUSBAudioMuteControl);

    UInt8					unitID;
    UInt8					interfaceNumber;
    UInt8					channelNumber;
    EMUUSBAudioDevice *	audioDevice;
    thread_call_t			setValueThreadCall;
    USBDeviceRequest		usbDeviceRequest;
	void *					callerRefCon;

public:
    static EMUUSBAudioMuteControl *create (UInt8 theUnit, UInt8 theInterfaceNumber, UInt8 theChannelNumber, USBDeviceRequest theUSBDeviceRequest, void *theCallerRefCon, UInt32 usage, UInt32 subType = kIOAudioToggleControlSubTypeMute, UInt32 controlID = 0);

    virtual bool init (UInt8 theUnit, UInt8 theInterfaceNumber, UInt8 theChannelNumber, USBDeviceRequest theUSBDeviceRequest, void *theCallerRefCon, UInt32 usage, UInt32 subType, UInt32 controlID = 0, OSDictionary *properties = NULL);
    virtual void free ();

    virtual IOReturn performValueChange (OSObject * newValue);
    virtual void updateUSBValue ();
    virtual void updateUSBValue (SInt32 newValue);

    static void updateValueCallback (void *arg1, void *arg2);

private:
	UInt8		GetCurMute (UInt8 interfaceNumber, UInt8 channelNumber, IOReturn * error);
	IOReturn	SetCurMute (UInt8 interfaceNumber, UInt8 channelNumber, UInt8 theMuteState);
//	IORegistryEntry * FindEntryByNameAndProperty (const IORegistryEntry * start, const char * name, const char * key, UInt32 value);
};

#endif
