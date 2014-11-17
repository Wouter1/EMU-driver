/*	Copyright: 	© Copyright 2005 Apple Computer, Inc. All rights reserved.

	Disclaimer:	IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc.
			("Apple") in consideration of your agreement to the following terms, and your
			use, installation, modification or redistribution of this Apple software
			constitutes acceptance of these terms.  If you do not agree with these terms,
			please do not use, install, modify or redistribute this Apple software.

			In consideration of your agreement to abide by the following terms, and subject
			to these terms, Apple grants you a personal, non-exclusive license, under Apple’s
			copyrights in this original Apple software (the "Apple Software"), to use,
			reproduce, modify and redistribute the Apple Software, with or without
			modifications, in source and/or binary forms; provided that if you redistribute
			the Apple Software in its entirety and without modifications, you must retain
			this notice and the following text and disclaimers in all such redistributions of
			the Apple Software.  Neither the name, trademarks, service marks or logos of
			Apple Computer, Inc. may be used to endorse or promote products derived from the
			Apple Software without specific prior written permission from Apple.  Except as
			expressly stated in this notice, no other rights or licenses, express or implied,
			are granted by Apple herein, including but not limited to any patent rights that
			may be infringed by your derivative works or by other works in which the Apple
			Software may be incorporated.

			The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO
			WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
			WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR
			PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR IN
			COMBINATION WITH YOUR PRODUCTS.

			IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR
			CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
			GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
			ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION
			OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT
			(INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN
			ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/*=============================================================================
	IOServiceClient.cpp
	
=============================================================================*/

extern "C" {
#include <mach/mach.h>
};

#include "IOServiceClient.h"
#include <unistd.h>

#if DEBUG
	#include <stdio.h>
	//#define VERBOSE 1
#endif

// _________________________________________________________________________________________
//	IOServiceClient::IOServiceClient
//
IOServiceClient::IOServiceClient(CFRunLoopRef notifyRunLoop, CFMutableDictionaryRef matchingDict) :
	mRunLoop(notifyRunLoop)
{
	mMasterDevicePort = 0;
	mNotifyPort = NULL;
	mRunLoopSource = NULL;
	mServicePublishIterator = 0;
	mServiceTerminateIterator = 0;
	mIteratorsNeedEmptying = false;
	mMatchingDict = matchingDict;

	// This gets the master device mach port through which all messages
	// to the kernel go, and initiates communication with IOKit.
	require_noerr(IOMasterPort(MACH_PORT_NULL, &mMasterDevicePort), errexit);
	
	if (mRunLoop) {
		mNotifyPort = IONotificationPortCreate(mMasterDevicePort);
		require(mNotifyPort != NULL, errexit);
		mRunLoopSource = IONotificationPortGetRunLoopSource(mNotifyPort);
		require(mRunLoopSource != NULL, errexit);
			
		CFRunLoopAddSource(mRunLoop, mRunLoopSource, kCFRunLoopDefaultMode);
			
		CFRetain(mMatchingDict);
		require_noerr(IOServiceAddMatchingNotification(mNotifyPort, kIOPublishNotification, mMatchingDict, ServicePublishCallback, this, &mServicePublishIterator), errexit);
	
		CFRetain(mMatchingDict);
		require_noerr(IOServiceAddMatchingNotification(mNotifyPort, kIOTerminatedNotification, mMatchingDict, ServiceTerminateCallback, this, &mServiceTerminateIterator), errexit);
			
		// signal that the first call to ScanServices needs to empty the publish/terminate iterators
		mIteratorsNeedEmptying = true;
	}
	
errexit:
	;
}

// _________________________________________________________________________________________
//	IOServiceClient::~IOServiceClient
//
IOServiceClient::~IOServiceClient()
{
	if (mRunLoop != NULL && mRunLoopSource != NULL)
		CFRunLoopRemoveSource(mRunLoop, mRunLoopSource, kCFRunLoopDefaultMode);

	if (mRunLoopSource != NULL)
		CFRelease(mRunLoopSource);
	
	if (mServicePublishIterator != 0)
		IOObjectRelease(mServicePublishIterator);
	
	if (mServiceTerminateIterator != 0)
		IOObjectRelease(mServiceTerminateIterator);

	//if (mNotifyPort)
	//	IOObjectRelease(mNotifyPort);	// IONotificationPortDestroy crashes if called twice!

	if (mMasterDevicePort)
		mach_port_deallocate(mach_task_self(), mMasterDevicePort);
	
	if (mMatchingDict)
		CFRelease(mMatchingDict);
}

// _________________________________________________________________________________________
//	IOServiceClient::ServicePublishCallback
//
void		IOServiceClient::ServicePublishCallback(void *refcon, io_iterator_t it)
{
	((IOServiceClient *)refcon)->ServicesPublished(it);
}

// _________________________________________________________________________________________
//	IOServiceClient::ServiceTerminateCallback
//
void		IOServiceClient::ServiceTerminateCallback(void *refcon, io_iterator_t it)
{
	((IOServiceClient *)refcon)->ServicesTerminated(it);
}

// _________________________________________________________________________________________
//	IOServiceClient::ScanServices
//
void	IOServiceClient::ScanServices()
{
	if (mMasterDevicePort == 0)
		return;

	if (mIteratorsNeedEmptying) {
		mIteratorsNeedEmptying = false;
		ServicesPublished(mServicePublishIterator);
		ServicesTerminated(mServiceTerminateIterator);
		return;
	}

	io_iterator_t iter = 0;

	CFRetain(mMatchingDict);
	require_noerr(IOServiceGetMatchingServices(mMasterDevicePort, mMatchingDict, &iter), errexit);
	ServicesPublished(iter);

errexit:
	if (iter != 0)
		IOObjectRelease(iter);
}

// _________________________________________________________________________________________
//	IOServiceClient::ServicesPublished
//
void	IOServiceClient::ServicesPublished(io_iterator_t serviceIterator)
{
	io_service_t	ioServiceObj = 0;

	usleep(100 * 1000);	// 100 ms - defense against our doing anything
						// before IOUSBFamily gets a chance to construct
						// a user client [2877002]
	while ((ioServiceObj = IOIteratorNext(serviceIterator)) != 0) {
		ServicePublished(ioServiceObj);
		IOObjectRelease(ioServiceObj);
	}
}

// _________________________________________________________________________________________
//	IOServiceClient::ServicesTerminated
//
void	IOServiceClient::ServicesTerminated(io_iterator_t serviceIterator)
{
	io_service_t	ioServiceObj = 0;

	while ((ioServiceObj = IOIteratorNext(serviceIterator)) != 0) {
		ServiceTerminated(ioServiceObj);
		IOObjectRelease(ioServiceObj);
	}
}
