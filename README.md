EMU-USB driver
=============

OSX driver for Creative Labs EMU USB

The current version compiles with Xcode 5.1.1.
Original comes from source forge revision 7. http://sourceforge.net/projects/zaudiodrivermac/
But it does not compile on OSX 10.9 Mavericks. So I dumped part of the code and wrote new code
to get audio recording working.

The present version is in a pretty bad shape. I did a major effort to refactor the input side of the code,
it's on the way but not yet polished. The rest of the code is still in original state.
The first goal here was to get the audio input working properly, without the clicking issues of the official driver when used in OSX10.6 and higher.

* This version only works for audio RECORDING at 96kHz.
* The audio output does not work. 
* Only audio input stereo 96kHz was tested on EMU0404 USB. 

Please notify me how my driver works on other EMU USB devices.


Installation
========
1. Use your original control panel to set the EMU speed to 96 kHz
2. Turn off the EMU device
3. Download (click on "Download ZIP") and unzip the driver
4. Run the kextInstall script 
5. enter your admin password at the prompt

or

1. Use your original control panel to set the EMU speed to 96 kHz
2. manually copy EMUUSBAudio.kext into  /System/Library/Extensions/ 
3. reboot

Usage
======

1. Install the driver as above
2. Select the EMU input in your favourite tool (e.g., Audacity)
3. Make your recording

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


