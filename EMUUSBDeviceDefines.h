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
#ifndef _EMUUSBDeviceDefines_
#define _EMUUSBDeviceDefines_
#define kInternalClockString	"Internal Clock"
#define kExternalClockString	"S/PDIF Clock"
#define kExtras					"Extras"
enum {
	kStdDataLen			= 1,	// data length for stock settings
	kDigIOSampleRateLen	= 4,	// data length of digital IO sample rate 
	kDirectMonitorLen	= 20	// data length for direct monitor settings controller
};

enum {
	kXUChangeNotifier				= 'Xmod',		// notification of change
	kClockSourceController			= 'Csct',		// clock source controller
	kDigIOSampleRateController		= 'Dsrc',		// digital IO sample rate controller
	kDigIOSyncSrcController			= 'Dssc',		// digital IO sync source controller
	kDigIOAsyncSrcController		= 'Dasc',		// digital IO async source controller
	kDigIOSPDIFController			= 'Dspf',		// digital IO SPDIF controller
	kDevSoftLimitController			= 'Dslc',		// device soft limit controller
	kDirectMonMonoStereoController	= 'Most',		// direct monitor mono or stereo controller
	kDirectMonOnOffController		= 'Dmc ',		// direct monitor controller on/off switch
	kDirectMonInputController		= 'Dmin',		// direct monitor input controller
	kDirectMonOutputController		= 'Dmot',		// direct monitor output controller
	kDirectMonGainController		= 'Dmgn',		// direct monitor gain controller
	kDirectMonPanController			= 'Dmpn',		// direct monitor pan controller
	kClockSourceSelectController	= 'Csc2',		// regular clock source controller (may not be necessary)
	
	kEndOfControllers				= 'ZZZZ'
};

enum eClockSourceType {
	kClockSourceInternal,
	kClockSourceSpdifExternal
};

enum eSampleRate {
	sr_44kHz,
	sr_48kHz,
	sr_88kHz,
	sr_96kHz,
	sr_176kHz,
	sr_192kHz
};

enum {
	kNumFramesToClear	= 12
};

#define kDigitalSR1Str	"44.1khz"
#define kDigitalSR2Str	"48khz"
#define kDigitalSR3Str	"88.2khz"
#define kDigitalSR4Str	"96khz"
#define kDigitalSR5Str	"176.4khz"
#define kDigitalSR6Str	"192khz"


enum ePadOption {// currently not used
	edo_AnlgPlusFourPad	= 1,// always starts with one or the other
	edo_AnlgMinusTenPad = 2,
	edo_SoftLimit		= 3
};

enum eDigitalFormat {
	notdefined,
	spdif,
	aes
};

enum eExtensionUnitCode {
	kClockRate			= 0xe301,
	kClockSource		= 0xe302,
	kDigitalIOStatus	= 0xe303,
	kDeviceOptions		= 0xe304,
	kDirectMonitoring	= 0xe305,
	kMetering			= 0xe306
};

enum availableExtensionUnitSettinfs {
	kXUSampleRate			= 0x01,
	kXULocked				= 0x02,
	kXUSyncSource			= 0x04,
	kXUPaddingState			= 0x08,
	kXUSoftLimit			= 0x10,
	kXUDigitalSrcState		= 0x20,
	kXUDigitalOutputFormat	= 0x40,
	kXUDigitalSampleRate	= 0x80
};

enum extensionUnitControlSelector {
	kEnableProcessing		= 0x01,
	kClockRateSupport		= 0x02,	// clock rate support
	kClockRateSelector		= 0x03,	// clock rate
	kClockSourceSelector	= 0x02,	// clock source
	kDigSampRateSel			= 0x02,	// digital sample rate
	kDigitalSyncLock		= 0x03,	// digital lock state
	kDigitalSRC				= 0x04,	// digital src
	kDigitalFormat			= 0x05,	// the spdif format
	kAnalogPad				= 0x02,	// padding options
	kSoftLimitSelector		= 0x03,	// soft limit
	kDirectMonStereo		= 0x02,	// stereo control
	kDirectMonControl		= 0x03	// direct monitor control
};

enum {// 4 char clock source selector
	kCSSelectionInternal	= 'int ',
	kCSSelectionExternal	= 'ext '
};

enum {
	kSoftLimitOff = 0,
	kSoftLimitOn
};

#define kSoftLimitOffStr "Off"
#define kSoftLimitOnStr "On"

enum {
	kSPDIFNone		= 0,
	kSPDIFConsumer	= 1,
	kSPDIFPro		= 2
};

#define kSPDIFNoneStr		"None"
#define kSPDIFConsumerStr	"SPDIF"
#define kSPDIFProStr		"AES/EBU"

// direct monitor constants
enum {
	kMaxMonitorGain = 0x7FFFFFFF
};

enum {
	kMaxMonitorGaindB	= 0x7FFFFFFF	// max gain in db
};

enum {
	kMonitorPanLeft		= 0,		// pan hard left
	kMonitorPanRight	= 0x7FFFFFFF// pan hard right
};

enum {
	kMinMonitorPandB	= 0,		// pan in terms of db
	kMaxMonitorPandB	= 0x7FFFFFFF// pan in terms of db
};
#endif