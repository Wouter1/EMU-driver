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
//	File:		EMUHALPlugin.cpp
//	Contains:	HAL Plugin code for the custom properties exposed through CoreAudio
//	Written by:	David Tay 2006.
//--------------------------------------------------------------------------------
#include "EMUHALPlugin.h"
//#include <stdio.h>

#include <CoreAudio/AudioDriverPlugIn.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFString.h>



const CFStringRef	kControlValueRef		= CFSTR("IOAudioControlValue");
enum {
	kCtrlUsage	='Xtrl'
};

// clock-source hack stuff (AC)
#define NUM_CLOCK_SOURCES	2

/*
typedef struct {
	io_object_t			engine;
	io_object_t			device;
	UInt32				deviceID;
} PluginInfo, *PluginInfoPtr;
*/

typedef AudioDriverPlugInHostInfo	PluginInfo;
typedef AudioDriverPlugInHostInfo	*PluginInfoPtr;

struct CallbackInfo {
	PluginInfoPtr		pluginInfo;
	io_connect_t		service;
};


static PluginInfoPtr AudioDriverPlugInGetPlugInInfoForDeviceID(AudioDeviceID inDeviceID);
static	CFMutableArrayRef	gPluginInfoArray = NULL;

static io_connect_t		gEMUDigIOSampleRateController = IO_OBJECT_NULL;
static io_connect_t		gEMUDigIOSyncSrcController = IO_OBJECT_NULL;
static io_connect_t		gEMUDigIOAsyncSrcController = IO_OBJECT_NULL;
static io_connect_t		gEMUDigIOSPDIFController = IO_OBJECT_NULL;
static io_connect_t		gEMUDevSoftLimitController = IO_OBJECT_NULL;
static io_connect_t		gEMUClockSourceController = IO_OBJECT_NULL;
static io_connect_t		gEMUXUNotifier = IO_OBJECT_NULL;
#if DIRECTMONITOR
static io_connect_t		gEMUDirectMonMonoStereoController;
static io_connect_t		gEMUDirectMonOnOffController;
#endif

/***************/
// callback function

static void HALPluginCallback(CFMachPortRef, void *msg, CFIndex size, void *info) {
	debugIOLog("+HALPluginCallback");
	CallbackInfo *callbackInfo = (CallbackInfo *) info;
	AudioDevicePropertyID propertyID = 0;
	if (!callbackInfo || !callbackInfo->pluginInfo) {
		debugIOLog("Bad Plugin Info");
		goto done;
	}
	if (callbackInfo->service == gEMUDigIOSyncSrcController) {
		debugIOLog("kDigIOSyncSrcController");
		propertyID = kDigIOSyncSrcController;
	} else if (callbackInfo->service == gEMUClockSourceController) {
		debugIOLog("kClockSourceController");
		propertyID = kClockSourceController;
	} else if (callbackInfo->service == gEMUDigIOSampleRateController) {
		debugIOLog("kDigIOSampleRateController");	
		propertyID = kDigIOSampleRateController;
	} else {
		debugIOLog("unrecognized service");
		goto done;
	}
	callbackInfo->pluginInfo->mDevicePropertyChangedProc(callbackInfo->pluginInfo->mDeviceID,
														 0,  // channel doesn't matter
														 false, // input/output doesn't matter
														 propertyID);
done:
	debugIOLog("-HALPluginCallback");
}


static void RegisterCallback (io_connect_t  service, PluginInfoPtr info) {
	debugIOLog("+RegisterCallback");
	IONotificationPortRef portRef = IONotificationPortCreate(kIOMasterPortDefault);
	if (portRef) {
		debugIOLog("Created notification port");
		mach_port_t port = IONotificationPortGetMachPort(portRef);
		if (!port) {
			debugIOLog("Could not get mach port!");
		} else {
			io_connect_t connection = 0;
			kern_return_t result = IOServiceOpen(service,(task *)mach_task_self(),0,&connection);
			if (connection) {
				debugIOLog("service opened");
				result = IOConnectSetNotificationPort(connection,
																	0, //unused per IOAudioControlUserClient
																	port,
																	0);
				if (KERN_SUCCESS == result) {
					debugIOLog("SetNotificationPort successful");
					CallbackInfo *callbackInfo = new CallbackInfo;
					callbackInfo->pluginInfo = info;
					callbackInfo->service = service;
					CFMachPortContext context;
					context.version = 0;
					context.info = callbackInfo;
					context.retain = NULL;
					context.release = NULL;
					context.copyDescription = NULL;
					
					CFMachPortRef cfPort = CFMachPortCreateWithPort(NULL,port,HALPluginCallback,&context,NULL);
					if (cfPort) {
						debugIOLog("Created CFMachPort w/ callback info");
						CFRunLoopSourceRef runLoopSource = CFMachPortCreateRunLoopSource(NULL,cfPort,0);
						if (runLoopSource) {
							CFRunLoopAddSource(CFRunLoopGetCurrent(),runLoopSource,kCFRunLoopDefaultMode);
							debugIOLog("Created run loop source");
						}
					} else {
						debugIOLog("Could not create CFMachPort");
					}
				} else {
					debugIOLog("SetNotificationPort failed with error %x",err_get_code(result));
				}				
			} else {
				debugIOLog("could not open service");
			}
		}
	}
	debugIOLog("-RegisterCallback");
}

/***************/

static void CreateControlEntries(PluginInfoPtr pluginInfo) {
    
	OSStatus		result = noErr;
    io_iterator_t	iterator;
    kern_return_t	error;
    unsigned int	size;
	
	// Create iterator
	error = IORegistryEntryCreateIterator(pluginInfo->mIOAudioDevice,	// The root entry
														kIOServicePlane,		// IOService plane
														kIORegistryIterateRecursively, 
														&iterator );

#if DEBUG_PRINT
	debugIOLog("attempting to create the iterator %x\n", error);
#endif
	// Get the good control entry.
	if( KERN_SUCCESS == error ) {
		io_connect_t		connectRef = IOIteratorNext(iterator);
#if DEBUG_PRINT
		debugIOLog("connectRef is %x\n", connectRef);
#endif
		while (connectRef) {

			// ***********************************************************************
			// criteria we use to find the appropriate controls
			// if(IOObjectConformsTo(gControlReference, "IOAudioLevelControl"))
			// ***********************************************************************
			if (IOObjectConformsTo(connectRef, "EMUXUCustomControl")) {
				AudioDevicePropertyID	LcPropertyID;
				size = sizeof(LcPropertyID);
				result = IORegistryEntryGetProperty(connectRef, "IOAudioControlID", (char *)&LcPropertyID,
														&size );
#if DEBUG_PRINT
				debugIOLog("getting IOAudioControlID result %x propertyID %x\n", result, LcPropertyID);
#endif
				switch (LcPropertyID) {
					case kXUChangeNotifier:
#if DEBUG_PRINT

					debugIOLog("XUChangeNotifier connected\n");
#endif
						gEMUXUNotifier = connectRef;
						RegisterCallback(gEMUXUNotifier,pluginInfo);
						break;
					case kDigIOSampleRateController:
					debugIOLog("DigIOSampleRateController\n");
						gEMUDigIOSampleRateController = connectRef;
						RegisterCallback(gEMUDigIOSampleRateController,pluginInfo);
						break;
					case kDigIOSyncSrcController:
#if DEBUG_PRINT
					debugIOLog("kDigIOSyncSrcController\n");
#endif
						gEMUDigIOSyncSrcController = connectRef;
						RegisterCallback(gEMUDigIOSyncSrcController,pluginInfo);
						break;
					case kDigIOAsyncSrcController:
#if DEBUG_PRINT
					debugIOLog("kDigIOAsyncSrcController\n");
#endif
					gEMUDigIOAsyncSrcController = connectRef;
						break;
					case kDigIOSPDIFController:
#if DEBUG_PRINT
					debugIOLog("kDigIOSPDIFController\n");
#endif
						gEMUDigIOSPDIFController = connectRef;
						break;
					case kDevSoftLimitController:
						gEMUDevSoftLimitController = connectRef;
#if DEBUG_PRINT
						debugIOLog("kDevSoftLimitController connection %d\n", gEMUDevSoftLimitController);
#endif
						break;
					case kClockSourceController:
					debugIOLog("kClockSourceController\n");
						gEMUClockSourceController = connectRef;
						RegisterCallback(gEMUClockSourceController,pluginInfo);
						break;
#if DIRECTMONITOR
					case kDirectMonMonoStereoController:
						gEMUDirectMonMonoStereoController = connectRef;
						break;
					case kDirectMonOnOffController:
						gEMUDirectMonOnOffController = connectRef;
						break;
#endif
				}
			}

			// Next control
			connectRef = IOIteratorNext(iterator);

		} 
	}

	// Release iterator
	if(iterator)
		IOObjectRelease(iterator);
}


#pragma mark -
#pragma mark ***** PLUGIN FUNCTIONS *****
// ****************************************************************************
// AudioDriverPlugInOpen
// ****************************************************************************
OSStatus AudioDriverPlugInOpen( AudioDriverPlugInHostInfo* inHostInfo)
{
    OSStatus		result = noErr;

#if DEBUG_PRINT
	freopen("/dev/console","a",stdout);
    debugIOLog("%s() : **************************** \n", __FUNCTION__ );
    debugIOLog("%s() : HAL Plugin OPEN \n", __FUNCTION__ );
    debugIOLog("%s() : **************************** \n", __FUNCTION__ );
    //fflush(stdout);
#endif
	if (NULL == gPluginInfoArray) 
		gPluginInfoArray = CFArrayCreateMutable(NULL, 0, NULL);
	
	if (gPluginInfoArray) {
		PluginInfoPtr			pluginInfo = (PluginInfoPtr) malloc(sizeof(PluginInfo));
		
		if (pluginInfo) {
			pluginInfo->mIOAudioEngine = inHostInfo->mIOAudioEngine;
			pluginInfo->mIOAudioDevice = inHostInfo->mIOAudioDevice;
			pluginInfo->mDeviceID = inHostInfo->mDeviceID;
			pluginInfo->mDevicePropertyChangedProc = inHostInfo->mDevicePropertyChangedProc;
			pluginInfo->mStreamPropertyChangedProc = inHostInfo->mStreamPropertyChangedProc;
			
			CFArrayAppendValue(gPluginInfoArray, pluginInfo);
			
			CreateControlEntries(pluginInfo);
			
		}
		
		debugIOLog("mIOAudioEngine = %p",inHostInfo->mIOAudioEngine);
		debugIOLog("mIOAudioDevice = %p",inHostInfo->mIOAudioDevice);
	}	
    return result;
}

// AudioDriverPlugInClose
OSStatus AudioDriverPlugInClose(AudioDeviceID inDeviceID){
debugIOLog("HAL Plugin close\n");
	if (gEMUXUNotifier)
		IOServiceClose(gEMUXUNotifier);
	if (gEMUDigIOSampleRateController)
		IOServiceClose(gEMUDigIOSampleRateController);
	if (gEMUDigIOSyncSrcController)
		IOServiceClose(gEMUDigIOSyncSrcController);
	if (gEMUDigIOAsyncSrcController)
		IOServiceClose(gEMUDigIOAsyncSrcController);
	if (gEMUDigIOSPDIFController)
		IOServiceClose(gEMUDigIOSPDIFController);
	if (gEMUDevSoftLimitController)
		IOServiceClose(gEMUDevSoftLimitController);
	if (gEMUClockSourceController)
		IOServiceClose(gEMUClockSourceController);
#if DIRECTMONITOR
	if (gEMUDirectMonMonoStereoController)
		IOServiceClose(gEMUDirectMonMonoStereoController);
	if (gEMUDirectMonOnOffController)
		IOServiceClose(gEMUDirectMonOnOffController);
#endif
	ReleasePluginInfo(inDeviceID);
	if (gPluginInfoArray) {
		CFIndex	count = CFArrayGetCount(gPluginInfoArray);
		if (0 == count) {
			CFRelease(gPluginInfoArray);
			gPluginInfoArray = NULL;
		}
	}
	
    return noErr;
}

void ReleasePluginInfo(AudioDeviceID deviceID) {
	if (gPluginInfoArray) {
		CFIndex	count = CFArrayGetCount(gPluginInfoArray);
		for (CFIndex i = 0; i < count; ++i) {// first remove the entry
			PluginInfoPtr	pluginInfo = (PluginInfoPtr) CFArrayGetValueAtIndex(gPluginInfoArray, i);
			if (pluginInfo && deviceID == pluginInfo->mDeviceID) {
				debugIOLog("releasing entry\n");
				CFArrayRemoveValueAtIndex(gPluginInfoArray, i);
				free(pluginInfo);
			}
		}
		CFRelease(gPluginInfoArray);// remove the array
		gPluginInfoArray = NULL;
	}
}

PluginInfoPtr AudioDriverPlugInGetPlugInInfoForDeviceID(AudioDeviceID inDeviceID) {
	PluginInfoPtr	pluginInfo = NULL;
	if (NULL != gPluginInfoArray) {
		CFIndex	count = CFArrayGetCount(gPluginInfoArray);
		for (CFIndex index = 0; index < count; ++index) {
			pluginInfo = (PluginInfoPtr) CFArrayGetValueAtIndex(gPluginInfoArray, index);
			if (pluginInfo && inDeviceID == pluginInfo->mDeviceID)
				return pluginInfo;
		}
	}
	return pluginInfo;
}

io_object_t GetDeviceForDeviceID(AudioDeviceID inDeviceID) {
	io_object_t	device = IO_OBJECT_NULL;
	PluginInfoPtr	pluginInfo = AudioDriverPlugInGetPlugInInfoForDeviceID(inDeviceID);
	if (pluginInfo)
		device = pluginInfo->mIOAudioDevice;
	return device;
}
#pragma mark -
#pragma mark *********** DEVICE PROPERTY FUNCTIONS ***********

// AudioDriverPlugInDeviceGetPropertyInfo
OSStatus AudioDriverPlugInDeviceGetPropertyInfo(AudioDeviceID inDeviceID, UInt32 inLine,
								Boolean isInput, AudioDevicePropertyID inPropertyID,
								UInt32 * outSize, Boolean * outWritable) {
    OSStatus	result = kAudioHardwareUnknownPropertyError;
#if DEBUG_PRINT
	debugIOLog("+GetPropertyInfo \"%c%c%c%c\" \n", inPropertyID>>24, inPropertyID>>16, inPropertyID>>8, inPropertyID);
#endif	
    switch (inPropertyID) {
		case kDigIOSyncSrcController:
		case kDigIOAsyncSrcController:
		case kDigIOSPDIFController:
		case kDevSoftLimitController:
		case kClockSourceController:
		case kDigIOSampleRateController:
#if DIRECTMONITOR
		case kDirectMonMonoStereoController:
		case kDirectMonOnOffController:
#endif
            result = noErr;
            if( outSize) 
				*outSize = sizeof(UInt32);
            if( outWritable ) 
				*outWritable = true;
			break;
    }
	//debugIOLog("-GetPropertyInfo %d",result);
    return result;
}

// ****************************************************************************
// AudioDriverPlugInDeviceGetProperty
// ****************************************************************************
OSStatus AudioDriverPlugInDeviceGetProperty( AudioDeviceID	PmDevice,
                                    UInt32	PmLine, Boolean	PmIsInput,
                                    AudioDevicePropertyID	PmPropertyID,
                                    UInt32*	PmPropertyDataSize, void*	PmPropertyData) {
    OSStatus	result = kAudioHardwareUnknownPropertyError;
    SInt32		LcSInt32Value;
    UInt32		* LcUInt32Ptr = (UInt32 *)PmPropertyData;
	CFTypeRef   valueAsCFTypeRef = NULL;
	io_object_t	device = GetDeviceForDeviceID(PmDevice);
	
	
	PmPropertyDataSize = 0L;
	
#if DEBUG_PRINT
	debugIOLog("PmPropertyID \"%c%c%c%c\" device %x\n", PmPropertyID>>24, PmPropertyID>>16, PmPropertyID>>8, PmPropertyID,device);
#endif
	switch (PmPropertyID) {
		case kXUChangeNotifier:
			result = noErr;
#if DEBUG_PRINT
		debugIOLog("kXUChangeNotifier\n");
#endif
			valueAsCFTypeRef = IORegistryEntryCreateCFProperty(gEMUXUNotifier, kControlValueRef, kCFAllocatorDefault, 0);
			break;
		case kDigIOSampleRateController:
			result = noErr;            
#if DEBUG_PRINT
		debugIOLog("kDigIOSampleRateController\n");
#endif
			valueAsCFTypeRef = IORegistryEntryCreateCFProperty(
									gEMUDigIOSampleRateController, kControlValueRef, kCFAllocatorDefault, 0);
			break;

		case kDigIOSyncSrcController:
			result = noErr;
#if DEBUG_PRINT
		debugIOLog("kDigIOSyncSrcController\n");
#endif
			valueAsCFTypeRef = IORegistryEntryCreateCFProperty (
									gEMUDigIOSyncSrcController, kControlValueRef, kCFAllocatorDefault, 0);
			break;

		case kDigIOAsyncSrcController:
			result = noErr;
#if DEBUG_PRINT
		debugIOLog("kDigIOAsyncSrcController\n");
#endif
			valueAsCFTypeRef = IORegistryEntryCreateCFProperty (
									gEMUDigIOAsyncSrcController, kControlValueRef, kCFAllocatorDefault, 0);
			break;
		case kDigIOSPDIFController:
			result = noErr;
#if DEBUG_PRINT
		debugIOLog("kDigIOSPDIFController\n");
#endif
			valueAsCFTypeRef = IORegistryEntryCreateCFProperty (
									gEMUDigIOSPDIFController, kControlValueRef, kCFAllocatorDefault, 0);
			break;
		case kDevSoftLimitController:
			result = noErr;
#if DEBUG_PRINT
		debugIOLog("kDevSoftLimitController\n");
#endif
			valueAsCFTypeRef = IORegistryEntryCreateCFProperty (
									gEMUDevSoftLimitController, kControlValueRef, kCFAllocatorDefault, 0);
			break;
	
		case kClockSourceController:
		case kAudioDevicePropertyClockSource:
			result = noErr;
#if DEBUG_PRINT
		debugIOLog("kClockSourceController\n");
#endif
			valueAsCFTypeRef = IORegistryEntryCreateCFProperty (
									gEMUClockSourceController, kControlValueRef, kCFAllocatorDefault, 0);
			break;
#if DIRECTMONITOR
		case kDirectMonMonoStereoController:
			result = noErr;
			valueAsCFTypeRef = IORegistryEntryCreateCFProperty(
									gEMUDirectMonMonoStereoController, kControlValueRef, kCFAllocatorDefault, 0);
			break;
		case kDirectMonOnOffController:
			result = noErr;
			valueAsCFTypeRef = IORegistryEntryCreateCFProperty (
									gEMUDirectMonOnOffController, kControlValueRef, kCFAllocatorDefault, 0);			
			break;
#endif
		//MOTU digital performer hack (I really don't like this, but not sure what else to do) [AC]
		case 'Mhta':
			result = noErr;
			break;
		case kAudioDevicePropertyClockSourceNameForIDCFString:
			debugIOLog("handling 'lcsn' (ClockSource name) message");
			{				
				AudioValueTranslation *avtrans = (AudioValueTranslation *) PmPropertyData;
				UInt32 clockID = *((UInt32 *) avtrans->mInputData);
				debugIOLog("clockID = %d, isInput=%d",clockID,PmIsInput);
			}
			break;
			/*
		case kAudioDevicePropertyClockSource:
			debugIOLog("handling 'csrc' (ClockSource) message");
			{				
				AudioValueTranslation *avtrans = (AudioValueTranslation *) PmPropertyData;
				//UInt32 clockID = *((UInt32 *) avtrans->mInputData);
				//debugIOLog("clockID = %d, isInput=%d",clockID,PmIsInput);
			}
			break;		
			*/
		case kAudioDevicePropertyChannelNumberName:
		case kAudioObjectPropertyElementCategoryName:
			debugIOLog("handling 'lccn' (element/channel category name) message");
			{
				debugIOLog("channel# = %d, isInput=%d",PmLine,PmIsInput);
			}
			break;
		default:
			break;
		}
	if (valueAsCFTypeRef) {
	debugIOLog("getting value\n");
		CFNumberGetValue((CFNumberRef) valueAsCFTypeRef,
						CFNumberGetType((const CFNumberRef) valueAsCFTypeRef), &LcSInt32Value);
		CFRelease(valueAsCFTypeRef);
		if (PmPropertyDataSize)
			*PmPropertyDataSize = sizeof(LcSInt32Value);
		if (PmPropertyData)
			*LcUInt32Ptr = LcSInt32Value;
	}
	
     return result;
}                                            

// ****************************************************************************
// AudioDriverPlugInDeviceSetProperty
// ****************************************************************************
OSStatus AudioDriverPlugInDeviceSetProperty( AudioDeviceID	inDevice, const AudioTimeStamp* inWhen,
                                        UInt32	inLine, Boolean	isInput, AudioDevicePropertyID	inPropertyID,
                                        UInt32	inPropertyDataSize, const void*	inPropertyData) {
    OSStatus	result = kAudioHardwareUnknownPropertyError;
	debugIOLog("+AudioDriverPluginDeviceSetProperty");
    SInt32		LcSInt32  = *(SInt32*) inPropertyData;
    CFNumberRef		LcCFNumberRef = CFNumberCreate(CFAllocatorGetDefault(), kCFNumberSInt32Type, &LcSInt32);
	if (LcCFNumberRef) {
		switch (inPropertyID) {
			case kDigIOSyncSrcController:
					result = IORegistryEntrySetCFProperty(gEMUDigIOSyncSrcController, kControlValueRef, LcCFNumberRef );
#if DEBUG_PRINT
					debugIOLog("kDigIOSyncSrcController %d", result);
#endif
					break;
			case kDigIOAsyncSrcController:
					result = IORegistryEntrySetCFProperty(gEMUDigIOAsyncSrcController, kControlValueRef, LcCFNumberRef );
#if DEBUG_PRINT
					debugIOLog("kDigIOAsyncSrcController %d", result);
#endif
					break;
			case kDigIOSPDIFController:
					result = IORegistryEntrySetCFProperty(gEMUDigIOSPDIFController, kControlValueRef, LcCFNumberRef );
#if DEBUG_PRINT
					debugIOLog("kDigIOSPDIFController %d", result);
#endif
					CFRelease(LcCFNumberRef);
					break;
			case kDevSoftLimitController:
					result = IORegistryEntrySetCFProperty(gEMUDevSoftLimitController, kControlValueRef, LcCFNumberRef );
		#if DEBUG_PRINT
					debugIOLog("kDevSoftLimitController controller is %d result is %x\n", gEMUDevSoftLimitController, result);
		#endif
					break;
			case kClockSourceController:
					debugIOLog("kClockSourceController %d (%d)",LcSInt32, result);
					result = IORegistryEntrySetCFProperty(gEMUClockSourceController, kControlValueRef, LcCFNumberRef );
					break;
		#if DIRECTMONITOR
			case kDirectMonMonoStereoController:
					result = IORegistryEntrySetCFProperty(gEMUDirectMonMonoStereoController, kControlValueRef, LcCFNumberRef );
					break;
			case kDirectMonOnOffController:
					result = IORegistryEntrySetCFProperty(gEMUDirectMonOnOffController, kControlValueRef, LcCFNumberRef );
					break;
		#endif
		}	
		CFRelease(LcCFNumberRef);
     }
	debugIOLog("-AudioDriverPluginDeviceSetProperty");

     return result;
}

#pragma mark -
#pragma mark *********** STREAM PROPERTY FUNCTIONS ***********
// AudioDriverPlugInStreamGetPropertyInfo
OSStatus AudioDriverPlugInStreamGetPropertyInfo(AudioDeviceID PmDevice, io_object_t PmIOAudioStream,
									UInt32 PmChannel, AudioDevicePropertyID	PmPropertyID,
									UInt32*	PmSize, Boolean* PmWritable) {
    return kAudioHardwareUnknownPropertyError;
}

// AudioDriverPlugInStreamGetProperty
OSStatus AudioDriverPlugInStreamGetProperty(AudioDeviceID PmDevice, io_object_t PmIOAudioStream,
								UInt32 PmChannel, AudioDevicePropertyID PmPropertyID,
								UInt32*	PmPropertyDataSize, void* PmPropertyData) {
    return kAudioHardwareUnknownPropertyError;
}

// AudioDriverPlugInStreamSetProperty
OSStatus AudioDriverPlugInStreamSetProperty(AudioDeviceID PmDevice, io_object_t PmIOAudioStream,
								const AudioTimeStamp* PmWhen, UInt32 PmChannel,
								AudioDevicePropertyID PmPropertyID, UInt32 PmPropertyDataSize,
								const void*	PmPropertyData) {
    return kAudioHardwareUnknownPropertyError;
}

