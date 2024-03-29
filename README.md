<img align="right" width="400" src="E-MU_0404_USB.jpg"/>

EMU-USB driver
=============

OSX driver for Creative Labs EMU USB

* Tested on Mavericks, Yosemite, El Capitan, Sierra, High Sierra, Mojave, Catalina and Big Sur. M1 is NOT supported because I don't have a M1 machine (https://github.com/Wouter1/EMU-driver/issues/136)
* All sample rates are supported both for record and playback: 44.1, 48, 88.2, 96, 176.4 and 192 kHz
* I can only test on EMU0404 USB as I have no other EMU devices. But users reported it works also on EMU0202, EMU0204 and Tracker Pre USB.
* Midi support.

There is a known issue with the 0204 (control panel not working and device switching to 4 channels out which does not work); For now please check  [issue 115](https://github.com/Wouter1/EMU-driver/issues/115) for workarounds. 

[Here you can report proper workings and thanks](https://github.com/Wouter1/EMU-driver/issues/109) . Reports on the other devices like 0204 are especially welcome as I can not test or fix those.

Topics
========
 * <a href="Install.md">Installation, Usage, Uninstall</a>
 * <a href="Latency.md">Adjusting the latency</a>
 * <a href="FAQ.md">frequently asked questions</a> 
 * <a href="TechSpecs.md">Technical Specifications</a>
 * <a href="Developer.md">Developer notes (build from source code, etc)</a>
 * <a href="Acceptance.md">Acceptance Test procedure</a>


End Of Support
==============
I moved away from the Apple MacOS platform to Linux.
Therefore my support for this driver has stopped.


