/*
 *  test.h
 *  test
 *
 *  Created by Wouter Pasman on 07/10/14.
 *  Copyright (c) 2014 com.emu. All rights reserved.
 *
 */

extern "C" {
#pragma GCC visibility push(default)

/* External interface to the test, C-based */

CFStringRef testUUID(void);

#pragma GCC visibility pop
}
