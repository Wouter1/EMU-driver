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
/*==================================================================================================
	CAMutex.h
	
	
	Revision 1.4  2004/10/28 01:11:58  dwyatt
	[a radar for each of our projects] CAMutex.cpp (not .h) requires errno.h
	
	Revision 1.3  2004/10/28 00:14:21  ealdrich
	Radar 3855797: Add include of errno.h to mac os builds
	
	Revision 1.2  2004/08/26 08:13:33  jcm10
	finish bring up on Windows
	
	Revision 1.1  2003/12/17 20:56:59  dwyatt
	new base class for CAGuard
	
	created Wed Dec 17 2003, Doug Wyatt
	Copyright (c) 2003 Apple Computer, Inc.  All Rights Reserved

	$NoKeywords: $
==================================================================================================*/

#ifndef __CAMutex_h__
#define __CAMutex_h__

//==================================================================================================
//	Includes
//==================================================================================================

//	System Includes
#if !defined(__COREAUDIO_USE_FLAT_INCLUDES__)
	#include <CoreAudio/CoreAudioTypes.h>
#else
	#include <CoreAudioTypes.h>
#endif

#if TARGET_OS_MAC
	#include <pthread.h>
#elif TARGET_OS_WIN32
	#include <windows.h>
#else
	#error	Unsupported operating system
#endif

//==================================================================================================
//	A recursive mutex.
//==================================================================================================

class	CAMutex
{
//	Construction/Destruction
public:
					CAMutex(const char* inName);
	virtual			~CAMutex();

//	Actions
public:
	virtual bool	Lock();
	virtual void	Unlock();
	virtual bool	Try(bool& outWasLocked);	// returns true if lock is free, false if not
	
	virtual bool	IsFree() const;
	virtual bool	IsOwnedByCurrentThread() const;
		
//	Implementation
protected:
	const char*		mName;
#if TARGET_OS_MAC
	pthread_t		mOwner;
	pthread_mutex_t	mMutex;
#elif TARGET_OS_WIN32
	UInt32			mOwner;
	HANDLE			mMutex;
#endif

//	Helper class to manage taking and releasing recursively
public:
	class			Locker
	{
	
	//	Construction/Destruction
	public:
					Locker(CAMutex& inMutex) : mMutex(inMutex), mNeedsRelease(false) { mNeedsRelease = mMutex.Lock(); }
					~Locker() { if(mNeedsRelease) { mMutex.Unlock(); } }
	
	private:
					Locker(const Locker&);
		Locker&		operator=(const Locker&);
	
	//	Implementation
	private:
		CAMutex&	mMutex;
		bool		mNeedsRelease;
	
	};
};


#endif // __CAMutex_h__
