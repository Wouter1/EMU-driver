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
	MIDIDriverUtils.h
	
=============================================================================*/

#ifndef __MIDIDriverBase_h__
#define __MIDIDriverBase_h__

#include <list>
#include <pthread.h>
#include <CoreMIDI/CoreMIDI.h>
#include "CAHostTimeBase.h"
//#include "RealtimeAllocator.h"

// _________________________________________________________________________________________
// QueuedMIDIPacket
// 
//	A form of a MIDIPacket suitable for storing in an I/O queue.
//	Aim is to optimize performance by embedding smaller packets in the queue element,
//	only dynamically allocating memory for larger packets.
//
class QueuedMIDIPacket {
private:
	enum { kMaxEmbeddedLength = 64 };
	
	// Data members correspond to those of MIDIPacket
	MIDITimeStamp		mTimeStamp;
	int					mLength;
	Byte *				mPtr;
	Byte				mArray[kMaxEmbeddedLength];

	// no constructor/destructor (efficiency concerns when appending into a queue, copying)
	// must call the following manually:

public:	
	void				Create(const MIDIPacket *pkt)
	{
		mTimeStamp = pkt->timeStamp;
		int len = mLength = pkt->length;
		if (len <= kMaxEmbeddedLength) {
			mPtr = NULL;
			memcpy(mArray, pkt->data, len);
		} else {
			memcpy(mPtr = (Byte *)malloc(len), pkt->data, len);
		}
	}
	
	void				Dispose()
	{
		if (mPtr)
			free(mPtr);
	}
	
	Byte *				Data()
	{
		return mPtr ? mPtr : mArray;
	}
	
	void				PrependBytes(Byte *data, int addLen)
	{
		int newLength = mLength + addLen;
		
		if (newLength > kMaxEmbeddedLength) {
			mPtr = (Byte *)realloc(mPtr, newLength);
			memmove(mPtr + addLen, Data(), mLength);
			memcpy(mPtr, data, addLen);
		} else {
			memmove(mArray + addLen, mArray, mLength);
			memcpy(mArray, data, addLen);
		}
		mLength = newLength;
	}
	
	int					Length() const { return mLength; }
};

// _________________________________________________________________________________________
//	WriteQueueElem
// 
struct WriteQueueElem {
	QueuedMIDIPacket	packet;
	int					portNum;
	ByteCount			bytesSent;	// this much of the packet has been sent
};

// _________________________________________________________________________________________
//	WriteQueue
// 
//	Queue of outgoing MIDI packets.
class WriteQueue : public std::list<WriteQueueElem /*, TRealtimeAllocator<WriteQueueElem>*/ > {
public:
	~WriteQueue()
	{
		for (iterator it = begin(); it != end(); ++it)
			(*it).packet.Dispose();
	}
};

// _________________________________________________________________________________________
// IOBuffer
// 
//	Encapsulates everything needed for a piece of memory which is used as an I/O buffer.
//	Not necessary but is future-looking.
class IOBuffer {
public:
	IOBuffer();
	~IOBuffer();

	void		Allocate(void *owner, UInt32 size);

	Byte *		Buffer() const		{ return mBuffer; }
	operator Byte * () const		{ return mBuffer; }
	UInt32		Size() const		{ return mSize; }
	void *		Owner() const		{ return mOwner; }
	
	bool		IOPending() const	{ return mIOPending; }
	void		SetIOPending(bool v = true) { mIOPending = v; }
	
private:
	void *		mOwner;
	Byte *		mBuffer;		// points into region in mMemory
	Byte *		mMemory;		// allocated with operator new[]
	UInt32		mSize;
	bool		mIOPending;
};

#if 0
// obsolete: use CAGuard/CAMutex
// _________________________________________________________________________________________
//	Mutex
//
//	A pthread mutex with handling of recursive locking.
class Mutex {
public:
					Mutex();
	virtual 		~Mutex();
	
	virtual bool	Lock();		// return true if Unlock() should be called
	virtual void	Unlock();

	virtual void	Wait();
	virtual bool	WaitFor(UInt64 inNanos);
	virtual bool	WaitUntil(UInt64 inNanos);
	
	virtual void	Notify();
	virtual void	NotifyAll();
	
	bool			IsLocked() { return mOwner != NULL; }

private:
	Mutex(Mutex &a) { }	// prohibit copy constructor

	pthread_mutex_t	mMutex;
	pthread_cond_t	mCondVar;
	pthread_t		mOwner;
};

// _________________________________________________________________________________________
//	MutexLocker
//
//	Stack-based object to lock a Mutex.
class MutexLocker {
public:
	MutexLocker(Mutex &mutex) : mMutex(mutex) { mNeedUnlock = mutex.Lock(); }
	~MutexLocker() { if (mNeedUnlock) mMutex.Unlock(); }
	
	Mutex &		mMutex;
	bool		mNeedUnlock;
};
#endif

// _________________________________________________________________________________________
//
#if 0
// obsolete: use CAHostTimeBase
MIDITimeStamp	MIDIGetCurrentHostTime();

UInt64			MIDIConvertHostTimeToNanos(MIDITimeStamp hostTime);

MIDITimeStamp	MIDIConvertNanosToHostTime(UInt64 nanos);
#endif

// _________________________________________________________________________________________
//	MIDIDataBytes
//
// returns number of data bytes which follow the status byte.
// returns -1 for 0xF0 sysex beginning (indicating a variable number of data bytes
//		following).
// returns 0 if an unknown MIDI status byte is received and prints a warning.
inline int		MIDIDataBytes(Byte status)
{
	if (status >= 0x80 && status < 0xF0)
		return ((status & 0xE0) == 0xC0) ? 1 : 2;

	switch (status) {
	case 0xF0:
		return -1;
	case 0xF1:		// MTC
	case 0xF3:		// song select
		return 1;
	case 0xF2:		// song pointer
		return 2;
	case 0xF6:		// tune request
	case 0xF7:		// sysex conclude, nothing follows.
	case 0xF8:		// clock
	case 0xFA:		// start
	case 0xFB:		// continue
	case 0xFC:		// stop
	case 0xFE:		// active sensing
	case 0xFF:		// system reset
		return 0;
	}

#if DEBUG
	fprintf(stderr, "MIDIDataBytes: illegal status byte %02X\n", status);
#endif
	return 0;   // the MIDI spec says we should ignore illegals.
}

// _________________________________________________________________________________________
//
#if DEBUG
class Profiler {
public:
	Profiler(const char *funcName, UInt32 threshMicros = 0) : 
		mFuncName(funcName), 
		mStartTime(CAHostTimeBase::GetCurrentTime()),
		mThreshMicros(threshMicros) { }

	~Profiler()
	{
		UInt32 elapsed = CAHostTimeBase::ConvertFromNanos(CAHostTimeBase::GetCurrentTime() - mStartTime) / 1000;
		if (elapsed >= mThreshMicros)
			printf("%s took %ld us\n", mFuncName, elapsed);
	}

	const char *mFuncName;
	UInt64		mStartTime;
	UInt32		mThreshMicros;
};
#endif

#endif // __MIDIDriverBase_h__
