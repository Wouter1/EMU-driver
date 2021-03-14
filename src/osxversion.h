//
//  osxversion.h
//  EMUUSBAudio
//
//  Created by wouter on 06/06/16.
// macros to determine the OSX version we run on. 
//


#ifndef osxversion_h
#define osxversion_h
#include <AvailabilityInternal.h>


#define _STR(x) #x
#define STR(x) _STR(x)

#pragma message "Compiling for OSX " STR(__MAC_OS_X_VERSION_MIN_REQUIRED)

// prints the actual content of mac version.
#if __MAC_OS_X_VERSION_MIN_REQUIRED < 101100
#define HAVE_OLD_USB_INTERFACE
#endif


#endif /* osxversion_h */
