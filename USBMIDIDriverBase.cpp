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
	USBMIDIDriverBase.cpp
	
=============================================================================*/

#include "USBMIDIDriverBase.h"
#include <algorithm>
#include "USBMIDIDevice.h"

#ifndef CAN_USE_USB_UNPARSED_EVENTS
#define CAN_USE_USB_UNPARSED_EVENTS 1
#endif

#if DEBUG
	#include <stdio.h>
	//#define VERBOSE 1
	
	// sysex debug levels
	//#define SYSEX_DEBUG_LEVEL 2
	
	#if SYSEX_DEBUG_LEVEL >= 1
		static int gSysExCount = -1;
		#if SYSEX_DEBUG_LEVEL >= 2
			static int gSysExTraceCount = -1;
			#define kSysExTraceBufSize 0x20000/4
			static UInt32 gSysExTraceBuf[kSysExTraceBufSize];
			static int gSysExMessages = 0;
		#endif
	#endif
#endif

// _________________________________________________________________________________________
// USBMIDIDriverBase::USBMIDIDriverBase
//
USBMIDIDriverBase::USBMIDIDriverBase(CFUUIDRef factoryID) :
	MIDIDriver(factoryID)
{
}


// _________________________________________________________________________________________
// USBMIDIDriverBase::CreateUSBMIDIDevice
//
USBMIDIDevice *	USBMIDIDriverBase::CreateUSBMIDIDevice(	USBDevice *		inUSBDevice,
														USBInterface *	inUSBInterface,
														MIDIDeviceRef	inMIDIDevice)
{
	return new USBMIDIDevice(this, inUSBDevice, inUSBInterface, inMIDIDevice);
}

// _________________________________________________________________________________________
// USBMIDIDriverBase::Send
//
OSStatus	USBMIDIDriverBase::Send(const MIDIPacketList *pktlist, void *endptRef1, void *endptRef2)
{
	USBMIDIDevice *usbmDev = (USBMIDIDevice *)endptRef1;
	if (usbmDev == NULL) return kMIDIUnknownEndpoint;

	usbmDev->Send(pktlist, (int)endptRef2);	// endptRef2 = port number

	return noErr;
}



// _________________________________________________________________________________________
// USBMIDIDriverBase::USBMIDIHandleInput
//
void	USBMIDIDriverBase::USBMIDIHandleInput(	USBMIDIDevice *	usbmDev, 
												MIDITimeStamp	when, 
												Byte *			readBuf,
												ByteCount		bufSize)
{
	Byte *src = readBuf, *srcend = src + bufSize;
	Byte pbuf[512];
	MIDIPacketList *pktlist = (MIDIPacketList *)pbuf;
	MIDIPacket *pkt = MIDIPacketListInit(pktlist);
	int prevCable = -1;	// signifies none
	bool insysex = false;

	for ( ; src < srcend; src += 4) {
		int cin = src[0] & 0x0F;
		
		if (cin < 2)
			// skip over reserved cin's before doing any more work
			continue;
		
		int cable = src[0] >> 4;
		int msglen;
		// support single-entity devices that seem to use an arbitrary cable number
		// (besides which, it's good to have range-checking)
		if (cable < 0) cable = 0;
		else if (cable >= usbmDev->mNumEntities) cable = usbmDev->mNumEntities - 1;
		
		if (prevCable != -1 && cable != prevCable) {
			MIDIReceived(usbmDev->mSources[prevCable], pktlist);
			pkt = MIDIPacketListInit(pktlist);
			insysex = false;
		}
		prevCable = cable;
		
		#if SYSEX_DEBUG_LEVEL >= 2
			if (gSysExTraceCount >= 0)
				gSysExTraceBuf[gSysExTraceCount++] = *(UInt32 *)src;
		#endif

		switch (cin) {
		case 0x0:		// reserved
		case 0x1:		// reserved
			break;
		case 0xF:		// single byte
			msglen = 1;
AddMessage:
			while (true) {
				pkt = MIDIPacketListAdd(pktlist, sizeof(pbuf), pkt, when, msglen, src + 1);
				if (pkt != NULL) break;
				if (prevCable != -1)
					MIDIReceived(usbmDev->mSources[prevCable], pktlist);
				pkt = MIDIPacketListInit(pktlist);
				insysex = false;
			}
			break;
		case 0x2:		// 2-byte system common
		case 0xC:		// program change
		case 0xD:		// mono pressure
			msglen = 2;
			goto AddMessage;
		case 0x4:		// sysex starts or continues			
			#if SYSEX_DEBUG_LEVEL >= 2
				if (gSysExCount == -1) {
					gSysExTraceCount = 0;
					gSysExCount = 0;
					gSysExTraceBuf[gSysExTraceCount++] = *(UInt32 *)src;
				}
				gSysExCount += 3;
			#elif SYSEX_DEBUG_LEVEL >= 1
				if (gSysExCount == -1)
					gSysExCount = 0;
				gSysExCount += 3;
			#endif
			if (insysex) {
				// MIDIPacketListAdd will make a separate packet unnecessarily,
				// so do our own concatentation onto the current packet
				msglen = 3;
AddSysEx:
				Byte *dest = &pkt->data[pkt->length];
				if (dest + msglen > pbuf + sizeof(pbuf)) {
					if (prevCable != -1)
						MIDIReceived(usbmDev->mSources[prevCable], pktlist);
					pkt = MIDIPacketListInit(pktlist);
					insysex = false;
					goto AddMessage;
				}
				memcpy(dest, src + 1, msglen);
				pkt->length += msglen;
				break;
			}
			insysex = true;
			// --- fall ---
		case 0x3:		// 3-byte system common
		case 0x8:		// note-off
		case 0x9:		// note-on
		case 0xA:		// poly pressure
		case 0xB:		// control change
		case 0xE:		// pitch bend
			msglen = 3;
			goto AddMessage;
		case 0x5:		// single byte system-common, or sys-ex ends with one byte
			if (src[1] != 0xF7) {
				msglen = 1;
				goto AddMessage;
			}
			// --- fall ---
		case 0x6:		// sys-ex ends with two bytes
		case 0x7:		// sys-ex ends with three bytes
			msglen = cin - 4;

			#if SYSEX_DEBUG_LEVEL >= 1
				if (gSysExCount > 0) {
					gSysExCount += msglen;
					
					#if SYSEX_DEBUG_LEVEL >= 2
						char fname[64];
						sprintf(fname, "/tmp/sysx%d", ++gSysExMessages);
						FILE *f = fopen(fname, "w");
						for (int i = 0; i < gSysExTraceCount; ++i) {
							if (gSysExTraceBuf[i] == 0)
								fprintf(f, "\n");
							else
								fprintf(f, "%08x\n", gSysExTraceBuf[i]);
						}
						fclose(f);
						gSysExTraceCount = -1;
					#endif
					
					printf("=== driver: sysex end, %d bytes ===\n", gSysExCount);
					
					gSysExCount = -1;
				}
			#endif
			
			if (insysex) {
				insysex = false;
				goto AddSysEx;
			}
			goto AddMessage;
		}
	}
	#if SYSEX_DEBUG_LEVEL >= 2
		gSysExTraceBuf[gSysExTraceCount++] = 0;
		//syscall(180, 0xd0000000 + (src - readBuf), src-readBuf, 0, 0, 0);
	#endif
	
	if (pktlist->numPackets > 0 && prevCable != -1)
		MIDIReceived(usbmDev->mSources[prevCable], pktlist);
}

// _________________________________________________________________________________________
// USBMIDIDriverBase::USBMIDIPrepareOutput
//
// WriteQueue is an STL list of WriteQueueElem's to be transmitted, presumably containing
// at least one element.
// Fill one USB buffer, destBuf, with a size of bufSize, with outgoing data in USB-MIDI format.
// Return the number of bytes written.
ByteCount	USBMIDIDriverBase::USBMIDIPrepareOutput(	USBMIDIDevice *	usbmDev, 
														WriteQueue &	writeQueue, 			
														Byte *			destBuf, 
														ByteCount 		bufSize)
{
	Byte *dest = destBuf, *destend = dest + bufSize;
	
	while (!writeQueue.empty()) {		
		WriteQueue::iterator wqit = writeQueue.begin();
		WriteQueueElem *wqe = &(*wqit);
		Byte *dataStart = wqe->packet.Data();
		Byte *src = dataStart + wqe->bytesSent;
		Byte *srcend = dataStart + wqe->packet.Length();
		int srcLeft;

#if !CAN_USE_USB_UNPARSED_EVENTS
		// have to check to see if we have 1 or 2 bytes of dangling unsent sysex (won't contain F7)
		srcLeft = srcend - src;
		if (srcLeft < 3 && !(src[0] & 0x80)) {
			// advance to the next packet
			WriteQueueElem *dangler = wqe;
			if (++wqit == writeQueue.end())
				break;	// no more packets past the dangler
			wqe = &(*wqit);
			if (wqe->portNum == dangler->portNum) {
				// here we go, another packet to this destination; insert dangling bytes into front of it
				wqe->packet.PrependBytes(src, srcLeft);
				// now we can remove the dangler from the queue
				dangler->packet.Dispose();
				writeQueue.pop_front();
			}
			// now just start processing *this* packet
			dataStart = wqe->packet.Data();
			src = dataStart + wqe->bytesSent;
			srcend = dataStart + wqe->packet.mLength;
		}
#endif

		Byte cableNibble = wqe->portNum << 4;

		while ((srcLeft = srcend - src) > 0 && dest <= destend - 4) {
			Byte c = *src++;
			
			switch (c >> 4) {
			case 0x0: case 0x1: case 0x2: case 0x3:
			case 0x4: case 0x5: case 0x6: case 0x7:
				// data byte, presumably a sysex continuation
				// F0 is also handled the same way
inSysEx:
				--srcLeft;
				if (srcLeft < 2 && (
					srcLeft == 0 
				|| (srcLeft == 1 && src[0] != 0xF7))) {
					// we don't have 3 sysex bytes to fill the packet with
#if CAN_USE_USB_UNPARSED_EVENTS
					// so we have to revert to non-parsed mode
					*dest++ = cableNibble | 0xF;
					*dest++ = c;
					*dest++ = 0;
					*dest++ = 0;
#else
					// undo consumption of the first source byte
					--src;
					wqe->bytesSent = src - dataStart;
					goto DoneFilling;
#endif
				} else {
					dest[1] = c;
					if ((dest[2] = *src++) == 0xF7) {
						dest[0] = cableNibble | 6;		// sysex ends with following 2 bytes
						dest[3] = 0;
					} else if ((dest[3] = *src++) == 0xF7)
						dest[0] = cableNibble | 7;		// sysex ends with following 3 bytes
					else
						dest[0] = cableNibble | 4;		// sysex continues
					dest += 4;
				}
				break;
			case 0x8:	// note-off
			case 0x9:	// note-on
			case 0xA:	// poly pressure
			case 0xB:	// control change
			case 0xE:	// pitch bend
				*dest++ = cableNibble | (c >> 4);
				*dest++ = c;
				*dest++ = *src++;
				*dest++ = *src++;
				break;
			case 0xC:	// program change
			case 0xD:	// mono pressure
				*dest++ = cableNibble | (c >> 4);
				*dest++ = c;
				*dest++ = *src++;
				*dest++ = 0;
				break;
			case 0xF:	// system message
				switch (c) {
				case 0xF0:	// sysex start
					goto inSysEx;
				case 0xF8:	// clock
				case 0xFA:	// start
				case 0xFB:	// continue
				case 0xFC:	// stop
				case 0xFE:	// active sensing
				case 0xFF:	// system reset
					*dest++ = cableNibble | 0xF;// 1-byte system realtime
					*dest++ = c;
					*dest++ = 0;
					*dest++ = 0;
					break;
				case 0xF6:	// tune request (0)
				case 0xF7:	// EOX
					*dest++ = cableNibble | 5;	// 1-byte system common or sysex ends with one byte
					*dest++ = c;
					*dest++ = 0;
					*dest++ = 0;
					break;
				case 0xF1:	// MTC (1)
				case 0xF3:	// song select (1)
					*dest++ = cableNibble | 2;	// 2-byte system common
					*dest++ = c;
					*dest++ = *src++;
					*dest++ = 0;
					break;
				case 0xF2:	// song pointer (2)
					*dest++ = cableNibble | 3;	// 3-byte system common
					*dest++ = c;
					*dest++ = *src++;
					*dest++ = *src++;
					break;
				default:
					// unknown MIDI message! advance until we find a status byte
					while (src < srcend && *src < 0x80)
						++src;
					break;
				}
				break;
			}
		
			if (src >= srcend) {
				// source packet completely sent
				wqe->packet.Dispose();
				writeQueue.erase(wqit);
				if (writeQueue.empty())
					break;
			} else
				wqe->bytesSent = src - dataStart;
		} // ran out of source data or filled buffer
#if !CAN_USE_USB_UNPARSED_EVENTS
DoneFilling:
#endif
		
		if (dest > destend - 4)
			// destBuf completely filled
			break;

		// we didn't fill the output buffer, loop around to look for more 
		// source data in the write queue
		
	} // while walking writeQueue
	return dest - destBuf;
}

