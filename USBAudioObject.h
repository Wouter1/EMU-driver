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

#include <libkern/c++/OSArray.h>
#include <IOKit/usb/IOUSBInterface.h>

#include "EMUUSBAudioCommon.h"

#ifndef __USBAudioObject__
#define __USBAudioObject__
#define	kUSBAudioStreamInterfaceSubclass	2
#define	kRootAlternateSetting				0

enum {
	kMuteBit					= 0,
	kVolumeBit					= 1,
	kBassBit					= 2,
	kMidBit						= 3,
	kTrebleBit					= 4,
	kEQBit						= 5,
	kAGBit						= 6,
	kDelayBit					= 7,
	kBassBoostBit				= 8,
	kLoudnessBit				= 9
};

#define CLASS_PROPERTY_NAME					"class"
#define SUBCLASS_PROPERTY_NAME				"subClass"
#define PROTOCOL_PROPERTY_NAME				"protocol"
#define VENDOR_PROPERTY_NAME				"vendor"
#define PRODUCT_PROPERTY_NAME				"product"
#define VERSION_PROPERTY_NAME				"version"


enum {
    sampleFreqControlBit					= 0,
    pitchControlBit							= 1,
    maxPacketsOnlyBit						= 7
};

enum {
	kAsynchSyncType							= 0x01,
	kAdaptiveSyncType						= 0x02,
	kSynchronousSyncType					= 0x03
};

enum {
    DEVICE									= 0x01,
    CONFIGURATION							= 0x02,
    STRING									= 0x03,
    INTERFACE								= 0x04,
    ENDPOINT								= 0x05
};

enum {
	GET_STATUS								= 0x00,
	CLEAR_FREATURE							= 0x01,
	SET_FEATURE								= 0x03,
	SET_ADDRESS								= 0x05,
	GET_DESCRIPTOR							= 0x06,
	SET_DESCRIPTOR							= 0x07,
	GET_CONFIGURATION						= 0x08,
	SET_CONFIGURATION						= 0x09,
	GET_INTERFACE							= 0x0A,
	SET_INTERFACE							= 0x0B,
	SYNCH_FRAME								= 0x0C
};

enum {
    // For descriptor type
    CS_UNDEFINED							= 0x20,
    CS_DEVICE								= 0x21,
    CS_CONFIGURATION						= 0x22,
    CS_STRING								= 0x23,
    CS_INTERFACE							= 0x24,
    CS_ENDPOINT								= 0x25
};

enum {
	// Audio Interface Class Code
    AUDIO									= 0x01
};

enum {
	// Audio Interface Subclass Codes
	SUBCLASS_UNDEFINED						= 0x00,
    AUDIOCONTROL							= 0x01,
    AUDIOSTREAMING							= 0x02,
	MIDISTREAMING							= 0x03,
	VENDOR_SPECIFIC							= 0xff
};

enum {
    // Audio Control (AC) interface descriptor subtypes
    AC_DESCRIPTOR_UNDEFINED					= 0x00,
    HEADER									= 0x01,
    INPUT_TERMINAL							= 0x02,
    OUTPUT_TERMINAL							= 0x03,
    MIXER_UNIT								= 0x04,
    SELECTOR_UNIT							= 0x05,
    FEATURE_UNIT							= 0x06,
    PROCESSING_UNIT							= 0x07,
    EXTENSION_UNIT							= 0x08
};

enum {
    USB_STREAMING							= 0x0101
};

enum {
    // Audio Stream (AS) interface descriptor subtypes
    AS_DESCRIPTOR_UNDEFINED					= 0x00,
    AS_GENERAL								= 0x01,
    FORMAT_TYPE								= 0x02,
    FORMAT_SPECIFIC							= 0x03
};

enum {
    FORMAT_TYPE_UNDEFINED					= 0x00,
    FORMAT_TYPE_I							= 0x01,
    FORMAT_TYPE_II							= 0x02,
    FORMAT_TYPE_III							= 0x03
};

enum {
    // Audio data format type I codes
    TYPE_I_UNDEFINED						= 0x0000,
    PCM										= 0x0001,
    PCM8									= 0x0002,
    IEEE_FLOAT								= 0x0003,
    ALAW									= 0x0004,
    MULAW									= 0x0005
};

enum {
	// Audio data format type II codes
	TYPE_II_UNDEFINED						= 0x1000,
	MPEG									= 0x1001,
	AC3										= 0x1002
};

enum {
	// Audio data format type III codes
	TYPE_III_UNDEFINED						= 0x2000,
	IEC1937_AC3								= 0x2001,
	IEC1937_MPEG1_Layer1					= 0x2002,
	IEC1937_MPEG1_Layer2or3					= 0x2003,
	IEC1937_MPEG2_NOEXT						= 0x2003,
	IEC1937_MPEG2_EXT						= 0x2004,
	IEC1937_MPEG2_Layer1_LS					= 0x2005,
	IEC1937_MPEG2_Layer2or3_LS				= 0x2006
};

enum {
	// MPEG control selectors
	MPEG_CONTROL_UNDEFINED					= 0x00,
	MP_DUAL_CHANNEL_CONTROL					= 0x01,
	MP_SECOND_STEREO_CONTROL				= 0x02,
	MP_MULTILINGUAL_CONTROL					= 0x03,
	MP_DYN_RANGE_CONTROL					= 0x04,
	MP_SCALING_CONTROL						= 0x05,
	MP_HILO_SCALING_CONTROL					= 0x06
};

enum {
	// AC-3 control selectors
	AC_CONTROL_UNDEFINED					= 0x00,
	AC_MODE_CONTROL							= 0x01,
	AC_DYN_RANGE_CONTROL					= 0x02,
	AC_SCALING_CONTROL						= 0x03,
	AC_HILO_SCALING_CONTROL					= 0x04
};

enum {
    // Audio Class-specific endpoint descriptor subtypes
    DESCRIPTOR_UNDEFINED					= 0x00,
    EP_GENERAL								= 0x01
};

enum {
    REQUEST_CODE_UNDEFINED					= 0x00,
    SET_CUR									= 0x01,
    GET_CUR									= 0x81,
    SET_MIN									= 0x02,
    GET_MIN									= 0x82,
    SET_MAX									= 0x03,
    GET_MAX									= 0x83,
    SET_RES									= 0x04,
    GET_RES									= 0x84,
    SET_MEM									= 0x05,
    GET_MEM									= 0x85,
    GET_STAT								= 0xff
};

enum {
    FU_CONTROL_UNDEFINED					= 0x00,
    MUTE_CONTROL							= 0x01,
    VOLUME_CONTROL							= 0x02,
    BASS_CONTROL							= 0x03,
    MID_CONTROL								= 0x04,
    TREBLE_CONTROL							= 0x05,
    GRAPHIC_EQUALIZER_CONTROL				= 0x06,
    AUTOMATIC_GAIN_CONTROL					= 0x07,
    DELAY_CONTROL							= 0x08,
    BASS_BOOST_CONTROL						= 0x09,
    LOUDNESS_CONTROL						= 0x0a
};

enum {
	EP_CONTROL_UNDEFINED					= 0x00,
	SAMPLING_FREQ_CONTROL					= 0x01,
	PITCH_CONTROL							= 0x02
};

#define USB_AUDIO_IS_FUNCTION(subtype)	((subtype >= MIXER_UNIT) && (subtype <= EXTENSION_UNIT))
#define USB_AUDIO_IS_TERMINAL(subtype)	((subtype == INPUT_TERMINAL) || (subtype == OUTPUT_TERMINAL))

typedef struct USBDeviceDescriptor {
    UInt8									bLength;
    UInt8									bDescriptorType;
    UInt16									bcdUSB;
    UInt8									bDeviceClass;
    UInt8									bDeviceSubClass;
    UInt8									bDeviceProtocol;
    UInt8									bMaxPacketSize0;
    UInt16									idVendor;
    UInt16									idProduct;
    UInt16									bcdDevice;
    UInt8									iManufacturer;
    UInt8									iProduct;
    UInt8									iSerialNumber;
    UInt8									bNumConfigurations;
} USBDeviceDescriptor, *USBDeviceDescriptorPtr;

typedef struct USBConfigurationDescriptor {
    UInt8									bLength;
    UInt8									bDescriptorType;
    UInt16									wTotalLength;
    UInt8									bNumInterfaces;
    UInt8									bConfigurationValue;
    UInt8									iConfiguration;
    UInt8									bmAttributes;
    UInt8									MaxPower;	// expressed in 2mA units
} USBConfigurationDescriptor, *USBConfigurationDescriptorPtr;

typedef struct USBInterfaceDescriptor {
    UInt8									bLength;
    UInt8									bDescriptorType;
    UInt8									bDescriptorSubtype;
    UInt8									bAlternateSetting;
    UInt8									bNumEndpoints;
    UInt8									bInterfaceClass;
    UInt8									bInterfaceSubClass;
    UInt8									bInterfaceProtocol;
    UInt8									iInterface;
} USBInterfaceDescriptor, *USBInterfaceDescriptorPtr;

typedef struct USBEndpointDescriptor {
    UInt8									bLength;
    UInt8									bDescriptorType;
    UInt8									bEndpointAddress;
    UInt8									bmAttributes;
    UInt16									wMaxPacketSize;
    UInt8									bInterval;
    UInt8									bRefresh;
    UInt8									bSynchAddress;
} USBEndpointDescriptor, *USBEndpointDescriptorPtr;

typedef struct ACFunctionDescriptorHeader {
    UInt8									bLength;
    UInt8									bDescriptorType;
    UInt8									bDescriptorSubtype;
    UInt8									bFunctionID;
} ACFunctionDescriptorHeader;

typedef struct ACInterfaceHeaderDescriptor {
    UInt8									bLength;
    UInt8									bDescriptorType;
    UInt8									bDescriptorSubtype;
    UInt8									bcdADC[2];					// Not PPC aligned
    UInt8									wTotalLength[2];			// Not PPC aligned
    UInt8									bInCollection;
    UInt8									baInterfaceNr[1];			// There are bInCollection of these
} ACInterfaceHeaderDescriptor, *ACInterfaceHeaderDescriptorPtr;

typedef struct ACInterfaceDescriptor {
    UInt8									bLength;
    UInt8									bDescriptorType;
    UInt8									bInterfaceNumber;
    UInt8									bAlternateSetting;
    UInt8									bNumEndpoints;
    UInt8									bInterfaceClass;
    UInt8									bInterfaceSubClass;
    UInt8									bInterfaceProtocol;
    UInt8									iInterface;
} ACInterfaceDescriptor, *ACInterfaceDescriptorPtr;

typedef struct ACInputTerminalDescriptor {
    UInt8									bLength;
    UInt8									bDescriptorType;
    UInt8									bDescriptorSubtype;
    UInt8									bTerminalID;
    UInt16									wTerminalType;
    UInt8									bAssocTerminal;
    UInt8									bNrChannels;
    UInt16									wChannelConfig; 
    UInt8									iChannelNames;
    UInt8									iTerminal;
} ACInputTerminalDescriptor, *ACInputTerminalDescriptorPtr;

typedef struct ACOutputTerminalDescriptor {
    UInt8									bLength;
    UInt8									bDescriptorType;
    UInt8									bDescriptorSubtype;
    UInt8									bTerminalID;
    UInt16									wTerminalType;
    UInt8									bAssocTerminal;
    UInt8									bSourceID;
    UInt8									iTerminal;
} ACOutputTerminalDescriptor, *ACOutputTerminalDescriptorPtr;

typedef struct ACFeatureUnitDescriptor {
    UInt8									bLength;
    UInt8									bDescriptorType;
    UInt8									bDescriptorSubtype;
    UInt8									bUnitID;
    UInt8									bSourceID;
    UInt8									bControlSize;
    UInt8									bmaControls[2];
    // bmaControls size is actually bControlSize, so it might be one or two bytes
} ACFeatureUnitDescriptor, *ACFeatureUnitDescriptorPtr;

typedef struct ACMixerUnitDescriptor {
    UInt8									bLength;
    UInt8									bDescriptorType;
    UInt8									bDescriptorSubtype;
    UInt8									bUnitID;
	UInt8									bNrInPins;
	UInt8									baSourceID[1];	// there are bNrInPins of these
	// Can't have a static structure to define these locations:
	// UInt8								bNrChannels
	// UInt16								wChannelConfig
	// UInt8								iChannelNames
	// UInt8								bmControls[]	// you don't know the size of this, calculate it using bLength and bNrInPins
	// UInt8								iMixer
} ACMixerUnitDescriptor, *ACMixerUnitDescriptorPtr;

typedef struct ACSelectorUnitDescriptor {
    UInt8									bLength;
    UInt8									bDescriptorType;
    UInt8									bDescriptorSubtype;
    UInt8									bUnitID;
	UInt8									bNrInPins;
	UInt8									baSourceID[1];	// there are bNrInPins of these
	// Can't have a static structure to define this location:
	// UInt8								iSelector
} ACSelectorUnitDescriptor, *ACSelectorUnitDescriptorPtr;

typedef struct ACProcessingUnitDescriptor {
    UInt8									bLength;
    UInt8									bDescriptorType;
    UInt8									bDescriptorSubtype;
    UInt8									bUnitID;
	UInt16									wProcessType;
	UInt8									bNrInPins;
	UInt8									baSourceID[1];	// there are bNrInPins of these
	// Can't have a static structure to define these locations:
	// UInt8								bNrChannels
	// UInt16								wChannelConfig
	// UInt8								iChannelNames
	// UInt8								bControlSize
	// UInt8								bmControls[]
	// UInt8								iProcessing
	// UInt8								processSpecific[]	// no set size, calculate
} ACProcessingUnitDescriptor, *ACProcessingUnitDescriptorPtr;

typedef struct ACExtensionUnitDescriptor {
    UInt8									bLength;
    UInt8									bDescriptorType;
    UInt8									bDescriptorSubtype;
    UInt8									bUnitID;
	UInt16									wExtensionCode;
	UInt8									bNrInPins;
	UInt8									baSourceID[1];	// there are bNrInPins of these
	// Can't have a static structure to define these locations:
	// UInt8								bNrChannels
	// UInt16								wChannelConfig
	// UInt8								iChannelNames
	// UInt8								bControlSize
	// UInt8								bmControls[]
	// UInt8								iExtension
} ACExtensionUnitDescriptor, *ACExtensionUnitDescriptorPtr;

/*	From USB Device Class Definition for Audio Data Formats 2.4.1:
	The Type III Format Type is identical to the Type I PCM Format Type, set up for two-channel 16-bit PCM data.
	It therefore uses two audio subframes per audio frame.  The subframe size is two bytes and the bit resolution
	is 16 bits.  The Type III Format Type descriptor is identical to the Type I Format Type descriptor but with
	the bNrChannels field set to two, the bSubframeSize field set to two and the bBitResolution field set to 16.
	All the techniques used to correctly transport Type I PCM formatted streams over USB equally apply to Type III
	formatted streams.

	The non-PCM encoded audio bitstreams that are transferred within the basic 16-bit data area of the IEC1937
	subframes (time-slots 12 [LSB] to 27 [MSB]) are placed unaltered in the two available 16-bit audio subframes
	per audio frame of the Type III formatted USB stream.  The additional information in the IEC1937 subframes
	(channel status, user bit, etc.) is discarded.  Refer to the IEC1937 standard for a detailed description of the
	exact contents of the subframes.
*/
typedef struct ASFormatTypeIDescriptor {
    UInt8									bLength;
    UInt8									bDescriptorType;
    UInt8									bDescriptorSubtype;
    UInt8									bFormatType;
    UInt8									bNrChannels;
    UInt8									bSubframeSize;
    UInt8									bBitResolution;
    UInt8									bSamFreqType;
    UInt8									sampleFreq[3];	// sample rates are 24 bit values -- arg!
    //... fill in for sample freqs - probably a union for either a min/max or an array
} ASFormatTypeIDescriptor, *ASFormatTypeIDescriptorPtr;

typedef struct ASFormatTypeIIDescriptor {
    UInt8									bLength;
    UInt8									bDescriptorType;
    UInt8									bDescriptorSubtype;
    UInt8									bFormatType;
	UInt16									wMaxBitRate;
	UInt16									wSamplesPerFrame;
    UInt8									bSamFreqType;
    UInt8									sampleFreq[3];	// sample rates are 24 bit values -- arg!
    //... fill in for sample freqs - probably a union for either a min/max or an array
} ASFormatTypeIIDescriptor, *ASFormatTypeIIDescriptorPtr;

typedef struct ASInterfaceDescriptor {
    UInt8									bLength;
    UInt8									bDescriptorType;
    UInt8									bDescriptorSubtype;
    UInt8									bTerminalLink;
    UInt8									bDelay;
    UInt8									wFormatTag[2];	// because it's not PPC aligned when declared as UInt16
} ASInterfaceDescriptor, *ASInterfaceDescriptorPtr;

typedef struct ASFormatSpecificDescriptorHeader {
    UInt8									bLength;
    UInt8									bDescriptorType;
    UInt8									bDescriptorSubtype;
    UInt8									wFormatTag[2];	// because it's not PPC aligned when declared as UInt16
} ASFormatSpecificDescriptorHeader, *ASFormatSpecificDescriptorHeaderPtr;

typedef struct ASMPEGFormatSpecificDescriptor {
    UInt8									bLength;
    UInt8									bDescriptorType;
    UInt8									bDescriptorSubtype;
    UInt8									wFormatTag[2];	// because it's not PPC aligned when declared as UInt16
	UInt8									bmMPEGCapabilities[2];	// because it's not PPC aligned when declared as UInt16
	UInt8									bmMPEGFeatures;
} ASMPEGFormatSpecificDescriptor, *ASMPEGFormatSpecificDescriptorPtr;

typedef struct ASAC3FormatSpecificDescriptor {
    UInt8									bLength;
    UInt8									bDescriptorType;
    UInt8									bDescriptorSubtype;
    UInt8									wFormatTag[2];	// because it's not PPC aligned when declared as UInt16
	UInt8									bmBSID[4];		// because it's not PPC aligned when declared as UInt32
	UInt8									bmAC3Features;
} ASAC3FormatSpecificDescriptor, *ASAC3FormatSpecificDescriptorPtr;

typedef struct ASEndpointDescriptor {
    UInt8									bLength;
    UInt8									bDescriptorType;
    UInt8									bDescriptorSubtype;
    UInt8									bmAttributes;
    UInt8									bLockDelayUnits;
    UInt8									wLockDelay[2];	// because it's not PPC aligned when declared as UInt16
} ASEndpointDescriptor, *ASEndpointDescriptorPtr;


/* -------------------------------------------------------------------
	Classes
------------------------------------------------------------------- */
class EMUUSBACDescriptorObject : public OSObject {
    OSDeclareDefaultStructors (EMUUSBACDescriptorObject);

private:
	UInt8					unitID;
	UInt8					sourceID;
	UInt8					descriptorSubType;

public:
    virtual void			free (void);

	UInt8					GetDescriptorSubType (void) {return descriptorSubType;}
	virtual UInt8			GetNumInPins (void) {return 1;}
	UInt8					GetSourceID (void) {return sourceID;}
	UInt8					GetUnitID (void) {return unitID;}

	void					SetDescriptorSubType (UInt8 subType) {descriptorSubType = subType;}
	void					SetSourceID (UInt8 srcID) {sourceID = srcID;}
	void					SetUnitID (UInt8 termID) {unitID = termID;}

};

class EMUUSBInputTerminalObject : public EMUUSBACDescriptorObject {
private:
	UInt16							terminalType;
	UInt16							channelConfig;
	UInt8							assocTerminal;
	UInt8							numChannels;

public:
    virtual void					free (void);

	UInt8							GetAssocTerminal (void) {return assocTerminal;}
	UInt16							GetChannelConfig (void) {return channelConfig;}
	UInt8							GetNumChannels (void) {return numChannels;}
	UInt16							GetTerminalType (void) {return terminalType;}

	void							SetAssocTerminal (UInt8 assocTerm) {assocTerminal = assocTerm;}
	void							SetChannelConfig (UInt16 chanConfig) {channelConfig = chanConfig;}
	void							SetNumChannels (UInt8 numChan) {numChannels = numChan;}
	void							SetTerminalType (UInt16 termType) {terminalType = termType;}
};

class EMUUSBOutputTerminalObject : public EMUUSBACDescriptorObject {
private:
	UInt16							terminalType;
	UInt8							assocTerminal;

public:
    virtual void					free (void);

	UInt8							GetAssocTerminal (void) {return assocTerminal;}
	UInt16							GetTerminalType (void) {return terminalType;}

	void							SetAssocTerminal (UInt8 assocTerm) {assocTerminal = assocTerm;}
	void							SetTerminalType (UInt16 termType) {terminalType = termType;}
};

class EMUUSBMixerUnitObject : public EMUUSBACDescriptorObject {
private:
	UInt8 *							baSourceIDs;		// there are numInPins of these
	UInt8 *							bmControls;			// you don't know the size of this, calculate it using bLength and numInPins
	UInt16							channelConfig;
	UInt8							controlSize;		// This is the calculated size of bmControls
	UInt8							numInPins;
	UInt8							numChannels;

public:
    virtual void					free (void);

	UInt16							GetChannelConfig (void) {return channelConfig;}
	virtual UInt8					GetNumInPins (void) {return numInPins;}
	UInt8							GetNumChannels (void) {return numChannels;}
	UInt8 *							GetSources (void) {return baSourceIDs;}

	void							InitControlsArray (UInt8 * channelConfig, UInt8 bmControlSize);
	void							InitSourceIDs (UInt8 * baSrcIDs, UInt8 nrInPins);
	void							SetChannelConfig (UInt16 chanConfig) {channelConfig = chanConfig;}
	void							SetNumChannels (UInt8 numChan) {numChannels = numChan;}
};

class EMUUSBSelectorUnitObject : public EMUUSBACDescriptorObject {
private:
	UInt8 *							baSourceIDs;		// there are numInPins of these
	UInt8							numInPins;

public:
	virtual void					free (void);

	virtual UInt8					GetNumInPins (void) {return numInPins;}
	UInt8 *							GetSelectorSources (void) {return baSourceIDs;}

	void							InitSourceIDs (UInt8 * baSrcIDs, UInt8 nrInPins);
};

class EMUUSBProcessingUnitObject : public EMUUSBACDescriptorObject {
private:
	UInt8 *							bmControls;			// you don't know the size of this, calculate it using bLength and numInPins
	UInt8 *							baSourceIDs;		// there are numInPins of these
	UInt16							processType;
	UInt16							channelConfig;
	UInt8							numInPins;
	UInt8							controlSize;		// This is the calculated size of bmControls
	UInt8							numChannels;

public:
	virtual void					free (void);

	virtual UInt8					GetNumInPins (void) {return numInPins;}
	UInt8 *							GetSources (void) {return baSourceIDs;}

	void							InitSourceIDs (UInt8 * baSrcIDs, UInt8 nrInPins);
	void							InitControlsArray (UInt8 * bmCntrls, UInt8 bmControlSize);
	void							SetChannelConfig (UInt16 chanConfig) {channelConfig = chanConfig;}
	void							SetProcessType (UInt16 wProcessType) {processType = wProcessType;}
	void							SetNumChannels (UInt8 numChan) {numChannels = numChan;}
};

class EMUUSBExtensionUnitObject : public EMUUSBACDescriptorObject {
private:
	UInt8 *				bmControls;			// you don't know the size of this, calculate it using bLength and numInPins
	UInt8 *				baSourceIDs;		// there are numInPins of these
	UInt16				extensionCode;
	UInt16				channelConfig;
	UInt8				numInPins;
	UInt8				controlSize;		// This is the calculated size of bmControls
	UInt8				numChannels;

public:
	virtual void		free (void);

	virtual UInt8		GetNumInPins (void) {return numInPins;}
	UInt8 *				GetSources (void) {return baSourceIDs;}

	void				InitControlsArray (UInt8 * channelConfig, UInt8 bmControlSize);
	void				InitSourceIDs (UInt8 * baSrcIDs, UInt8 nrInPins);
	UInt16				GetExtensionCode() {return extensionCode;}
	void				SetExtensionCode(UInt16	xuCode) {extensionCode = xuCode;}
	void				SetChannelConfig (UInt16 chanConfig) {channelConfig = chanConfig;}
	void				SetNumChannels (UInt8 numChan) {numChannels = numChan;}
};

class EMUUSBFeatureUnitObject : public EMUUSBACDescriptorObject {
private:
	UInt8 *							bmaControls;
	UInt8							controlSize;
	UInt8							numControls;

public:
    virtual void					free (void);

	Boolean							ChannelHasControl(UInt8 channelNum, UInt8 controlMask);
	UInt8 *							GetControlsBMA (void) {return bmaControls;}
	UInt8							GetControlSize (void) {return controlSize;}
	UInt8							GetNumControls (void) {return numControls;}
	Boolean							MasterHasMuteControl (void);

	void							InitControlsArray (UInt8 * bmaControlsArrary, UInt8 numCntrls);
	void							SetControlSize (UInt8 cntrlSize) {controlSize = cntrlSize;}
};

class EMUUSBAudioControlObject : public OSObject {
	friend class EMUUSBAudioConfigObject;

    OSDeclareDefaultStructors (EMUUSBAudioControlObject);

private:
    OSArray *						mInputTerminals;
    OSArray *						mOutputTerminals;
	OSArray *						mFeatureUnits;
	OSArray *						mMixerUnits;
	OSArray *						mSelectorUnits;
	OSArray *						mProcessingUnits;
	OSArray *						mExtensionUnits;
    UInt16							adcVersion;			// in BCD
    UInt8							interfaceNumber;
    UInt8							alternateSetting;
    UInt8							numEndpoints;
    UInt8							interfaceClass;		// should always be 1
    UInt8							interfaceSubClass;	// should always be 1
    UInt8							interfaceProtocol;
    UInt8							numStreamInterfaces;
	UInt8 *							streamInterfaceNumbers;
	UInt8							GetNumExtensionUnits (void);
	EMUUSBFeatureUnitObject *			GetFeatureUnitObject (UInt8 unitID);
	EMUUSBFeatureUnitObject *			GetIndexedFeatureUnitObject (UInt8 index);
	EMUUSBInputTerminalObject *		GetInputTerminalObject (UInt8 unitID);
	EMUUSBMixerUnitObject *			GetIndexedMixerUnitObject (UInt8 index);
	EMUUSBSelectorUnitObject *			GetIndexedSelectorUnitObject (UInt8 index);
	EMUUSBMixerUnitObject *			GetMixerObject (UInt8 unitID);
	EMUUSBOutputTerminalObject *		GetOutputTerminalObject (UInt8 unitID);
	EMUUSBSelectorUnitObject *			GetSelectorUnitObject (UInt8 unitID);
	EMUUSBProcessingUnitObject *		GetProcessingUnitObject (UInt8 unitID);
	EMUUSBExtensionUnitObject *		GetExtensionUnitObject (UInt8 unitID);
	EMUUSBACDescriptorObject *			GetACDescriptorObject (UInt8 unitID);

    void							SetAlternateSetting (UInt8 altIntNumber) {alternateSetting = altIntNumber;}
    void							SetInterfaceClass (UInt8 intClass) {interfaceClass = intClass;}
    void							SetInterfaceNumber (UInt8 intNumber) {interfaceNumber = intNumber;}
    void							SetInterfaceProtocol (UInt8 intProtocol) {interfaceProtocol = intProtocol;}
    void							SetInterfaceSubClass (UInt8 intSubClass) {interfaceSubClass = intSubClass;}
    void							SetNumEndpoints (UInt8 numEndpts) {numEndpoints = numEndpts;}

public:
    static EMUUSBAudioControlObject * 	create (void);
    virtual void					free (void);
    USBInterfaceDescriptorPtr		ParseACInterfaceDescriptor (USBInterfaceDescriptorPtr theInterfacePtr, UInt8 const currentInterface);
	UInt8							GetExtensionUnitID(UInt16 extCode);
	UInt8							GetNumControls (UInt8 featureUnitID);
	Boolean							ChannelHasMuteControl (UInt8 featureUnitID, UInt8 channelNum);
	Boolean							ChannelHasVolumeControl (UInt8 featureUnitID, UInt8 channelNum);
	Boolean							ChannelHasControl(UInt8 featureUnitID, UInt8 channelNum, UInt8 controlMask);
	UInt16							GetADCVersion (void) {return adcVersion;}
    UInt8							GetAltInterfaceNum (void) {return alternateSetting;}
	UInt8							GetFeatureSourceID (UInt8 featureUnitID);
	UInt8							GetFeatureUnitID (UInt8 featureUnitID);
	UInt8							GetFeatureUnitIDConnectedToOutputTerminal (UInt8 outputTerminalID);
//	UInt8							GetFirstASInterface (void) {return firstASInterface;}
	UInt8							GetIndexedFeatureUnitID (UInt8 featureUnitIndex);
	UInt8							GetIndexedMixerUnitID (UInt8 mixerUnitIndex);
	UInt8							GetIndexedSelectorUnitID (UInt8 selectorUnitIndex);
	UInt16							GetIndexedInputTerminalType (UInt8 index);
	UInt8							GetIndexedInputTerminalID (UInt8 index);
	UInt8							GetIndexedOutputTerminalID (UInt8 index);
	UInt16							GetIndexedOutputTerminalType (UInt8 index);
	UInt16							GetInputTerminalType (UInt8 terminalID);
    UInt8							GetInterfaceNum (void) {return interfaceNumber;}
    UInt8							GetInterfaceClass (void) {return interfaceClass;}
	UInt8							GetInterfaceProtocol (void) {return interfaceProtocol;}
    UInt8							GetInterfaceSubClass (void) {return interfaceSubClass;}
	UInt8							GetNumEndpoints (void) {return numEndpoints;}
	UInt8							GetNumInputTerminals (void);
	UInt8							GetNumOutputTerminals (void);
	UInt8							GetNumSelectorUnits (void);
	UInt8							GetNumSelectorSources (UInt8 unitID);
	UInt8							GetNumSources (UInt8 unitID);
	UInt8							GetNumStreamInterfaces (void) {return numStreamInterfaces;}
	UInt8 *							GetStreamInterfaceNumbers (void) {return streamInterfaceNumbers;}
	UInt16							GetOutputTerminalType (UInt8 terminalID);
	UInt8 *							GetSelectorSources (UInt8 unitID);
	UInt8 *							GetMixerSources (UInt8 unitID);
	UInt8 *							GetExtensionUnitSources (UInt8 unitID);
	UInt8 *							GetProcessingUnitSources (UInt8 unitID);
	UInt8							GetSourceID (UInt8 unitID);
	UInt8 *							GetSourceIDs (UInt8 unitID);
	UInt8							GetSubType (UInt8 unitID);
	Boolean							MasterHasMuteControl (UInt8 featureUnitID);
};

class EMUUSBEndpointObject : public OSObject {

    OSDeclareDefaultStructors (EMUUSBEndpointObject);

private:
    UInt8							address;
    UInt8							attributes;
    UInt16							maxPacketSize;
    UInt8							synchAddress;
	UInt8							refreshInt;		// interpreted as 2^(10-refreshInt)ms between refreshes
	UInt8							pollInt;
public:
    static EMUUSBEndpointObject * 		create (void);
    virtual void					free (void);

	UInt8							GetAddress (void) {return address;}
	UInt8							GetAttributes (void) {return attributes;}
	UInt8							GetDirection (void) {return ((address & 0x80) >> 7);}
	UInt16							GetMaxPacketSize (void) {return maxPacketSize;}
	UInt8							GetSynchAddress (void) {return synchAddress;}
	UInt8							GetSyncType (void) {return (attributes >> 2);}
	UInt8							GetRefreshInt (void) {return refreshInt;}
	UInt8							GetPollInt(void) {return pollInt;}
	
    void							SetAddress (UInt8 theAddr) {address = theAddr;}
    void							SetAttributes (UInt8 theAttributes) {attributes = theAttributes;}
    void							SetMaxPacketSize (UInt16 maxPacket) {maxPacketSize = maxPacket;}
    void							SetSynchAddress (UInt8 synchAddr) {synchAddress = synchAddr;}
	void							SetRefreshInt (UInt8 refresh) {refreshInt = refresh;}
	void							SetPollInt(UInt8 bInterval) {pollInt = bInterval;}
};

class EMUUSBCSASIsocADEndpointObject {
private:
    Boolean							sampleFreqControl;
    Boolean							pitchControl;
    Boolean							maxPacketsOnly;
    UInt8							lockDelayUnits;
	UInt16							lockDelay;
    UInt8							attributes;

	// You shouldn't need to get the attributes directly, instead use DoesMaxPacketsOnly (), HasPitchControl (), and HasSampleFreqControl ()
	UInt8							GetAttributes (void) {return attributes;}

public:
    EMUUSBCSASIsocADEndpointObject (Boolean theSampleFreqControl, Boolean thePitchControl, Boolean theMaxPacketsOnly, UInt8 theLockDelayUnits, UInt16 theLockDelay);

	Boolean							DoesMaxPacketsOnly (void) {return maxPacketsOnly;}
	UInt8							GetLockDelay (void) {return lockDelay;}
	UInt8							GetLockDelayUnits (void) {return lockDelayUnits;}
	Boolean							HasPitchControl (void) {return pitchControl;}
	Boolean							HasSampleFreqControl (void) {return sampleFreqControl;}
};

class EMUUSBAudioStreamObject : public OSObject {
	friend class EMUUSBAudioConfigObject;

    OSDeclareDefaultStructors (EMUUSBAudioStreamObject);

private:
    OSArray *						theEndpointObjects;
    EMUUSBCSASIsocADEndpointObject *	theIsocEndpointObject;

    UInt32 *						sampleFreqs;
	UInt32							bmAC3BSID;
	UInt16							bmMPEGCapabilities; 
    UInt16							formatTag;
	UInt16							maxBitRate;
	UInt16							samplesPerFrame;
	UInt8							bmMPEGFeatures;
	UInt8							bmAC3Features;
    UInt8							interfaceNumber;
    UInt8							alternateSetting;
    UInt8							numEndpoints;
    UInt8							interfaceClass;		// should always be 1
    UInt8							interfaceSubClass;	// should always be 2
    UInt8							interfaceProtocol;
    UInt8							terminalLink;
    UInt8							delay;
    UInt8							numChannels;
    UInt8							subframeSize;
    UInt8							bitResolution;
    UInt8							numSampleFreqs;

	EMUUSBEndpointObject *				GetEndpointByAddress (UInt8 address);
    EMUUSBEndpointObject *				GetIndexedEndpointObject (UInt8 index);
    EMUUSBCSASIsocADEndpointObject *	GetIndexedASIsocEndpointObject (UInt8 index);
    EMUUSBEndpointObject *				GetEndpointObjectByAddress (UInt8 address);
    EMUUSBCSASIsocADEndpointObject *	GetASIsocEndpointObjectByAddress (UInt8 address);

    void							SetAlternateSetting (UInt8 altIntNumber) {alternateSetting = altIntNumber;}
    void							SetInterfaceClass (UInt8 intClass) {interfaceClass = intClass;}
    void							SetInterfaceNumber (UInt8 intNumber) {interfaceNumber = intNumber;}
    void							SetInterfaceProtocol (UInt8 intProtocol) {interfaceProtocol = intProtocol;}
    void							SetInterfaceSubClass (UInt8 intSubClass) {interfaceSubClass = intSubClass;}
    void							SetNumEndpoints (UInt8 numEndpts) {numEndpoints = numEndpts;}

public:
    static EMUUSBAudioStreamObject *	create (void);
    virtual void					free (void);

    USBInterfaceDescriptorPtr		ParseASInterfaceDescriptor (USBInterfaceDescriptorPtr theInterfacePtr, UInt8 const currentInterface);

	UInt32							GetAC3BSID (void) {return bmAC3BSID;}
    UInt8							GetAltInterfaceNum (void) {return alternateSetting;}
	UInt8							GetDelay (void) {return delay;}
//	UInt8							GetEndpointAddress (void) {if (theEndpointObject) return theEndpointObject->GetAddress (); else return 0;}
//	UInt8							GetEndpointAttributes (void) {if (theEndpointObject) return theEndpointObject->GetAttributes (); else return 0;}
//	UInt8							GetEndpointDirection (void) {if (theEndpointObject) return theEndpointObject->GetDirection (); else return 0;}
	UInt16							GetFormatTag (void) {return formatTag;}
    UInt8							GetInterfaceNum (void) {return interfaceNumber;}
    UInt8							GetInterfaceClass (void) {return interfaceClass;}
	UInt8							GetInterfaceProtocol (void) {return interfaceProtocol;}
    UInt8							GetInterfaceSubClass (void) {return interfaceSubClass;}
	UInt8							GetIsocAssociatedEndpointAddress (UInt8 address);
#if !CUSTOMDEVICE
	UInt8							GetIsocAssociatedEndpointRefreshInt (UInt8 address);
#endif
	UInt8							GetEndpointPollInt(UInt8 address);
	UInt8							GetIsocEndpointAddress (UInt8 direction);
	UInt8							GetIsocEndpointDirection (UInt8 index);
	UInt8							GetIsocEndpointSyncType (UInt8 address);
//	UInt8							GetMaxPacketSize (void) {if (theEndpointObject) return theEndpointObject->GetMaxPacketSize (); else return 0;}
	UInt16							GetMaxBitRate (void) {return maxBitRate;}
    UInt8							GetNumChannels (void) {return numChannels;}
	UInt8							GetNumEndpoints (void) {return numEndpoints;}
    UInt8							GetNumSampleRates (void) {return numSampleFreqs;}
	UInt16							GetSamplesPerFrame (void) {return samplesPerFrame;}
    UInt8							GetSampleSize (void) {return bitResolution;}
    UInt8							GetSubframeSize (void) {return subframeSize;}
    UInt32 *						GetSampleRates (void) {return sampleFreqs;}
    UInt8							GetTerminalLink (void) {return terminalLink;}

	Boolean							IsocEndpointDoesMaxPacketsOnly (void) {if (theIsocEndpointObject) return theIsocEndpointObject->DoesMaxPacketsOnly (); else return 0;}
	UInt8							IsocEndpointGetLockDelay (void) {if (theIsocEndpointObject) return theIsocEndpointObject->GetLockDelay (); else return 0;}
	UInt8							IsocEndpointGetLockDelayUnits (void) {if (theIsocEndpointObject) return theIsocEndpointObject->GetLockDelayUnits (); else return 0;}
	Boolean							IsocEndpointHasPitchControl (void) {if (theIsocEndpointObject) return theIsocEndpointObject->HasPitchControl (); else return 0;}
	Boolean							IsocEndpointHasSampleFreqControl (void) {if (theIsocEndpointObject) return theIsocEndpointObject->HasSampleFreqControl (); else return 0;}
};

class EMUUSBAudioConfigObject : public OSObject {
    OSDeclareDefaultStructors (EMUUSBAudioConfigObject);

private:
    IOUSBConfigurationDescriptor *	theConfigurationDescriptorPtr;
    OSArray *						theControls;
	UInt8							theControlInterfaceNum;
    OSArray *						theStreams;

public:
    static EMUUSBAudioConfigObject *	create (const IOUSBConfigurationDescriptor * newConfigurationDescriptor, UInt8 controlInterfaceNum);
    virtual bool					init (const IOUSBConfigurationDescriptor * newConfigurationDescriptor, UInt8 controlInterfaceNum);
    virtual void					free (void);

	Boolean							ChannelHasMuteControl (UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 featureUnitID, UInt8 channelNum);
	Boolean							ChannelHasVolumeControl (UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 featureUnitID, UInt8 channelNum);
	UInt8							FindNextAltInterfaceWithNumChannels (UInt8 interfaceNum, UInt8 startingAltInterface, UInt8 numChannelsRequested);
	UInt8							FindNextAltInterfaceWithSampleSize (UInt8 interfaceNum, UInt8 startingAltInterface, UInt8 sampleSizeRequested);
	UInt8							FindNextAltInterfaceWithSampleRate (UInt8 interfaceNum, UInt8 startingAltInterface, UInt32 sampleRateRequested);
	UInt8							FindAltInterfaceWithSettings (UInt8 interfaceNum, UInt8 numChannels, UInt8 sampleSize, UInt32 sampleRate = 0);
	UInt32							GetAC3BSID (UInt8 interfaceNum, UInt8 altInterfaceNum);
	UInt8							GetFeatureUnitIDConnectedToOutputTerminal (UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 outputTerminalID);
    UInt8							GetFirstStreamInterfaceNum (void);
	void							GetControlledStreamNumbers (UInt8 **controledStreams, UInt8 *numControledStreams);
    UInt8							GetControlInterfaceNum (void);
	UInt16							GetFormat (UInt8 interfaceNum, UInt8 altInterfaceNum);
	UInt32							GetHighestSampleRate (UInt8 interfaceNum, UInt8 altInterfaceNum);
	UInt32							GetEndpointMaxPacketSize(UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 address);
	UInt8							GetEndpointPollInterval(UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 direction);
	UInt8							GetIsocAssociatedEndpointAddress (UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 address);
#if !CUSTOMDEVICE
	UInt8							GetIsocAssociatedEndpointRefreshInt (UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 address);
#endif
	UInt8							GetEndpointPollInt(UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 address);
	UInt8							GetIsocEndpointAddress (UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 direction);
	UInt8							GetIsocEndpointDirection (UInt8 interfaceNum, UInt8 altInterfaceNum);
	UInt8							GetIsocEndpointSyncType (UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 address);
	UInt8							GetIndexedFeatureUnitID (UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 featureUnitIndex);
	UInt8							GetIndexedMixerUnitID (UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 mixerUnitIndex);
	UInt8							GetIndexedSelectorUnitID (UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 selectorUnitIndex);
	UInt16							GetIndexedInputTerminalType (UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 index);
	UInt8							GetIndexedInputTerminalID (UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 index);
	UInt8							GetIndexedOutputTerminalID (UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 index);
	UInt16							GetIndexedOutputTerminalType (UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 index);
	UInt16							GetInputTerminalType (UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 terminalID);
    UInt8							GetInterfaceClass (UInt8 interfaceNum, UInt8 altInterfaceNum);
    UInt8							GetInterfaceSubClass (UInt8 interfaceNum, UInt8 altInterfaceNum);
	UInt32							GetLowestSampleRate (UInt8 interfaceNum, UInt8 altInterfaceNum);
	UInt16							GetMaxBitRate (UInt8 interfaceNum, UInt8 altInterfaceNum);
    UInt8							GetNumAltStreamInterfaces (UInt8 interfaceNum);
	UInt8							GetNumExtensionUnits(UInt8 interfaceNum, UInt8 altInterfaceNum);
	UInt8							FindExtensionUnitID(UInt8 interfaceNum, UInt16 extCode);
    UInt8							GetNumChannels (UInt8 interfaceNum, UInt8 altInterfaceNum);
	UInt8							GetNumControls (UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 featureUnitID);
    UInt8							GetNumSampleRates (UInt8 interfaceNum, UInt8 altInterfaceNum);
	UInt8							GetNumInputTerminals (UInt8 interfaceNum, UInt8 altInterfaceNum);
	UInt8							GetNumOutputTerminals (UInt8 interfaceNum, UInt8 altInterfaceNum);
	UInt8							GetNumSelectorUnits (UInt8 interfaceNum, UInt8 altInterfaceNum);
	UInt8							GetNumSources (UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 unitID);
    UInt8							GetNumStreamInterfaces (void);
	UInt16							GetOutputTerminalType (UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 terminalID);
	UInt16							GetSamplesPerFrame (UInt8 interfaceNum, UInt8 altInterfaceNum);
    UInt32 *						GetSampleRates (UInt8 interfaceNum, UInt8 altInterfaceNum);
    UInt8							GetSampleSize (UInt8 interfaceNum, UInt8 altInterfaceNum);
	UInt8 *							GetSelectorSources (UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 unitID);
    UInt8							GetSubframeSize (UInt8 interfaceNum, UInt8 altInterfaceNum);
	UInt8							GetSubType (UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 unitID);
	UInt8							GetSourceID (UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 unitID);		// Used for units that have only one input source
	UInt8 *							GetSourceIDs (UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 unitID);		// Used for units that have multiple input sources
    UInt8							GetTerminalLink (UInt8 interfaceNum, UInt8 altInterfaceNum);
	UInt8							GetUnitType (UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 terminalNum);
	Boolean							IsocEndpointDoesMaxPacketsOnly (UInt8 interfaceNum, UInt8 altInterfaceNum);
	UInt8							IsocEndpointGetLockDelay (UInt8 interfaceNum, UInt8 altInterfaceNum);
	UInt8							IsocEndpointGetLockDelayUnits (UInt8 interfaceNum, UInt8 altInterfaceNum);
	Boolean							IsocEndpointHasPitchControl (UInt8 interfaceNum, UInt8 altInterfaceNum);
	Boolean							IsocEndpointHasSampleFreqControl (UInt8 interfaceNum, UInt8 altInterfaceNum);
	Boolean							MasterHasMuteControl (UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 featureUnitID);
	Boolean							VerifySampleRateIsSupported (UInt8 interfaceNum, UInt8 altInterfaceNum, UInt32 verifyRate);

private:
	EMUUSBAudioControlObject *			GetControlObject (UInt8 interfaceNum, UInt8 altInterfaceNum);
    EMUUSBAudioStreamObject *			GetStreamObject (UInt8 interfaceNum, UInt8 altInterfaceNum);
    void							ParseConfigurationDescriptor (void);
    USBInterfaceDescriptorPtr		ParseInterfaceDescriptor (USBInterfaceDescriptorPtr theInterfacePtr, UInt8 * interfaceClass, UInt8 * interfaceSubClass);
	void							DumpConfigMemoryToIOLog (void);

};

#endif
