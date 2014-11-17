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
//
//	File:		AppleUSBAudioStream.h
//
//	Contains:	Support for the USB Audio Class Stream Interface.
//
//	Technology:	OS X
//
//--------------------------------------------------------------------------------

#ifndef _EMUUSBAUDIOCOMMON_H
#define _EMUUSBAUDIOCOMMON_H

#define LOCKING	1
#define CONTIGUOUS 1
#ifdef DEBUGLOGGING
#define CONSOLELOGGING 1
#define DEBUG_LEVEL 1
#endif
#define	CUSTOMDEVICE	1
#define PREPINPUT	0
#define UHCISUPPORT 0
#include <libkern/OSTypes.h>
#if DEBUGLOGGING
#include <IOKit/usb/IOUSBLog.h>
#include <IOKit/firewire/IOFireLog.h>
#endif
enum {
	kCtrlUsage      = 'Xtrl',
	kEMUXUControl	= 'XemU'
};

#ifdef __ppc__
		typedef AbsoluteTime EmuTime;
		#define EmuAbsoluteTime
		#define EmuAbsoluteTimePtr
#else
		typedef uint64_t EmuTime;
		#define EmuAbsoluteTime(x) (*(uint64_t *)&x)
		#define EmuAbsoluteTimePtr(x) ((uint64_t *)(x))
#endif

#define RELEASEOBJ(obj) if (obj) {obj->release(); obj = NULL;}
#define kMaxTryCount	3
#define kEMURefreshRate	32
// time related macros taken from 10.3.9
#define AbsoluteTime_to_scalar(x)	(*(uint64_t *)(x))
#define CMP_ABSOLUTETIME(t1, t2)				\
	(AbsoluteTime_to_scalar(t1) >				\
		AbsoluteTime_to_scalar(t2)? (int)+1 :	\
	 (AbsoluteTime_to_scalar(t1) <				\
		AbsoluteTime_to_scalar(t2)? (int)-1 : 0))
#define CMP_TIMES(t1, t2) \
	((t1 > t2) ? (int) +1: ((t1<t2) ? (int)-1:0))
/* t1 += t2 */
#define ADD_ABSOLUTETIME(t1, t2)				\
	(AbsoluteTime_to_scalar(t1) +=				\
				AbsoluteTime_to_scalar(t2))

/* t1 -= t2 */
#define SUB_ABSOLUTETIME(t1, t2)				\
	(AbsoluteTime_to_scalar(t1) -=				\
				AbsoluteTime_to_scalar(t2))

//	-----------------------------------------------------------------
#define SoundAssertionMessage( cond, file, line, handler ) \
    "Sound assertion \"" #cond "\" failed in " #file " at line " #line " goto " #handler ""

#define SoundAssertionFailed( cond, file, line, handler ) \
	{debugIOLog( SoundAssertionMessage( cond, file, line, handler )); IOSleep(20);};

//	-----------------------------------------------------------------
#define	FailIf( cond, handler )										\
    if( cond ){														\
        SoundAssertionFailed( cond, __FILE__, __LINE__, handler )	\
        goto handler; 												\
    }

//	-----------------------------------------------------------------
#define	FailWithAction( cond, action, handler )						\
    if( cond ){														\
        SoundAssertionFailed( cond, __FILE__, __LINE__, handler )	\
            { action; }												\
        goto handler; 												\
    }

//	-----------------------------------------------------------------
#define FailMessage(cond, handler)									\
	if (cond) {														\
		SoundAssertionFailed(cond, __FILE__, __LINE__, handler)		\
		goto handler;												\
	}

//  -----------------------------------------------------------------
//
// System Logging or USB Prober Logging
//
//#define sleepTime 20
#define sleepTime 0
#if 1
	#ifdef DEBUGLOGGING /* { */
		#ifdef CONSOLELOGGING /* { */
			#define debugIOLog( message... ) \
				do {FireLog("%.0s" #message "\n", message); IOSleep(sleepTime);} while (0)
		#elif defined (DEBUGLOG_TO_KPRINTF) /* }{ */
				#define debugIOLog( message... ) \
					do { kprintf ( message ); kprintf ( "\n" ); } while (0)
		#else /* }{ */
		#define debugIOLog( message... ) ;
		#endif /* } */
	#else /* }{ */
		#define debugIOLog( message... ) ;
	#endif /* } */
#else
	#define debugIOLog(message...) ;
#endif

#if 0
	#ifdef DEBUGLOGGING /* { */
		#ifdef CONSOLELOGGING /* { */
			#define debugIOLog2( message... ) \
				do {FireLog("%.0s" #message "\n", message); IOSleep(sleepTime);} while (0)
		#elif defined (DEBUGLOG_TO_KPRINTF) /* }{ */
				#define debugIOLog2( message... ) \
					do { kprintf ( message ); kprintf ( "\n" ); } while (0)
		#else /* }{ */
		#define debugIOLog2( message... ) ;
		#endif /* } */
	#else /* }{ */
		#define debugIOLog2( message... ) ;
	#endif /* } */
#else
	#define debugIOLog2(message...) ;
#endif
#endif /* _EMUUSBAUDIOCOMMON_H */
