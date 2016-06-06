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

#ifndef __EMUUSBAudio__EMUUSBAudioDevice__
#define __EMUUSBAudio__EMUUSBAudioDevice__

#include <IOKit/audio/IOAudioDevice.h>
#include <IOKit/audio/IOAudioEngine.h>
#include <IOKit/audio/IOAudioPort.h>
#include <IOKit/audio/IOAudioControl.h>
#include <IOKit/audio/IOAudioStream.h>
#include <IOKit/audio/IOAudioDefines.h>
#include <IOKit/audio/IOAudioSelectorControl.h>
#include <IOKit/audio/IOAudioToggleControl.h>

#include "USB.h"
#include <IOUSBInterface.h>
//#include <IOKit/usb/IOUSBRootHubDevice.h>

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

/*!
 * This class is the EMU device controller, implementing the IOAudioDevice.
 * It manages the inputs at a high level, like 
 * detecting changes in volume or sample rate which goes through extension units (XUs). 
 * I could not find any documentation on XUs, but 
 * an XU is something on the EMU, for the available units refer to extensionUnitControlSelector.
 * Changes in XUs are detected through EMUUSBAudioDevice::deviceXUChangeHandler
 * and then forwarded to a CustomControl object.
 * <XU>Control.cpp files: contain code to handle the <XU> control like volumelevel and mute.
 */
class EMUUSBAudioDevice : public IOAudioDevice {
	friend class EMUUSBAudioEngine;
	friend class EMUXUCustomControl;
    OSDeclareDefaultStructors (EMUUSBAudioDevice);
    
public:
    /*! The USB control interface */
    IOUSBInterface *		mControlInterface;
    
protected:
    /*! average time between USB frames (units: 0.1ns ). Typical value is eg 10000009268  */
	UInt64						mWallTimePerUSBCycle;
    /*! a reference frame number, taken the first time updateWallTimePerUSBCycle is called. 
     It is used to calculate the average USB frame rate in the jitter filter.*/
	UInt64						mReferenceUSBFrame;
    /*! wall time for the mReferenceUSBFrame.  */
	AbsoluteTime				mReferenceWallTime;
    /*! reference USB frame this frame is set to the current frame every mAnchorResetCount USB cycles */
	UInt64						mNewReferenceUSBFrame;
    /*! time associated with current mNewReferenceUSBFrame */
	AbsoluteTime				mNewReferenceWallTime;
    /*! last reference frame", last time we ran updateWallTimePerUSBCycle */
	UInt64						mLastUSBFrame;
    /*! the wall time for mLastUSBFrame */
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
    /*! unit ID, copied from mDeviceStatusBuffer periodically */
	UInt32					mQueryXU;// the XU to query
	UInt32					mCurSampleRate;
	OSArray *				mMonoControlsArray;		// this flag is set by EMUUSBAudioEngine::performFormatChange
    /*! array of registered engines. I think this is about supporting multiple EMU devices. */
	OSArray *				mRegisteredEngines;
    /*! number of EMUUSBAudioEngine's. size of mRegisteredEngines. Seems always 1 */
	UInt32					mNumEngines;
	UInt32					mAvailXUs;			// what extension unit features are available
	EMUXUCustomControl *	mXUChanged;//IOAudioSelectorControl
    /*! stores the selected clock source. 0= built-in 1=SPDif I think */
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
    /*! counter for number of USB frames. When we hit kRefreshCount we refresh mNewReferenceUSBFrame */
	UInt32					mAnchorResetCount;
    
    /*! original doc: device status buffer NOT XU setting.
     This is a temporary store of the device status. EMUUSBAudioDevice::statusHandler 
     copies the unitID contained in here to mQueryXU.
     */
	UInt16*					mDeviceStatusBuffer;
    
	/*! original doc: slots for the various XUs. These contain IDs for the extension units. see getExtensionUnitID */
	UInt8					mClockRateXU;		// clock rate XU id
	UInt8					mClockSrcXU;		// clock source XU id
	UInt8					mDigitalIOXU;		// digital IO XU id
	UInt8					mDeviceOptionsXU;	// device options XU id
#if DIRECTMONITOR
	UInt8					mDirectMonXU;		// direct monitoring XU id
#endif
	
    /*! indicator that we have built-in and SPDif clock controllers.
     When the value is changed, controlChangedHandler is called. See doControlStuff */
	IOAudioSelectorControl* mRealClockSelector;
	
	EMUUSBAudioEngine		*mAudioEngine;
	
	IOAudioToggleControl*	mOuputMuteControl;
	EMUUSBAudioHardLevelControl*	mHardwareOutputVolume;
	UInt8					mHardwareOutputVolumeID;
	
	EMUUSBUserClient*		mUserClient;
	
	void  *propertyChangedProc;
    
	Boolean					mDeviceIsInMonoMode;
	Boolean					mTerminatingDriver;
    
    /*! Sets up a timer to call back to statusHandler.
     find the feedback (USB) endpoint & start the feedback cycle going
     @discussion (in original code): possible ways to do this
      start a read of the interrupt endpoint
      in the completion routine, schedule the next read or start the timer
      OR - add a timer that will fire off
      and start the checking as done below
     */
	void					setupStatusFeedback();
    /*! called from TimerAction, scheduled as a timer */
	void					doTimerAction(IOTimerEventSource * timer);

    /*! get the extension unit unitID. for some extension code.
     @param extCode the eExtensionUnitCode
     */
	UInt8					getExtensionUnitID(UInt16 extCode);
    
	IOReturn				protectedXUChangeHandler(IOAudioControl *audioControl, SInt32 oldValue, SInt32 newValue);
	IOReturn				getAnchorFrameAndTimeStamp(UInt64 *frame, AbsoluteTime *time);
    
    /*! set the given frame number = current famenr of the USB bus, and time=current system time
     @param frame ptr to memory where current frame nr has to be stored
     @param time ptr to place where current time has to be stored
     @return kIOReturnSuccess if mControlInterface is not null, else error.
     */
	IOReturn				getFrameAndTimeStamp(UInt64 *frame, AbsoluteTime *time);
	UInt64					jitterFilter(UInt64 prev, UInt64 curr);
    /*!  @abstrace updates the speed-estimation settings. Called from TimerAction
     
     @discussion These settings are used by getFrameAndTimeStamp. I think this mechanism is needed bcause we need to pass timestamps about the moment the read stream turned back in the ring buffer. But we are not processing this input data in real time.
     @return true unless mControlInterface is null. */
	bool					updateWallTimePerUSBCycle();
    /*! 
     @abstract this watchdog times the rate with which USB frames arrive.
     @discussion This is called from the workloop, as a timer event. This then calls doTimerAction after a few basic checks.
     This TimerAction runs at a period of kRefreshInterval ms (typically 128ms).
     */
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
	const char * 			TerminalTypeString (UInt16 terminalType);
    
	SInt32			getEngineInfoIndex (EMUUSBAudioEngine * inAudioEngine);
	IOReturn		doControlStuff (IOAudioEngine *audioEngine, UInt8 interfaceNum, UInt8 altSettingNum);
	IOReturn		doPlaythroughSetup (EMUUSBAudioEngine * usbAudioEngine, OSArray * playThroughPaths, UInt8 interfaceNum, UInt8 altSettingNum);
	IOReturn		addSelectorSourcesToSelectorControl (IOAudioSelectorControl * theSelectorControl, OSArray * arrayOfPathsFromOutputTerminal, UInt32 pathsToOutputTerminalN, UInt8 selectorIndex);
	OSString *		getNameForPath (OSArray * arrayOfPathsFromOutputTerminal, UInt32 * pathIndex, UInt8 startingPoint);
	OSString *		getNameForMixerPath (OSArray * arrayOfPathsFromOutputTerminal, UInt32 * pathIndex, UInt8 startingPoint);
    
    /*! This is called when we receive a change in one of the user controls (level control, clock control, etc) */
	static	IOReturn		controlChangedHandler (OSObject *target, IOAudioControl *audioControl, SInt32 oldValue, SInt32 newValue);
	static	IOReturn		deviceXUChangeHandler(OSObject *target, IOAudioControl *audioControl, SInt32 oldValue, SInt32 newValue);
	
    /*! the actual code behind controlChangedHandler */
	IOReturn		protectedControlChangedHandler (IOAudioControl *audioControl, SInt32 oldValue, SInt32 newValue);
	/*!
     interface to getting/ setting the various extension unit settings
     get the settings of some XU by eExtensionUnitCode
     @param extCode the eExtensionUnitCode
     @param controlSelector the extensionUnitControlSelector
    */
	IOReturn		getExtensionUnitSettings(UInt16 extCode, UInt8 controlSelector, void* setting, UInt32 length);
    /*! Send a new setting to the given extension unit and control.
     @param extCode the code for the extension unit ID. The actual ID will be looked up.
     */
	IOReturn		setExtensionUnitSettings(UInt16 extCode, UInt8 controlSelector, void* setting, UInt32 length);
    /*! Send a new setting to the given extension unit and control.
     @param unitID the extension unit ID.
     */
	IOReturn		getExtensionUnitSetting(UInt8 unitID, UInt8 controlSelector, void* setting, UInt32 length);
    /*! This is the real function that sets controls in the USB device. 
     Builds a request structure and calls deviceRequest */
	IOReturn		setExtensionUnitSetting(UInt8 unitID, UInt8 controlSelector, void* settings, UInt32 length);
    
    /*! Sends an request with an outgoing datablock of given lengt.
     @param  unitID the unit ID. eg device->mHardwareOutputVolumeID
     @param controlSelector control selector, eg VOLUME_CONTROL
     @param requestType type of request, eg SET_CUR
     @param channelNumber eg kMasterVolumeIndex
     @param channelNr use 0 for general extension unit request, or the feature nr for feature unit request
     @param data the data to add to the call
     @param length the number of bytes in data */
    IOReturn deviceRequestIn(UInt8 unitID, UInt8 controlSelector, UInt8 requestType,  UInt8 channelNr, UInt8* data, UInt32 length);
    
    /*! Sends an request with an outgoing datablock of given lengt.
     @param  unitID the unit ID. eg device->mHardwareOutputVolumeID
     @param controlSelector control selector, eg VOLUME_CONTROL
     @param requestType type of request, eg SET_CUR
     @param channelNumber eg kMasterVolumeIndex
     @param channelNr use 0 for general extension unit request, or the feature nr for feature unit request
     @param data the data to add to the call
     @param length the number of bytes in data */
    IOReturn deviceRequestOut(UInt8 unitID, UInt8 controlSelector, UInt8 requestType,  UInt8 channelNr, UInt8* data, UInt32 length);

	IOReturn		doSelectorControlChange (IOAudioControl * audioControl, SInt32 oldValue, SInt32 newValue);
	UInt8			getSelectorSetting (UInt8 selectorID);
	IOReturn		setSelectorSetting (UInt8 selectorID, UInt8 setting);
	IOReturn		getFeatureUnitSetting (UInt8 controlSelector, UInt8 unitID, UInt8 channelNumber, UInt8 requestType, SInt16 * target);
    
    /*! prepares and sends a control command to USB device using deviceRequest.
     VOLUME_CONTROL, device->mHardwareOutputVolumeID, kMasterVolumeIndex, SET_CUR, HostToUSBWord((SInt16)newValue), sizeof(UInt16)
     @param controlSelector control selector, eg VOLUME_CONTROL
     @param  unitID the unit ID. eg device->mHardwareOutputVolumeID
     @param channelNumber eg kMasterVolumeIndex
     @param requestType type of request, eg SET_CUR
     @param newValue the value to set, an uint16
     @param newValueLen length of the data value (so always 2 as this sends uint16)
     */
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
    /*! just calls mClockSelector->setValue(newValue) */
	IOReturn		doClockSourceSelectorChange (IOAudioControl *audioControl, SInt32 oldValue, SInt32 newValue);
    
	void			setMonoState (Boolean state);
	UInt8			getHubSpeed ();
	inline UInt32			getHardwareSampleRate() {return mCurSampleRate;}
	inline void				setHardwareSampleRate(UInt32 inSampleRate) { mCurSampleRate = inSampleRate;}
	//virtual	IOReturn		deviceRequest (IOUSBDevRequest * request, IOUSBCompletion * completion = NULL);			// Depricated, don't use
    
    /*! Send a request to the USB device over mControlInterface (default pipe 0?)
     and get the result. At most 5 attempts will be done if the call does not succeed immediately.
     @param request The parameter block to send to the device (with the pData as an IOMemoryDesriptor)
     @param completion optional on-completion callback. Default/null will execute request synchronously
     */
	virtual	IOReturn		deviceRequest (IOUSBDevRequestDesc * request, IOUSBCompletion * completion = NULL);
	static	IOReturn		deviceRequest (IOUSBDevRequest * request, EMUUSBAudioDevice * self, IOUSBCompletion * completion = 0);
	static void				StatusAction(OSObject *owner, IOTimerEventSource *sender);

    /*! function that is attached to timer, to periodically get USB status. see also setupStatusFeedback */
	static 	void			statusHandler(void* target, void* parameter, IOReturn result, UInt32 bytesLeft);
    
    /*! original docu: Create and add the custom controls each time the default controls are removed.
      This works better than the previous scheme where the custom controls were created just once
      and added each time when the default controls were removed. */
	void					addCustomAudioControls(IOAudioEngine* engine);
	void					removeCustomAudioControls(IOAudioEngine* engine);
    /*! Tell all engines about a new sample rate */
	void					setOtherEngineSampleRate(EMUUSBAudioEngine* curEngine, UInt32 newSampleRate);
	void					doStatusCheck(IOTimerEventSource* timer);
    /*!  This seems to check if there were changes in the device: io status, sample rate, clock source, etc. */
	void					queryXU();
	virtual bool			matchPropertyTable (OSDictionary * table, SInt32 *score);
	
	void			RegisterHALCallback(void *toRegister);
	
	IOReturn		GetVariousUnits(UInt8* inputFeatureUnitIDs,UInt8& numberOfInputFeatureUnits,UInt8* outputFeatureUnitIDs,
									UInt8& numberOfOutputFeatureUnits,UInt8& mixerUnitID,UInt8& processingUintID);
    
	EMUUSBAudioEngine* GetEngine(void){ return mAudioEngine; }
	
	void	addHardVolumeControls(IOAudioEngine* audioEngine);
    /*! This function is called from IOAudioControl::setValue when user changes volume. */
	static	IOReturn hardwareVolumeChangedHandler (OSObject *target, IOAudioControl *audioControl, SInt32 oldValue, SInt32 newValue);
    /*! This function is called from IOAudioControl::setValue when user changes mute setting. */
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

#endif /* defined(__EMUUSBAudio__EMUUSBAudioDevice__) */
