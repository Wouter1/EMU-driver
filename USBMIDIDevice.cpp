/*	Copyright: 	© Copyright 2005 Apple Computer, Inc. All rights reserved.

	Disclaimer:	IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc.
			("Apple") in consideration of your agreement to the following terms, and your
			use, installation, modification or redistribution of this Apple software
			constitutes acceptance of these terms.  If you do not agree with these terms,
			please do not use, install, modify or redistribute this Apple software.

			In consideration of your agreement to abide by the following terms, and subject
			to these terms, Apple grants you a personal, non-exclusive license, under AppleÕs
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
	USBMIDIDevice.cpp
	
=============================================================================*/

#include "USBMIDIDevice.h"
#include "USBMIDIDriverBase.h"

#if DEBUG
	//#define DUMP_OUTPUT 1
	//#define DUMP_INPUT 1
	//#define VERBOSE 1
	//#define TRACE_IO 1
#endif


// _________________________________________________________________________________________
//	USBMIDIDevice::USBMIDIDevice
//
USBMIDIDevice::USBMIDIDevice(		USBMIDIDriverBase *	driver,
									USBDevice *			usbDevice,
									USBInterface *		usbInterface,
									MIDIDeviceRef		midiDevice) :
	MIDIDriverDevice(midiDevice),
	mDriver(driver),
	mUSBDevice(usbDevice),
	mUSBInterface(usbInterface),
	mUSBIntfIntf(NULL),
	mShuttingDown(false),
	mWriteQueueMutex("USBMIDIDevice.mWriteQueueMutex")

#if COALESCE_WRITES
	, mWriteSignalTimer(NULL),
	mWriteSignalled(0)
#endif
{
	if (usbInterface)
		usbInterface->DisownDevice();	// we're taking over ownership of it here
}

// _________________________________________________________________________________________
//	USBMIDIDevice::Initialize
//
bool	USBMIDIDevice::Initialize()
{
	mUSBInterface = mDriver->CreateInterface(this);
	if (mUSBInterface == NULL || !mUSBInterface->Open())
		return false;
	mUSBIntfIntf = mUSBInterface->GetPluginInterface();
	if (mUSBIntfIntf == NULL)
		return false;

	FindPipes();
	
	int i;
	for (i = 0; i < kNumReadBufs; ++i)
		mReadBuf[i].Allocate(this, mInPipe.mMaxPacketSize);
	for (i = 0; i < kNumWriteBufs; ++i)
		mWriteBuf[i].Allocate(this, mOutPipe.mMaxPacketSize);
	mCurWriteBuf = 0;
	
	// don't go any further if we don't have a valid pipe
	require(HaveOutPipe() || HaveInPipe(), errexit);

#if VERBOSE
	printf("starting MIDI, mOutPipe=0x%lX, mInPipe=0x%lX\n", (long)mOutPipe.mPipeIndex, (long)mInPipe.mPipeIndex);
#endif

	SetUpEndpoints(true);
	
	{
		CFRunLoopRef ioRunLoop = MIDIGetDriverIORunLoop();
		CFRunLoopSourceRef source;
		
		if (ioRunLoop != NULL) {
			source = (*mUSBIntfIntf)->GetInterfaceAsyncEventSource(mUSBIntfIntf);
			if (source == NULL) {
				require_noerr((*mUSBIntfIntf)->CreateInterfaceAsyncEventSource(mUSBIntfIntf, &source), errexit);
				require(source != NULL, errexit);
			}
			if (!CFRunLoopContainsSource(ioRunLoop, source, kCFRunLoopDefaultMode))
				CFRunLoopAddSource(ioRunLoop, source, kCFRunLoopDefaultMode);
		}
		
#if COALESCE_WRITES
		{
			CFRunLoopTimerContext context;
			context.version = 0;
			context.info = this;
			context.retain = NULL;
			context.release = NULL;
			context.copyDescription = NULL;
	
			mWriteSignalTimer = CFRunLoopTimerCreate(NULL, 0 /* fire date */, 100000000000. /* interval */, 0 /* flags */, 0 /* order */, WriteSignalCallback, &context);
			CFRunLoopAddTimer(ioRunLoop, mWriteSignalTimer, kCFRunLoopDefaultMode);
		}
#endif
	}

	if (HaveInPipe()) {
		for (i = 0; i < kNumReadBufs; ++i)
			DoRead(mReadBuf[i]);
	}

	mDriver->StartInterface(this);
	// Start MIDI.  Do driver specific initialization.
	// Here, the driver can do things like send MIDI to the interface to
	// configure it.
	
	return true;
errexit:
	return false;
}

// _________________________________________________________________________________________
//	USBMIDIDevice::~USBMIDIDevice
//
USBMIDIDevice::~USBMIDIDevice()
{
	SetUpEndpoints(false);

	if (HaveOutPipe() || HaveInPipe())
		mDriver->StopInterface(this);

	mShuttingDown = true;

	if (HaveOutPipe())
		verify_noerr((*mUSBIntfIntf)->AbortPipe(mUSBIntfIntf, mOutPipe.mPipeIndex));

	if (HaveInPipe())
		verify_noerr((*mUSBIntfIntf)->AbortPipe(mUSBIntfIntf, mInPipe.mPipeIndex)); 

	const int sleepMicros = 10000;	// 10 ms
	const int timeoutMicros = 2*1000*1000;	// 2 seconds
	int maxIterations = timeoutMicros / sleepMicros;
	while (--maxIterations > 0) {
		int i;
		bool anyPending = false;
		for (i = 0; i < kNumReadBufs; ++i)
			if (mReadBuf[i].IOPending()) {
				anyPending = true;
#if VERBOSE
				printf("mReadBuf[%d] IO pending\n", i);
#endif
				break;
			}
		for (i = 0; i < kNumWriteBufs; ++i)
			if (mWriteBuf[i].IOPending()) {
				anyPending = true;
#if VERBOSE
				printf("mWriteBuf[%d] IO pending\n", i);
#endif
				break;
			}
		if (!anyPending)
			break;
		usleep(sleepMicros);
	}
	CFRunLoopRef ioRunLoop = MIDIGetDriverIORunLoop();
	CFRunLoopSourceRef source;
	
	if (ioRunLoop != NULL) {
		source = (*mUSBIntfIntf)->GetInterfaceAsyncEventSource(mUSBIntfIntf);
		if (source != NULL && CFRunLoopContainsSource(ioRunLoop, source, kCFRunLoopDefaultMode))
			CFRunLoopRemoveSource(ioRunLoop, source, kCFRunLoopDefaultMode);
	}
	
#if COALESCE_WRITES
	if (mWriteSignalTimer != NULL) {
		CFRunLoopRemoveTimer(ioRunLoop, mWriteSignalTimer, kCFRunLoopDefaultMode);
		CFRunLoopTimerInvalidate(mWriteSignalTimer);
		CFRelease(mWriteSignalTimer);
	}
#endif
	
	delete mUSBInterface;
	delete mUSBDevice;
	
#if VERBOSE
	printf("driver stopped MIDI\n");
#endif
}

// _________________________________________________________________________________________
//	USBMIDIDriverBase::FindPipes
//
//	Overridable method.  Responsible for setting the mInPipe and mOutPipe members.
//  Default implementation here just finds the last pipe of each direction.
void	USBMIDIDevice::FindPipes()
{
	UInt8	   		numEndpoints = 0;	//, pipeNum, direction, transferType, interval;
	UInt16			pipeIndex;			//, maxPacketSize; 		
	
	numEndpoints = 0;
	require_noerr((*mUSBIntfIntf)->GetNumEndpoints(mUSBIntfIntf, &numEndpoints), errexit);
		// find the number of endpoints for this interface

	for (pipeIndex = 1; pipeIndex <= numEndpoints; ++pipeIndex) {
		USBPipe pipe;
		require_noerr(mUSBInterface->GetPipe(pipeIndex, pipe), nextPipe);
		if (pipe.mDirection == kUSBOut)
			mOutPipe = pipe;
		else if (pipe.mDirection == kUSBIn)
			mInPipe = pipe;
nextPipe: ;
	}
errexit: ;
}

// _________________________________________________________________________________________
//	Dump
//
#if DUMP_INPUT || DUMP_OUTPUT
static void Dump(const char *label, Byte *buffer, ByteCount len)
{
	printf("%2ld %-3.3s: ", len, label);
	ByteCount i = 0;
	while (true) {
		printf("%02X ", buffer[i]);
		if (++i >= len) break;
		if (i % 32 == 0)
			printf("\n        ");
	}
	printf("\n");
}
#endif

// _________________________________________________________________________________________
//	USBMIDIDevice::HandleInput
//
void	USBMIDIDevice::HandleInput(IOBuffer &readBuf, ByteCount bytesReceived)
{
	UInt64 now = CAHostTimeBase::GetCurrentTime();
#if DUMP_INPUT
	Dump("IN", readBuf, bytesReceived);
#endif
	mDriver->HandleInput(this, now, readBuf, bytesReceived);
}

// _________________________________________________________________________________________
//	USBMIDIDevice::Send
//
void	USBMIDIDevice::Send(const MIDIPacketList *pktlist, int portNumber)
{
//	Profiler prof("USBMIDIDevice::Send", 200);
	if (HaveOutPipe()) {
		CAMutex::Locker lock(mWriteQueueMutex);
		const MIDIPacket *srcpkt = pktlist->packet;
		for (int i = pktlist->numPackets; --i >= 0; ) {
			WriteQueueElem wqe;
			
			wqe.packet.Create(srcpkt);
			wqe.portNum = portNumber;
			wqe.bytesSent = 0;
			mWriteQueue.push_back(wqe);
			
#if TRACE_IO
			Byte *p = wqe.packet.Data();
			syscall(180, 0xBC200000, srcpkt->length, *(UInt32 *)p, *(UInt32 *)(p + 4), *(UInt32 *)(p + 8));
#endif

			srcpkt = MIDIPacketNext(srcpkt);
		}
#if COALESCE_WRITES
		if (!mWriteBuf[mCurWriteBuf].IOPending() && !mWriteSignalled) {
			mWriteSignalled = 1;
			CFRunLoopTimerSetNextFireDate(mWriteSignalTimer, CFAbsoluteTimeGetCurrent());
		}
#else
		if (!mWriteBuf[mCurWriteBuf].IOPending())
			DoWrite();
#endif
	}
}

// _________________________________________________________________________________________
//	USBMIDIDevice::DoRead
//
void	USBMIDIDevice::DoRead(IOBuffer &readBuf)
{
	readBuf.SetIOPending(true);
	verify_noerr((*mUSBIntfIntf)->ReadPipeAsync(mUSBIntfIntf, mInPipe.mPipeIndex, readBuf, mInPipe.mMaxPacketSize, ReadCallback, &readBuf));
}

// _________________________________________________________________________________________
//	USBMIDIDevice::ReadCallback
//
//	This is the IOAsyncCallback (static method).
void	USBMIDIDevice::ReadCallback(void *refcon, IOReturn asyncReadResult, void *arg0)
{
//	Profiler prof("USBMIDIDevice::ReadCallback", 200);
	IOBuffer &buffer = *(IOBuffer *)refcon;
	buffer.SetIOPending(false);

	if (asyncReadResult == kIOReturnAborted)
		goto done;
	require_noerr(asyncReadResult, done);
	{
		USBMIDIDevice *self = (USBMIDIDevice *)buffer.Owner();
		if (!self->mShuttingDown) {
			ByteCount bytesReceived = (ByteCount)arg0;
			if (bytesReceived == 0)
				debug_string("0 bytes received");
			
			//printf("ReadCallback: arg0 is %ld\n", (long)bytesReceived);
			self->HandleInput(buffer, bytesReceived);
			// chain another async read
			self->DoRead(buffer);
		}
	}
done:
	;
}

// _________________________________________________________________________________________
//	USBMIDIDevice::DoWrite
//
//	Must only be called with mWriteQueueMutex acquired and mWritePending false.
void	USBMIDIDevice::DoWrite()
{
	if (!mWriteQueue.empty()) {
		IOBuffer &writeBuffer = mWriteBuf[mCurWriteBuf];
		ByteCount msglen = mDriver->PrepareOutput(this, mWriteQueue, writeBuffer);

		if (msglen > 0) {
#if TRACE_IO
			Byte *p = writeBuffer;
			syscall(180, 0xBC240000, msglen, *(UInt32 *)p, *(UInt32 *)(p + 4), *(UInt32 *)(p + 8));
#endif
#if DUMP_OUTPUT
			Dump("OUT", writeBuffer, msglen);
#endif
			writeBuffer.SetIOPending(true);
			verify_noerr((*mUSBIntfIntf)->WritePipeAsync(mUSBIntfIntf, mOutPipe.mPipeIndex, writeBuffer, msglen, WriteCallback, this));
		}
	}
}

// _________________________________________________________________________________________
//	USBMIDIDevice::WriteCallback
//
//	This is the IOAsyncCallback (static method).
void	USBMIDIDevice::WriteCallback(void *refcon, IOReturn asyncWriteResult, void *arg0)
{
//	Profiler prof("USBMIDIDevice::WriteCallback", 200);
	USBMIDIDevice *self = (USBMIDIDevice *)refcon;
	CAMutex::Locker lock(self->mWriteQueueMutex);
	// chain another write
	self->mWriteBuf[self->mCurWriteBuf].SetIOPending(false);
	if (++self->mCurWriteBuf == kNumWriteBufs) 
		self->mCurWriteBuf = 0;
	check_noerr(asyncWriteResult);
	if (!self->mShuttingDown && !asyncWriteResult)
		self->DoWrite();
}

#if COALESCE_WRITES
// _________________________________________________________________________________________
//	USBMIDIDevice::WriteSignalCallback
//
//	Runloop timer fired
void	USBMIDIDevice::WriteSignalCallback(CFRunLoopTimerRef timer, void *info)
{
	USBMIDIDevice *self = (USBMIDIDevice *)info;
	CAMutex::Locker lock(self->mWriteQueueMutex);
	self->mWriteSignalled = 0;
	if (!self->mShuttingDown)
		self->DoWrite();
}
#endif
