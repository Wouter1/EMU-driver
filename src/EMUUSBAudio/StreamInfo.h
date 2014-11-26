//
//  StreamInfo.h
//  EMUUSBAudio
//
//  Created by Wouter Pasman on 25/11/14.
//  Copyright (c) 2014 com.emu. All rights reserved.
//

#ifndef EMUUSBAudio_StreamInfo_h
#define EMUUSBAudio_StreamInfo_h

#include <IOKit/usb/IOUSBPipe.h>
#include <IOKit/usb/IOUSBDevice.h>
#include <IOKit/usb/IOUSBInterface.h>
#include <IOKit/audio/IOAudioStream.h>
#include <IOKit/IOSubMemoryDescriptor.h>

/*!
 @abstract  the state or our mInput/mOutput USB stream.
 @discussion To handle an USB stream the program joggles with a set of USB frames,
 callback handlers etc. These are grouped in this object.
 
 See also the USB Device interface guide.
 
 The functions used to read from and write to isochronous endpoints are ReadIsochPipeAsync and WriteIsochPipeAsync. Both functions include the following two parameters:
 
 * numFrames—The number of frames for which to transfer data
 
 * frameList—A pointer to an array of structures that describe the frames. specifies the list of transfers you want to occur.
 */
struct StreamInfo {
    /*! The point where the next raw USB byte can be written in bufferPtr. Always in [0, bufferSize> */
    UInt32		bufferOffset;
    
    /*! =numChannels * #bytes per sample for this stream =  frame size.
     this will be 6 for stereo 24 bit audio. */
    UInt32		multFactor;
    
    /*! The max number of bytes that we can receive in a single frame which is part of a USB frameList.
     Is set to (averageFrameSamples + 1) * mInput.multFactor. Eg 97*6=582 for 96kHz/24 stereo  */
    UInt32		maxFrameSize;
    
    /*! This is the length of the bufferDescriptors array and usbIsocFrames.
     // I think sizes of 4 and 8 are usual.*/
    UInt32		numUSBFrameLists;
    /*! The number of usb frames in our lists. Hard set to RECORD_NUM_USB_FRAMES_PER_LIST 
        or PLAY_NUM_USB_FRAMES_PER_LIST (64 usually). USB calls back to us only after 'completion' 
        (after all frames were read, or something went wrong), calling back to readHandler.
     */
    UInt32		numUSBFramesPerList;
    /*! = mInput.numUSBFramesPerList / kNumberOfFramesPerMillisecond = 8 usually.
     Used as increment for usbFrameToQueueAt*/
    UInt32		numUSBTimeFrames;
    /*!
     @abstract Number of frames we have in use for reading USB.
     @discussion Hard set to RECORD_NUM_USB_FRAME_LISTS_TO_QUEUE or PLAY_NUM_USB_FRAME_LISTS_TO_QUEUE. Typically 2 or 4.  */
    UInt32		numUSBFrameListsToQueue;
    
    /*!
     size of the bufferPtr array. See bufferMemoryDescriptor.
     @discussion
     numSamplesInBuffer * multFactor = # bytes in the buffer.
     where numSamplesInBuffer =PAGE_SIZE * (2 + (sampleRate.whole > 48000) + (sampleRate.whole > 96000))
     and PAGE_SIZE=4096 bytes.
     */
    UInt32		bufferSize;
    
    /*! The nummer of channels coming in. Typically 1 (mono) or 2 (stereo). */
    UInt32		numChannels;
    UInt32		frameOffset;
    UInt8		streamDirection;
    UInt8		interfaceNumber;
    UInt8		alternateSettingID;
    
    /*! the framelist that we are expecting to complete from next. 
     Basically runs from 0 to numUSBFrameListsToQueue-1 and then
     restarts at 0. Updated after readHandler handled the block. */
    volatile UInt32						currentFrameList;
    
    IOUSBInterface				  *streamInterface;
    IOAudioStream				  *audioStream;
    /*! IOUSBPipe used for isochronous reading input streams from USB*/
    IOUSBPipe					  *pipe;
    IOUSBPipe					  *associatedPipe;
    
    /*! @discussion array of IOUSBLowLatencyIsocFrame containing USB status for a frame. 
       size (for mInput) = numUSBFrameLists * numUSBFramesPerList */
    IOUSBLowLatencyIsocFrame	  *usbIsocFrames;
    
    /*! @abstract
     array IOUSBLowLatencyIsocCompletion[numUSBFrameLists]
     @discussion for each frameList there is this callback on completion.
     each one also contains a parameter for the callback.
     The parameter is:
     - for write buffer: number of bytes from the 0 wrap, 0 if this buffer didn't wrap
     - for read buffer; the frameListNum
     */
    IOUSBLowLatencyIsocCompletion *usbCompletion;
    
    // you want ddescriptors? we got descriptors!
    /*!  Big mem block to store all data from reading USB data according to the framelists.
     size mOutput.bufferSize or mInput.numUSBFrameLists * readUSBFrameListSize bytes */
    IOBufferMemoryDescriptor	*usbBufferDescriptor;
    
    /*! The memory descriptor for the The intermediate ring buffer of size bufferSize.*/
    IOBufferMemoryDescriptor	*bufferMemoryDescriptor;
    
    /*! array of pointers to IOMemoryDescriptor of length [frameListnum]. This is where raw USB data will come in.
     @discussion Contains copy of the received USB data.
     When a framelist is complete, readhHandler copies the data from the frame list
     to the mInput buffer so that the frameList can be redeployed.
     We need this (1) to have a fixed ring buffer as HAL is expecting us to have
     (2) to free up the framelist so that we can redeploy it to continue reading
     (3) so that we can do int-to-float conversion 'offline'.
     However it seems that these are no strong reasons, probably we could just simulate a ring buffer
     and redeploy the frameList only after conversion for the HAL.
     */
    IOSubMemoryDescriptor		**bufferDescriptors;
    
    /*! shortcut to bufferMemoryDescriptor actual buffer bytes. Really UInt8*. */
    void *						bufferPtr;
    
    /*! the expected USB MBus Frame number where we are looking for. Initially this is at frameOffset from the current frame number */
    UInt64						usbFrameToQueueAt;
    /*! an array of size [frameListnum] holding usbFrameToQueueAt for each frame when it was requested for read */
    UInt64 *					frameQueuedForList;
    
    
    
    
};


#endif
