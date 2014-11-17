/*
 *  test.cp
 *  test
 *
 *  Created by Wouter Pasman on 07/10/14.
 *  Copyright (c) 2014 com.emu. All rights reserved.
 *
 */

#include "test.h"
#include "testPriv.h"

CFStringRef testUUID(void)
{
	Ctest* theObj = new Ctest;
	return theObj->UUID();
}

CFStringRef Ctest::UUID()
{
	return CFSTR("0001020304050607");
}
