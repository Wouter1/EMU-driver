//
//  EMUUSBAudio.c
//  EMUUSBAudio
//
//  Created by Wouter Pasman on 06/10/14.
//  Copyright (c) 2014 com.emu. All rights reserved.
//

#include <mach/mach_types.h>

kern_return_t EMUUSBAudio_start(kmod_info_t * ki, void *d);
kern_return_t EMUUSBAudio_stop(kmod_info_t *ki, void *d);

kern_return_t EMUUSBAudio_start(kmod_info_t * ki, void *d)
{
    return KERN_SUCCESS;
}

kern_return_t EMUUSBAudio_stop(kmod_info_t *ki, void *d)
{
    return KERN_SUCCESS;
}
