//
//  EMUHALPlugin.h
//  EMUUSBAudio
//
//  Created by Wouter Pasman on 06/10/14.
//  Copyright (c) 2014 com.emu. All rights reserved.
//

#ifndef __EMUUSBAudio__EMUHALPlugin__
#define __EMUUSBAudio__EMUHALPlugin__


#include "EMUUSBDeviceDefines.h"

#include <IOKit/IOTypes.h>
#include <IOKit/IOLib.h>

#include "CoreAudio/CoreAudio.h"
#include <CoreAudio/AudioHardware.h>
//#if DEBUGLOGGING
//#include <FireLog.h>
//#endif



//#include <CoreFoundation/CoreFoundation.h>
#define DEBUG_PRINT	1
#define DIRECTMONITOR 0	// currently no direct monitor

//stuff lifted from EMUUSBAudioCommon.h
#ifdef DEBUGLOGGING
#define DEBUGLOG_TO_KPRINTF 1
//#define CONSOLELOGGING
#define DEBUG_LEVEL 1
#endif

#ifdef DEBUGLOGGING /* { */
#ifdef CONSOLELOGGING /* { */
#define debugIOLog( message... ) \
do {FireLog("%.0s" #message "\n", message); } while (0)
#elif defined (DEBUGLOG_TO_KPRINTF) /* }{ */
#define debugIOLog( message... ) \
do { printf ("HALPlugin: %.0s" #message "\n", message ); } while (0)
#else /* }{ */
#define debugIOLog( message... ) ;
#endif /* } */
#else /* }{ */
#define debugIOLog( message... ) ;
#endif /* } */

void ReleasePluginInfo(AudioDeviceID deviceID);
io_object_t GetDeviceForDeviceID(AudioDeviceID deviceID);
#endif // __EMUHALPLUGIN__

#endif /* defined(__EMUUSBAudio__EMUHALPlugin__) */
