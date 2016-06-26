//
//  IOUSBDevice.h
//  EMUUSBAudio
//
//  Created by Wouter Pasman on 05/06/16.
//  Copyright (c) 2016 com.emu. All rights reserved.
//

#ifndef __EMUUSBAudio__IOUSBDevice__
#define __EMUUSBAudio__IOUSBDevice__

#include "osxversion.h"




#ifdef HAVE_OLD_USB_INTERFACE

#include "USBAudioObject.h"

#include <IOKit/usb/IOUSBDevice.h>

class IOUSBDevice1 : public IOUSBDevice {
public:
    inline bool isHighSpeed() {
        // we never should get superspeed or higher, as this is USB2 device.
        return GetSpeed() == kUSBDeviceSpeedHigh;
    }
    
    /*!
     * set the sample rate to the device.
     @param inSampleRate the input samplerate
     @param endpointAddress the usbAudio->GetIsocEndpointAddress
     */
    IOReturn devRequestSampleRate(UInt32 inSampleRate, UInt16 endpointAddress) {
        IOUSBDevRequest		devReq;
        UInt32				theSampleRate = OSSwapHostToLittleInt32 (inSampleRate);
        
        devReq.bmRequestType = USBmakebmRequestType (kUSBOut, kUSBClass, kUSBEndpoint);
        devReq.bRequest = SET_CUR;
        devReq.wValue = (SAMPLING_FREQ_CONTROL << 8) | 0;
        devReq.wIndex = endpointAddress;
        // 3 + (usbAudioDevice->isHighHubSpeed()?1:0);// USB 2.0 device has maxPacket size of 4
        devReq.wLength = 4;
        devReq.pData = &theSampleRate;
        return DeviceRequest (&devReq);
    }
    
    UInt64 getFrameNumber() {
        return GetBus()->GetFrameNumber();
    }
    
};

class FindInterfaceRequest: public IOUSBFindInterfaceRequest {
    
};

#else


/********* 10.11 *************/
#include <IOUSBController.h>

// which one do we really need?
//#include <IOKit/usb/IOUSBHostHIDDevice.h>

#include "USB.h"
#include <IOUSBInterface.h>
#include <sys/utfconv.h>
#include "USBAudioObject.h"
#include <IOKit/usb/USBSpec.h>


class IOUSBDevice1: public IOUSBHostDevice {
public:
    inline UInt8 GetProductStringIndex(void ) {
        return getDeviceDescriptor()->iProduct;
    };
    
    /*!
     @function GetVendorID
     returns the Vendor ID of the device
     */
    inline  UInt16 GetVendorID(void) {
        return getDeviceDescriptor()->idVendor;
    }
    
    /*!
     @function GetProductID
     returns the Product ID of the device
     */
    inline UInt16 GetProductID(void) {
        return getDeviceDescriptor()->idProduct;
    }
    
    /*!
     @function GetFullConfigurationDescriptor
     return a pointer to all the descriptors for the requested configuration.
     @param configIndex The configuration index (not the configuration value)
     @result Pointer to the descriptors, which are cached in the IOUSBDevice object.
     */
    inline const ConfigurationDescriptor * GetFullConfigurationDescriptor(UInt8 configIndex) {
        return getConfigurationDescriptor(configIndex);
    }
    
    /*!
     @function GetStringDescriptor
     Get a string descriptor as ASCII, in the specified language (default is US English)
     @param index Index of the string descriptor to get.
     @param buf Pointer to place to store ASCII string
     @param maxLen Size of buffer pointed to by buf
     @param lang Language to get string in (default is US English)
     */
    IOReturn GetStringDescriptor(UInt8 index, char* buf, int maxLen, UInt16 lang = 0x409) {
        size_t utf8len = 0;
        const StringDescriptor* stringDescriptor = getStringDescriptor(index, lang);
        if (stringDescriptor != NULL && stringDescriptor->bLength > StandardUSB::kDescriptorSize)
        {
            utf8_encodestr(reinterpret_cast<const u_int16_t*>(stringDescriptor->bString),
                    stringDescriptor->bLength - kDescriptorSize,
                    reinterpret_cast<u_int8_t*>(buf), &utf8len, maxLen,
                    '/', UTF_LITTLE_ENDIAN);
            return kIOReturnSuccess;
        }
        return kIOReturnError;
    }
    
    /*!
     @function ResetDevice
     Reset the device, returning it to the addressed, unconfigured state.
     This is useful if a device has got badly confused. Note that the AppleUSBComposite driver will automatically reconfigure the device if it is a composite device.
     */
    inline IOReturn ResetDevice() {
        return kIOReturnSuccess; //Replacement: none...
    }
    
    /*!
     @function GetManufacturerStringIndex
     returns the index of string descriptor describing manufacturer
     */
    inline UInt8 GetManufacturerStringIndex() {
        return getDeviceDescriptor()->iManufacturer;
    }
    
    inline bool isHighSpeed() {
        // we never should get superspeed or higher, as this is USB2 device.
        return getSpeed() == kUSBHostConnectionSpeedHigh;
    }
    

    /*!
     * set the sample rate to the device.
     @param inSampleRate the input samplerate
     @param endpointAddress the usbAudio->GetIsocEndpointAddress
     */
    IOReturn devRequestSampleRate(UInt32 inSampleRate, UInt16 endpointAddress) {
        DeviceRequest		devReq;
        UInt32				theSampleRate = OSSwapHostToLittleInt32 (inSampleRate);
        
        devReq.bmRequestType = USBmakebmRequestType (kUSBOut, kUSBClass, kUSBEndpoint);
        devReq.bRequest = SET_CUR;
        devReq.wValue = (SAMPLING_FREQ_CONTROL << 8) | 0;
        devReq.wIndex = endpointAddress;
        // 3 + (usbAudioDevice->isHighHubSpeed()?1:0);// USB 2.0 device has maxPacket size of 4
        devReq.wLength = 4;
        uint32_t bytesTransferred;

        //return DeviceRequest (&devReq);
        
        //virtual IOReturn deviceRequest(IOService* forClient, StandardUSB::DeviceRequest& request, void* dataBuffer, udsfdsint32_t& bytesTransferred, uint32_t completionTimeoutMs = kUSBHostDefaultControlCompletionTimeoutMS);
        return deviceRequest(this, devReq, &theSampleRate, bytesTransferred);
    }

    /*!
     Search first interface (after current, if it's not NULL) that  matches the request.
     The request probably should have bInterfaceClass==kUSBHubClass.
     */
    IOUSBInterface1* FindNextInterface(IOUSBInterface1*        current,
                                          FindInterfaceRequest* request) {
        OSIterator* iterator = getChildIterator(gIOServicePlane);
        if (iterator==NULL) {
            debugIOLog("ERR can't create an interface iterator to find the device!");
            return NULL;
        }
        
        OSObject* candidate = NULL;
        IOUSBInterface1 *found=NULL;
        bool searchCurrent = (current!=NULL);
        
        while( (candidate = iterator->getNextObject()) != NULL)
        {
            IOUSBInterface1* interfaceCandidate = OSDynamicCast(IOUSBInterface1, candidate);
            if (interfaceCandidate==NULL) continue;
            
            if (searchCurrent) {
                if (candidate == current)  {
                    searchCurrent=false; // found current!
                }
            } else {
                // next matching one is the one we want
                if (request->matches(interfaceCandidate->getInterfaceDescriptor())) {
                    found=interfaceCandidate;
                    break;
                }
            }
            
        }
        OSSafeReleaseNULL(iterator);
        return found;
    }
    
};
#endif

#endif /* defined(__EMUUSBAudio__IOUSBDevice__) */
