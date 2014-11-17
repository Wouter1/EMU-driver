EMU-driver
==========

OSX Kernel extension for Creative Labs EMU driver

The current version compiles with Xcode 5.1.1.
Original comes from source forge revision 7. http://sourceforge.net/projects/zaudiodrivermac/
But it does not compile on OSX 10.9 Mavericks. So I dumped part of the code and wrote new code
to get audio recording working.

The present version is in a pretty bad shape. Nothing was done to refactor or clean up the original code.
The first goal here was to get the audio input working properly, without the clicking issues of the official driver when used in OSX10.6 and higher.

* This version ONLY works for audio input at 96kHz.
* The audio output does not work. 
* The control panel also does not recognise this driver.


Installation
========
1. Use the original EMU driver provided by E-MU
2. Use the control panel to set the EMU speed to 96 kHz

Then proceed installing this new driver:

3. Run the kextInstall script 
4. enter your admin password at the prompt

or

3. manually copy EMUUSBAudio.kext into  /System/Library/Extensions/ 
4. reboot

The original version of the driver 'EMUUSBAudio original.kext' is also provided here. 
You can copy that one back to  /System/Library/Extensions/ and reboot restore the original driver.


Usage
======

1. Install the driver as above
2. Select the EMU input in your favourite tool (e.g., Audacity)
3. Make your recording

