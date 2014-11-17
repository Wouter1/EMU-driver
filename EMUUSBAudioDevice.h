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
//--------------------------------------------------------------------------------
//
//	File:		EMUUSBAudioDevice.h
//
//	Contains:	Support for the USB Audio Class Control Interface.
//
//	Technology:	OS X
//
//--------------------------------------------------------------------------------

#ifndef _EMUUSBAUDIODEVICE_H
#define _EMUUSBAUDIODEVICE_H


#include <IOKit/audio/IOAudioDevice.h>
#include <IOKit/audio/IOAudioEngine.h>
#include <IOKit/audio/IOAudioPort.h>
#include <IOKit/audio/IOAudioControl.h>
#include <IOKit/audio/IOAudioStream.h>
#include <IOKit/audio/IOAudioDefines.h>
#include <IOKit/audio/IOAudioSelectorControl.h>
#include <IOKit/audio/IOAudioToggleControl.h>

#include <IOKit/usb/USB.h>
#include <IOKit/usb/IOUSBInterface.h>
#include <IOKit/usb/IOUSBRootHubDevice.h>

#include "EMUUSBAudioEngine.h"
#include "EMUUSBAudioCommon.h"
#include "EMUXUCustomControl.h"
#include "USBAudioObject.h"
#define DIRECTMONITOR		0	// no direct monitor support for now
#define kStringBufferSize				255

enum {
	kVolumeControl						= 1,
	kMuteControl						= 2
};

enum {
	kNegativeInfinity	= 0x8000
};

enum {
	kStatusCheckInterval = 20
};

typedef struct DirectMonCtrlBlock {
	UInt32		monInput;	// input set to monitor
	UInt32		monOutput;	// output set to monitor input
	UInt32		monGain;	// gain setting
	UInt32		monState;	// turn the monitoring on /off
	UInt32		monPan;		// pan setting
} DirectMonCtrlBlock;


class IOUSBInterface;
class EMUXUCustomControl;
class EMUUSBAudioEngine;
class EMUUSBUserClient;
class EMUUSBAudioHardLevelControl;

/*
 * @class EMUUSBAudioDevice
 * @abstract : universal Apple USB driver
 * @discussion : Current version of the driver deals with outputing stereo 16 bits data at 44100kHz
 * It's gonna create an EMUUSBAudioEngine, and a serie
 */

#define kEngine							"engine"
#define kInterface						"interface"
#define kAltSetting						"altsetting"
#define kInputGainControls				"inputgaincontrols"
#define kInputMuteControls				"inputmutecontrols"
#define kPassThruVolControls			"passthrucontrols"
#define kOutputVolControls				"outputvolcontrols"
#define kPassThruPathsArray				"passthrupathsarray"

enum {
	kDefaultSettingLength = 1,
	kStatusPacketSize	= 2	// the data packet size is 2
};

class EMUUSBAudioDevice : public IOAudioDevice {
	friend class EMUUSBAudioEngine;
	friend class EMUXUCustomControl;
    OSDeclareDefaultStructors (EMUUSBAudioDevice);

public:
    IOUSBInterface *		mControlInterface;

protected:
	UInt64						mWallTimePerUSBCycle;
	UInt64						mReferenceUSBFrame;
	AbsoluteTime				mReferenceWallTime;
	UInt64						mNewReferenceUSBFrame;
	AbsoluteTime				mNewReferenceWallTime;
	UInt64						mLastUSBFrame;
	UInt64						mLastWallTimeNanos;
    EMUUSBAudioConfigObject *	mUSBAudioConfig;
	OSArray *				mControlGraph;
    IORecursiveLock *		mInterfaceLock;
	IOUSBPipe*				mStatusPipe;
	IOTimerEventSource*		mStatusCheckTimer;
	IOTimerEventSource *	mUpdateTimer;
	IOMemoryDescriptor *	mStatusBufferDesc;
	IOUSBCompletion			mStatusCheckCompletion;
	thread_call_t			mInitHardwareThread;
	IOUSBController *		mBus;// DT 
	bool					mUHCI;
	UInt32					mQueryXU;// the XU to query
	UInt32					mCurSampleRate;
	OSArray *				mMonoControlsArray;		// this flag is set by EMUUSBAudioEngine::performFormatChange
	OSArray *				mRegisteredEngines;
	UInt32					mAvailXUs;			// what extension unit features are available
	EMUXUCustomControl *	mXUChanged;//IOAudioSelectorControl
	EMUXUCustomControl*		mClockSelector;// custom controls via XU the clock selector
	EMUXUCustomControl*		mDigitalIOStatus;	// digital IO sample rate status
	EMUXUCustomControl*		mDigitalIOSyncSrc;	// digital IO sync or not
	EMUXUCustomControl*		mDigitalIOAsyncSrc;	// digital IO async src or not
	EMUXUCustomControl*		mDigitalIOSPDIF;	// spdif settings
	EMUXUCustomControl*		mDevOptionCtrl;		// device options - currently only soft limit
#if DIRECTMONITOR
	IOAudioToggleControl*	mDirectMonStereo;	// direct monitoring mono/ stereo selector
	IOAudioToggleControl*	mDirectMonitor;		// direct monitoring on/ off switch
	IOAudioSelectorControl*	mMonitorInput;		// direct monitoring input
	IOAudioSelectorControl*	mMonitorOutput;		// direct monitoring output
	IOAudioLevelControl*	mMonitorGain;		// direct monitoring gain
	IOAudioLevelControl* mMonitorPan;		// direct monitoring pan
	// values for the various XU settings arranged from largest to smallest
	DirectMonCtrlBlock		mCurDirMonSettings;
#endif	
	UInt32					mNumEngines;
	UInt32					mAnchorResetCount;

	UInt16*					mDeviceStatusBuffer;	// device status buffer NOT XU setting

	// slots for the various XUs
	UInt8					mClockRateXU;		// clock rate XU id
	UInt8					mClockSrcXU;		// clock source XU id
	UInt8					mDigitalIOXU;		// digital IO XU id
	UInt8					mDeviceOptionsXU;	// device options XU id
#if DIRECTMONITOR
	UInt8					mDirectMonXU;		// direct monitoring XU id
#endif	
	
	IOAudioSelectorControl* mRealClockSelector;
	
	EMUUSBAudioEngine		*mAudioEngine;
	
	IOAudioToggleControl*	mOuputMuteControl;
	EMUUSBAudioHardLevelControl*	mHardwareOutputVolume;
	UInt8					mHardwareOutputVolumeID;
	
	EMUUSBUserClient*		mUserClient;
	
	void  *propertyChangedProc;

	Boolean					mDeviceIsInMonoMode;
	Boolean					mTerminatingDriver;
	void					setupStatusFeedback();
	void					doTimerAction(IOTimerEventSource * timer);
	UInt8					getExtensionUnitID(UInt16 extCode);

	IOReturn				protectedXUChangeHandler(IOAudioControl *audioControl, SInt32 oldValue, SInt32 newValue);
	IOReturn				getAnchorFrameAndTimeStamp(UInt64 *frame, AbsoluteTime *time);
	IOReturn				getFrameAndTimeStamp(UInt64 *frame, AbsoluteTime *time);
	UInt64					jitterFilter(UInt64 prev, UInt64 curr);
	bool					updateWallTimePerUSBCycle();
	static void				TimerAction(OSObject * owner, IOTimerEventSource * sender);
	void					checkUHCI();

    
public:
	UInt8					mInterfaceNum;

	bool					hasSampleRateXU() {return (NULL != mClockRateXU);}
	// all other methods
	virtual	bool			start (IOService * provider);
    virtual	void			free ();
    virtual	void			stop (IOService *provider);
	virtual void			detach (IOService *provider);
    virtual bool			terminate(IOOptionBits options = 0);
	virtual void			close(IOService *forClient, IOOptionBits options = 0);	
	virtual	bool			initHardware (IOService * provider);
	static	void			initHardwareThread (EMUUSBAudioDevice * aua, void * provider);
	static	IOReturn		initHardwareThreadAction (OSObject * owner, void * provider, void * arg2, void * arg3, void * arg4);
	virtual	IOReturn		protectedInitHardware (IOService * provider);
    virtual	IOReturn		message (UInt32 type, IOService * provider, void * arg);
	bool					ControlsStreamNumber (UInt8 streamNumber);
	IOReturn				createControlsForInterface (IOAudioEngine *audioEngine, UInt8 interfaceNum, UInt8 altSettingNum);
	virtual void			setConfigurationApp (const char *bundleID);
	virtual	IOReturn		performPowerStateChange (IOAudioDevicePowerState oldPowerState, IOAudioDevicePowerState newPowerState, UInt32 *microSecsUntilComplete);
	EMUUSBAudioConfigObject * GetUSBAudioConfigObject (void) {return mUSBAudioConfig;}
	virtual	bool			willTerminate (IOService * provider, IOOptionBits options);

	OSArray * 		BuildConnectionGraph (UInt8 controlInterfaceNum);
	OSArray * 		BuildPath (UInt8 controlInterfaceNum, UInt8 startingUnitID, OSArray *allPaths, OSArray * thisPath);
	char * 			TerminalTypeString (UInt16 terminalType);

	SInt32			getEngineInfoIndex (EMUUSBAudioEngine * inAudioEngine);
	IOReturn		doControlStuff (IOAudioEngine *audioEngine, UInt8 interfaceNum, UInt8 altSettingNum);
	IOReturn		doPlaythroughSetup (EMUUSBAudioEngine * usbAudioEngine, OSArray * playThroughPaths, UInt8 interfaceNum, UInt8 altSettingNum);
	IOReturn		addSelectorSourcesToSelectorControl (IOAudioSelectorControl * theSelectorControl, OSArray * arrayOfPathsFromOutputTerminal, UInt32 pathsToOutputTerminalN, UInt8 selectorIndex);
	OSString *		getNameForPath (OSArray * arrayOfPathsFromOutputTerminal, UInt32 * pathIndex, UInt8 startingPoint);
	OSString *		getNameForMixerPath (OSArray * arrayOfPathsFromOutputTerminal, UInt32 * pathIndex, UInt8 startingPoint);

	static	IOReturn		controlChangedHandler (OSObject *target, IOAudioControl *audioControl, SInt32 oldValue, SInt32 newValue);
	static	IOReturn		deviceXUChangeHandler(OSObject *target, IOAudioControl *audioControl, SInt32 oldValue, SInt32 newValue);
	
	IOReturn		protectedControlChangedHandler (IOAudioControl *audioControl, SInt32 oldValue, SInt32 newValue);
	
	// interface to getting/ setting the various extension unit settings
	IOReturn		getExtensionUnitSettings(UInt16 extCode, UInt8 controlSelector, void* setting, UInt32 length);
	IOReturn		setExtensionUnitSettings(UInt16 extCode, UInt8 controlSelector, void* setting, UInt32 length);
	IOReturn		getExtensionUnitSetting(UInt8 unitID, UInt8 controlSelector, void* setting, UInt32 length);
	IOReturn		setExtensionUnitSetting(UInt8 unitID, UInt8 controlSelector, void* settings, UInt32 length);
	IOReturn		doSelectorControlChange (IOAudioControl * audioControl, SInt32 oldValue, SInt32 newValue);
	UInt8			getSelectorSetting (UInt8 selectorID);
	IOReturn		setSelectorSetting (UInt8 selectorID, UInt8 setting);
	IOReturn		getFeatureUnitSetting (UInt8 controlSelector, UInt8 unitID, UInt8 channelNumber, UInt8 requestType, SInt16 * target);
	IOReturn		setFeatureUnitSetting (UInt8 controlSelector, UInt8 unitID, UInt8 channelNumber, UInt8 requestType, UInt16 newValue, UInt16 newValueLen);
	OSArray *		getPlaythroughPaths ();
	UInt8			getBestFeatureUnitInPath (OSArray * thePath, UInt32 direction, UInt8 interfaceNum, UInt8 altSettingNum, UInt32 controlTypeWanted);
	void			addVolumeControls (EMUUSBAudioEngine * usbAudioEngine, UInt8 featureUnitID, UInt8 interfaceNum, UInt8 altSettingNum, UInt32 usage);
	void			addMuteControl (EMUUSBAudioEngine * usbAudioEngine, UInt8 featureUnitID, UInt8 interfaceNum, UInt8 altSettingNum, UInt32 usage);
	IOReturn		getCurMute (UInt8 unitID, UInt8 channelNumber, SInt16 * target);
	IOReturn		getCurVolume (UInt8 unitID, UInt8 channelNumber, SInt16 * target);
	IOReturn		getMaxVolume (UInt8 unitID, UInt8 channelNumber, SInt16 * target);
	IOReturn		getMinVolume (UInt8 unitID, UInt8 channelNumber, SInt16 * target);
	IOReturn		getVolumeResolution (UInt8 unitID, UInt8 channelNumber, UInt16 * target);
	IOReturn		setCurVolume (UInt8 unitID, UInt8 channelNumber, SInt16 volume);
	IOReturn		setCurMute (UInt8 unitID, UInt8 channelNumber, SInt16 mute);
	IOReturn		doInputSelectorChange (IOAudioControl *audioControl, SInt32 oldValue, SInt32 newValue);
	IOReturn		doVolumeControlChange (IOAudioControl *audioControl, SInt32 oldValue, SInt32 newValue);
	IOReturn		doToggleControlChange (IOAudioControl *audioControl, SInt32 oldValue, SInt32 newValue);
	IOReturn		doPassThruSelectorChange (IOAudioControl * audioControl, SInt32 oldValue, SInt32 newValue);
	IOReturn		doClockSourceSelectorChange (IOAudioControl *audioControl, SInt32 oldValue, SInt32 newValue);

	void			setMonoState (Boolean state);
	UInt8			getHubSpeed ();
	inline UInt32			getHardwareSampleRate() {return mCurSampleRate;}
	inline void				setHardwareSampleRate(UInt32 inSampleRate) { mCurSampleRate = inSampleRate;}
	virtual	IOReturn		deviceRequest (IOUSBDevRequest * request, IOUSBCompletion * completion = NULL);			// Depricated, don't use
	virtual	IOReturn		deviceRequest (IOUSBDevRequestDesc * request, IOUSBCompletion * completion = NULL);
	static	IOReturn		deviceRequest (IOUSBDevRequest * request, EMUUSBAudioDevice * self, IOUSBCompletion * completion = 0);
	static void				StatusAction(OSObject *owner, IOTimerEventSource *sender);
	static 	void			statusHandler(void* target, void* parameter, IOReturn result, UInt32 bytesLeft);

	void					addCustomAudioControls(IOAudioEngine* engine);
	void					removeCustomAudioControls(IOAudioEngine* engine);
	void					setOtherEngineSampleRate(EMUUSBAudioEngine* curEngine, UInt32 newSampleRate);
	void					doStatusCheck(IOTimerEventSource* timer);
	void					queryXU();
	virtual bool			matchPropertyTable (OSDictionary * table, SInt32 *score);
	
	void			RegisterHALCallback(void *toRegister);
	
	IOReturn		GetVariousUnits(UInt8* inputFeatureUnitIDs,UInt8& numberOfInputFeatureUnits,UInt8* outputFeatureUnitIDs,
									UInt8& numberOfOutputFeatureUnits,UInt8& mixerUnitID,UInt8& processingUintID);
									
	EMUUSBAudioEngine* GetEngine(void){ return mAudioEngine; }
	
	void	addHardVolumeControls(IOAudioEngine* audioEngine);
	static	IOReturn hardwareVolumeChangedHandler (OSObject *target, IOAudioControl *audioControl, SInt32 oldValue, SInt32 newValue);
	static	IOReturn hardwareMuteChangedHandler (OSObject *target, IOAudioControl *audioControl, SInt32 oldValue, SInt32 newValue);
	
	void	SetUserClient(EMUUSBUserClient* userClient) { mUserClient = userClient; }

	
#if defined(ENABLE_TIMEBOMB)
	// This function checks to see whether the driver's timebomb has triggered.
	// if so, it will post a dialog and return true.
	bool                    checkForTimebomb();
#endif
	
protected:
	EMUUSBAudioEngine*	getOtherEngine(EMUUSBAudioEngine* curEngine);
};

#endif /* _APPLEUSBAUDIODEVICE_H */

