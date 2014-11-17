/*
   This file is part of the EMU CA0189 USB Audio Driver.

   Copyright (C) 2008 EMU Systems/Creative Technology Ltd. 

   This driver is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This driver is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library.   If not, a copy of the GNU Lesser General Public 
   License can be found at <http://www.gnu.org/licenses/>.
*/
/*
	defines for the various items not defined in the stock system driver
*/
// process types for processing units added for EMU
enum {
	PROCESS_UNDEFINED					= 0x00,
	PROCESS_UPMIX_DOWNMIX				= 0x01,
	PROCESS_DOLBY_PROLOGIC				= 0x02,
	PROCESS_3D_STEREO_EXTENDER			= 0x03,
	PROCESS_REVERBERATION				= 0x04,
	PROCESS_CHORUS						= 0x05,
	PROCESS_DYNAMIC_RANGE_COMPRESSION	= 0x06
};

// up mix down mix unit control selectors
enum {
	UP_DOWN_MIX_UNDEFINED	= 0x00,
	UP_DOWN_MIX_ENABLE		= 0x01,
	UP_DOWN_MIX_MODE_SELECT	= 0x02
};

//Dolby Prologic selectors
enum {
	DOLBY_CONTROL_UNDEFINED		= 0x00,
	DOLBY_CONTROL_ENABLE		= 0x01,
	DOLBY_CONTROL_MODE_SELECT	= 0x02
};

// 3D stereo selectors
enum {
	THREE_D_CONTROL_UNDEFINED	= 0x00,
	THREE_D_CONTROL_ENABLE		= 0x01,
	THREE_D_CONTROL_SPACIOUSNESS	= 0x02
};

// Reverb selectors
enum {
	REVERB_CONTROL_UNDEFINED	= 0x00,
	REVERB_CONTROL_ENABLE		= 0x01,
	REVERB_CONTROL_TYPE			= 0x02,
	REVERB_CONTROL_LEVEL		= 0x03,
	REVERB_CONTROL_TIME			= 0x04,
	REVERB_CONTROL_FEEDBACK		= 0x05
};

// Chorus selectors
enum {
	CHORUS_UNDEFINED	= 0x00,
	CHORUS_ENABLE		= 0x01,
	CHORUS_LEVEL		= 0x02,
	CHORUS_RATE			= 0x03,
	CHORUS_DEPTH		= 0x04
};

// Dynamic range selectors
enum {
	DYNAMIC_RANGE_UNDEFINED			= 0x00,
	DYNAMIC_RANGE_ENABLE			= 0x01,
	DYNAMIC_RANGE_COMPRESSION_RATIO	= 0x02,
	DYNAMIC_RANGE_MAX_AMPLITUDE		= 0x03,
	DYNAMIC_RANGE_THRESHHOLD		= 0x04,
	DYNAMIC_RANGE_ATTACK_TIME		= 0x05,
	DYNAMIC_RANGE_RELEASE_TIME		= 0x06
};

// Extension unit selectors
enum {
	EXTENSION_UNIT_UNDEFINED	= 0x00,
	EXTENSION_UNIT_ENABLE		= 0x01
};
