/*
 *  EMUHALPlugin.cp
 *  EMUHALPlugin
 *
 *  Created by Wouter Pasman on 07/10/14.
 *  Copyright (c) 2014 com.emu. All rights reserved.
 *
 */

#include "EMUHALPlugin.h"
#include "EMUHALPluginPriv.h"

CFStringRef EMUHALPluginUUID(void)
{
	CEMUHALPlugin* theObj = new CEMUHALPlugin;
	return theObj->UUID();
}

CFStringRef CEMUHALPlugin::UUID()
{
	return CFSTR("0001020304050607");
}
