/*
 *  EMUHALPlugin.h
 *  EMUHALPlugin
 *
 *  Created by Wouter Pasman on 07/10/14.
 *  Copyright (c) 2014 com.emu. All rights reserved.
 *
 */

extern "C" {
#pragma GCC visibility push(default)

/* External interface to the EMUHALPlugin, C-based */

CFStringRef EMUHALPluginUUID(void);

#pragma GCC visibility pop
}
