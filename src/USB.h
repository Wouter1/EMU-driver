//
// This defines general type names that point to equivalent USB structures,
// regardless of whether we compile on 10.9 or 10.11.
//
//  Created by wouter on 05/06/16.
//

#ifndef USB_EXT_h
#define USB_EXT_h


#include "osxversion.h"

/********************* 10.9 DEFINITIONS **********************/
#ifdef HAVE_OLD_USB_INTERFACE
#include <IOKit/usb/USB.h>


typedef IOUSBIsocFrame IsocFrame;

class LowLatencyIsocFrame: public  IOUSBLowLatencyIsocFrame {
public:
    /*!
     TODO DOC
     FIXME remove unused params
     */
    void set(IOReturn status, UInt16 reqCount, UInt16 actCount, AbsoluteTime t) {
        frStatus = status;
        frReqCount = reqCount;
        frActCount = actCount;
        frTimeStamp = t;
    }
    
    
    
    /*! @return true if the the status has been set, which means the transfer was completed.
     */
    inline bool isDone() {
        return -1 != frStatus && kUSBLowLatencyIsochTransferKey != frStatus;
    }
    
    inline AbsoluteTime getTime() {
        return frTimeStamp;
    }
    
    /*! @return nr of actually completed frames */
    inline uint32_t getCompleteCount() {
        return frActCount;
    }
    
    /*! set the timestamp to -1 */
    inline void resetTime() {
        frTimeStamp = 0xFFFFFFFFFFFFFFFFull;
    }
    
};




typedef IOUSBLowLatencyIsocCompletionAction LowLatencyCompletionAction;

class LowLatencyCompletion : public IOUSBLowLatencyIsocCompletion{
public:
    /*!
     * init target, action and parameter
     * @param t the target
     * @param a the action
     * @param p the parameter
     */
    void set(void * t, LowLatencyCompletionAction a , void *p) {
        target = t;
        action = a;
        parameter = p;
    }
    
    
};


typedef IOUSBEndpointDescriptor EndpointDescriptor;
typedef IOUSBConfigurationDescriptor  ConfigurationDescriptor;
typedef IOUSBIsocCompletion IsocCompletion;

class Completion : public IOUSBCompletion {
public:
    /*!
     * set target, action , parameter
     @param t target / owner
     @param a action
     @param p parameter
     */
    void set(void *t, IOUSBCompletionAction a, void *p ) {
        target = t;
        action = a;
        parameter = p;
    }
};

typedef  IOUSBCompletionAction CompletionAction;
typedef IOUSBLowLatencyIsocCompletionAction LowLatencyCompletionAction;

#else
/********************* 10.11 DEFINITIONS **********************/

// SEE ALSO IOUSBHostInterface for the new functionality compared to 10.9

#include <IOKit/usb/StandardUSB.h>
// replaces IOKit/usb/USB.h
#include <IOKit/usb/IOUSBHostIOSource.h>

//definitions that were removed in 10.11 but that we still need
#define USBToHostWord OSSwapLittleToHostInt16
#define HostToUSBWord OSSwapHostToLittleInt16
#define USBToHostLong OSSwapLittleToHostInt32
#define HostToUSBLong OSSwapHostToLittleInt32

// map the old USBmakebmRequestType and its variables into the new type
#define USBmakebmRequestType makeDeviceRequestbmRequestType
#define kUSBIn kRequestDirectionIn
#define kUSBOut kRequestDirectionOut
#define kUSBClass kRequestTypeClass
#define kUSBInterface kRequestRecipientInterface


#define kUSBControl     kEndpointTypeControl
#define kUSBIsoc        kEndpointTypeIsochronous
#define kUSBBulk        kEndpointTypeBulk
#define kUSBInterrupt   kEndpointTypeInterrupt

#define kUSBEndpoint kRequestRecipientEndpoint

// support usbDevice->getProperty()
#define kUSBDevicePropertyLocationID kUSBHostPropertyLocationID

typedef IOUSBHostIsochronousFrame IsocFrame;
// CHECK are there no explit low latency versions anymore?

class LowLatencyIsocFrame: public IOUSBHostIsochronousFrame {
public:
    /*!
     * init status, requestCount, completeCount and timestamp
     * @param s the status
     * @param requestc the requestCount = #bytes to read
     * @param actualcc the actual CompleteCount = #bytes actual read
     * @param t the AbsoluteTime
     */
    void set(IOReturn s, uint32_t requestc, uint32_t actualcc, AbsoluteTime t) {
        status=s;
        requestCount=requestc;
        completeCount=actualcc;
        timeStamp=t;
    }
    
    /*! @return true if the the status has been set properly, which means the transfer was completed.
     */
    inline bool isDone() {
        return -1 != status && kIOReturnInvalid != status;
    }
    
    inline AbsoluteTime getTime() {
        return timeStamp;
    }
    
    /*! @return nr of actually completed frames */
    inline uint32_t getCompleteCount() {
        return completeCount;
    }
    
    /*! set the timestamp to -1 */
    inline void resetTime() {
        timeStamp = 0xFFFFFFFFFFFFFFFFull;
    }
};
typedef IOUSBHostIsochronousCompletionAction LowLatencyCompletionAction;

class LowLatencyCompletion : public IOUSBHostIsochronousCompletion {
public:
    /*!
     * init owner, action and parameter
     * @param t the owner
     * @param a the action
     * @param p the parameter
     */
    
    void set(void * t, LowLatencyCompletionAction a , void *p) {
        owner = t;
        action = a;
        parameter = p;
        
    }
};
// EndpointDescriptor already exists in OSX11, and it is the one we need
// ConfigurationDescriptor already exists in OSX11, and it is the one we need
typedef IOUSBHostIsochronousCompletion IsocCompletion;


class Completion : public IOUSBHostCompletion {
public:
    /*!
     * set target, action , parameter
     @param t target / owner
     @param a action
     @param p parameter
     */
    void set(void *t, IOUSBHostCompletionAction a, void *p ) {
        owner = t;
        action = a;
        parameter = p;
    }
};


/* NOTE, NO pData field anymore. */
typedef StandardUSB::DeviceRequest IOUSBDevRequestDesc;


/*!
 @struct IOUSBFindInterfaceRequest copy of the 10.9 structure, enhanced with match
 @discussion Structure used with FindNextInterface.
 */
class FindInterfaceRequest {
public:
    UInt16	bInterfaceClass;		// requested class
    UInt16 	bInterfaceSubClass;		// requested subclass
    UInt16 	bInterfaceProtocol;		// requested protocol
    UInt16	bAlternateSetting;		// requested alt setting

    /*!
     @return true if this request matches the given descriptor.
     */
    bool matches(const InterfaceDescriptor *descriptor) {
        return
            descriptor->bInterfaceClass == bInterfaceClass &&
            descriptor->bInterfaceSubClass == bInterfaceSubClass &&
            descriptor->bInterfaceProtocol == bInterfaceProtocol &&
            descriptor->bAlternateSetting == bAlternateSetting;
    }
};




#endif // 10.11+


#endif /* USB_EXT_h */
