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
     * @param status value for frStatus field
     * @param reqCount value for frReqCount field
     * @param actCount the value for frActCount field
     * @param t the value for frTimeStamp
     */
    void set(IOReturn status, UInt16 reqCount, UInt16 actCount, AbsoluteTime t) ;
    
    
    /*! @return true if the the status has been set, which means the transfer was completed.
     */
    bool isDone() ;
    
    AbsoluteTime getTime();
    
    /*! @return nr of actually completed frames */
    uint32_t getCompleteCount();
    
    /*! set the timestamp to -1 */
    void resetTime();
    
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
    void set(void * t, LowLatencyCompletionAction a , void *p) ;
    
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
    void set(void *t, IOUSBCompletionAction a, void *p ) ;
    
};

typedef  IOUSBCompletionAction CompletionAction;
typedef IOUSBLowLatencyIsocCompletionAction LowLatencyCompletionAction;

typedef IOUSBFindInterfaceRequest FindInterfaceRequest;

#else
/********************* 10.11 DEFINITIONS **********************/

// SEE ALSO IOUSBHostInterface for the new functionality compared to 10.9

#include <TargetConditionals.h>
#include <IOKit/usb/StandardUSB.h>
#include <IOKit/usb/USB.h>
#include <IOKit/usb/IOUSBHostIOSource.h>

//definitions that were removed in 10.11 but that we still need
//#define USBToHostWord OSSwapLittleToHostInt16
//#define HostToUSBWord OSSwapHostToLittleInt16
//#define USBToHostLong OSSwapLittleToHostInt32
//#define HostToUSBLong OSSwapHostToLittleInt32

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
    void set(IOReturn s, uint32_t requestc, uint32_t actualcc, AbsoluteTime t);
    
    /*! @return true if the the status has been set properly, which means the transfer was completed.
     */
    bool isDone() ;
    
    AbsoluteTime getTime();
    
    /*! @return nr of actually completed frames */
    uint32_t getCompleteCount() ;
    
    /*! set the timestamp to -1 */
    void resetTime() ;
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
    
    void set(void * t, LowLatencyCompletionAction a , void *p);
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
    void set(void *t, IOUSBHostCompletionAction a, void *p );
};


/* NOTE, NO pData field anymore. */
//typedef StandardUSB::DeviceRequest IOUSBDevRequestDesc;


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
    bool matches(const InterfaceDescriptor *descriptor);
};




#endif // 10.11+


#endif /* USB_EXT_h */
