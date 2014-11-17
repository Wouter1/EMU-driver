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
	USBMIDIDevice.h
	
=============================================================================*/

#ifndef __USBMIDIDevice_h__
#define __USBMIDIDevice_h__

#include "MIDIDriverDevice.h"
#include "USBDevice.h"
#include "CAMutex.h"

class USBMIDIDriverBase;

#define COALESCE_WRITES 1

// _________________________________________________________________________________________
//	USBMIDIDevice
// 
//	This class is the runtime state for one USB MIDI device.
class USBMIDIDevice : public MIDIDriverDevice {
public:
					USBMIDIDevice(	USBMIDIDriverBase *	driver,
									USBDevice *			usbDevice,
									USBInterface *		usbInterface,
									MIDIDeviceRef		midiDevice);
	
	virtual			~USBMIDIDevice();
	
	// we have two-stage construction, so that overridden virtual methods 
	// are correctly dispatched to subclasses
	virtual bool	Initialize();
						// return true for success
	
	virtual void	FindPipes();

	virtual void	DoRead(IOBuffer &readBuf);
	static void		ReadCallback(void *refcon, IOReturn result, void *arg0);
	virtual void	DoWrite();
	static void		WriteCallback(void *refcon, IOReturn result, void *arg0);
#if COALESCE_WRITES
	static void		WriteSignalCallback(CFRunLoopTimerRef timer, void *info);
#endif

	virtual void	HandleInput(IOBuffer &readBuf, ByteCount bytesReceived);
	virtual void	Send(const MIDIPacketList *pktlist, int portNumber);
	
	bool			HaveInPipe() const { return mInPipe.mPipeIndex != UInt8(kUSBNoPipeIdx); }
	bool			HaveOutPipe() const { return mOutPipe.mPipeIndex != UInt8(kUSBNoPipeIdx); }
	bool			WritePending() const { return mWriteBuf[0].IOPending(); }
	
	USBDevice *		GetUSBDevice() { return mUSBDevice; }
	
	// Leave data members public, so they may be accessed directly by driver methods.
	USBMIDIDriverBase *			mDriver;
	USBDevice *					mUSBDevice;
	USBInterface *				mUSBInterface;
	IOUSBInterfaceInterface **	mUSBIntfIntf;
	USBPipe						mInPipe, mOutPipe;		// may be kUSBNoPipeIdx
	bool						mShuttingDown;
	
	enum { kNumReadBufs = 2, kNumWriteBufs = 1 };
	
	IOBuffer					mReadBuf[kNumReadBufs], mWriteBuf[kNumWriteBufs];
	int							mCurWriteBuf;
	CAMutex						mWriteQueueMutex;
	WriteQueue					mWriteQueue;
#if COALESCE_WRITES
	CFRunLoopTimerRef			mWriteSignalTimer;
	UInt8						mWriteSignalled;
#endif
};


#endif // __USBMIDIDevice_h__
