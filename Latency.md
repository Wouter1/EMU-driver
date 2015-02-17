<img align="right" width="400" src="E-MU_0404_USB.jpg"/>

Latency
=======
The default latency of the driver is set to 4.2ms. This is the 'high quality' setting, 
to ensure that not even the highest jitter in the USB system affects the audio quality.
However for some applications, low latency is more important than an occasional click in 
the audio. Therefore, the latency can be adjusted to 2.2ms (good low latency value) as follows:

1. Run (double click) the lowLatency script.
2. Run the installation procedure.

Only run this script if you want to use the low latency version of the driver.
The latency is also influenced by your application's buffer setting. You can use 
this low-latency version of the driver and increase your application's buffer size
to get a more flexible way of adjusting the latency.