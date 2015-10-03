<img align="right" width="400" src="E-MU_0404_USB.jpg"/>


Installation
========
If you need a low-latency driver, first check the  <a href="Latency.md">latency</a> section.


<h4>Prepare the machine</h4>


First time installation only: prepare your machine to take developer kernel extensions.

On El Capitano (be aware of issue #65):

1. reboot in recovery mode (hold cmd-R while booting)
2. go to utilities-terminal
3. enter this command:
    ```csrutil disable```
4. reboot your machine

On Yosemite and earlier:

1. open a terminal and enter this command:
    ```sudo nvram boot-args="kext-dev-mode=1"```
2. reboot your machine. 

<h4>Install the driver</h4>

After preparation, you can install the driver:

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
Download and install the  <a href="http://support.creative.com/Products/ProductDetails.aspx?catID=237&catName=USB+Audio%2fMIDI+Interfaces&subCatID=611&subCatName=USB+Audio%2fMIDI+Interfaces&prodID=15185&prodName=0404+USB+2.0&bTopTwenty=1&VARSET=prodfaq:PRODFAQ_15185,VARSET=CategoryID:237">original EMU driver provided by E-MU</a>

OR

1. rename EMUUSBAudio original.kext to EMUUSBAudio.kext
2. Turn off your EMU device
3. run the kextInstall script

 loose ends and I'd like to clean up the code further.

Complete Removal
============
1. Turn off the EMU.
2. Open a terminal
3. execute ```cd /System/Library/Extensions/```
4. execute ```sudo rm -rf EMUUSBAudio.kext``` 
5. enter your admin password.

