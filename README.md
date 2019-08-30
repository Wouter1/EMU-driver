<img align="right" width="400" src="E-MU_0404_USB.jpg"/>

EMU-USB driver
=============

OSX driver for Creative Labs EMU USB

* Tested on Mavericks, Yosemite, El Capitan and Sierra. It reportedly works fine also in High Sierra and Mojave.
* All sample rates are supported both for record and playback: 44.1, 48, 88.2, 96, 176.4 and 192 kHz
* I can only test on EMU0404 USB as I have no other EMU devices. But users reported it works also on EMU0202, EMU0204 and Tracker Pre USB.
* Midi support.

Warning for users of 2018 or later macs: there is an issue with audio glitches, see #120. My tests suggest that these problems may be limited to only the 88200 and 176400 rate, but other machines may behave differently. The driver will work and you will have sound, but there may be small ticks in your sound. Given the many audio issues with the latest apple machines reported on the web, this seems caused by a macos hardware or kernel bug. Therefore we do not consider this an issue with the EMU driver.

There is a known issue with the 0204 (control panel not working and device switching to 4 channels out which does not work); For now please check  [issue 115](https://github.com/Wouter1/EMU-driver/issues/115) for workarounds. 

Please notify me how my driver works on other EMU USB devices.

Topics
========
 * <a href="Install.md">Installation, Usage, Uninstall</a>
 * <a href="Latency.md">Adjusting the latency</a>
 * <a href="FAQ.md">frequently asked questions</a> 
 * <a href="TechSpecs.md">Technical Specifications</a>
 * <a href="Developer.md">Developer notes (build from source code, etc)</a>
 * <a href="Acceptance.md">Acceptance Test procedure</a>


