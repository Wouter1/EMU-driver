/*
 *  testPriv.h
 *  test
 *
 *  Created by Wouter Pasman on 07/10/14.
 *  Copyright (c) 2014 com.emu. All rights reserved.
 *
 */

#include <CoreFoundation/CoreFoundation.h>

#pragma GCC visibility push(hidden)

class Ctest {
	public:
		CFStringRef UUID(void);
};

#pragma GCC visibility pop
