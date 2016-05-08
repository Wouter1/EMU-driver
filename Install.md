<img align="right" width="400" src="E-MU_0404_USB.jpg"/>


Installation
========
If you need a low-latency driver, first check the  <a href="Latency.md">latency</a> section.
This can not be used on El Capitan (issue #65).


<h4>Prepare the machine</h4>

First time installation only: prepare your machine to take developer kernel extensions.

On El Capitan:

1. reboot in recovery mode (hold cmd-R while booting)
2. go to utilities-terminal
3. enter this command:
    ```csrutil enable --without kext```
4. reboot your machine

On Yosemite and earlier:

1. open a terminal and enter this command:
    ```sudo nvram boot-args="kext-dev-mode=1"```
2. reboot your machine. 

<h4>Install the driver</h4>

After preparation, you can install the driver:

1. Turn off the EMU device
2. Download (click on "Download ZIP") and unzip the driver
3. If on El Capitan: run the highLatency script
4. Run the kextInstall script 
5. enter your admin password at the prompt


If you like you can also copy the control panel into your applications directory. You also have to copy the Skins folder along with the app.

NOTE: After the install, it takes OSX about a minute to incorporate the driver into the kernel.

<h4>Using the control panel</h4>
You may have to eenable running 3rd-party applications before you can run the Control Panel:

1. Go into System Preferences->Security&Privacy
2. Under the General tab, change Allow Downloaded Apps From to Anywhere
3. Start the Control Panel

Usage
======

1. Install the driver as above
2. Use the EMU control panel to select the sample rate you want
3. Select the EMU input in your favourite tool (e.g., Audacity)
4. Make your recording

Uninstall
=======
Download and install the  <a href="http://support.creative.com/Products/ProductDetails.aspx?catID=237&catName=USB+Audio%2fMIDI+Interfaces&subCatID=611&subCatName=USB+Audio%2fMIDI+Interfaces&prodID=15185&prodName=0404+USB+2.0&bTopTwenty=1&VARSET=prodfaq:PRODFAQ_15185,VARSET=CategoryID:237">original EMU driver provided by E-MU</a>

OR

1. rename EMUUSBAudio original.kext to EMUUSBAudio.kext
2. Turn off your EMU device
3. run the kextInstall script

Complete Removal
============

WARNING. Be very careful with the ```rm -rf```, if you give it the wrong arguments you may remove essential system files.

1. Turn off the EMU.
2. ```sudo rm -rf /System/Library/Extensions/EMUUSBAudio.kext``` (you need admin password).
3. ```sudo rm -rf /Library/Audio/MIDI\ Drivers/EMUMIDIDriver.plugin``` (you need admin password).

