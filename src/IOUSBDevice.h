//
//  IOUSBDevice.h
//  EMUUSBAudio
//
//  Created by Wouter Pasman on 05/06/16.
// This is a stub for IOUSBInterface, to make the old code run on
// the new IOUSBHostInterface.
//

#ifndef __EMUUSBAudio__IOUSBDevice__
#define __EMUUSBAudio__IOUSBDevice__

#include "osxversion.h"




#ifdef HAVE_OLD_USB_INTERFACE

#include "USBAudioObject.h"

#include <IOKit/usb/IOUSBDevice.h>

class IOUSBDevice1 : public IOUSBDevice {
public:
    bool isHighSpeed();
    
    /*!
     * set the sample rate to the device.
     @param inSampleRate the input samplerate
     @param endpointAddress the usbAudio->GetIsocEndpointAddress
     */
    IOReturn devRequestSampleRate(UInt32 inSampleRate, UInt16 endpointAddress) ;
    
    UInt64 getFrameNumber();
    
};


#else


/********* 10.11 *************/

// which one do we really need?
//#include <IOKit/usb/IOUSBHostHIDDevice.h>

#include "USB.h"
#include <IOUSBInterface.h>
#include <sys/utfconv.h>
#include "USBAudioObject.h"



class IOUSBDevice1: public IOUSBHostDevice {
public:
    UInt8 GetProductStringIndex(void ) ;
    
    /*!
     @function GetVendorID
     returns the Vendor ID of the device
     */
    UInt16 GetVendorID(void) ;
    
    /*!
     @function GetProductID
     returns the Product ID of the device
     */
    UInt16 GetProductID(void) ;
    
    /*!
     @function GetFullConfigurationDescriptor
     return a pointer to all the descriptors for the requested configuration.
     @param configIndex The configuration index (not the configuration value)
     @result Pointer to the descriptors, which are cached in the IOUSBDevice object.
     */
    const ConfigurationDescriptor * GetFullConfigurationDescriptor(UInt8 configIndex);
    
    /*!
     @function GetStringDescriptor
     Get a string descriptor as ASCII, in the specified language (default is US English)
     @param index Index of the string descriptor to get.
     @param buf Pointer to place to store ASCII string
     @param maxLen Size of buffer pointed to by buf
     @param lang Language to get string in (default is US English)
     */
    IOReturn GetStringDescriptor(UInt8 index, char* buf, int maxLen, UInt16 lang = 0x409);
    
    /*!
     @function ResetDevice
     Reset the device, returning it to the addressed, unconfigured state.
     This is useful if a device has got badly confused. Note that the AppleUSBComposite driver will automatically reconfigure the device if it is a composite device.
     */
    IOReturn ResetDevice() ;
    
    /*!
     @function GetManufacturerStringIndex
     returns the index of string descriptor describing manufacturer
     */
    UInt8 GetManufacturerStringIndex();
    
    bool isHighSpeed() ;
    

    /*!
     * set the sample rate to the device.
     @param inSampleRate the input samplerate
     @param endpointAddress the usbAudio->GetIsocEndpointAddress
     */
    IOReturn devRequestSampleRate(UInt32 inSampleRate, UInt16 endpointAddress);
    
    /*!
     Search first interface (after current, if it's not NULL) that  matches the request.
     The request probably should have bInterfaceClass==kUSBHubClass.
     */
    IOUSBInterface1* FindNextInterface(IOUSBInterface1* current, FindInterfaceRequest* request);
    
};
#endif

#endif /* defined(__EMUUSBAudio__IOUSBDevice__) */
