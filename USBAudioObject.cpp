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
/*
    USB audio class device descriptor abstraction.

    A USB device is described by a chunk of memory on the device. This memory is formatted
	with length bytes so that you can skip data you don't care about. It is aranged into
	a list, starting with the device descriptor, one or more configuration descriptors, 
	one or more stream descriptors, and one or more HID descriptors.  It is possible for
	the configutation and stream descriptors to come in any order and not be contiguous.
*/

#include <libkern/c++/OSArray.h>

#include <IOKit/audio/IOAudioTypes.h>
#include "USBAudioObject.h"
#include "EMUUSBAudioCommon.h"
#include "EMUUSBDeviceDefines.h"

OSDefineMetaClassAndStructors(EMUUSBAudioConfigObject, OSObject);

// 	Public methods
EMUUSBAudioConfigObject * EMUUSBAudioConfigObject::create(const IOUSBConfigurationDescriptor * newConfigurationDescriptor, UInt8 controlInterfaceNum) {
    EMUUSBAudioConfigObject*		configObject = new EMUUSBAudioConfigObject;
debugIOLog("EMUUSBAudioConfigObject::create");
    if (configObject && !configObject->init(newConfigurationDescriptor, controlInterfaceNum)) {
        configObject->release();
        configObject = NULL;
    }
	return configObject;
}

bool EMUUSBAudioConfigObject::init(const IOUSBConfigurationDescriptor * newConfigurationDescriptor, UInt8 controlInterfaceNum) {
    bool	result = false;

	if (OSObject::init() && newConfigurationDescriptor) {
		debugIOLog("EMUUSBAudioConfigObject::init");
		UInt32	length = USBToHostWord(newConfigurationDescriptor->wTotalLength);
		theControlInterfaceNum = controlInterfaceNum;
		theConfigurationDescriptorPtr = (IOUSBConfigurationDescriptor *)IOMalloc(length + 1);
		if (theConfigurationDescriptorPtr) {
			memcpy(theConfigurationDescriptorPtr, newConfigurationDescriptor, length);
			((UInt8 *)theConfigurationDescriptorPtr)[length] = 0;
			ParseConfigurationDescriptor();
			result = true;
			IOFree(theConfigurationDescriptorPtr, length + 1);
			theConfigurationDescriptorPtr = NULL;
		}
	}
    return result;
}

void EMUUSBAudioConfigObject::free (void) {
	if (theControls) {
		theControls->release();
		theControls = NULL;
	}

	if (theStreams) {
		theStreams->release();
		theStreams = NULL;
	}

	if (theConfigurationDescriptorPtr) 
		IOFree (theConfigurationDescriptorPtr, USBToHostWord(theConfigurationDescriptorPtr->wTotalLength) + 1);

    OSObject::free();
}

Boolean EMUUSBAudioConfigObject::ChannelHasMuteControl(UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 featureUnitID, UInt8 channelNum) {
    EMUUSBAudioControlObject *		thisControl = GetControlObject(interfaceNum, altInterfaceNum);
    Boolean						theControl = FALSE;

    if (thisControl)
		theControl = thisControl->ChannelHasMuteControl(featureUnitID, channelNum);

    return theControl;
}

Boolean EMUUSBAudioConfigObject::ChannelHasVolumeControl(UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 featureUnitID, UInt8 channelNum) {
    EMUUSBAudioControlObject *		thisControl = GetControlObject(interfaceNum, altInterfaceNum);
    Boolean						theControl = FALSE;

	if (thisControl)
		theControl = thisControl->ChannelHasVolumeControl(featureUnitID, channelNum);

    return theControl;
}

UInt8 EMUUSBAudioConfigObject::FindNextAltInterfaceWithNumChannels(UInt8 interfaceNum, UInt8 startingAltInterface, UInt8 numChannelsRequested) {
    UInt16			numAltInterfaces = GetNumAltStreamInterfaces(interfaceNum);
	UInt8			altInterface = 255;

	for (UInt8 altInterfaceIndx = startingAltInterface; altInterfaceIndx < numAltInterfaces && altInterface == 255; ++altInterfaceIndx) {
		if (numChannelsRequested == GetNumChannels(interfaceNum, altInterfaceIndx)) 
			altInterface = altInterfaceIndx;
	}

	return altInterface;
}

UInt8 EMUUSBAudioConfigObject::FindNextAltInterfaceWithSampleSize(UInt8 interfaceNum, UInt8 startingAltInterface, UInt8 sampleSizeRequested) {
    UInt16			numAltInterfaces = GetNumAltStreamInterfaces(interfaceNum);
	UInt8			altInterface = 255;

	for (UInt8 altInterfaceIndx = startingAltInterface; altInterfaceIndx < numAltInterfaces && altInterface == 255; ++altInterfaceIndx) {
		if (sampleSizeRequested == GetSampleSize(interfaceNum, altInterfaceIndx)) 
			altInterface = altInterfaceIndx;
	}

	return altInterface;
}

UInt8 EMUUSBAudioConfigObject::FindNextAltInterfaceWithSampleRate(UInt8 interfaceNum, UInt8 startingAltInterface, UInt32 sampleRateRequested) {
	UInt16		numAltInterfaces = GetNumAltStreamInterfaces(interfaceNum);
	UInt8		altInterface = 255;
debugIOLog("FindNextAltInterfaceWithSampleRate sampleRate%d numAltInterfaces %d", sampleRateRequested, numAltInterfaces);
	for (UInt8 altInterfaceIndx = startingAltInterface; altInterfaceIndx < numAltInterfaces && altInterface == 255; ++altInterfaceIndx) {
		if (TRUE == VerifySampleRateIsSupported(interfaceNum, altInterfaceIndx, sampleRateRequested))
			altInterface = altInterfaceIndx;
	}
debugIOLog("FindNextAltInterfaceWithSampleRate altInterface is %d", altInterface);
	return altInterface;
}

UInt8 EMUUSBAudioConfigObject::FindAltInterfaceWithSettings(UInt8 interfaceNum, UInt8 numChannels, UInt8 sampleSize, UInt32 sampleRate) {
	UInt8			potentialAltInterface = 1;
	UInt8			theAltInterface = 255;

	while(potentialAltInterface != 255) {
		if((GetFormat(interfaceNum, potentialAltInterface) & 0x0FFF) != 0) {	// Make sure it's not an undefined format
			potentialAltInterface = FindNextAltInterfaceWithNumChannels(interfaceNum, potentialAltInterface, numChannels);
			if(255 == potentialAltInterface) {
				debugIOLog("Undefined format! No alternate setting ID for interface %d, %d channels", interfaceNum, numChannels);
				break;		// Done, so break out of while loop
			}
			debugIOLog("FindAltInterfaceWithSettings potentialAltInterface %d sampleRate %d", potentialAltInterface, sampleRate);
			if(FindNextAltInterfaceWithSampleSize(interfaceNum, potentialAltInterface, sampleSize) == potentialAltInterface) {
				if(0 != sampleRate) {
					// they want a specific sample rate on this interface
					if(FindNextAltInterfaceWithSampleRate(interfaceNum, potentialAltInterface, sampleRate) == potentialAltInterface) 
						return potentialAltInterface;//	theAltInterface = potentialAltInterface;
				} else {
					// they don't care about the sample rate
					return potentialAltInterface;
				}
			}
			
		} 
		++potentialAltInterface;
	} 
	
	return theAltInterface;
}

UInt32 EMUUSBAudioConfigObject::GetAC3BSID(UInt8 interfaceNum, UInt8 altInterfaceNum) {
	EMUUSBAudioStreamObject*	thisStream = GetStreamObject(interfaceNum, altInterfaceNum);
	UInt32					bmBSID = 0;
	
	if (thisStream)
		bmBSID = thisStream->GetAC3BSID();
	
	return bmBSID;
}

UInt8 EMUUSBAudioConfigObject::GetFeatureUnitIDConnectedToOutputTerminal(UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 outputTerminalID) {
    EMUUSBAudioControlObject*		thisControl = GetControlObject(interfaceNum, altInterfaceNum);
    UInt8						featureUnitID = 0;

	debugIOLog("GetFeatureUnitIDConnectedToOutputTerminal (%d, %d, %d)", interfaceNum, altInterfaceNum, outputTerminalID);
 	debugIOLog("GetControlObject returns %p", thisControl);
    if (thisControl)
		featureUnitID = thisControl->GetFeatureUnitIDConnectedToOutputTerminal(outputTerminalID);

    return featureUnitID;
}

UInt8 EMUUSBAudioConfigObject::GetFirstStreamInterfaceNum(void) {
    EMUUSBAudioStreamObject* 		thisStream = NULL;
	UInt8						interfaceNum = 0;

	if (theStreams) {
		thisStream = OSDynamicCast(EMUUSBAudioStreamObject, theStreams->getObject(0));
		if (thisStream)
			interfaceNum = thisStream->GetInterfaceNum();
	}
    return interfaceNum;
}

void EMUUSBAudioConfigObject::GetControlledStreamNumbers(UInt8 **controledStreams, UInt8 *numControledStreams) {
	*controledStreams = 0;
	*numControledStreams = 0;

	if (NULL != theControls) {
		EMUUSBAudioControlObject* thisControl = OSDynamicCast(EMUUSBAudioControlObject, theControls->getObject (0));
		if (thisControl) {
			*controledStreams = thisControl->GetStreamInterfaceNumbers();
			*numControledStreams = thisControl->GetNumStreamInterfaces();
		}
	}
}

UInt8 EMUUSBAudioConfigObject::GetControlInterfaceNum(void) {
	UInt8		interfaceNum = 0;

	if (NULL != theControls) {
		EMUUSBAudioControlObject* 	thisControl = OSDynamicCast(EMUUSBAudioControlObject, theControls->getObject (0));
		if (thisControl)
			interfaceNum = thisControl->GetInterfaceNum();
	}
    return interfaceNum;
}

UInt16 EMUUSBAudioConfigObject::GetFormat(UInt8 interfaceNum, UInt8 altInterfaceNum) {
	EMUUSBAudioStreamObject* 		thisStream = GetStreamObject(interfaceNum, altInterfaceNum);
	UInt16						formatTag = TYPE_I_UNDEFINED;
	debugIOLog("in GetFormat");
	if (thisStream) {
		formatTag = USBToHostWord(thisStream->GetFormatTag());
		debugIOLog("formatTag is %d", formatTag);
	}
	return formatTag;
}

UInt32 EMUUSBAudioConfigObject::GetHighestSampleRate(UInt8 interfaceNum, UInt8 altInterfaceNum) {
	UInt32*	sampleRates = GetSampleRates(interfaceNum, altInterfaceNum);
	UInt32	highSampleRate = 0;
	if (sampleRates) {
		UInt8		numSampleRates = GetNumSampleRates(interfaceNum, altInterfaceNum);
		UInt8		highest = 0;

		for (UInt32 i = 1; i < numSampleRates; ++i) {
			if (sampleRates[i] > sampleRates[highest])
				highest = i;
		}
		highSampleRate = sampleRates[highest];
	}
	return highSampleRate;
}

UInt8 EMUUSBAudioConfigObject::GetIsocAssociatedEndpointAddress(UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 address) {
	EMUUSBAudioStreamObject* 	thisStream = GetStreamObject(interfaceNum, altInterfaceNum);
	UInt8					assocEndpointAddress = 0;
	
	if (thisStream)
		assocEndpointAddress = thisStream->GetIsocAssociatedEndpointAddress(address);
	
	return assocEndpointAddress;
}
#if !CUSTOMDEVICE
UInt8 EMUUSBAudioConfigObject::GetIsocAssociatedEndpointRefreshInt(UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 address) {
	EMUUSBAudioStreamObject* 		thisStream = GetStreamObject(interfaceNum, altInterfaceNum);
	UInt8						assocEndpointRefresh = 0;
	
	if (thisStream)
		assocEndpointRefresh = thisStream->GetIsocAssociatedEndpointRefreshInt(address);
	
	return assocEndpointRefresh;
}
#endif
UInt8 EMUUSBAudioConfigObject::GetEndpointPollInt(UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 address) {
	EMUUSBAudioStreamObject*	stream = GetStreamObject(interfaceNum, altInterfaceNum);
	UInt8					pollInterval = 0;
	
	if (stream)
		pollInterval = stream->GetEndpointPollInt(address);
	
	return pollInterval;
}

UInt8 EMUUSBAudioConfigObject::GetIsocEndpointAddress(UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 direction) {
	EMUUSBAudioStreamObject* 		thisStream = GetStreamObject(interfaceNum, altInterfaceNum);
	UInt8						endpointAddress = 0;
	
	if (thisStream)
		endpointAddress = thisStream->GetIsocEndpointAddress(direction);
	
	return endpointAddress;
}

UInt32 EMUUSBAudioConfigObject::GetEndpointMaxPacketSize(UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 address) {
	UInt32	maxPacketSize = 0;
	EMUUSBAudioStreamObject*	stream = GetStreamObject(interfaceNum, altInterfaceNum);
	if (stream) {
		EMUUSBEndpointObject*	endpoint = stream->GetEndpointByAddress(address);
		debugIOLog("GetEndpointMaxPacketSize interfaceNum %d altInterfaceNum %d address %d", interfaceNum, altInterfaceNum, address);
		if (endpoint)
			maxPacketSize = endpoint->GetMaxPacketSize();
	}
	return maxPacketSize;
}

UInt8 EMUUSBAudioConfigObject::GetEndpointPollInterval(UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 direction) {
	UInt8	pollInterval = 1;	// default
	EMUUSBAudioStreamObject*	stream = GetStreamObject(interfaceNum, altInterfaceNum);
	if (stream) {
		UInt8	address = stream->GetIsocEndpointAddress(direction);
		EMUUSBEndpointObject*	endpoint = stream->GetEndpointByAddress(address);
		if (endpoint)
			pollInterval = endpoint->GetPollInt();
	}
	return pollInterval;
}

// Use GetTerminalLink to get the unit number of the input or output terminal that the endpoint is associated with.
// With that terminal, you can figure out if it's an input or output terminal, and the direction of the endpoint.

UInt8 EMUUSBAudioConfigObject::GetIsocEndpointDirection(UInt8 interfaceNum, UInt8 altInterfaceNum) {
    EMUUSBAudioStreamObject * 				thisStream = GetStreamObject(interfaceNum, altInterfaceNum);
    EMUUSBAudioControlObject * 			thisControl = GetControlObject(theControlInterfaceNum, 0);
    UInt8								endpointDirection = 0xFF;
	UInt8								numEndpoints = 0;
	UInt8								index = 0;
	UInt8								terminalID;
	UInt8								direction = 0xFF;

  	debugIOLog("GetIsocEndpointDirection(%d, %d), thisStream = %p", interfaceNum, altInterfaceNum, thisStream);
 	debugIOLog("GetIsocEndpointDirection(%d, %d), thisControl = %p", interfaceNum, altInterfaceNum, thisControl);
	if(NULL != thisStream && NULL != thisControl) {
		UInt8	terminalLink = thisStream->GetTerminalLink();		// returns the unitID of the terminal the endpoint goes with
		debugIOLog("GetIsocEndpointDirection(%d, %d), terminalLink = %d", interfaceNum, altInterfaceNum, terminalLink);
		if(0 != terminalLink) {
			UInt8	numOutputs = thisControl->GetNumOutputTerminals();
			debugIOLog("GetIsocEndpointDirection(%d, %d), numOutputs = %d", interfaceNum, altInterfaceNum, numOutputs);
			while(0xFF == direction && index < numOutputs) {
				terminalID = thisControl->GetIndexedOutputTerminalID(index);
				debugIOLog("GetIsocEndpointDirection(%d, %d), terminalID = %d", interfaceNum, altInterfaceNum, terminalID);
				if(terminalID == terminalLink) {
					direction = kUSBIn;
					numEndpoints = numOutputs;
					debugIOLog("GetIsocEndpointDirection(%d, %d), found an output terminal(#%d) at index %d", interfaceNum, altInterfaceNum, terminalID, index);
					break;// get out of the loop
				} 
				++index;
			}

			if(0xFF == direction) {// get the output
				UInt8	numInputs = thisControl->GetNumInputTerminals();
				debugIOLog("GetIsocEndpointDirection(%d, %d), Didn't find an output terminal, checking for an input terminal", interfaceNum, altInterfaceNum);
				debugIOLog("GetIsocEndpointDirection(%d, %d), numInputs = %d", interfaceNum, altInterfaceNum, numInputs);
				index = 0;
				while(0xFF == direction && index < numInputs) {
					terminalID = thisControl->GetIndexedInputTerminalID(index);
					debugIOLog("GetIsocEndpointDirection, terminalID = %d", interfaceNum, altInterfaceNum, terminalID);
					if(terminalID == terminalLink) {
						direction = kUSBOut;
						numEndpoints = numInputs;
						debugIOLog("GetIsocEndpointDirection(%d, %d), found an input terminal(#%d) at index %d", interfaceNum, altInterfaceNum, terminalID, index);
						break;
					}
					++index;
				}
			}
		}

		if(0xFF != direction) {
			for(index = 0; index < numEndpoints; ++index) {
				endpointDirection = thisStream->GetIsocEndpointDirection(index);
				if(endpointDirection == direction) {
					break;		// found the right endpoint, get out of for loop
				}
			}
		}
	}

	debugIOLog("GetIsocEndpointDirection(%d, %d), endpointDirection = %d", interfaceNum, altInterfaceNum, endpointDirection);
    return endpointDirection;
}

UInt8 EMUUSBAudioConfigObject::GetIsocEndpointSyncType(UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 address) {
	EMUUSBAudioStreamObject* 	thisStream = GetStreamObject(interfaceNum, altInterfaceNum);
	UInt8					endpointSyncType = 0;
	
	if (thisStream)
		endpointSyncType = thisStream->GetIsocEndpointSyncType(address);
	
	return endpointSyncType;
}

UInt8 EMUUSBAudioConfigObject::GetIndexedFeatureUnitID(UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 featureUnitIndex) {
    EMUUSBAudioControlObject*		thisControl = GetControlObject(interfaceNum, altInterfaceNum);
    UInt8						featureUnitID = 0;

    if (thisControl)
		featureUnitID = thisControl->GetIndexedFeatureUnitID(featureUnitIndex);

    return featureUnitID;
}

UInt8 EMUUSBAudioConfigObject::GetIndexedMixerUnitID(UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 mixerUnitIndex) {
    EMUUSBAudioControlObject*		thisControl = GetControlObject(interfaceNum, altInterfaceNum);
    UInt8						mixerUnitID = 0;

    if (thisControl)
		mixerUnitID = thisControl->GetIndexedMixerUnitID(mixerUnitIndex);

    return mixerUnitID;
}

UInt8 EMUUSBAudioConfigObject::GetIndexedSelectorUnitID(UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 selectorUnitIndex) {
    EMUUSBAudioControlObject*		thisControl = GetControlObject(interfaceNum, altInterfaceNum);
    UInt8						selectorUnitID = 0;

    if (thisControl)
		selectorUnitID = thisControl->GetIndexedSelectorUnitID(selectorUnitIndex);

    return selectorUnitID;
}

UInt16 EMUUSBAudioConfigObject::GetIndexedInputTerminalType(UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 index) {
    EMUUSBAudioControlObject*		thisControl = GetControlObject(interfaceNum, altInterfaceNum);
    UInt16						terminalType = USBToHostWord(INPUT_UNDEFINED);

    if (thisControl)
		terminalType = USBToHostWord(thisControl->GetIndexedInputTerminalType(index));

    return terminalType;
}

UInt8 EMUUSBAudioConfigObject::GetIndexedInputTerminalID(UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 index) {
    EMUUSBAudioControlObject*		thisControl = GetControlObject(interfaceNum, altInterfaceNum);
    UInt16						terminalID = 0;

    if (thisControl)
		terminalID = thisControl->GetIndexedInputTerminalID(index);

    return terminalID;
}

UInt8 EMUUSBAudioConfigObject::GetIndexedOutputTerminalID(UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 index) {
    EMUUSBAudioControlObject*		thisControl = GetControlObject(interfaceNum, altInterfaceNum);
    UInt16						terminalID = 0;

    if (thisControl)
		terminalID = thisControl->GetIndexedOutputTerminalID(index);

    return terminalID;
}

UInt16 EMUUSBAudioConfigObject::GetIndexedOutputTerminalType(UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 index) {
    EMUUSBAudioControlObject*		thisControl = GetControlObject(interfaceNum, altInterfaceNum);
    UInt16						terminalType = USBToHostWord(OUTPUT_UNDEFINED);

    if (thisControl)
		terminalType = USBToHostWord(thisControl->GetIndexedOutputTerminalType(index));

    return terminalType;
}

UInt16 EMUUSBAudioConfigObject::GetInputTerminalType(UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 terminalNum) {
    EMUUSBAudioControlObject*		thisControl = GetControlObject(interfaceNum, altInterfaceNum);
    UInt16						terminalType = USBToHostWord(INPUT_UNDEFINED);

    if (thisControl)
		terminalType = USBToHostWord(thisControl->GetInputTerminalType(terminalNum));

    return terminalType;
}

UInt8 EMUUSBAudioConfigObject::GetInterfaceClass (UInt8 interfaceNum, UInt8 altInterfaceNum) {
    EMUUSBAudioStreamObject* 		thisStream = GetStreamObject (interfaceNum, altInterfaceNum);
    UInt8						interfaceClass = 0;

    if (thisStream)
        interfaceClass = thisStream->GetInterfaceClass ();

    return interfaceClass;
}

UInt8 EMUUSBAudioConfigObject::GetInterfaceSubClass(UInt8 interfaceNum, UInt8 altInterfaceNum) {
    EMUUSBAudioStreamObject* 		thisStream = GetStreamObject(interfaceNum, altInterfaceNum);
    UInt8						interfaceSubClass = 0;

    if (thisStream)
        interfaceSubClass = thisStream->GetInterfaceSubClass();

    return interfaceSubClass;
}

UInt32 EMUUSBAudioConfigObject::GetLowestSampleRate (UInt8 interfaceNum, UInt8 altInterfaceNum) {
	UInt32*	sampleRates = GetSampleRates(interfaceNum, altInterfaceNum);
	UInt32		lowSampleRate = 0xFFFFFFFF;
	if (sampleRates) {
		UInt8		numSampleRates = GetNumSampleRates(interfaceNum, altInterfaceNum);
		UInt8		lowest = 0;

		for (UInt32 i = 1; i < numSampleRates; ++i) {
			if (sampleRates[i] < sampleRates[lowest])
				lowest = i;
		}
		lowSampleRate = sampleRates[lowest];
	}
	return lowSampleRate;
}

UInt16 EMUUSBAudioConfigObject::GetMaxBitRate(UInt8 interfaceNum, UInt8 altInterfaceNum) {
	EMUUSBAudioStreamObject * 		thisStream = GetStreamObject(interfaceNum, altInterfaceNum);
	UInt16						maxBitRate = 0;
	
	if (thisStream)
		maxBitRate = USBToHostWord(thisStream->GetMaxBitRate());
	
	return maxBitRate;
}

UInt8 EMUUSBAudioConfigObject::GetNumAltStreamInterfaces(UInt8 interfaceNum) {
    UInt16		numAltInterfaces = 0;
	if (NULL != theStreams) {
		UInt16	total = theStreams->getCount();
		for (UInt16 indx = 0; indx < total; ++indx) {
			EMUUSBAudioStreamObject*	stream = OSDynamicCast(EMUUSBAudioStreamObject, theStreams->getObject(indx));
			if (stream && stream->GetInterfaceNum() == interfaceNum) {
				++numAltInterfaces;
			}
		}
	}
    return numAltInterfaces;
}

UInt8 EMUUSBAudioConfigObject::GetNumChannels(UInt8 interfaceNum, UInt8 altInterfaceNum) {
    EMUUSBAudioStreamObject* 	stream = GetStreamObject(interfaceNum, altInterfaceNum);
    UInt8					numChannels = 0;

    if (stream) {
        numChannels = stream->GetNumChannels();
    } else {
		debugIOLog("GetNumChannels: no audio stream object!");
	}
    return numChannels;
}

UInt8 EMUUSBAudioConfigObject::GetNumControls(UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 featureUnitID) {
    EMUUSBAudioControlObject*		control = GetControlObject(interfaceNum, altInterfaceNum);
    UInt8						numControls = 0;

    if (control)
		numControls = control->GetNumControls(featureUnitID);

    return numControls;
}

UInt8 EMUUSBAudioConfigObject::GetNumExtensionUnits(UInt8 interfaceNum, UInt8 altInterfaceNum) {
    EMUUSBAudioControlObject*		control = GetControlObject(interfaceNum, altInterfaceNum);
    UInt8						numExtensions = 0;

    if (control)
		numExtensions = control->GetNumExtensionUnits();
	return numExtensions;
}

UInt8 EMUUSBAudioConfigObject::GetNumSampleRates(UInt8 interfaceNum, UInt8 altInterfaceNum) {
    EMUUSBAudioStreamObject* 	stream = GetStreamObject(interfaceNum, altInterfaceNum);
    UInt8					numSampleRates = 0;

    if (stream)
        numSampleRates = stream->GetNumSampleRates();

    return numSampleRates;
}

UInt8 EMUUSBAudioConfigObject::GetNumInputTerminals(UInt8 interfaceNum, UInt8 altInterfaceNum) {
    EMUUSBAudioControlObject* 	control = GetControlObject(interfaceNum, altInterfaceNum);
	UInt8					numInputTerminals = 0;

    if (control)
		numInputTerminals = control->GetNumInputTerminals();

	return numInputTerminals;
}

UInt8 EMUUSBAudioConfigObject::GetNumOutputTerminals(UInt8 interfaceNum, UInt8 altInterfaceNum) {
    EMUUSBAudioControlObject* 	control = GetControlObject(interfaceNum, altInterfaceNum);
	UInt8					numOutputTerminals = 0;

    if (control)
		numOutputTerminals = control->GetNumOutputTerminals();

	return numOutputTerminals;
}

UInt8 EMUUSBAudioConfigObject::GetNumSelectorUnits(UInt8 interfaceNum, UInt8 altInterfaceNum) {
    EMUUSBAudioControlObject* 	control = GetControlObject(interfaceNum, altInterfaceNum);
	UInt8					numSelectorUnits = 0;

    if (control)
		numSelectorUnits = control->GetNumSelectorUnits();

	return numSelectorUnits;
}


UInt8 EMUUSBAudioConfigObject::GetNumSources(UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 unitID) {
    EMUUSBAudioControlObject* 		control = GetControlObject(interfaceNum, altInterfaceNum);
	UInt8						numSources = 0;

    if (control)
		numSources = control->GetNumSources (unitID);

	return numSources;
}

UInt8 EMUUSBAudioConfigObject::GetNumStreamInterfaces(void) {
	UInt16	numInterfaces = 0;

	if(NULL != theStreams) {
		UInt16	interfaceNum = 0;
		UInt16	total = theStreams->getCount();
		for(UInt16 indx = 0; indx < total; ++indx) {
			EMUUSBAudioStreamObject*	stream = OSDynamicCast(EMUUSBAudioStreamObject, theStreams->getObject(indx));
			if(stream && stream->GetInterfaceNum() != interfaceNum) {
				interfaceNum = stream->GetInterfaceNum();
				++numInterfaces;
			}
		}
	}
    return numInterfaces;
}

UInt16 EMUUSBAudioConfigObject::GetSamplesPerFrame(UInt8 interfaceNum, UInt8 altInterfaceNum) {
	EMUUSBAudioStreamObject * 		stream = GetStreamObject(interfaceNum, altInterfaceNum);
	UInt16						samplesPerFrame = 0;
	
	if(stream)
		samplesPerFrame = USBToHostWord(stream->GetMaxBitRate());
	
	return samplesPerFrame;
}

UInt32 * EMUUSBAudioConfigObject::GetSampleRates(UInt8 interfaceNum, UInt8 altInterfaceNum) {
    EMUUSBAudioStreamObject * 		thisStream = GetStreamObject(interfaceNum, altInterfaceNum);
    UInt32 *					sampleRates = 0;

    if(thisStream)
        sampleRates = thisStream->GetSampleRates();

    return sampleRates;
}

UInt8 EMUUSBAudioConfigObject::GetSampleSize(UInt8 interfaceNum, UInt8 altInterfaceNum) {
    EMUUSBAudioStreamObject * 		stream = GetStreamObject(interfaceNum, altInterfaceNum);
    UInt8						sampleSize = 0;

    if(stream)
        sampleSize = stream->GetSampleSize();

    return sampleSize;
}

UInt8 * EMUUSBAudioConfigObject::GetSelectorSources(UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 unitID) {
    EMUUSBAudioControlObject *		control = GetControlObject(interfaceNum, altInterfaceNum);
    UInt8 *						sources = NULL;

    if(control)
        sources = control->GetSelectorSources(unitID);

    return sources;
}

UInt8 EMUUSBAudioConfigObject::GetSubType(UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 unitID) {
    EMUUSBAudioControlObject *		control = GetControlObject(interfaceNum, altInterfaceNum);
    UInt8						subType = 0;

    if(control)
        subType = control->GetSubType(unitID);

    return subType;
}

UInt8 EMUUSBAudioConfigObject::GetSourceID(UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 unitID) {
    EMUUSBAudioControlObject *		control = GetControlObject(interfaceNum, altInterfaceNum);
    UInt8						sourceID = 0;

    if(control)
        sourceID = control->GetSourceID(unitID);

    return sourceID;
}

UInt8 EMUUSBAudioConfigObject::GetUnitType(UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 unitID) {
    EMUUSBAudioControlObject *		control = GetControlObject(interfaceNum, altInterfaceNum);
    UInt8						unitType = 0;

    if(control)
        unitType = control->GetSourceID(unitID);

    return unitType;
}

UInt8 * EMUUSBAudioConfigObject::GetSourceIDs(UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 unitID) {
    EMUUSBAudioControlObject *		control = GetControlObject(interfaceNum, altInterfaceNum);
    UInt8 *						sourceIDs = 0;

    if(control)
        sourceIDs = control->GetSourceIDs(unitID);

    return sourceIDs;
}

UInt8 EMUUSBAudioConfigObject::GetSubframeSize(UInt8 interfaceNum, UInt8 altInterfaceNum) {
    EMUUSBAudioStreamObject * 		stream = GetStreamObject(interfaceNum, altInterfaceNum);
    UInt8						subframeSize = 0;

    if(stream)
        subframeSize = stream->GetSubframeSize();

    return subframeSize;
}

UInt8 EMUUSBAudioConfigObject::GetTerminalLink(UInt8 interfaceNum, UInt8 altInterfaceNum) {
    EMUUSBAudioStreamObject* 		stream = GetStreamObject(interfaceNum, altInterfaceNum);
    UInt8						terminalLink = 0;

    if(stream)
        terminalLink = stream->GetTerminalLink();

    return terminalLink;
}

UInt16 EMUUSBAudioConfigObject::GetOutputTerminalType(UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 terminalNum) {
    EMUUSBAudioControlObject *	control = GetControlObject(interfaceNum, altInterfaceNum);
    UInt16					terminalType = USBToHostWord(OUTPUT_UNDEFINED);

    if(control)
		terminalType = USBToHostWord(control->GetOutputTerminalType(terminalNum));

    return terminalType;
}

Boolean EMUUSBAudioConfigObject::IsocEndpointDoesMaxPacketsOnly(UInt8 interfaceNum, UInt8 altInterfaceNum) {
    EMUUSBAudioStreamObject * 	thisStream = GetStreamObject(interfaceNum, altInterfaceNum);
	Boolean					maxPacketsOnly = FALSE;

    if(thisStream)
        maxPacketsOnly = thisStream->IsocEndpointDoesMaxPacketsOnly();

    return maxPacketsOnly;
}

UInt8 EMUUSBAudioConfigObject::IsocEndpointGetLockDelay(UInt8 interfaceNum, UInt8 altInterfaceNum) {
    EMUUSBAudioStreamObject * 	thisStream = GetStreamObject(interfaceNum, altInterfaceNum);
	UInt8					lockDelay = 0;

    if(thisStream)
        lockDelay = thisStream->IsocEndpointGetLockDelay();

    return lockDelay;
}

UInt8 EMUUSBAudioConfigObject::IsocEndpointGetLockDelayUnits(UInt8 interfaceNum, UInt8 altInterfaceNum) {
    EMUUSBAudioStreamObject * 		thisStream = GetStreamObject(interfaceNum, altInterfaceNum);
	UInt8						lockDelayUnits = 0;

    if(thisStream)
        lockDelayUnits = thisStream->IsocEndpointGetLockDelayUnits();

    return lockDelayUnits;
}

Boolean EMUUSBAudioConfigObject::IsocEndpointHasPitchControl(UInt8 interfaceNum, UInt8 altInterfaceNum) {
    EMUUSBAudioStreamObject * 		thisStream = GetStreamObject(interfaceNum, altInterfaceNum);
	Boolean						hasPitchCntrl = FALSE;

    if(thisStream)
        hasPitchCntrl = thisStream->IsocEndpointHasPitchControl();

    return hasPitchCntrl;
}

Boolean EMUUSBAudioConfigObject::IsocEndpointHasSampleFreqControl(UInt8 interfaceNum, UInt8 altInterfaceNum) {
    EMUUSBAudioStreamObject * 		thisStream = GetStreamObject(interfaceNum, altInterfaceNum);
	Boolean						hasSampleFreqCntrl = FALSE;

    if(thisStream)
        hasSampleFreqCntrl = thisStream->IsocEndpointHasSampleFreqControl();

    return hasSampleFreqCntrl;
}

Boolean EMUUSBAudioConfigObject::MasterHasMuteControl(UInt8 interfaceNum, UInt8 altInterfaceNum, UInt8 featureUnitID) {
    EMUUSBAudioControlObject *		thisControl = GetControlObject(interfaceNum, altInterfaceNum);
    Boolean						theControl = FALSE;

     if(thisControl)
		theControl = thisControl->MasterHasMuteControl(featureUnitID);

    return theControl;
}

Boolean EMUUSBAudioConfigObject::VerifySampleRateIsSupported(UInt8 interfaceNum, UInt8 altInterfaceNum, UInt32 verifyRate) {
	Boolean		result = FALSE;
	UInt32*		sampleRates = GetSampleRates(interfaceNum, altInterfaceNum);
	UInt8		numSampleRates = GetNumSampleRates(interfaceNum, altInterfaceNum);
debugIOLog("VerifySampleRateIsSupported %d interfaceNum %d altInterfaceNum %d", verifyRate, interfaceNum, altInterfaceNum);
	if(numSampleRates) {
		// There are a discrete number of sample rates supported, so check each one to see if they support the one we want.
		for(UInt8 rateIndx = 0; rateIndx < numSampleRates && result == FALSE; ++rateIndx) 
			result = (sampleRates[rateIndx] == verifyRate) ;
		
	} else if(sampleRates) {
		// There is a range of sample rates supported, so check to see if the one we want is within that range.
		result = (sampleRates[0] <= verifyRate && sampleRates[1] >= verifyRate);
	}
	debugIOLog("verifySampleRateIsSupported %d", result);
	return result;
}

/*
    Private methods
*/
EMUUSBAudioStreamObject * EMUUSBAudioConfigObject::GetStreamObject(UInt8 interfaceNum, UInt8 altInterfaceNum) {
	if(NULL != theStreams) {
		EMUUSBAudioStreamObject*	stream = NULL;
		UInt8					indx = 0;
		UInt8					total = theStreams->getCount();
		while(indx < total) {
			stream = OSDynamicCast(EMUUSBAudioStreamObject, theStreams->getObject(indx));
			if(stream && interfaceNum == stream->GetInterfaceNum()
						&& altInterfaceNum == stream->GetAltInterfaceNum())
				return stream;
			++indx;
		}
	}
    return NULL;
}

EMUUSBAudioControlObject * EMUUSBAudioConfigObject::GetControlObject(UInt8 interfaceNum, UInt8 altInterfaceNum) {
	if(NULL != theControls) {
		EMUUSBAudioControlObject*	control = NULL;
		UInt8					indx = 0;
		UInt8					total = theControls->getCount();
		while(indx < total) {
			control = OSDynamicCast(EMUUSBAudioControlObject, theControls->getObject(indx));
			if(control && interfaceNum == control->GetInterfaceNum() 
					&& altInterfaceNum == control->GetAltInterfaceNum())
				return control;			
			++indx;
		}
	}

    return NULL;
}

void EMUUSBAudioConfigObject::ParseConfigurationDescriptor(void) {
	if (theConfigurationDescriptorPtr && theConfigurationDescriptorPtr->bLength &&
				(CONFIGURATION == theConfigurationDescriptorPtr->bDescriptorType)) {
		USBInterfaceDescriptorPtr			theInterfacePtr;
		EMUUSBAudioControlObject *				theControlObject = NULL;
		EMUUSBAudioStreamObject *				theStreamObject = NULL;
		UInt8 *								streamInterfaceNumbers;
		UInt8								thisInterfaceNumber;
		UInt8								lastInterfaceNumber = 0;
		UInt8								numStreamInterfaces = 0;
		UInt8								numParsedInterfaces = 0;
		bool								haveControlInterface = FALSE;
		bool								foundStreamInterface = FALSE;

		theInterfacePtr =(USBInterfaceDescriptorPtr)((UInt8 *)theConfigurationDescriptorPtr + theConfigurationDescriptorPtr->bLength);
		while(theInterfacePtr && 0 != theInterfacePtr->bLength) {
			if(INTERFACE ==((ACInterfaceDescriptorPtr)theInterfacePtr)->bDescriptorType) {
				UInt8		interfaceClass, interfaceSubClass;

				debugIOLog("in INTERFACE in ParseConfigurationDescriptor");
				thisInterfaceNumber =((ACInterfaceDescriptorPtr)theInterfacePtr)->bInterfaceNumber;
#if !CUSTOMDEVICE
				if(AUDIO ==((ACInterfaceDescriptorPtr)theInterfacePtr)->bInterfaceClass) {
#else
				if(VENDOR_SPECIFIC ==((ACInterfaceDescriptorPtr)theInterfacePtr)->bInterfaceClass) {// changed to VENDOR_SPECIFIC
#endif
					theInterfacePtr = ParseInterfaceDescriptor(theInterfacePtr, &interfaceClass, &interfaceSubClass);
					debugIOLog("theControlInterfaceNum = %d, thisInterfaceNumber = %d", theControlInterfaceNum, thisInterfaceNumber);
					if(AUDIOCONTROL == interfaceSubClass && theControlInterfaceNum == thisInterfaceNumber) {
						debugIOLog("found a AUDIOCONTROL CS_INTERFACE ");
						if(NULL != theControls) 
							theControlObject = OSDynamicCast(EMUUSBAudioControlObject, theControls->getLastObject());
						FailIf(NULL == theControlObject, Exit);
						theInterfacePtr = theControlObject->ParseACInterfaceDescriptor(theInterfacePtr,((ACInterfaceDescriptorPtr)theInterfacePtr)->bInterfaceNumber);
						GetControlledStreamNumbers(&streamInterfaceNumbers, &numStreamInterfaces);
						haveControlInterface = TRUE;
					} else if(haveControlInterface && AUDIOSTREAMING == interfaceSubClass) {
						debugIOLog("in CS_INTERFACE in ParseConfigurationDescriptor");
						for(UInt8 index = 0; index < numStreamInterfaces; ++index) {
							debugIOLog("comparing thisInterfaceNum = %d with %d", thisInterfaceNumber, streamInterfaceNumbers[index]);
							if(thisInterfaceNumber == streamInterfaceNumbers[index]) {
								debugIOLog("found a AUDIOSTREAMING CS_INTERFACE ");
								if(NULL != theStreams) 
									theStreamObject = OSDynamicCast(EMUUSBAudioStreamObject, theStreams->getLastObject());
								
								FailIf(NULL == theStreamObject, Exit);
								theInterfacePtr = theStreamObject->ParseASInterfaceDescriptor(theInterfacePtr,((ACInterfaceDescriptorPtr)theInterfacePtr)->bInterfaceNumber);
								foundStreamInterface = TRUE;
								break;			// Get out of for loop
							}
						}
						if(thisInterfaceNumber != lastInterfaceNumber) {
							lastInterfaceNumber = thisInterfaceNumber;
							++numParsedInterfaces;
							if(numParsedInterfaces > numStreamInterfaces) 
								break;		// Get out of while loop, we've parsed everything associated with this control interface
							
						}
					} else if(MIDISTREAMING == interfaceSubClass) {
						debugIOLog("MIDI, jumping forward %d bytes", theInterfacePtr->bLength);
						for(UInt8 index = 0; index < numStreamInterfaces; ++index) {
							if(thisInterfaceNumber == streamInterfaceNumbers[index]) {
								debugIOLog("MIDI Stuff thisInterfaceNumber %d", thisInterfaceNumber);
								break;
							}
						}
						theInterfacePtr =(USBInterfaceDescriptorPtr)((UInt8 *)theInterfacePtr + theInterfacePtr->bLength);
					} else if(AUDIOCONTROL == interfaceSubClass) {
						debugIOLog("Found a control interface that we don't care about");
						debugIOLog("jumping forward %d bytes",((((ACInterfaceHeaderDescriptorPtr)theInterfacePtr)->wTotalLength[1] << 8) |(((ACInterfaceHeaderDescriptorPtr)theInterfacePtr)->wTotalLength[0])));
						theInterfacePtr =(USBInterfaceDescriptorPtr)((UInt8 *)theInterfacePtr +((((ACInterfaceHeaderDescriptorPtr)theInterfacePtr)->wTotalLength[1] << 8) |(((ACInterfaceHeaderDescriptorPtr)theInterfacePtr)->wTotalLength[0])));
					} else {
						debugIOLog("Unknown, jumping forward %d bytes", theInterfacePtr->bLength);
						theInterfacePtr =(USBInterfaceDescriptorPtr)((UInt8 *)theInterfacePtr + theInterfacePtr->bLength);
					}
				} else {
					debugIOLog("not an audio interface in ParseConfigurationDescriptor, jumping forward %d bytes", theInterfacePtr->bLength);
					theInterfacePtr =(USBInterfaceDescriptorPtr)((UInt8 *)theInterfacePtr + theInterfacePtr->bLength);
				}
			} else {
				debugIOLog("in default in ParseConfigurationDescriptor, jumping forward %d bytes", theInterfacePtr->bLength);
				theInterfacePtr =(USBInterfaceDescriptorPtr)((UInt8 *)theInterfacePtr + theInterfacePtr->bLength);
			}
		}

		if(theControlObject && FALSE == foundStreamInterface) {
			theControls->removeObject(theControls->getCount() - 1);
		}
	}
Exit:
    return;
}

USBInterfaceDescriptorPtr EMUUSBAudioConfigObject::ParseInterfaceDescriptor(USBInterfaceDescriptorPtr theInterfacePtr, UInt8 * interfaceClass, UInt8 * interfaceSubClass) {
	debugIOLog("in ParseInterfaceDescriptor");

	FailIf(NULL == theInterfacePtr, Exit);
	FailIf(0 == theInterfacePtr->bLength, Exit);

	if(NULL != interfaceClass)
		*interfaceClass = theInterfacePtr->bInterfaceClass;
	if(NULL != interfaceSubClass)
		*interfaceSubClass = theInterfacePtr->bInterfaceSubClass;

	if(AUDIOCONTROL == theInterfacePtr->bInterfaceSubClass) {
		debugIOLog("found a AUDIOCONTROL interface");
		if(theControlInterfaceNum == theInterfacePtr->bDescriptorSubtype) {
			EMUUSBAudioControlObject*	theControlObject = EMUUSBAudioControlObject::create();

			FailIf(NULL == theControlObject, Exit);
			theControlObject->SetInterfaceNumber(theInterfacePtr->bDescriptorSubtype);
			theControlObject->SetAlternateSetting(theInterfacePtr->bAlternateSetting);
			theControlObject->SetNumEndpoints(theInterfacePtr->bNumEndpoints);
			theControlObject->SetInterfaceClass(theInterfacePtr->bInterfaceClass);
			theControlObject->SetInterfaceSubClass(theInterfacePtr->bInterfaceSubClass);
			theControlObject->SetInterfaceProtocol(theInterfacePtr->bInterfaceProtocol);

			if(NULL == theControls) {
				theControls = OSArray::withObjects((const OSObject **)&theControlObject, 1);
				FailIf(NULL == theControls, Exit);
			} else {
				theControls->setObject(theControlObject);
			}

			theControlObject->release();
		}
	} else if(AUDIOSTREAMING == theInterfacePtr->bInterfaceSubClass) {
		debugIOLog("found a AUDIOSTREAMING interface");
		EMUUSBAudioStreamObject*	theStreamObject = EMUUSBAudioStreamObject::create();

		FailIf(NULL == theStreamObject, Exit);
		debugIOLog("stream parameters: subtype %d, alternate %d, endpoints %d, class %d, subclass %d, protocol %d",
				theInterfacePtr->bDescriptorSubtype, theInterfacePtr->bAlternateSetting,
				theInterfacePtr->bNumEndpoints,theInterfacePtr->bInterfaceClass,
				theInterfacePtr->bInterfaceSubClass, theInterfacePtr->bInterfaceProtocol);
		theStreamObject->SetInterfaceNumber(theInterfacePtr->bDescriptorSubtype);
		theStreamObject->SetAlternateSetting(theInterfacePtr->bAlternateSetting);
		theStreamObject->SetNumEndpoints(theInterfacePtr->bNumEndpoints);
		theStreamObject->SetInterfaceClass(theInterfacePtr->bInterfaceClass);
		theStreamObject->SetInterfaceSubClass(theInterfacePtr->bInterfaceSubClass);
		theStreamObject->SetInterfaceProtocol(theInterfacePtr->bInterfaceProtocol);

		if(NULL == theStreams) {
			debugIOLog("creating theStreams");
			theStreams = OSArray::withObjects((const OSObject **)&theStreamObject, 1);
			FailIf(NULL == theStreams, Exit);
		} else {
			debugIOLog("adding object to theStreams");
			theStreams->setObject(theStreamObject);
		}

		theStreamObject->release();
	}

	theInterfacePtr =(USBInterfaceDescriptorPtr)((UInt8 *)theInterfacePtr + theInterfacePtr->bLength);

Exit:
	return theInterfacePtr;
}

UInt8 EMUUSBAudioConfigObject::FindExtensionUnitID(UInt8 interfaceNum, UInt16 extCode) {
    EMUUSBAudioControlObject*		control = GetControlObject(interfaceNum, 0);
    UInt8						unitID = 0;
	if (control) 
		unitID = control->GetExtensionUnitID(extCode);
	
	return unitID;
}
/* ------------------------------------------------------
    EMUUSBAudioControlObject
------------------------------------------------------ */
/*
	Public methods
*/
OSDefineMetaClassAndStructors (EMUUSBAudioControlObject, OSObject);

EMUUSBAudioControlObject * EMUUSBAudioControlObject::create(void) {
    EMUUSBAudioControlObject *		controlObject = new EMUUSBAudioControlObject;
debugIOLog("EMUUSBAudioControlObject::create");
    if(controlObject && (false == controlObject->init())) {
        controlObject->release();
        controlObject = 0;
    }

    return controlObject;
}

void EMUUSBAudioControlObject::free(void) {
	if(mInputTerminals) {
		mInputTerminals->release();
		mInputTerminals = NULL;
	}

	if(NULL != mOutputTerminals) {
		mOutputTerminals->release();
		mOutputTerminals = NULL;
	}

	if(NULL != mFeatureUnits) {
		mFeatureUnits->release();
		mFeatureUnits = NULL;
	}

	if(NULL != mMixerUnits) {
		mMixerUnits->release();
		mMixerUnits = NULL;
	}

	if(NULL != mSelectorUnits) {
		mSelectorUnits->release();
		mSelectorUnits = NULL;
	}

	if(NULL != mProcessingUnits) {
		mProcessingUnits->release();
		mProcessingUnits = NULL;
	}

	if(NULL != mExtensionUnits) {
		mExtensionUnits->release();
		mExtensionUnits = NULL;
	}

	if(NULL != streamInterfaceNumbers) {
		IOFree(streamInterfaceNumbers, numStreamInterfaces);
	}
    OSObject::free();
}

UInt8 EMUUSBAudioControlObject::GetNumControls(UInt8 featureUnitID) {
	EMUUSBFeatureUnitObject *		thisFeatureUnit = GetFeatureUnitObject(featureUnitID);
    UInt8						numControls = 0;

	if(thisFeatureUnit)
        numControls = thisFeatureUnit->GetNumControls();

    return numControls;
}

// Channel #1 is left channel, #2 is right channel
Boolean EMUUSBAudioControlObject::ChannelHasControl(UInt8 featureUnitID, UInt8 channelNum, UInt8 controlMask) {
	EMUUSBFeatureUnitObject *		thisFeatureUnit = GetFeatureUnitObject(featureUnitID);
	Boolean						result = FALSE;

	if(thisFeatureUnit)
		result = thisFeatureUnit->ChannelHasControl(channelNum, controlMask);
	return result;
}
Boolean EMUUSBAudioControlObject::ChannelHasMuteControl (UInt8 featureUnitID, UInt8 channelNum) {
	EMUUSBFeatureUnitObject *				thisFeatureUnit;
	Boolean								result;

	result = FALSE;
	thisFeatureUnit = GetFeatureUnitObject (featureUnitID);
	if (thisFeatureUnit)
		result = thisFeatureUnit->ChannelHasControl(channelNum, kMuteBit);

	return result;
}

Boolean EMUUSBAudioControlObject::ChannelHasVolumeControl (UInt8 featureUnitID, UInt8 channelNum) {
	EMUUSBFeatureUnitObject *		thisFeatureUnit = GetFeatureUnitObject (featureUnitID);
	Boolean						result = FALSE;

	if (thisFeatureUnit)
		result = thisFeatureUnit->ChannelHasControl(channelNum, kVolumeBit);

	return result;
}

EMUUSBACDescriptorObject * EMUUSBAudioControlObject::GetACDescriptorObject(UInt8 unitID) {
    EMUUSBACDescriptorObject * 	thisACDescriptorObject = GetInputTerminalObject(unitID);

	thisACDescriptorObject = GetInputTerminalObject(unitID);
	if(!thisACDescriptorObject)
		thisACDescriptorObject = GetOutputTerminalObject(unitID);
	if(!thisACDescriptorObject)
		thisACDescriptorObject = GetMixerObject(unitID);
	if(!thisACDescriptorObject)
		thisACDescriptorObject = GetSelectorUnitObject(unitID);
	if(!thisACDescriptorObject)
		thisACDescriptorObject = GetFeatureUnitObject(unitID);
	if(!thisACDescriptorObject)
		thisACDescriptorObject = GetProcessingUnitObject(unitID);
	if(!thisACDescriptorObject)
		thisACDescriptorObject = GetExtensionUnitObject(unitID);

    return thisACDescriptorObject;
}

UInt8 EMUUSBAudioControlObject::GetFeatureSourceID(UInt8 featureUnitID) {
	EMUUSBFeatureUnitObject *	featureUnit = GetFeatureUnitObject(featureUnitID);
	UInt8					sourceID = 0;

	if(featureUnit)
		sourceID = featureUnit->GetSourceID();

	return sourceID;
}

UInt8 EMUUSBAudioControlObject::GetIndexedFeatureUnitID(UInt8 featureUnitIndex) {
	EMUUSBFeatureUnitObject *		featureUnit = GetIndexedFeatureUnitObject(featureUnitIndex);
	UInt8						unitID = 0;

	if(featureUnit)
		unitID = featureUnit->GetUnitID();

	return unitID;
}

UInt8 EMUUSBAudioControlObject::GetIndexedMixerUnitID(UInt8 mixerUnitIndex) {
	EMUUSBMixerUnitObject *	mixerUnit = GetIndexedMixerUnitObject(mixerUnitIndex);
	UInt8					unitID = 0;

	if(mixerUnit)
		unitID = mixerUnit->GetUnitID();

	return unitID;
}

UInt8 EMUUSBAudioControlObject::GetIndexedSelectorUnitID(UInt8 selectorUnitIndex) {
	EMUUSBSelectorUnitObject *		selectorUnit = GetIndexedSelectorUnitObject(selectorUnitIndex);
	UInt8						unitID = 0;

	if(selectorUnit)
		unitID = selectorUnit->GetUnitID();

	return unitID;
}

UInt8 EMUUSBAudioControlObject::GetFeatureUnitIDConnectedToOutputTerminal(UInt8 outputTerminalID) {
    EMUUSBOutputTerminalObject *	thisOutputTerminal = GetOutputTerminalObject(outputTerminalID);
	UInt8						outputTerminalSourceID = 0;
	UInt8						featureUnitID = 0;

	thisOutputTerminal = GetOutputTerminalObject(outputTerminalID);
    if(thisOutputTerminal)
		outputTerminalSourceID = thisOutputTerminal->GetSourceID();

	if(0 != outputTerminalSourceID) {
		EMUUSBFeatureUnitObject*	featureUnit = GetFeatureUnitObject(outputTerminalSourceID);
		if(featureUnit)
			featureUnitID = outputTerminalSourceID;
	}

	return featureUnitID;
}

UInt16 EMUUSBAudioControlObject::GetIndexedInputTerminalType(UInt8 index) {
    UInt16		terminalType = USBToHostWord(INPUT_UNDEFINED);

	if(NULL != mInputTerminals)  {
		EMUUSBInputTerminalObject*		inputTerminal = NULL;
		inputTerminal = OSDynamicCast(EMUUSBInputTerminalObject, mInputTerminals->getObject(index));

		if(inputTerminal)
			terminalType = USBToHostWord(inputTerminal->GetTerminalType());
	}
	return terminalType;
}

UInt8 EMUUSBAudioControlObject::GetIndexedInputTerminalID (UInt8 index) {
    EMUUSBInputTerminalObject *	inputTerminal = NULL;
    UInt8						terminalID = 0;

	if (NULL != mInputTerminals) {
		inputTerminal = OSDynamicCast (EMUUSBInputTerminalObject, mInputTerminals->getObject (index));
		if (inputTerminal)
			terminalID = inputTerminal->GetUnitID ();
	}
	return terminalID;
}

UInt8 EMUUSBAudioControlObject::GetIndexedOutputTerminalID (UInt8 index) {
    EMUUSBOutputTerminalObject *		outputTerminal = NULL;
    UInt8							terminalID = 0;

	if (NULL != mOutputTerminals) {
		outputTerminal = OSDynamicCast (EMUUSBOutputTerminalObject, mOutputTerminals->getObject (index));
    if (outputTerminal)
		terminalID = outputTerminal->GetUnitID ();
	}
	return terminalID;
}

UInt16 EMUUSBAudioControlObject::GetIndexedOutputTerminalType(UInt8 index) {
    UInt16		terminalType = USBToHostWord(OUTPUT_UNDEFINED);

	if(NULL != mOutputTerminals) {
		EMUUSBOutputTerminalObject*	outputTerminal = NULL;	
		outputTerminal = OSDynamicCast(EMUUSBOutputTerminalObject, mOutputTerminals->getObject(index));

		if(outputTerminal)
			terminalType = USBToHostWord(outputTerminal->GetTerminalType());
	}
	return terminalType;
}

EMUUSBFeatureUnitObject * EMUUSBAudioControlObject::GetIndexedFeatureUnitObject (UInt8 index) {
	EMUUSBFeatureUnitObject *				indexedFeatureUnit;

	indexedFeatureUnit = NULL;
	if (NULL != mFeatureUnits) {
		indexedFeatureUnit = OSDynamicCast (EMUUSBFeatureUnitObject, mFeatureUnits->getObject (index));
	}

	return indexedFeatureUnit;
}

EMUUSBMixerUnitObject * EMUUSBAudioControlObject::GetIndexedMixerUnitObject (UInt8 index) {
	EMUUSBMixerUnitObject *				indexedMixerUnit;

	indexedMixerUnit = NULL;
	if (NULL != mMixerUnits) {
		indexedMixerUnit = OSDynamicCast (EMUUSBMixerUnitObject, mMixerUnits->getObject (index));
	}

	return indexedMixerUnit;
}

EMUUSBSelectorUnitObject * EMUUSBAudioControlObject::GetIndexedSelectorUnitObject(UInt8 index) {
	EMUUSBSelectorUnitObject *		indexedSelectorUnit = NULL;

	if(NULL != mSelectorUnits)
		indexedSelectorUnit = OSDynamicCast(EMUUSBSelectorUnitObject, mSelectorUnits->getObject(index));

	return indexedSelectorUnit;
}

EMUUSBFeatureUnitObject * EMUUSBAudioControlObject::GetFeatureUnitObject(UInt8 unitID) {
	if(NULL != mFeatureUnits) {
		EMUUSBFeatureUnitObject*	featureUnit = NULL;
		UInt8					indx = 0;
		UInt8					total = mFeatureUnits->getCount();
		while(indx < total) {
			featureUnit = OSDynamicCast(EMUUSBFeatureUnitObject, mFeatureUnits->getObject(indx));
			if(featureUnit && unitID == featureUnit->GetUnitID())
				return featureUnit;
			++indx;
		}
	}
    return NULL;
}

EMUUSBInputTerminalObject * EMUUSBAudioControlObject::GetInputTerminalObject(UInt8 unitID) {
	if(NULL != mInputTerminals) {
		EMUUSBInputTerminalObject*	inputTerminal = NULL;
		UInt8					indx = 0;
		UInt8					total = mInputTerminals->getCount();
		while(indx < total) {
			inputTerminal = OSDynamicCast(EMUUSBInputTerminalObject, mInputTerminals->getObject(indx));
			if(inputTerminal && unitID == inputTerminal->GetUnitID())
				return inputTerminal;
			++indx;
		}
	}
    return NULL;
}

EMUUSBOutputTerminalObject * EMUUSBAudioControlObject::GetOutputTerminalObject(UInt8 unitID) {
	if(NULL != mOutputTerminals) {
		EMUUSBOutputTerminalObject*	outputTerminal = NULL;
		UInt8						indx = 0;
		UInt8						total = mOutputTerminals->getCount();
		while(indx < total) {
			outputTerminal = OSDynamicCast(EMUUSBOutputTerminalObject, mOutputTerminals->getObject(indx));
			if(outputTerminal && unitID == outputTerminal->GetUnitID())
				return outputTerminal;
			
			++indx;
		}
	}
    return NULL;
}

UInt16 EMUUSBAudioControlObject::GetInputTerminalType(UInt8 unitID) {
    EMUUSBInputTerminalObject *	inputTerminal = GetInputTerminalObject(unitID);
    UInt16						terminalType= USBToHostWord(INPUT_UNDEFINED);

    if(inputTerminal)
		terminalType = USBToHostWord(inputTerminal->GetTerminalType());

    return terminalType;
}

UInt8 EMUUSBAudioControlObject::GetNumInputTerminals(void) {
	UInt8		count = 0;

	if(NULL != mInputTerminals)
		count = mInputTerminals->getCount();

	return count;
}

UInt8 EMUUSBAudioControlObject::GetNumOutputTerminals(void) {
	UInt8		count = 0;

	if(NULL != mOutputTerminals) 
		count = mOutputTerminals->getCount();

	return count;
}

UInt8 EMUUSBAudioControlObject::GetNumSelectorUnits(void) {
	UInt8		count = 0;

	if(NULL != mSelectorUnits) 
		count = mSelectorUnits->getCount();

	return count;
}

UInt8 EMUUSBAudioControlObject::GetNumExtensionUnits(void) {
	UInt8	count = 0;
	if (NULL != mExtensionUnits)
		count = mExtensionUnits->getCount();
	return count;
}

UInt8 EMUUSBAudioControlObject::GetNumSources(UInt8 unitID) {
    EMUUSBACDescriptorObject *		processingUnit = GetACDescriptorObject(unitID);
    UInt8						numSources = 0;

    if(processingUnit)
		numSources = processingUnit->GetNumInPins();

    return numSources;
}

UInt8 EMUUSBAudioControlObject::GetSourceID(UInt8 unitID) {
	EMUUSBACDescriptorObject *		unit = GetACDescriptorObject(unitID);
	UInt8						sourceID = 0;

	if(NULL != unit) 
		sourceID = unit->GetSourceID();

	return sourceID;
}

UInt8 * EMUUSBAudioControlObject::GetSourceIDs(UInt8 unitID) {
	EMUUSBACDescriptorObject *		theUnit = GetACDescriptorObject(unitID);
	UInt8 *						sourceIDs = NULL;

	if(NULL != theUnit) {
		switch(theUnit->GetDescriptorSubType()) {
			case MIXER_UNIT:
				sourceIDs = GetMixerSources(unitID);
				break;
			case SELECTOR_UNIT:
				sourceIDs = GetSelectorSources(unitID);
				break;
			case PROCESSING_UNIT:
				sourceIDs = GetProcessingUnitSources(unitID);
				break;
			case EXTENSION_UNIT:
				sourceIDs = GetExtensionUnitSources(unitID);
				break;
		}
	}
	return sourceIDs;
}

UInt8 EMUUSBAudioControlObject::GetSubType(UInt8 unitID) {
	EMUUSBACDescriptorObject *	theUnit = GetACDescriptorObject(unitID);
	UInt8					subType = 0;

	if(NULL != theUnit) 
		subType = theUnit->GetDescriptorSubType();

	return subType;
}

EMUUSBProcessingUnitObject * EMUUSBAudioControlObject::GetProcessingUnitObject(UInt8 unitID) {
	if(NULL != mProcessingUnits) {
		EMUUSBProcessingUnitObject*	processingUnit = NULL;
		UInt8						indx = 0;
		UInt8						total = mProcessingUnits->getCount();
		while(indx < total) {
			processingUnit = OSDynamicCast(EMUUSBProcessingUnitObject, mProcessingUnits->getObject(indx));
			if(processingUnit && unitID == processingUnit->GetUnitID())
				return processingUnit;
			++indx;
		}
	}

    return NULL;
}

EMUUSBMixerUnitObject * EMUUSBAudioControlObject::GetMixerObject(UInt8 unitID) {
	if(NULL != mMixerUnits) {
		EMUUSBMixerUnitObject*	mixerObject = NULL;
		UInt8				indx = 0;
		UInt8				total = mMixerUnits->getCount();
		while(indx < total) {
			mixerObject = OSDynamicCast(EMUUSBMixerUnitObject, mMixerUnits->getObject(indx));
			if(mixerObject && unitID == mixerObject->GetUnitID()) 
				return mixerObject;			
			++indx;
		}
	}

    return NULL;
}

EMUUSBExtensionUnitObject * EMUUSBAudioControlObject::GetExtensionUnitObject(UInt8 unitID) {
	if(NULL != mExtensionUnits) {
		UInt8	indx = 0;
		UInt8	total = mExtensionUnits->getCount();
		EMUUSBExtensionUnitObject*	xu = NULL;
		while(indx < total) {
			xu = OSDynamicCast(EMUUSBExtensionUnitObject, mExtensionUnits->getObject(indx));
			if(xu && unitID == xu->GetUnitID()) 
				return xu;// we're done
			++indx;
		}
	}

    return NULL;
}
UInt8	EMUUSBAudioControlObject::GetExtensionUnitID(UInt16 extCode) {
	if (NULL != mExtensionUnits) {
		UInt8	indx = 0;
		UInt8	total = mExtensionUnits->getCount();
		EMUUSBExtensionUnitObject*	xu = NULL;
		while(indx < total) {
			xu = OSDynamicCast(EMUUSBExtensionUnitObject, mExtensionUnits->getObject(indx));
			if (xu && xu->GetExtensionCode() == extCode)
				return xu->GetUnitID();
			++indx;
		}
	}
	return 0;
}

EMUUSBSelectorUnitObject * EMUUSBAudioControlObject::GetSelectorUnitObject(UInt8 unitID) {
	if(NULL != mSelectorUnits) {
		EMUUSBSelectorUnitObject*	selectorObject = NULL;
		UInt8					indx = 0;
		UInt8					total = mSelectorUnits->getCount();
		while(indx < total) {
			selectorObject = OSDynamicCast(EMUUSBSelectorUnitObject, mSelectorUnits->getObject(indx));
			if(selectorObject && unitID == selectorObject->GetUnitID()) 
				return selectorObject;
			++indx;
		}
	}

    return NULL;
}

UInt16 EMUUSBAudioControlObject::GetOutputTerminalType(UInt8 unitID) {
    EMUUSBOutputTerminalObject *	outputTerminal = GetOutputTerminalObject(unitID);
    UInt16						terminalType = USBToHostWord(OUTPUT_UNDEFINED);

    if(outputTerminal)
		terminalType = USBToHostWord(outputTerminal->GetTerminalType());

    return terminalType;
}

UInt8 * EMUUSBAudioControlObject::GetSelectorSources(UInt8 unitID) {
    EMUUSBSelectorUnitObject *	selectorUnit = GetSelectorUnitObject(unitID);
    UInt8 *					selectorSources = NULL;

    if(selectorUnit)
		selectorSources = selectorUnit->GetSelectorSources();

    return selectorSources;
}

UInt8 * EMUUSBAudioControlObject::GetMixerSources(UInt8 unitID) {
    EMUUSBMixerUnitObject *	mixerUnit = GetMixerObject(unitID);
    UInt8 *					mixerSources = NULL;

    if(mixerUnit)
		mixerSources = mixerUnit->GetSources();

    return mixerSources;
}

UInt8 * EMUUSBAudioControlObject::GetExtensionUnitSources(UInt8 unitID) {
    EMUUSBExtensionUnitObject *	extensionUnit = GetExtensionUnitObject(unitID);
    UInt8 *						extensionUnitSources = NULL;

    if(extensionUnit)
		extensionUnitSources = extensionUnit->GetSources();

    return extensionUnitSources;
}

UInt8 * EMUUSBAudioControlObject::GetProcessingUnitSources(UInt8 unitID) {
    EMUUSBProcessingUnitObject *	processingUnit = GetProcessingUnitObject(unitID);
    UInt8 *						processingUnitSources = NULL;

    if(processingUnit)
		processingUnitSources = processingUnit->GetSources();

    return processingUnitSources;
}

Boolean EMUUSBAudioControlObject::MasterHasMuteControl(UInt8 featureUnitID) {
	EMUUSBFeatureUnitObject *	featureUnit = GetFeatureUnitObject(featureUnitID);
	Boolean					result = FALSE;

	if(featureUnit)
		result = featureUnit->ChannelHasControl(0, kMuteBit);

	return result;
}

USBInterfaceDescriptorPtr EMUUSBAudioControlObject::ParseACInterfaceDescriptor(USBInterfaceDescriptorPtr theInterfacePtr, UInt8 const currentInterface) {
    FailIf(NULL == theInterfacePtr, Exit);
    FailIf(0 == theInterfacePtr->bLength, Exit);
    FailIf(CS_INTERFACE != theInterfacePtr->bDescriptorType, Exit);

    while(theInterfacePtr->bLength && CS_INTERFACE == theInterfacePtr->bDescriptorType) {
        switch(theInterfacePtr->bDescriptorSubtype) {
            case HEADER:
                debugIOLog("in HEADER in ParseACInterfaceDescriptor");
                numStreamInterfaces =((ACInterfaceHeaderDescriptorPtr)theInterfacePtr)->bInCollection;
				debugIOLog("numStreamInterfaces = %d", numStreamInterfaces);
				streamInterfaceNumbers =(UInt8 *)IOMalloc(numStreamInterfaces);
				memcpy(streamInterfaceNumbers,((ACInterfaceHeaderDescriptorPtr)theInterfacePtr)->baInterfaceNr, numStreamInterfaces);
                break;
            case INPUT_TERMINAL:
				{
                debugIOLog("in INPUT_TERMINAL in ParseACInterfaceDescriptor");
				EMUUSBInputTerminalObject*		inputTerminal = new EMUUSBInputTerminalObject;
				FailIf(NULL == inputTerminal, Exit);
				inputTerminal->SetDescriptorSubType(theInterfacePtr->bDescriptorSubtype);
                inputTerminal->SetUnitID(((ACInputTerminalDescriptorPtr)theInterfacePtr)->bTerminalID);
                inputTerminal->SetTerminalType(USBToHostWord(((ACInputTerminalDescriptorPtr)theInterfacePtr)->wTerminalType));
                inputTerminal->SetAssocTerminal(((ACInputTerminalDescriptorPtr)theInterfacePtr)->bAssocTerminal);
                inputTerminal->SetNumChannels(((ACInputTerminalDescriptorPtr)theInterfacePtr)->bNrChannels);
                inputTerminal->SetChannelConfig(USBToHostWord(((ACInputTerminalDescriptorPtr)theInterfacePtr)->wChannelConfig));

				if(NULL == mInputTerminals) 
					mInputTerminals = OSArray::withObjects((const OSObject **)&inputTerminal, 1);
				else 
					mInputTerminals->setObject(inputTerminal);

				inputTerminal->release();
				FailIf(NULL == mInputTerminals, Exit);
				}
                break;
            case OUTPUT_TERMINAL:
				{
                debugIOLog("in OUTPUT_TERMINAL in ParseACInterfaceDescriptor");
				EMUUSBOutputTerminalObject*	outputTerminal = new EMUUSBOutputTerminalObject;
				FailIf(NULL == outputTerminal, Exit);
				outputTerminal->SetDescriptorSubType(theInterfacePtr->bDescriptorSubtype);
                outputTerminal->SetUnitID(((ACOutputTerminalDescriptorPtr)theInterfacePtr)->bTerminalID);
                outputTerminal->SetTerminalType(USBToHostWord(((ACOutputTerminalDescriptorPtr)theInterfacePtr)->wTerminalType));
                outputTerminal->SetAssocTerminal(((ACOutputTerminalDescriptorPtr)theInterfacePtr)->bAssocTerminal);
                outputTerminal->SetSourceID(((ACOutputTerminalDescriptorPtr)theInterfacePtr)->bSourceID);

				if(NULL == mOutputTerminals)
					mOutputTerminals = OSArray::withObjects((const OSObject **)&outputTerminal, 1);
				else
					mOutputTerminals->setObject(outputTerminal);

				outputTerminal->release();
				FailIf(NULL == mOutputTerminals, Exit);
				}
                break;
            case FEATURE_UNIT:
				{
				UInt8					numControls;
                debugIOLog("in FEATURE_UNIT in ParseACInterfaceDescriptor");
				EMUUSBFeatureUnitObject*	featureUnit =  new EMUUSBFeatureUnitObject;
				FailIf(NULL == featureUnit, Exit);
				featureUnit->SetDescriptorSubType(theInterfacePtr->bDescriptorSubtype);
                featureUnit->SetUnitID(((ACFeatureUnitDescriptorPtr)theInterfacePtr)->bUnitID);
                featureUnit->SetSourceID(((ACFeatureUnitDescriptorPtr)theInterfacePtr)->bSourceID);
                featureUnit->SetControlSize(((ACFeatureUnitDescriptorPtr)theInterfacePtr)->bControlSize);
                // subtract 7 because that's how many fields are guaranteed to be in the struct
				numControls =(((ACFeatureUnitDescriptorPtr)theInterfacePtr)->bLength - 7) /((ACFeatureUnitDescriptorPtr)theInterfacePtr)->bControlSize;
				debugIOLog("There are %d controls on this feature unit", numControls);
				featureUnit->InitControlsArray(&((ACFeatureUnitDescriptorPtr)theInterfacePtr)->bmaControls[0], numControls);

				if(NULL == mFeatureUnits)
					mFeatureUnits = OSArray::withObjects((const OSObject **)&featureUnit, 1);
				else 
					mFeatureUnits->setObject(featureUnit);

				featureUnit->release();
				FailIf(NULL == mFeatureUnits, Exit);
                break;
				}
            case MIXER_UNIT:
				{
				UInt32				controlSize;
				UInt16 *			channelConfig;
				UInt8				nrChannels;

                debugIOLog("in MIXER_UNIT in ParseACInterfaceDescriptor");
				EMUUSBMixerUnitObject*	mixerUnit = new EMUUSBMixerUnitObject;
				FailIf(NULL == mixerUnit, Exit);
				debugIOLog("descriptor length = %d", theInterfacePtr->bLength);
				mixerUnit->SetDescriptorSubType(theInterfacePtr->bDescriptorSubtype);
				mixerUnit->SetUnitID(((ACMixerUnitDescriptorPtr)theInterfacePtr)->bUnitID);
				debugIOLog("unit ID = %d",((ACMixerUnitDescriptorPtr)theInterfacePtr)->bUnitID);
				debugIOLog("numInPins = %d",((ACMixerUnitDescriptorPtr)theInterfacePtr)->bNrInPins);
				mixerUnit->InitSourceIDs(&((ACMixerUnitDescriptorPtr)theInterfacePtr)->baSourceID[0],((ACMixerUnitDescriptorPtr)theInterfacePtr)->bNrInPins);
				nrChannels =((ACMixerUnitDescriptorPtr)theInterfacePtr)->baSourceID[((ACMixerUnitDescriptorPtr)theInterfacePtr)->bNrInPins];
				debugIOLog("nrChannels = %d", nrChannels);
				mixerUnit->SetNumChannels(nrChannels);
				channelConfig =(UInt16 *)&((ACMixerUnitDescriptorPtr)theInterfacePtr)->baSourceID[((ACMixerUnitDescriptorPtr)theInterfacePtr)->bNrInPins + 1];
				*channelConfig = USBToHostWord(*channelConfig);
				debugIOLog("channelConfig = %d", *channelConfig);
				mixerUnit->SetChannelConfig(*channelConfig);
				controlSize =((ACMixerUnitDescriptorPtr)theInterfacePtr)->bLength - 10 -((ACMixerUnitDescriptorPtr)theInterfacePtr)->bNrInPins;
				debugIOLog("controlSize = %d", controlSize);
				mixerUnit->InitControlsArray(&((ACMixerUnitDescriptorPtr)theInterfacePtr)->baSourceID[((ACMixerUnitDescriptorPtr)theInterfacePtr)->bNrInPins + 3], controlSize);

				if(NULL == mMixerUnits) 
					mMixerUnits = OSArray::withObjects((const OSObject **)&mixerUnit, 1);
				else 
					mMixerUnits->setObject(mixerUnit);

				mixerUnit->release();
				FailIf(NULL == mMixerUnits, Exit);
				break;
				}
            case SELECTOR_UNIT:
				{
                debugIOLog("in SELECTOR_UNIT in ParseACInterfaceDescriptor");
				EMUUSBSelectorUnitObject*	selectorUnit = new EMUUSBSelectorUnitObject;
				FailIf(NULL == selectorUnit, Exit);
				selectorUnit->SetDescriptorSubType(theInterfacePtr->bDescriptorSubtype);
				selectorUnit->SetUnitID(((ACSelectorUnitDescriptorPtr)theInterfacePtr)->bUnitID);
				selectorUnit->InitSourceIDs(&((ACSelectorUnitDescriptorPtr)theInterfacePtr)->baSourceID[0],((ACSelectorUnitDescriptorPtr)theInterfacePtr)->bNrInPins);
				debugIOLog("numInPins on selector = %d",((ACSelectorUnitDescriptorPtr)theInterfacePtr)->bNrInPins);

				if(NULL == mSelectorUnits) 
					mSelectorUnits = OSArray::withObjects((const OSObject **)&selectorUnit, 1);
				else 
					mSelectorUnits->setObject(selectorUnit);
				selectorUnit->release();
				FailIf(NULL == mSelectorUnits, Exit);
				}
				break;
            case PROCESSING_UNIT:
				{
				UInt16 *			channelConfig;
				UInt8				controlSize;
				UInt8				nrChannels;
// pc driver makes additional stuff here - dolby processing, etc to see if all that is necessary
                debugIOLog("in PROCESSING_UNIT in ParseACInterfaceDescriptor");
				EMUUSBProcessingUnitObject*	processingUnit = new EMUUSBProcessingUnitObject;
				FailIf(NULL == processingUnit, Exit);
				processingUnit->SetDescriptorSubType(theInterfacePtr->bDescriptorSubtype);
				processingUnit->SetUnitID(((ACProcessingUnitDescriptorPtr)theInterfacePtr)->bUnitID);
				processingUnit->SetProcessType(((ACProcessingUnitDescriptorPtr)theInterfacePtr)->wProcessType);
				debugIOLog("processing unit type = 0x%x",((ACProcessingUnitDescriptorPtr)theInterfacePtr)->wProcessType);
				processingUnit->InitSourceIDs(&((ACProcessingUnitDescriptorPtr)theInterfacePtr)->baSourceID[0],((ACProcessingUnitDescriptorPtr)theInterfacePtr)->bNrInPins);
				debugIOLog("numInPins = %d",((ACProcessingUnitDescriptorPtr)theInterfacePtr)->bNrInPins);
				nrChannels =((ACProcessingUnitDescriptorPtr)theInterfacePtr)->baSourceID[((ACProcessingUnitDescriptorPtr)theInterfacePtr)->bNrInPins];
				debugIOLog("nrChannels = %d", nrChannels);
				processingUnit->SetNumChannels(nrChannels);
				channelConfig =(UInt16 *)&((ACProcessingUnitDescriptorPtr)theInterfacePtr)->baSourceID[((ACProcessingUnitDescriptorPtr)theInterfacePtr)->bNrInPins + 1];
				*channelConfig = USBToHostWord(*channelConfig);
				debugIOLog("channelConfig = %d", *channelConfig);
				processingUnit->SetChannelConfig(*channelConfig);
				controlSize =((ACProcessingUnitDescriptorPtr)theInterfacePtr)->baSourceID[((ACProcessingUnitDescriptorPtr)theInterfacePtr)->bNrInPins + 4];
				debugIOLog("controlSize = %d", controlSize);
				processingUnit->InitControlsArray(&((ACProcessingUnitDescriptorPtr)theInterfacePtr)->baSourceID[((ACProcessingUnitDescriptorPtr)theInterfacePtr)->bNrInPins + 5], controlSize);

				if(NULL == mProcessingUnits)
					mProcessingUnits = OSArray::withObjects((const OSObject **)&processingUnit, 1);
				 else 
					mProcessingUnits->setObject(processingUnit);	

				processingUnit->release();
				FailIf(NULL == mProcessingUnits, Exit);
				}
				break;
            case EXTENSION_UNIT:
				{
                debugIOLog("in EXTENSION_UNIT in ParseACInterfaceDescriptor");
				EMUUSBExtensionUnitObject*	extensionUnit = new EMUUSBExtensionUnitObject;
				FailIf(NULL == extensionUnit, Exit);
				extensionUnit->SetDescriptorSubType(theInterfacePtr->bDescriptorSubtype);
				extensionUnit->SetUnitID(((ACExtensionUnitDescriptorPtr)theInterfacePtr)->bUnitID);
				extensionUnit->SetExtensionCode(USBToHostWord(((ACExtensionUnitDescriptorPtr) theInterfacePtr)->wExtensionCode));
				extensionUnit->InitSourceIDs(&((ACExtensionUnitDescriptorPtr)theInterfacePtr)->baSourceID[0],((ACExtensionUnitDescriptorPtr)theInterfacePtr)->bNrInPins);

				if(NULL == mExtensionUnits) 
					mExtensionUnits = OSArray::withObjects((const OSObject **)&extensionUnit, 1);
				else 
					mExtensionUnits->setObject(extensionUnit);

				extensionUnit->release();
				FailIf(!mExtensionUnits, Exit);// check for NO mExtensionUnits here to avoid leaking extensionUnit
				}
				break;
            default:
                break;
        }
		theInterfacePtr =(USBInterfaceDescriptorPtr)((UInt8 *)theInterfacePtr + theInterfacePtr->bLength);
    }

Exit:
	debugIOLog("-EMUUSBAudioControlObject::ParseACInterfaceDescriptor(%p, %d)", theInterfacePtr, currentInterface);
    return theInterfacePtr;
}

/* ------------------------------------------------------
    EMUUSBAudioStreamObject
------------------------------------------------------ */
OSDefineMetaClassAndStructors (EMUUSBAudioStreamObject, OSObject);

EMUUSBAudioStreamObject * EMUUSBAudioStreamObject::create(void) {
    EMUUSBAudioStreamObject *	streamObject = new EMUUSBAudioStreamObject;
debugIOLog("EMUUSBAudioStreamObject::create");
    if(streamObject && (FALSE == streamObject->init())) {
        streamObject->release();
        streamObject = NULL;
    }
    return streamObject;
}

void EMUUSBAudioStreamObject::free(void) {
	if(numSampleFreqs)
		IOFree(sampleFreqs, numSampleFreqs * sizeof(UInt32));
	else
		IOFree(sampleFreqs, 2 * sizeof(UInt32));// there is a base line of 2
	if(NULL != theEndpointObjects) {
		theEndpointObjects->release();
		theEndpointObjects = NULL;
	}
	if (theIsocEndpointObject) {
		delete theIsocEndpointObject;
		theIsocEndpointObject = NULL;
	}
    OSObject::free();
}

EMUUSBEndpointObject * EMUUSBAudioStreamObject::GetIndexedEndpointObject(UInt8 index) {// assumes theEndpointObjects exists
	return OSDynamicCast(EMUUSBEndpointObject, theEndpointObjects->getObject(index));
}

UInt8 EMUUSBAudioStreamObject::GetIsocAssociatedEndpointAddress(UInt8 address) {
    UInt8					assocEndpointAddress = 0;
    EMUUSBEndpointObject *		thisEndpoint = GetEndpointByAddress(address);

	if(thisEndpoint) 
		assocEndpointAddress = thisEndpoint->GetSynchAddress();

    return assocEndpointAddress;
}
#if !CUSTOMDEVICE
UInt8 EMUUSBAudioStreamObject::GetIsocAssociatedEndpointRefreshInt(UInt8 address) {
    EMUUSBEndpointObject *		thisEndpoint = GetEndpointByAddress(address);
    UInt8					assocEndpointRefresh = 0;

	if(thisEndpoint) 
		assocEndpointRefresh = thisEndpoint->GetRefreshInt();
	
    return assocEndpointRefresh;
}
#endif
UInt8 EMUUSBAudioStreamObject::GetEndpointPollInt(UInt8 address) {
	EMUUSBEndpointObject*	thisEndpoint = GetEndpointByAddress(address);
	UInt8				pollInterval = 0;
	
	if (thisEndpoint)
		pollInterval = thisEndpoint->GetPollInt();
	return pollInterval;
}

UInt8 EMUUSBAudioStreamObject::GetIsocEndpointAddress(UInt8 direction) {
	debugIOLog("GetIsocEndpointAddress, looking for direction %d", direction);
	if(theEndpointObjects) {
		EMUUSBEndpointObject *		 endpointObject = NULL;
		UInt8	indx = 0;
		UInt8	total = theEndpointObjects->getCount();
		while(indx < total) {
			endpointObject = GetIndexedEndpointObject(indx);
			if(endpointObject && direction == endpointObject->GetDirection()) 
				return endpointObject->GetAddress();			
		++indx;
		}
	}
	return 0;
}

UInt8 EMUUSBAudioStreamObject::GetIsocEndpointDirection(UInt8 index) {
 	UInt8	direction = 0xFF;

	if(theEndpointObjects) {
		EMUUSBEndpointObject *		 endpointObject = NULL;
		endpointObject = GetIndexedEndpointObject(index);

		if(NULL != endpointObject)
			direction = endpointObject->GetDirection();
	}
	return direction;
}

UInt8 EMUUSBAudioStreamObject::GetIsocEndpointSyncType(UInt8 address) {
    EMUUSBEndpointObject *		endpointObject = GetEndpointByAddress(address);
	UInt8					syncType = 0;

	if(NULL != endpointObject)
        syncType = endpointObject->GetSyncType();

	return syncType;
}

inline UInt32 ConvertSampleFreq(UInt8 *p) {	// convert little-endian 24-bit (unsigned) to native 32-bit.
	return ((p[2] << 16) | (p[1] << 8) | p[0]);
}

USBInterfaceDescriptorPtr EMUUSBAudioStreamObject::ParseASInterfaceDescriptor(USBInterfaceDescriptorPtr theInterfacePtr, UInt8 const currentInterface) {
	UInt16		wFormatTag;
    Boolean		done = FALSE;

    FailIf((NULL == theInterfacePtr || !theInterfacePtr->bLength), Exit);
    while(theInterfacePtr->bLength && !done) {
        if(CS_INTERFACE == theInterfacePtr->bDescriptorType) {
            switch(theInterfacePtr->bDescriptorSubtype) {
                case AS_GENERAL:
                    debugIOLog("in AS_GENERAL in ParseASInterfaceDescriptor");
                    terminalLink =((ASInterfaceDescriptorPtr)theInterfacePtr)->bTerminalLink;
                    delay =((ASInterfaceDescriptorPtr)theInterfacePtr)->bDelay;
                    formatTag = USBToHostWord((((ASInterfaceDescriptorPtr)theInterfacePtr)->wFormatTag[1] << 8) |((ASInterfaceDescriptorPtr)theInterfacePtr)->wFormatTag[0]);
                    theInterfacePtr =(USBInterfaceDescriptorPtr)((UInt8 *)theInterfacePtr + theInterfacePtr->bLength);
                    break;
                case FORMAT_TYPE:
                    debugIOLog("in FORMAT_TYPE in ParseASInterfaceDescriptor");
					switch(((ASFormatTypeIDescriptorPtr)theInterfacePtr)->bFormatType) {
						case FORMAT_TYPE_I:
						case FORMAT_TYPE_III:
							debugIOLog("in FORMAT_TYPE_I/FORMAT_TYPE_III in FORMAT_TYPE");
							numChannels =((ASFormatTypeIDescriptorPtr)theInterfacePtr)->bNrChannels;
							subframeSize =((ASFormatTypeIDescriptorPtr)theInterfacePtr)->bSubframeSize;
							bitResolution =((ASFormatTypeIDescriptorPtr)theInterfacePtr)->bBitResolution;
							numSampleFreqs =((ASFormatTypeIDescriptorPtr)theInterfacePtr)->bSamFreqType;
							if(0 != numSampleFreqs) {
								debugIOLog("device has a discrete number of sample rates");
								sampleFreqs =(UInt32 *)IOMalloc(numSampleFreqs * sizeof(UInt32));
								for(UInt32 indx = 0; indx < numSampleFreqs; ++indx) 
									sampleFreqs[indx] = ConvertSampleFreq( &((ASFormatTypeIDescriptorPtr)theInterfacePtr)->sampleFreq[indx * 3]);
							} else {
								debugIOLog("device has a variable number of sample rates");
								sampleFreqs =(UInt32 *)IOMalloc(2 * sizeof(UInt32));
								sampleFreqs[0] = ConvertSampleFreq( &((ASFormatTypeIDescriptorPtr)theInterfacePtr)->sampleFreq[0] );
								sampleFreqs[1] = ConvertSampleFreq( &((ASFormatTypeIDescriptorPtr)theInterfacePtr)->sampleFreq[3] );
							}
							break;
						case FORMAT_TYPE_II:
							debugIOLog("in FORMAT_TYPE_II in FORMAT_TYPE");
							maxBitRate = USBToHostWord(((ASFormatTypeIIDescriptorPtr)theInterfacePtr)->wMaxBitRate);
							samplesPerFrame = USBToHostWord(((ASFormatTypeIIDescriptorPtr)theInterfacePtr)->wSamplesPerFrame);
							numSampleFreqs =((ASFormatTypeIIDescriptorPtr)theInterfacePtr)->bSamFreqType;
							if(0 != numSampleFreqs) {
								debugIOLog("device has a discrete number of sample rates");
								sampleFreqs =(UInt32 *)IOMalloc(numSampleFreqs * sizeof(UInt32));
								for(UInt32 indx = 0; indx < numSampleFreqs; indx++) {
									sampleFreqs[indx] = ConvertSampleFreq( &((ASFormatTypeIIDescriptorPtr)theInterfacePtr)->sampleFreq[indx * 3] );
								}
							} else {
								debugIOLog("device has a variable number of sample rates");
								sampleFreqs =(UInt32 *)IOMalloc(2 * sizeof(UInt32));
								sampleFreqs[0] = ConvertSampleFreq(&((ASFormatTypeIIDescriptorPtr)theInterfacePtr)->sampleFreq[0]);
								sampleFreqs[1] = ConvertSampleFreq(&((ASFormatTypeIIDescriptorPtr)theInterfacePtr)->sampleFreq[3]);
							}
							break;
						default:
							debugIOLog("!!!!unknown format type in FORMAT_TYPE!!!!");
					}
                    theInterfacePtr =(USBInterfaceDescriptorPtr)((UInt8 *)theInterfacePtr + theInterfacePtr->bLength);
                    break;
				case FORMAT_SPECIFIC:
                    debugIOLog("in FORMAT_SPECIFIC in ParseASInterfaceDescriptor");
					wFormatTag = USBToHostWord(((ASFormatSpecificDescriptorHeaderPtr)theInterfacePtr)->wFormatTag[1] << 8 |((ASFormatSpecificDescriptorHeaderPtr)theInterfacePtr)->wFormatTag[0]);
					switch(wFormatTag) {
						case MPEG:
							debugIOLog("in FORMAT_SPECIFIC in MPEG");
							bmMPEGCapabilities = USBToHostWord(
												((ASMPEGFormatSpecificDescriptorPtr)theInterfacePtr)->bmMPEGCapabilities[1] << 8 |
												((ASMPEGFormatSpecificDescriptorPtr)theInterfacePtr)->bmMPEGCapabilities[0]);
							bmMPEGFeatures =((ASMPEGFormatSpecificDescriptorPtr)theInterfacePtr)->bmMPEGFeatures;
							break;
						case AC3:
							debugIOLog("in FORMAT_SPECIFIC in AC3");
							bmAC3BSID = USBToHostLong(
										((ASAC3FormatSpecificDescriptorPtr)theInterfacePtr)->bmBSID[3] << 24 |
										((ASAC3FormatSpecificDescriptorPtr)theInterfacePtr)->bmBSID[2] << 16 |
										((ASAC3FormatSpecificDescriptorPtr)theInterfacePtr)->bmBSID[1] << 8 |
										((ASAC3FormatSpecificDescriptorPtr)theInterfacePtr)->bmBSID[0]);
							bmAC3Features =((ASAC3FormatSpecificDescriptorPtr)theInterfacePtr)->bmAC3Features;
							break;
						default:
							debugIOLog("!!!!unknown format type 0x%x in FORMAT_SPECIFIC!!!!", wFormatTag);
							break;
					}
                    theInterfacePtr =(USBInterfaceDescriptorPtr)((UInt8 *)theInterfacePtr + theInterfacePtr->bLength);
                    break;
                default:
                    debugIOLog("in default in ParseASInterfaceDescriptor");
                    theInterfacePtr =(USBInterfaceDescriptorPtr)((UInt8 *)theInterfacePtr + theInterfacePtr->bLength);
            }
        } else {
            switch(theInterfacePtr->bDescriptorType) {
                case INTERFACE:
                    // need to make a new interface object for this new interface or new alternate interface
                    debugIOLog("in INTERFACE in ParseASInterfaceDescriptor");
                    done = TRUE;
                    break;
                case ENDPOINT:
					{
                    debugIOLog("in ENDPOINT in ParseASInterfaceDescriptor");
					EMUUSBEndpointObject*	endpoint = EMUUSBEndpointObject::create();
					endpoint->SetAddress(((USBEndpointDescriptorPtr)theInterfacePtr)->bEndpointAddress);
                    endpoint->SetAttributes(((USBEndpointDescriptorPtr)theInterfacePtr)->bmAttributes);
                    endpoint->SetMaxPacketSize(USBToHostWord(((USBEndpointDescriptorPtr)theInterfacePtr)->wMaxPacketSize));
					endpoint->SetRefreshInt(((USBEndpointDescriptorPtr)theInterfacePtr)->bRefresh);
					endpoint->SetPollInt(((USBEndpointDescriptorPtr)theInterfacePtr)->bInterval);
#if !CUSTOMDEVICE
					endpoint->SetSynchAddress((((USBEndpointDescriptorPtr)theInterfacePtr)->bSynchAddress | 0x80));
                    debugIOLog("in ENDPOINT in ParseASInterfaceDescriptor endpointAddress %d, maxPacketSize %d, bInterval %d, syncAddress %d",
							((USBEndpointDescriptorPtr)theInterfacePtr)->bEndpointAddress, USBToHostWord(((USBEndpointDescriptorPtr)theInterfacePtr)->wMaxPacketSize),
							((USBEndpointDescriptorPtr)theInterfacePtr)->bInterval, ((USBEndpointDescriptorPtr)theInterfacePtr)->bSynchAddress);
#endif

					if(NULL == theEndpointObjects) 
						theEndpointObjects = OSArray::withObjects((const OSObject **)&endpoint, 1);
					else 
						theEndpointObjects->setObject(endpoint);
					
					endpoint->release();
					FailIf(NULL == theEndpointObjects, Exit);
                    theInterfacePtr =(USBInterfaceDescriptorPtr)((UInt8 *)theInterfacePtr + theInterfacePtr->bLength);
                    }
					break;
                case CS_ENDPOINT:
                    debugIOLog("in CS_ENDPOINT in ParseASInterfaceDescriptor");
                    if(EP_GENERAL ==((ASEndpointDescriptorPtr)theInterfacePtr)->bDescriptorSubtype) {
                        theIsocEndpointObject = new EMUUSBCSASIsocADEndpointObject(((ASEndpointDescriptorPtr)theInterfacePtr)->bmAttributes &(1 << sampleFreqControlBit),
                                                                            ((ASEndpointDescriptorPtr)theInterfacePtr)->bmAttributes &(1 << pitchControlBit),
                                                                            ((ASEndpointDescriptorPtr)theInterfacePtr)->bmAttributes &(1 << maxPacketsOnlyBit),
                                                                            ((ASEndpointDescriptorPtr)theInterfacePtr)->bLockDelayUnits,
																			USBToHostWord(((UInt16 *)((ASEndpointDescriptorPtr)theInterfacePtr)->wLockDelay)[0]));
                    }
                    theInterfacePtr =(USBInterfaceDescriptorPtr)((UInt8 *)theInterfacePtr + theInterfacePtr->bLength);
                    break;
                default:
                    debugIOLog("in default in else in ParseASInterfaceDescriptor");
                    theInterfacePtr =(USBInterfaceDescriptorPtr)((UInt8 *)theInterfacePtr + theInterfacePtr->bLength);
            }
        }
    }
Exit:
	debugIOLog("-EMUUSBAudioStreamObject::ParseASInterfaceDescriptor(%x, %d)", theInterfacePtr, currentInterface);
    return theInterfacePtr;
}
/*
 	 Private methods
*/
EMUUSBEndpointObject * EMUUSBAudioStreamObject::GetEndpointByAddress(UInt8 address) {
	if(theEndpointObjects) {
		EMUUSBEndpointObject *		thisEndpoint = NULL;
		UInt8					indx = 0;
		UInt8	limit = theEndpointObjects->getCount();
		while(indx < limit) {
			thisEndpoint = GetIndexedEndpointObject(indx);
			if(thisEndpoint) {
				UInt8	theAddress = thisEndpoint->GetAddress();
				if(theAddress == address) 
					return thisEndpoint;
			}
			++indx;
		}
	}
    return NULL;
}

EMUUSBEndpointObject * EMUUSBAudioStreamObject::GetEndpointObjectByAddress(UInt8 address) {
	if(theEndpointObjects) {
		EMUUSBEndpointObject*	endpoint = NULL;
		UInt8				indx = 0;
		UInt8				total = theEndpointObjects->getCount();
		while(indx < total) {
			endpoint = OSDynamicCast(EMUUSBEndpointObject, theEndpointObjects->getObject(indx));
			if(endpoint && address == endpoint->GetAddress()) 
				return endpoint;
			++indx;
		}
	}
    return NULL;
}

//UInt8 GetEndpointAttributes (void) {if (theEndpointObject) return theEndpointObject->GetAttributes (); else return 0;}
//UInt8 GetEndpointDirection (void) {if (theEndpointObject) return theEndpointObject->GetDirection (); else return 0;}

/* ------------------------------------------------------
    EMUUSBEndpointObject
------------------------------------------------------ */
OSDefineMetaClassAndStructors (EMUUSBEndpointObject, OSObject);

EMUUSBEndpointObject * EMUUSBEndpointObject::create(void) {
    EMUUSBEndpointObject *		endpointObject = new EMUUSBEndpointObject;
debugIOLog("EMUUSBEndpointObject::create");
    if(endpointObject && (FALSE == endpointObject->init())) {
        endpointObject->release();
        endpointObject = 0;
    }
    return endpointObject;
}

void EMUUSBEndpointObject::free (void) {
    OSObject::free ();
}

/* ------------------------------------------------------
    USBASIsocEndpointObject
------------------------------------------------------ */
EMUUSBCSASIsocADEndpointObject::EMUUSBCSASIsocADEndpointObject (Boolean theSampleFreqControl, Boolean thePitchControl, Boolean theMaxPacketsOnly, UInt8 theLockDelayUnits, UInt16 theLockDelay)
    :	sampleFreqControl				(theSampleFreqControl),
        pitchControl					(thePitchControl),
        maxPacketsOnly					(theMaxPacketsOnly),
        lockDelayUnits					(theLockDelayUnits),
        lockDelay						(theLockDelay)
{
}

/* ------------------------------------------------------
    EMUUSBInputTerminalObject
------------------------------------------------------ */
//OSDefineMetaClassAndStructors (EMUUSBInputTerminalObject, OSObject);

void EMUUSBInputTerminalObject::free (void) {
    EMUUSBACDescriptorObject::free ();
}

/* ------------------------------------------------------
    EMUUSBOutputTerminalObject
------------------------------------------------------ */
//OSDefineMetaClassAndStructors (EMUUSBOutputTerminalObject, OSObject);

void EMUUSBOutputTerminalObject::free (void) {
    EMUUSBACDescriptorObject::free ();
}

/* ------------------------------------------------------
    EMUUSBMixerUnitObject
------------------------------------------------------ */
void EMUUSBMixerUnitObject::free (void) {
	if (bmControls) {
		IOFree(bmControls, controlSize);
		bmControls = NULL;
	}
    EMUUSBACDescriptorObject::free ();
}

void EMUUSBMixerUnitObject::InitControlsArray (UInt8 * bmCntrls, UInt8 bmControlSize) {
	controlSize = bmControlSize;
	bmControls = ((UInt8 *)IOMalloc (bmControlSize));
	if (bmControls)
		memcpy(bmControls, bmCntrls, bmControlSize);
}

void EMUUSBMixerUnitObject::InitSourceIDs (UInt8 * baSrcIDs, UInt8 nrInPins) {
	numInPins = nrInPins;
	baSourceIDs = (UInt8 *)IOMalloc (nrInPins);
	if (baSourceIDs)
		memcpy(baSourceIDs, baSrcIDs, nrInPins);
}

/* ------------------------------------------------------
    EMUUSBSelectorUnitObject
------------------------------------------------------ */
void EMUUSBSelectorUnitObject::free (void) {
	if (baSourceIDs) {
		IOFree(baSourceIDs, numInPins);
		baSourceIDs = NULL;
	}
    EMUUSBACDescriptorObject::free ();
}

void EMUUSBSelectorUnitObject::InitSourceIDs (UInt8 * baSrcIDs, UInt8 nrInPins) {
	numInPins = nrInPins;
	baSourceIDs = (UInt8 *)IOMalloc (nrInPins);
	if (baSourceIDs)
		memcpy (baSourceIDs, baSrcIDs, nrInPins);
}

/* ------------------------------------------------------
    EMUUSBProcessingUnitObject
------------------------------------------------------ */
void EMUUSBProcessingUnitObject::free (void) {
	if (baSourceIDs) {
		IOFree(baSourceIDs, numInPins);
		baSourceIDs = NULL;
	}
    EMUUSBACDescriptorObject::free ();
}

void EMUUSBProcessingUnitObject::InitSourceIDs (UInt8 * baSrcIDs, UInt8 nrInPins) {
	numInPins = nrInPins;
	baSourceIDs = (UInt8 *)IOMalloc (nrInPins);
	if (baSourceIDs)
	memcpy (baSourceIDs, baSrcIDs, nrInPins);
}

void EMUUSBProcessingUnitObject::InitControlsArray (UInt8 * bmCntrls, UInt8 bmControlSize) {
	controlSize = bmControlSize;
	bmControls = ((UInt8 *)IOMalloc (bmControlSize));
	if (bmControls)
		memcpy(bmControls, bmCntrls, bmControlSize);
}

/* ------------------------------------------------------
    EMUUSBFeatureUnitObject
------------------------------------------------------ */
//OSDefineMetaClassAndStructors (EMUUSBFeatureUnitObject, OSObject);

void EMUUSBFeatureUnitObject::free (void) {
	if (bmaControls) {
		IOFree (bmaControls, numControls * controlSize);
		bmaControls = NULL;
	}
    EMUUSBACDescriptorObject::free ();
}

Boolean EMUUSBFeatureUnitObject::MasterHasMuteControl (void) {
	return ChannelHasControl (0, kMuteBit);		// Master channel is always bmaControls[0]
}

// Channel #1 is left channel, #2 is right channel
Boolean EMUUSBFeatureUnitObject::ChannelHasControl(UInt8 channelNum, UInt8 controlMask) {
	Boolean		result = FALSE;
	if(numControls >=(channelNum + 1)) {
		if(1 == controlSize)
			result =((UInt8*)bmaControls)[channelNum] &(1 << controlMask);
		else
			result =((UInt16*)bmaControls)[channelNum] &(1 << controlMask);
	}
	return result;
}

void EMUUSBFeatureUnitObject::InitControlsArray (UInt8 * bmaControlsArrary, UInt8 numCntrls) {
	numControls = numCntrls;
	bmaControls = (UInt8 *)IOMalloc (numControls * controlSize);
	if (bmaControls) {
		memcpy(bmaControls, bmaControlsArrary, numControls * controlSize);
		if (2 == controlSize) {
			for (UInt32 bmaControlIndex = 0; bmaControlIndex < numControls; ++bmaControlIndex) 
				((UInt16 *)bmaControls)[bmaControlIndex] = USBToHostWord (((UInt16 *)bmaControls)[bmaControlIndex]);
		}
	}
}
/* ------------------------------------------------------
    EMUUSBExtensionUnitObject
------------------------------------------------------ */
void EMUUSBExtensionUnitObject::free(void) {
	if (bmControls) {
		IOFree(bmControls, controlSize);
		bmControls = NULL;
	}
    EMUUSBACDescriptorObject::free();
}

void EMUUSBExtensionUnitObject::InitControlsArray(UInt8 * bmCntrls, UInt8 bmControlSize) {
	controlSize = bmControlSize;
	bmControls = ((UInt8 *)IOMalloc(bmControlSize));
	if (bmControls)
		memcpy(bmControls, bmCntrls, bmControlSize);
}

void EMUUSBExtensionUnitObject::InitSourceIDs(UInt8 * baSrcIDs, UInt8 nrInPins) {
	numInPins = nrInPins;
	baSourceIDs = (UInt8 *)IOMalloc(nrInPins);
	if (baSourceIDs)
		memcpy(baSourceIDs, baSrcIDs, nrInPins);
}

/* ------------------------------------------------------
    EMUUSBACDescriptorObject
------------------------------------------------------ */
OSDefineMetaClassAndStructors(EMUUSBACDescriptorObject, OSObject);

void EMUUSBACDescriptorObject::free(void) {
    OSObject::free();
}