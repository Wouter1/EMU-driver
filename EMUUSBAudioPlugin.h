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

#ifndef __APPLEUSBAUDIOPLUGIN__
#define __APPLEUSBAUDIOPLUGIN__

#include <IOKit/IOService.h>
#include <IOKit/audio/IOAudioStream.h>
#include <IOKit/usb/USB.h>

#include "EMUUSBAudioEngine.h"

class EMUUSBAudioPlugin : public IOService {

	OSDeclareDefaultStructors (EMUUSBAudioPlugin)

private:
	EMUUSBAudioEngine *			mOurProvider;

protected:
	struct ExpansionData { };

	ExpansionData *reserved;

public:
	// OSMetaClassDeclareReservedUsed (EMUUSBAudioPlugin, 0);
	virtual IOReturn	pluginProcessInput (float * destBuf, UInt32 numSampleFrames, UInt32 numChannels);
	// OSMetaClassDeclareReservedUsed (EMUUSBAudioPlugin, 1);
	virtual IOReturn	pluginSetDirection (IOAudioStreamDirection direction);

private:
	OSMetaClassDeclareReservedUsed (EMUUSBAudioPlugin, 0);
	OSMetaClassDeclareReservedUsed (EMUUSBAudioPlugin, 1);

	OSMetaClassDeclareReservedUnused (EMUUSBAudioPlugin, 2);
	OSMetaClassDeclareReservedUnused (EMUUSBAudioPlugin, 3);
	OSMetaClassDeclareReservedUnused (EMUUSBAudioPlugin, 4);
	OSMetaClassDeclareReservedUnused (EMUUSBAudioPlugin, 5);
	OSMetaClassDeclareReservedUnused (EMUUSBAudioPlugin, 6);
	OSMetaClassDeclareReservedUnused (EMUUSBAudioPlugin, 7);
	OSMetaClassDeclareReservedUnused (EMUUSBAudioPlugin, 8);
	OSMetaClassDeclareReservedUnused (EMUUSBAudioPlugin, 9);
	OSMetaClassDeclareReservedUnused (EMUUSBAudioPlugin, 10);
	OSMetaClassDeclareReservedUnused (EMUUSBAudioPlugin, 11);
	OSMetaClassDeclareReservedUnused (EMUUSBAudioPlugin, 12);
	OSMetaClassDeclareReservedUnused (EMUUSBAudioPlugin, 13);
	OSMetaClassDeclareReservedUnused (EMUUSBAudioPlugin, 14);
	OSMetaClassDeclareReservedUnused (EMUUSBAudioPlugin, 15);

public:
	virtual	bool		start (IOService * provider);
	virtual	void		stop (IOService * provider);

	IOReturn			pluginDeviceRequest (IOUSBDevRequest * request, IOUSBCompletion * completion = NULL);
	void				pluginSetConfigurationApp (const char * bundleID);

	virtual	IOReturn	pluginInit (IOService * provider, UInt16 vendorID, UInt16 modelID);
	virtual	IOReturn	pluginStart ();
	virtual	IOReturn	pluginReset ();
	virtual	IOReturn	pluginSetFormat (const IOAudioStreamFormat * const newFormat, const IOAudioSampleRate * const newSampleRate);
	virtual	IOReturn	pluginProcess (float * mixBuf, UInt32 numSampleFrames, UInt32 numChannels);
	virtual	IOReturn	pluginStop ();
};

#endif
