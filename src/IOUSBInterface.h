//
//  IOUSBInterface.h
//  EMUUSBAudio
//
//  Created by Wouter Pasman on 05/06/16.
// This is a stub for IOUSBInterface, to make the old code run on
// the new IOUSBHostInterface.
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
    UInt8 getInterfaceNumber();
    
    /*! @result the index of the string descriptor describing the interface
     */
    UInt8  getInterfaceStringIndex();
    
    /*!
     @function GetDevice
     returns the device the interface is part of.
     @result Pointer to the IOUSBDevice object which is the parent of this IOUSBInterface object.
     */
    IOUSBDevice1 *getDevice1();

    /*!
     @function findPipe
     Find a pipe of the interface that matches the requirements,
     starting from the beginning of the interface's pipe list
     @param request Requirements for pipe to match, updated with the found pipe's
     properties.
     
     @result Pointer to the retained pipe, or NULL if no pipe matches the request.
     */
    IOUSBPipe* findPipe(uint8_t direction, uint8_t type);
    
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
                        IOMemoryDescriptor *    data) ;
};
    
#else

/******************** 10.11 and higher *********************/
#include <IOKit/usb/IOUSBHostInterface.h>
class IOUSBDevice1; // declare that it's a known class

#include "IOUSBPipe.h"
    

class IOUSBInterface1: public IOUSBHostInterface {
public:
    /*!
     @function GetDevice
     returns the device the interface is part of.
     @result Pointer to the IOUSBDevice object which is the parent of this IOUSBInterface object.
     */
    IOUSBDevice1 *getDevice1();
    
    UInt8 getInterfaceNumber();
    
    /*! @result the index of the string descriptor describing the interface
     */
    UInt8  getInterfaceStringIndex() ;
    
    /*!
     @function findPipe
     Find a pipe of the interface that matches the requirements,
     starting from the beginning of the interface's pipe list
     @param direction the direction for the required pipe. eg kUSBInterrupt or kUSBIsoc or kUSBAnyType
     @param type the type of the required pipe: kUSBIn or kUSBOut
     @result Pointer to the retained pipe, or NULL if no pipe matches the request.
     */
    IOUSBPipe* findPipe(uint8_t direction, uint8_t type) ;

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
                        IOMemoryDescriptor *    data);
    
    IOReturn SetAlternateInterface(IOService* forClient, UInt16 alternateSetting);
    
    /*!
     @param index value from zero to kUSBMaxPipes-1
     @return a (already retained) handle to the IOUSBPipe at the corresponding index
     */
    IOUSBPipe* GetPipeObj(UInt8 index);
    
    
    
};
#endif

#endif /* defined(__EMUUSBAudio__IOUSBInterface__) */
