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

* This version only works for audio RECORDING (44.1, 48, 88.2 and 96kHz).
* The audio output does not work. 
* I can only test on EMU0404 USB as I have no other EMU devices. 

Please notify me how my driver works on other EMU USB devices.


Installation
========
1. Turn off the EMU device
2. Download (click on "Download ZIP") and unzip the driver
3. Run the kextInstall script 
4. enter your admin password at the prompt

or

1. manually copy EMUUSBAudio.kext into  /System/Library/Extensions/ 
2. reboot

If you like you can also copy the control panel into your applications directory.

Ext not loading
======
The driver is not signed and Yosemite refuses to load if you are not a developer. 
To allow loading, open a console and give the following command

```
sudo nvram boot-args="kext-dev-mode=1"
```

and reboot the machine. 


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


