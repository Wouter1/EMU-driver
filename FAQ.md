<img align="right" width="400" src="E-MU_0404_USB.jpg"/>

FAQ
=============

Here's the Frequently Asked Questions

|question|answer|
|---|---|
|Can your code be modified to report latencies ? | Yes input and output latency are both reported separately from the driver to the kernel. The driver picks the values from the plist, or picks default values of 4.2ms.|
|Is the driver reported latency the real true latency?|No. It's only an estimation of the latency inside the driver and USB connection. I don't know the EMU internal latency. And your application will add latency on top of this.|
|can the USB frame rate be changed?|Yes, by changing the sample rate with the Control panel|
|Are the buffers the driver uses one for input and one for output? |Yes, there are separate input and output buffers|
|is latency round trip latency? |No, it's one way latency. For a roundtrip you have to add input and output latency, plus your application's latency|
|A minimum round trip latency is 4.4ms? | Yes if you use the current low latency setting of 2.2ms, and have a program that does not add extra latency. The default would be 8.4ms roundtrip. You can lower the latency even further (check <a href="Latency.md"adjusting the latency</a>) but at the cost of more distortion.|
|Is there a hard lower limit or just that you start to get dropouts/buffer underruns? | Yes, USB has freedom of about 1ms in the transport times. This causes uncertainty in the exact data rate of the EMU clock which leads to extra buffer requirements. In all, it is quite unlikely but possible that we will get above 2ms from the actual datarate. 4ms is extremely unlikely and was never detected.|
|Are the input and output latencies necessarily the same? |Not necessarily, but the driver has been designed to make them the same.|
|Are the latencies constant?|No, there is fluctuation because we can not exactly determine the position of the USB stream due to the timing inaccuracies mentioned above|
|Would it be useful to be able to set input and output latency separately?| This would be so only if the driver design would support this. It seems not useful with the current driver.|
|Is the buffer in the DAW conceptually separate from the buffer in the driver? | yes |
| Can you reduce the driver buffer to 0 and increase the buffer in the DAW to compensate? | No.|
|Why do you need a separate buffer in the driver?| The driver needs its own buffer because (1) it has to be in kernel space (2) it needs to convert the incoming float samples into 24bit integers (3) it needs to mix various input sources into one stream to the EMU|

