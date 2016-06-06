//
//  USB.h extended version/adapter
//  EMUUSBAudio
//
//  Created by wouter on 05/06/16.
//  Copyright © 2016 com.emu. All rights reserved.
//

#ifndef USB_EXT_h
#define USB_EXT_h


#include "osxversion.h"

#ifdef HAVE_OLD_USB_INTERFACE
#include <IOKit/usb/USB.h>
#else

/************** Extended version to define old USB.h structures as well ***********/

#include <IOKit/usb/StandardUSB.h>
#include <IOKit/IOMemoryDescriptor.h>


/*********** HACK HACK HACK the definitions below are straight copies from 10.9 ************/
/********** These supposedly are useless in 10.11 ****************/
/*** Or else, they should be in some proper file. USB.h that defines all these does not exist in 10.11 */



/*!
 @typedef IOUSBLowLatencyIsocFrame
 @discussion    Structure used to encode information about each isoc frame that is processed
 at hardware interrupt time (low latency).
 @param frStatus Returns status associated with the frame.
 @param frReqCount Input specifiying how many bytes to read or write.
 @param frActCount Actual # of bytes transferred.
 @param frTimeStamp Time stamp that indicates time when frame was procesed.
 */
struct IOUSBLowLatencyIsocFrame {
    IOReturn                        frStatus;
    UInt16                          frReqCount;
    UInt16                          frActCount;
    AbsoluteTime		    frTimeStamp;
};
typedef struct IOUSBLowLatencyIsocFrame IOUSBLowLatencyIsocFrame;



/*!
 @typedef IOUSBLowLatencyIsocCompletionAction
 @discussion Function called when Low Latency Isochronous USB I/O completes.
 @param target The target specified in the IOUSBLowLatencyIsocCompletion struct.
 @param parameter The parameter specified in the IOUSBLowLatencyIsocCompletion struct.
 @param status Completion status.
 @param pFrames Pointer to the low latency frame list containing the status for each frame transferred.
 */
typedef void (*IOUSBLowLatencyIsocCompletionAction)(
void *				target,
void *				parameter,
IOReturn			status,
IOUSBLowLatencyIsocFrame	*pFrames);



/*!
 @typedef IOUSBLowLatencyIsocCompletion
 @discussion Struct specifying action to perform when an Low Latency Isochronous USB I/O completes.
 @param target The target to pass to the action function.
 @param action The function to call.
 @param parameter The parameter to pass to the action function.
 */
typedef struct IOUSBLowLatencyIsocCompletion {
    void * 				target;
    IOUSBLowLatencyIsocCompletionAction	action;
    void *				parameter;
} IOUSBLowLatencyIsocCompletion;

/*!
 @typedef IOUSBEndpointDescriptor
 @discussion Descriptor for a USB Endpoint.  See the USB Specification at <a href="http://www.usb.org" target="_blank">http://www.usb.org</a>.
 */
struct IOUSBEndpointDescriptor {
    UInt8 			bLength;
    UInt8 			bDescriptorType;
    UInt8 			bEndpointAddress;
    UInt8 			bmAttributes;
    UInt16 			wMaxPacketSize;
    UInt8 			bInterval;
};
typedef struct IOUSBEndpointDescriptor	IOUSBEndpointDescriptor;
typedef IOUSBEndpointDescriptor *	IOUSBEndpointDescriptorPtr;


/*!
 @typedef IOUSBConfigurationDescriptor
 @discussion Standard USB Configuration Descriptor.  It is variable length, so this only specifies the known fields.  We use the wTotalLength field to read the whole descriptor.
 See the USB Specification at <a href="http://www.usb.org" target="_blank">http://www.usb.org</a>.
 */
struct IOUSBConfigurationDescriptor {
    UInt8 			bLength;
    UInt8 			bDescriptorType;
    UInt16 			wTotalLength;
    UInt8 			bNumInterfaces;
    UInt8 			bConfigurationValue;
    UInt8 			iConfiguration;
    UInt8 			bmAttributes;
    UInt8 			MaxPower;
};
typedef struct IOUSBConfigurationDescriptor 	IOUSBConfigurationDescriptor;
typedef IOUSBConfigurationDescriptor *		IOUSBConfigurationDescriptorPtr;

/*!
 @typedef IOUSBCompletionAction
 @discussion Function called when USB I/O completes.
 @param target The target specified in the IOUSBCompletion struct.
 @param parameter The parameter specified in the IOUSBCompletion struct.
 @param status Completion status.
 @param bufferSizeRemaining Bytes left to be transferred.
 */
typedef void (*IOUSBCompletionAction)(
void *			target,
void *			parameter,
IOReturn		status,
UInt32			bufferSizeRemaining);


/*!
 @typedef IOUSBCompletion
 @discussion Struct specifying action to perform when a USB I/O completes.
 @param target The target to pass to the action function.
 @param action The function to call.
 @param parameter The parameter to pass to the action function.
 */
typedef struct IOUSBCompletion {
    void * 			target;
    IOUSBCompletionAction	action;
    void *			parameter;
} IOUSBCompletion;


/*!
 @struct IOUSBDevRequest
 @discussion Parameter block for control requests, using a simple pointer
 for the data to be transferred.
 @field bmRequestType Request type: kUSBStandard, kUSBClass or kUSBVendor
 @field bRequest Request code
 @field wValue 16 bit parameter for request, host endianess
 @field wIndex 16 bit parameter for request, host endianess
 @field wLength Length of data part of request, 16 bits, host endianess
 @field pData Pointer to data for request - data returned in bus endianess
 @field wLenDone Set by standard completion routine to number of data bytes
	actually transferred
 */
typedef struct {
    UInt8       bmRequestType;
    UInt8       bRequest;
    UInt16      wValue;
    UInt16      wIndex;
    UInt16      wLength;
    void *      pData;
    UInt32      wLenDone;
} IOUSBDevRequest;
typedef IOUSBDevRequest * IOUSBDeviceRequestPtr;


/*!
 @struct IOUSBDevRequestDesc
 @discussion Parameter block for control requests, using a memory descriptor
 for the data to be transferred.  Only available in the kernel.
 @field bmRequestType Request type: kUSBStandard, kUSBClass or kUSBVendor
 @field bRequest Request code
 @field wValue 16 bit parameter for request, host endianess
 @field wIndex 16 bit parameter for request, host endianess
 @field wLength Length of data part of request, 16 bits, host endianess
 @field pData Pointer to memory descriptor for data for request - data returned in bus endianess
 @field wLenDone Set by standard completion routine to number of data bytes
 actually transferred
 */
typedef struct {
    UInt8                   bmRequestType;
    UInt8                   bRequest;
    UInt16                  wValue;
    UInt16                  wIndex;
    UInt16                  wLength;
    IOMemoryDescriptor *    pData;
    UInt32                  wLenDone;
} IOUSBDevRequestDesc;

/*!
 @defined USBmakebmRequestType
 @discussion Macro to encode the bRequest field of a Device Request.  It is used when constructing an IOUSBDevRequest.
 */
#define USBmakebmRequestType(direction, type, recipient)		\
(((direction & kUSBRqDirnMask) << kUSBRqDirnShift) |			\
((type & kUSBRqTypeMask) << kUSBRqTypeShift) |			\
(recipient & kUSBRqRecipientMask))


#endif // 10.11+ extras


#endif /* USB_EXT_h */
