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
#ifndef __EMUHALPLUGIN__
#define __EMUHALPLUGIN__


#include "EMUUSBDeviceDefines.h"
#include <CoreAudio/AudioHardware.h>
#include <IOKit/IOKitLib.h>
#if DEBUGLOGGING
#include <FireLog.h>
#endif



#include <CoreFoundation/CoreFoundation.h>
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