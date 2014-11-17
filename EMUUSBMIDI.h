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
/*=============================================================================
	EMUUSBMIDI.h
	
=============================================================================*/

#ifndef __EMUUSBMIDI_h__
#define __EMUUSBMIDI_h__
#pragma once
#include "USBVendorMIDIDriver.h"
#define	kStringBufferSize	255
#ifdef DEBUG_MSG
#define debugIOLog(message...) do {fprintf(stdout, message); fprintf(stdout, "\n");} while(0)
#else
#define debugIOLog(message...) ;
#endif
class EMUUSBMIDIDevice : public USBVendorMIDIDriver {
public:
	EMUUSBMIDIDevice();
	~EMUUSBMIDIDevice();
	
	// USBMIDIDriverBase overrides
	virtual bool			MatchDevice(		USBDevice *		inUSBDevice);

	virtual MIDIDeviceRef	CreateDevice(		USBDevice *		inUSBDevice,
												USBInterface *	inUSBInterface);

	virtual USBInterface *	CreateInterface(	USBMIDIDevice *	usbmDev);

	virtual void		StartInterface(			USBMIDIDevice *	usbmDev);
							// pipes are opened, do any extra initialization (send config msgs etc)
							
	virtual void		StopInterface(			USBMIDIDevice *	usbmDev);
							// pipes are about to be closed, do any preliminary cleanup
							
	virtual void		HandleInput(			USBMIDIDevice *	usbmDev,
												MIDITimeStamp	when,
												Byte *			readBuf,
												ByteCount 		readBufSize);
							// a USB message arrived, parse it into a MIDIPacketList and
							// call MIDIReceived

	virtual ByteCount	PrepareOutput(			USBMIDIDevice *	usbmDev,
												WriteQueue &	writeQueue,
												Byte *			destBuf);
							// dequeue from WriteQueue into a single USB message, return
};

#endif // __EMUUSBMIDI_h__
