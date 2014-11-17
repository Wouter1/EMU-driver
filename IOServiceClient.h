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
	IOServiceClient.h
	
=============================================================================*/

#ifndef __IOServiceClient_h__
#define __IOServiceClient_h__

#include <CoreServices/CoreServices.h>	// we need Debugging.h, CF, etc.
#include <IOKit/IOKitLib.h>

// _________________________________________________________________________________________
//	IOServiceClient
//
//	Does initial scans and plug-and-play notifications for one type of IOService.
//	Abstract base class.
class IOServiceClient {
public:
					IOServiceClient(CFRunLoopRef notifyRunLoop, CFMutableDictionaryRef matchingDict);
						// the run loop is the one in which publish/terminate service
						// notifications are received.
						
						// does NOT do an initial scan; use ScanServices
						// (to allow subclasses to finish construction first)
						
						// consumes references to matchingDict, which specifies
						// the type of service being subscribed to

	virtual			~IOServiceClient();
	
	void			ScanServices();
						// performs an initial (or subsequent manual) iteration of the
						// instances of the services
	
	virtual void	ServicePublished(io_service_t ios) = 0;
						// called when a new service is published
						
	virtual void	ServiceTerminated(io_service_t ios) = 0;
						// called when a service is terminated

private:
	static void		ServicePublishCallback(void *refcon, io_iterator_t it);
	static void		ServiceTerminateCallback(void *refcon, io_iterator_t it);

	void			ServicesPublished(io_iterator_t it);
	void			ServicesTerminated(io_iterator_t it);

	CFRunLoopRef			mRunLoop;
	CFRunLoopSourceRef		mRunLoopSource;
	mach_port_t				mMasterDevicePort;
	IONotificationPortRef	mNotifyPort;
	io_iterator_t			mServicePublishIterator;
	io_iterator_t			mServiceTerminateIterator;
	bool					mIteratorsNeedEmptying;
	CFDictionaryRef			mMatchingDict;
};

#endif // __IOServiceClient_h__
