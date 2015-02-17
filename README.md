<img align="right" width="400" src="E-MU_0404_USB.jpg"/>

EMU-USB driver
=============

OSX driver for Creative Labs EMU USB

* Tested on Mavericks and Yosemite. Probably also works on other versions of OSX. 
* All sample rates are supported both for record and playback: 44.1, 48, 88.2, 96, 176.4 and 192 kHz
* I can only test on EMU0404 USB as I have no other EMU devices. I received feedback that it works on EMU0204 USB.
* Midi is not tested, no idea if it works (#23). 


--
<img align="left" width="80" src="warning-sign.jpg"/>

There is a bug in Quicktime 10, at least on Mavericks, that causes distortion in playback (see #26). We recommend to install Quicktime 7 (http://support.apple.com/kb/DL923).

--
Please notify me how my driver works on other EMU USB devices.

Topics
========
 * <a href="FAQ.md">frequently asked questions</a> 
 * <a href="TechSpecs.md">Technical Specifications</a>
 * <a href="Latency.md">Adjusting the latency</a>




Installation
========

First time installation only: prepare your machine to take developer kernel extensions:

1. open a terminal and enter this command:
    ```sudo nvram boot-args="kext-dev-mode=1"```
2. reboot your machine. 

Install the driver:

1. Turn off the EMU device
2. Download (click on "Download ZIP") and unzip the driver
3. Run the kextInstall script 
4. enter your admin password at the prompt

or

1. manually copy EMUUSBAudio.kext into  /System/Library/Extensions/ 
2. reboot

If you like you can also copy the control panel into your applications directory.

NOTE: After the install, it takes OSX about a minute to incorporate the driver into the kernel.

Usage
======

1. Install the driver as above
2. Use the EMU control panel to select the sample rate you want
3. Select the EMU input in your favourite tool (e.g., Audacity)
4. Make your recording

Uninstall
=======
Download and install the original EMU driver provided by E-MU (http://support.creative.com/Products/ProductDetails.aspx?catID=237&catName=USB+Audio%2fMIDI+Interfaces&subCatID=611&subCatName=USB+Audio%2fMIDI+Interfaces&prodID=15185&prodName=0404+USB+2.0&bTopTwenty=1&VARSET=prodfaq:PRODFAQ_15185,VARSET=CategoryID:237)

OR

1. rename EMUUSBAudio original.kext to EMUUSBAudio.kext
2. Turn off your EMU device
3. run the kextInstall script


Building from source code
===================

1. Open the project in Xcode (double click src/EMUUSBAudio.xcodeproj)
2. Select Product/Build menu item
3. Select View/Navigators/Project Navigator menu item
4. right click on the  EMUUSBAudio.kext and click "Show in finder"
5. Copy the kext into the directory where the kextInstall script is

After that the kExtInstall script is ready for use (see Installation)





Tech notes
==========
The current version compiles with Xcode 5.1.1.
Original comes from source forge revision 7. http://sourceforge.net/projects/zaudiodrivermac/
But it did not compile on OSX 10.9 Mavericks. So I dumped part of the code, cleaned up, documented, 
refactored and wrote new code to get it working.

The present version is in a pretty bad shape. I did a major effort to refactor the input side of the code,
it's on the way but not yet polished. The rest of the code is still in original state.
Audio input and output (record and playback) are now working fine.
There are a few small loose ends and I'd like to clean up the code further.
