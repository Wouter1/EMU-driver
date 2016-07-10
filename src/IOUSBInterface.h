//
//  IOUSBInterface.h
//  EMUUSBAudio
//
//  Created by Wouter Pasman on 05/06/16.
//  Copyright (c) 2016 com.emu. All rights reserved.
//

#ifndef __EMUUSBAudio__IOUSBInterface__
#define __EMUUSBAudio__IOUSBInterface__

#include "osxversion.h"


#ifdef HAVE_OLD_USB_INTERFACE
/******************** 10.9 *********************/

#include <IOKit/usb/IOUSBInterface.h>
#include "IOUSBDevice.h"
#include "USB.h"


class IOUSBInterface1: public IOUSBInterface {
public:
    inline UInt8 getInterfaceNumber() {
        return GetInterfaceNumber();
    }

    /*! @result the index of the string descriptor describing the interface
     */
    inline UInt8  getInterfaceStringIndex() {
        return GetInterfaceStringIndex();
    };
    
    /*!
     @function GetDevice
     returns the device the interface is part of.
     @result Pointer to the IOUSBDevice object which is the parent of this IOUSBInterface object.
     */
    inline IOUSBDevice1 *getDevice1() {
        return OSDynamicCast(IOUSBDevice1, GetDevice());
    }

    /*!
     @function findPipe
     Find a pipe of the interface that matches the requirements,
     starting from the beginning of the interface's pipe list
     @param request Requirements for pipe to match, updated with the found pipe's
     properties.
     
     @result Pointer to the pipe, or NULL if no pipe matches the request.
     */
    IOUSBPipe* findPipe(uint8_t direction, uint8_t type) {
        IOUSBFindEndpointRequest request;
        request.direction = direction;
        request.type = type;
        request.interval = 0xFF;
        request.maxPacketSize = 0;
        return FindNextPipe(NULL, &request);
    }
    
     /*! Send a request to the USB device over mControlInterface (default pipe 0?)
     block and wait to get the result. At most 5 attempts will be done if the call does not succeed immediately.
     See IOUSBDevRequestDesc. 
      @discussion Parameter block for control requests, using a memory descriptor
      for the data to be transferred.  Only available in the kernel.
      @field bmRequestType Request type: kUSBStandard, kUSBClass or kUSBVendor
      @field bRequest Request code
      @field wValue 16 bit parameter for request, host endianess
      @field wIndex 16 bit parameter for request, host endianess
      @field wLength Length of data part of request, 16 bits, host endianess
      @field pData Pointer to memory descriptor for data for request - data returned in bus endianess
      @field wLenDone Set by standard completion routine to number of data bytes
      @return kIOUSBPipeStalled if pipe stalled. Maybe other return values. 
      */
    IOReturn DevRequest(    UInt8                   type,
                  UInt8                   request,
                  UInt16                  value,
                  UInt16                  index,
                  UInt16                  length,
                  IOMemoryDescriptor *    data) {
        IOUSBDevRequestDesc			devReq;
        devReq.bmRequestType = type;
        devReq.bRequest = request;
        devReq.wValue = value;
        devReq.wIndex =index;
        devReq.wLength = length;
        devReq.pData = data;
        return DeviceRequest(&devReq);
        
    }
    

};


#else

/******************** 10.11 and higher *********************/
#include <IOKit/usb/IOUSBHostInterface.h>
//#include <IOUSBDevice.h> super class, can't include here.
class IOUSBDevice1; // declare that it's a known class

#include "EMUUSBLogging.h"
#include "IOUSBPipe.h"
//#include <IOKit/usb/USBSpec.h> deprecated. But where is USBAnyrDir?

class IOUSBInterface1: public IOUSBHostInterface {
public:
    /*!
     @function GetDevice
     returns the device the interface is part of.
     @result Pointer to the IOUSBDevice object which is the parent of this IOUSBInterface object.
     */
    inline IOUSBDevice1 *getDevice1() {
        // can't do OSDynamicCast because IOUSBDevice1 is parent and not completely defined.
        return (IOUSBDevice1 *)getDevice();
    }
    
//    /*!
//     * @brief Return the current frame number of the USB bus
//     *
//     * @description This method will return the current frame number of the USB bus.  This is most useful for
//     * scheduling future isochronous requests.
//     *
//     * @param theTime If not NULL, this will be updated with the current system time
//     *
//     * @result The current frame number
//     */
//    inline uint64_t getFrameNumber(AbsoluteTime* theTime = NULL) const {
//        return getFrameNumber();
//    }
    
    inline UInt8 getInterfaceNumber() {
        return getInterfaceDescriptor()->bInterfaceNumber;
    }
    
    /*! @result the index of the string descriptor describing the interface
     */
    inline UInt8  getInterfaceStringIndex() {
        return getInterfaceDescriptor()->iInterface;
    };
    
    /*!
     @function findPipe
     Find a pipe of the interface that matches the requirements,
     starting from the beginning of the interface's pipe list
     @param direction the direction for the required pipe. eg kUSBInterrupt or kUSBIsoc or kUSBAnyType
     @param type the type of the required pipe: kUSBIn or kUSBOut
     @result Pointer to the pipe, or NULL if no pipe matches the request.
     */
    IOUSBPipe* findPipe(uint8_t direction, uint8_t type) {
        debugIOLog("+findPipe: dir=%d, type = %d", direction, type);

        const StandardUSB::ConfigurationDescriptor* configDesc = getConfigurationDescriptor();
        if (configDesc==NULL)
        {
            debugIOLog("-findpipe: fail, no config descriptor available!");
            return NULL;
        }
        const StandardUSB::InterfaceDescriptor* ifaceDesc = getInterfaceDescriptor();
        if (ifaceDesc==NULL)
        {
            debugIOLog("-findpipe: fail, no interface descriptor available!");
            return NULL;
        }
        
        const EndpointDescriptor* ep = NULL;
        while ((ep = StandardUSB::getNextEndpointDescriptor(configDesc, ifaceDesc, ep)))
        {
            // check if endpoint matches type and direction
            uint8_t epDirection = StandardUSB::getEndpointDirection(ep);
            uint8_t epType = StandardUSB::getEndpointType(ep);
            
            debugIOLog("endpoint found: epDirection = %d, epType = %d", epDirection, epType);
            
            if ( direction == epDirection && type == epType )
            {
                IOUSBHostPipe* pipe = copyPipe(StandardUSB::getEndpointAddress(ep));
                if (pipe == NULL)
                {
                    debugIOLog("-findpipe: found matching pipe but copyPipe failed");
                    return NULL;
                }
                debugIOLog("-findpipe: success");
                //pipe->release(); //  FIXME HACK should be consistent with 10.9
                return OSDynamicCast(IOUSBPipe, pipe);
            }
        }
        debugIOLog("findPipe: no matching endpoint found");
        return NULL;
    }
    

    /*! Send a request to the USB device over mControlInterface (default pipe 0?)
     block and wait to get the result. At most 5 attempts will be done if the call does not succeed immediately.
     See IOUSBDevRequestDesc.
     @discussion Parameter block for control requests, using a memory descriptor
     for the data to be transferred.  Only available in the kernel.
     @field bmRequestType Request type: kUSBStandard, kUSBClass or kUSBVendor
     @field bRequest Request code
     @field wValue 16 bit parameter for request, host endianess
     @field wIndex 16 bit parameter for request, host endianess
     @field wLength Length of data part of request, 16 bits, host endianess
     @field pData Pointer to memory descriptor for data for request - data returned in bus endianess
     @field wLenDone Set by standard completion routine to number of data bytes
     
     */
    IOReturn DevRequest(    UInt8                   type,
                        UInt8                   request,
                        UInt16                  value,
                        UInt16                  index,
                        UInt16                  length,
                        IOMemoryDescriptor *    data) {
        StandardUSB::DeviceRequest			devReq;
        devReq.bmRequestType = type;
        devReq.bRequest = request;
        devReq.wValue = value;
        devReq.wIndex =index;
        devReq.wLength = length;
        uint32_t bytesTransferred;
        
        return deviceRequest(devReq, data, bytesTransferred);
        
    }
    
    IOReturn SetAlternateInterface(IOService* forClient, UInt16 alternateSetting) {
        return selectAlternateSetting(alternateSetting);
    }
    /*!
     @param index value from zero to kUSBMaxPipes-1
     @return a (already retained) handle to the IOUSBPipe at the corresponding index
     */
    IOUSBPipe* GetPipeObj(UInt8 index) {
        return (IOUSBPipe *)copyPipe(index);
    }
    
    
    
};
#endif

#endif /* defined(__EMUUSBAudio__IOUSBInterface__) */
