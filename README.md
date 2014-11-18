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
1. Use the original EMU driver provided by E-MU (http://support.creative.com/Products/ProductDetails.aspx?catID=237&catName=USB+Audio%2fMIDI+Interfaces&subCatID=611&subCatName=USB+Audio%2fMIDI+Interfaces&prodID=15185&prodName=0404+USB+2.0&bTopTwenty=1&VARSET=prodfaq:PRODFAQ_15185,VARSET=CategoryID:237)
2. Use the control panel to set the EMU speed to 96 kHz
3. Turn off the EMU device

Then proceed installing this new driver:

4. Run the kextInstall script 
5. enter your admin password at the prompt

or

4. manually copy EMUUSBAudio.kext into  /System/Library/Extensions/ 
5. reboot

The original version of the driver 'EMUUSBAudio original.kext' is also provided here. 
You can copy that one back to  /System/Library/Extensions/ and reboot restore the original driver.


Usage
======

1. Install the driver as above
2. Select the EMU input in your favourite tool (e.g., Audacity)
3. Make your recording


Building from source code
======

1. Open the project in Xcode (double click src/EMUUSBAudio.xcodeproj)
2. Select Product/Build menu item
3. Select View/Navigators/Project Navigator menu item
4. right click on the  EMUUSBAudio.kext and click "Show in finder"
5. Copy the kext into the directory where the kextInstall script is

After that the kExtInstall script is ready for use (see Installation)

