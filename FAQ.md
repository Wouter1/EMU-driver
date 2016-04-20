<img align="right" width="400" src="E-MU_0404_USB.jpg"/>

FAQ
=============

Here's the Frequently Asked Questions

|question|answer|
|---|---|
| Which version of the driver do I have? | Go to Apple in the menu bar and select About this Mac. Then go More Info.../System Report/Extensions and scroll down to EMUUSBAudio. You can now read off the version in the Version column. |
| Why do we need to set the sample rate of the driver? I can do this just from my application/DAW? | The driver determines the native sampling rate. If you select something different in your application, normally the system will *interpolate* to the requested frequency, and not change the actual sample rate in hardware. |
|I have some clicking noises when I play with Quicktime 10 | There is a bug in Quicktime 10, at least on Mavericks, that sometimes  causes distortion in playback (see #26). We suggest you try Quicktime 7 (http://support.apple.com/kb/DL923). |
| I have clicking noises and this is not with Quicktime 10 | The most common cause of this is that the CPU is overloaded. This happens typically if the buffer size (eg in Reaper) is set too small. Try increasing the audio the buffer sizes. Typically 64 can not be handled by older computers (say, before 2011).|
|Why is the CPU overloaded yet the CPU monitor does not show any overload? | With audio, the CPU is needed heavily for short periods of time. The average load may be fine but the peak load can still be too high. |
|Is the driver bit accurate?|No. The driver takes floats (required by Core Audio) and has to deliver 24bit integers to the EMU. But bit accuracy is already lost earlier, in the core audio system of the OS, where sample rate conversion is used to match slightly different data rates of CPU clock and EMU clock, sample rate conversion etc.|
|Can your code be modified to report latencies ? | Yes input and output latency are both reported separately from the driver to the kernel. The driver picks the values from the plist, or picks default values of 4.2ms.|
|Is the driver reported latency the real true latency?|No. It's only an estimation of the latency inside the driver and USB connection. And your application will add latency on top of this.|
|can the USB frame rate be changed?| The USB frame rate is the speed with which the driver sends data to the device. This is currently hard coded to use the highest available speed (which allows us to lower the latencies). In the past when USB2.0 was rare, lower speeds had to be supported but this is not necessary anymore.|
|What buffers does the driver have? |There is one ring buffer for input and one for output. And there are buffers for communication with USB input and output. |
|is the driver reported latency round trip latency? |No, it's one way latency. For a roundtrip you have to add input and output latency, plus your application's latency on input and output|
|What is the theoretic minimum round trip latency? | If you use the current low latency driver setting of 1.0 ms, use 96kHz and have a program that does not add extra latency (if there is any program that supports this) you would get around 6.4ms roundtrip. You can lower the latency even further (check <a href="Latency.md">adjusting the latency</a>) but you will quickly end up with distortion.|
|Is there a hard lower limit or just that you start to get dropouts/buffer underruns? | Yes, USB has freedom of about 0.5ms in the transport times. This causes uncertainty in the exact data rate of the EMU clock which leads to extra buffer requirements.|
|Are the input and output latencies the same? |No, the hardware inside the EMU has different AD and DA chips that induce different latencies.|
|Are the latencies constant?|No, there is fluctuation because we can not exactly determine the position of the USB stream due to the timing inaccuracies mentioned above. But by filtering we keep both the size and speed of the fluctuations small. I have no exact measurements as I don't have the required equipment but the plots at the end of issue #40 suggest fluctuations in the order of a few samples per minute. |
|Do you report input and output latency separately?| Yes, the reported latencies are considering the latencies of the AD and DA chips of the hardware that is actually used.|
|Is the buffer in the DAW conceptually separate from the buffer in the driver? | yes |
| Can you reduce the driver buffer to 0 and increase the buffer in the DAW to compensate? | No.|
|Why do you need a separate buffer in the driver?| The driver needs its own buffer because (1) it has to be in kernel space (2) it needs to convert the incoming float samples into 24bit integers (3) it needs to mix various input sources into one stream to the EMU|
|Can the 1ms USB polling rate be increased?|This is a hard maximum in Apple's USB driver. Maybe if you would install a different USB driver but even then it's doubtful as the USB protocol also has a 1ms freedom built in for streaming transport. |
|what is the relationship between the baseline USB polling rate and the “frame rate” |The USB rate with the EMU is one or 2 frames per millisecond (maximum for USB2 is 8 per ms). I think this runs in hardware. The polling rate is the rate at which the computer checks if data has come in. This is done at most once per millisecond and in software. This polling rate determines the accuracy of the timestamps on the incoming data|
|would 2 frames per millisecond give you a higher polling rate and better accuracy on the timing?| No, FAIK the polling rate stays once per millisecond. So the timer accuracy stays the same.|
|does a faster cpu allow lower driver latencies?|no. USB polling rate is timer based. If your computer would be too slow to keep up with the datarate, the connection would probably break|
|Is the polling rate fixed in hardware or software?| I don't know exactly. I would expect a hardware based interrupt is running behind this but the polling timestamping seems in software.|
|Would latency lower if the DAW delivered 24 bit integers instead of float?|No. latency is caused by the timing uncertainty of the USB transport.|
|it would presumably be easy in principle for a DAW to deliver 24bit integers in a single stream with sole control over the device|No delivery of data is actually pretty complex. This is because data has to be timed accurately to ensure there are no buffer over/underflows in the EMU|
|What determines the exact data output rate|This is determined by the EMU. Or, if you have set the EMU to external sync, the source of the SPDIF input|
|How is the playback data output kept at exactly the correct rate|By also opening a data input channel and measuring the incoming data rate|
|The extra processes and latency are the price we pay for the convenience of integrated system audio - is that correct?|Not exactly. The latency comes from timing uncertainty in the USB stream. The system indeed requires extra processes to synchronize multiple streams but the overhead here seems not so large|
|The latency shown in the menu bar in Reaper are slightly lower than the actual latency? | Yes, it seems Reaper is truncating (not rounding) the latencies shown in the menu bar. Therefore actual values can be 0.1ms higher than reported, and the roundtrip even 0.2 ms. |
|can SafetyOffset can be adjusted in original EMU kext or in Yours only?| There were some adjustable items in the original driver but insufficient and (if I remember correctly) in other units (not milliseconds as in my version of the driver). Also, the original driver had other issues besides timing issues. I totally reworked the synchronizatino mechanism to fix that. You can try to tweak the plist in the original driver, but I dont think that you can fix the original driver by tweaking parameters.|
|what is latencyADC and latencyDAC ?| These are internal settings for the my driver. They refer to the latency of the ADC and DAC. From top of my head, the number of samples.|

