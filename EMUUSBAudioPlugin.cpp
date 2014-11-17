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

#include "EMUUSBAudioPlugin.h"

#include <IOKit/IOLib.h>

#define super IOService

OSDefineMetaClassAndStructors (EMUUSBAudioPlugin, IOService)

OSMetaClassDefineReservedUsed(EMUUSBAudioPlugin, 0);
OSMetaClassDefineReservedUsed(EMUUSBAudioPlugin, 1);

OSMetaClassDefineReservedUnused(EMUUSBAudioPlugin, 2);
OSMetaClassDefineReservedUnused(EMUUSBAudioPlugin, 3);
OSMetaClassDefineReservedUnused(EMUUSBAudioPlugin, 4);
OSMetaClassDefineReservedUnused(EMUUSBAudioPlugin, 5);
OSMetaClassDefineReservedUnused(EMUUSBAudioPlugin, 6);
OSMetaClassDefineReservedUnused(EMUUSBAudioPlugin, 7);
OSMetaClassDefineReservedUnused(EMUUSBAudioPlugin, 8);
OSMetaClassDefineReservedUnused(EMUUSBAudioPlugin, 9);
OSMetaClassDefineReservedUnused(EMUUSBAudioPlugin, 10);
OSMetaClassDefineReservedUnused(EMUUSBAudioPlugin, 11);
OSMetaClassDefineReservedUnused(EMUUSBAudioPlugin, 12);
OSMetaClassDefineReservedUnused(EMUUSBAudioPlugin, 13);
OSMetaClassDefineReservedUnused(EMUUSBAudioPlugin, 14);
OSMetaClassDefineReservedUnused(EMUUSBAudioPlugin, 15);

// Standard IOService.h function methods
bool EMUUSBAudioPlugin::start (IOService * provider) {
	if (!super::start (provider)) 
		return FALSE;

	mOurProvider = (EMUUSBAudioEngine *)provider;
	// Tell AppleUSBAudio that we're loaded
	mOurProvider->registerPlugin (this);

	return TRUE;
}

void EMUUSBAudioPlugin::stop (IOService * provider) {
	// Tell the system that we're not an available resource anymore
	publishResource ("EMUUSBAudioPlugin", NULL);
	super::stop (provider);
}

IOReturn EMUUSBAudioPlugin::pluginDeviceRequest (IOUSBDevRequest * request, IOUSBCompletion * completion) {
	IOReturn		result = kIOReturnError;
debugIOLog("EMUUSBAudioPlugin::pluginDeviceRequest");
	if (mOurProvider)
		result = mOurProvider->pluginDeviceRequest (request, completion);

	return result;
}

void EMUUSBAudioPlugin::pluginSetConfigurationApp (const char * bundleID) {
	if (mOurProvider) 
		mOurProvider->pluginSetConfigurationApp (bundleID);
}

// Methods that the plugin will override
IOReturn EMUUSBAudioPlugin::pluginInit (IOService * provider, UInt16 vendorID, UInt16 modelID) {
	return kIOReturnSuccess;
}

// OSMetaClassDefineReservedUsed(EMUUSBAudioPlugin, 1);
IOReturn EMUUSBAudioPlugin::pluginSetDirection (IOAudioStreamDirection direction) {
	return kIOReturnSuccess;
}

IOReturn EMUUSBAudioPlugin::pluginStart () {
	return kIOReturnSuccess;
}

IOReturn EMUUSBAudioPlugin::pluginSetFormat (const IOAudioStreamFormat * const newFormat, const IOAudioSampleRate * const newSampleRate) {
debugIOLog("EMUUSBAudioPlugin::pluginSetFormat");
	return kIOReturnSuccess;
}

IOReturn EMUUSBAudioPlugin::pluginReset () {
	return kIOReturnSuccess;
}

IOReturn EMUUSBAudioPlugin::pluginProcess (float * mixBuf, UInt32 numSampleFrames, UInt32 numChannels) {
	return kIOReturnSuccess;
}

// OSMetaClassDefineReservedUsed(EMUUSBAudioPlugin, 0);
IOReturn EMUUSBAudioPlugin::pluginProcessInput (float * destBuf, UInt32 numSampleFrames, UInt32 numChannels) {
	return kIOReturnSuccess;
}

IOReturn EMUUSBAudioPlugin::pluginStop () {
	return kIOReturnSuccess;
}
