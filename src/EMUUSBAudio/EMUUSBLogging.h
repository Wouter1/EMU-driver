//
//  EMUUSBLogging.h
//  EMUUSBAudio
//
//  Created by Wouter Pasman on 14/10/14.
/*! Central logging settings for the driver.
 You can enable/disable logging for entire sections of the code here. See
 debugIOLogPC, debugIOLogAS etc below. debugIOLog itself is also still used in many places.
 */

#ifndef EMUUSBAudio_EMUUSBLogging_h
#define EMUUSBAudio_EMUUSBLogging_h

#include <IOKit/IOLib.h>


//  -----------------------------------------------------------------
//
// System Logging. Central manager as it got hard to find out all debug log places
// and make sure they were all in the right state.

// some other stuff in the code that can be turned on/off for debugging

#define DEBUGZEROTIME			TRUE
#define DEBUGLOADING			TRUE
#define DEBUGTIMESTAMPS			TRUE
#define DEBUGLOGGING          TRUE


//#include <IOKit/usb/IOUSBLog.h>
//  sleeptime for debug logging. Set between 0 and 20 and call IOSleep in doLog
#define sleepTime 1

// the log-always function. NOT for DEBUG messages. goes to system.log (check the console)
// the debugIOLog will be mapped into this when debugging is enabled.
// WARNING Logging is broken since Sierra, because OSX logging randomly misses log events.
#define doLog( message... ) \
do { printf ( message ); } while (0)

// you may consider using this do { printf ( message ); printf ("\n" ); IOSleep(sleepTime); } while (0)


// general debug logging stream. If possible use more focused log
// this maps to no-action if DEBUGLOGGING is not set.

#ifdef DEBUGLOGGING /* { */
#define debugIOLog( message... ) doLog(message);
#else
#define debugIOLog( message... ) ;
#endif


// Just set each messagetype as you need if you are debugging.
// map to debugIOLog() as we can then easily turn off all debugging.
// only messages that ALWAYS must print do use doLog.

// parse configuration descriptor log messages.define as debugIOLog(message) to turn on.
#define debugIOLogPC(message...) //debugIOLog(message);

// audio streaming interface log messages. define as debugIOLog(message) to turn on.
#define debugIOLogAS(message...) //debugIOLog(message);

// DOC
#define debugIOLog2(message...) //debugIOLog(message);

// framelist write debug messages
#define debugIOLogW(message...) //debugIOLog(message);

// debug time stamps.
#define debugIOLogT(message...) //debugIOLog(message);

// debug timing details for each handler call (large number of messages)
#define debugIOLogTD(message...) //debugIOLog(message);


// debug USB speed estimator and jitter filter timestamps.
#define debugIOLogTT(message...)  //debugIOLog(message);

// debug USB read messages
#define debugIOLogR(message...) //debugIOLog(message);

// debug USB read messages details
#define debugIOLogRD(message...) //debugIOLog(message);

// debug all control-level messages (setup, initialization, takedown).
#define debugIOLogC(message...) //debugIOLog(message);
#endif
