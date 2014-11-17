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
#include <CoreFoundation/CoreFoundation.h>

#include "EMUUSBLogging.h"


//// Copy of EMUUSBLogging, but with a different doLog as we are in user space here. 
//
//// the log-always function. NOT for DEBUG messages. goes to system.log (check the console)
//#define doLog( message... ) NSLog(message);
//
//// you may consider using this do { printf ( message ); printf ("\n" ); IOSleep(sleepTime); } while (0)
//
//
//// general debug logging stream. If possible use more focused log
//// this maps to no-action if DEBUGLOGGING is not set.
//
//#ifdef DEBUGLOGGING /* { */
//#define debugIOLog( message... ) doLog(message);
//#else
//#define debugIOLog( message... ) ;
//#endif
//
//
//// Just set each messagetype as you need if you are debugging.
//// map to debugIOLog() as we can then easily turn off all debugging.
//// only messages that ALWAYS must print do use doLog.
//
//#define debugIOLogPC(message...); // debugIOLog(message);
//


void ReleasePluginInfo(AudioDeviceID deviceID);
io_object_t GetDeviceForDeviceID(AudioDeviceID deviceID);
#endif // __EMUHALPLUGIN__