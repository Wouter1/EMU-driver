<img align="right" width="400" src="E-MU_0404_USB.jpg"/>

FAQ
=============

Here's the Frequently Asked Questions

|question|answer|
|---|---|
| Which version of the driver do I have? | Go to Apple in the menu bar and select About this Mac. Then go More Info.../System Report/Extensions and scroll down to EMUUSBAudio. You can now read off the version in the Version column. |
| Why do we need to set the sample rate of the driver? I can do this just from my application/DAW? | The driver determines the native sampling rate. If you select something different in your application, normally the system will *interpolate* to the requested frequency, and not change the actual sample rate in hardware. |
|Is the driver bit accurate?|No. The driver takes floats (required by Core Audio) and has to deliver 24bit integers to the EMU. But bit accuracy is already lost earlier, in the core audio system of the OS, where sample rate conversion is used to match slightly different data rates of CPU clock and EMU clock, sample rate conversion etc.|
|Can your code be modified to report latencies ? | Yes input and output latency are both reported separately from the driver to the kernel. The driver picks the values from the plist, or picks default values of 4.2ms.|
|Is the driver reported latency the real true latency?|No. It's only an estimation of the latency inside the driver and USB connection. I don't know the EMU internal latency. And your application will add latency on top of this.|
|can the USB frame rate be changed?|Yes, by changing the sample rate with the Control panel|
|Are the buffers the driver uses one for input and one for output? |Yes, there are separate input and output buffers|
|is latency round trip latency? |No, it's one way latency. For a roundtrip you have to add input and output latency, plus your application's latency|
|A minimum round trip latency is 4.4ms? | Yes if you use the current low latency setting of 2.2ms, and have a program that does not add extra latency. The default would be 8.4ms roundtrip. You can lower the latency even further (check <a href="Latency.md">adjusting the latency</a>) but at the cost of more distortion.|
|Is there a hard lower limit or just that you start to get dropouts/buffer underruns? | Yes, USB has freedom of about 1ms in the transport times. This causes uncertainty in the exact data rate of the EMU clock which leads to extra buffer requirements. In all, it is quite unlikely but possible that we will get above 2ms from the actual datarate. 4ms is extremely unlikely and was never detected.|
|Are the input and output latencies necessarily the same? |Not necessarily, but the driver has been designed to make them the same.|
|Are the latencies constant?|No, there is fluctuation because we can not exactly determine the position of the USB stream due to the timing inaccuracies mentioned above|
|Would it be useful to be able to set input and output latency separately?| This would be so only if the driver design would support this. It seems not useful with the current driver.|
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
