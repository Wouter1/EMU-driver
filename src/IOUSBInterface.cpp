//
//  IOUSBInterface.cpp
//  EMUUSBAudio
//
//  Created by Wouter Pasman on 05/06/16.
//

#include "IOUSBInterface.h"

#ifdef HAVE_OLD_USB_INTERFACE
/******************** 10.9 *********************/
#include "IOUSBInterface.h"
#include <IOKit/usb/IOUSBInterface.h>
#include "USB.h"


UInt8 IOUSBInterface1::getInterfaceNumber() {
    return GetInterfaceNumber();
}

UInt8 IOUSBInterface1::getInterfaceStringIndex() {
    return GetInterfaceStringIndex();
};

IOUSBDevice1 * IOUSBInterface1::getDevice1() {
    return OSDynamicCast(IOUSBDevice1, GetDevice());
}

IOUSBPipe* IOUSBInterface1::findPipe(uint8_t direction, uint8_t type) {
    IOUSBFindEndpointRequest request;
    request.direction = direction;
    request.type = type;
    request.interval = 0xFF;
    request.maxPacketSize = 0;
    IOUSBPipe* pipe= FindNextPipe(NULL, &request);
    if (pipe!=NULL) {
        pipe->retain();
    }
    return pipe;
}


IOReturn IOUSBInterface1::DevRequest(UInt8  type,
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




#else

/******************** 10.11 and higher *********************/
#include <IOKit/usb/IOUSBHostInterface.h>
#include "IOUSBDevice.h"
#include "EMUUSBLogging.h"

IOUSBDevice1 *IOUSBInterface1::getDevice1() {
    // can't do OSDynamicCast because IOUSBDevice1 is parent and not completely defined.
    return (IOUSBDevice1 *)getDevice();
}


UInt8 IOUSBInterface1::getInterfaceNumber() {
    return getInterfaceDescriptor()->bInterfaceNumber;
}

UInt8  IOUSBInterface1::getInterfaceStringIndex() {
    return getInterfaceDescriptor()->iInterface;
};

IOUSBPipe* IOUSBInterface1::findPipe(uint8_t direction, uint8_t type) {
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
            return OSDynamicCast(IOUSBPipe, pipe);
        }
    }
    debugIOLog("findPipe: no matching endpoint found");
    return NULL;
}

IOReturn IOUSBInterface1::DevRequest(    UInt8                   type,
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

IOReturn IOUSBInterface1::SetAlternateInterface(IOService* forClient, UInt16 alternateSetting) {
    return selectAlternateSetting(alternateSetting);
}

IOUSBPipe* IOUSBInterface1::GetPipeObj(UInt8 index) {
    return (IOUSBPipe *)copyPipe(index);
}



#endif
