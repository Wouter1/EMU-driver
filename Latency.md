<img align="right" width="400" src="E-MU_0404_USB.jpg"/>

Latency
=======
The default latency of the driver is set to 4.2ms. This is the 'high quality' setting, 
to ensure that not even the highest jitter in the USB system affects the audio quality.
However for some applications, low latency is more important than an occasional click in 
the audio. To adjust the latency to 1.0ms (good low latency value) you just select "y" when the installer asks
```Use low latency setting?```.

Latency Details
---------------
The total latency is the sum of (1) the driver latency, (2) your application's latency (buffer setting) and (3) the EMU internal latency. The EMU one-way internal latency typically is 1.3 ms (96k) to 1.8 ms (44k), the exact value depends on the selected sample rate and the type of EMU device that you have. The roundtrip will be twice this. If you select the low latency driver and e.g. a 64 sample buffer in your application, this gives you around 5.9 ms (96k) to 8.5 ms (44k) total roundtrip latency. The driver reports the estimated latency to the system, and eg reaper shows it in its menu bar (including its own buffer latency). 

Here is a table with measured roundtrip latencies in milliseconds. The driver was installed with the low latency setting. The values N=128 and N=64 show the DAW buffer size. 

|rate|latency N=128|latency N=64|
|---|------|------|
|44k|11.6|8.5|
|48k|10.8|8.1|
|96k|7.3|6.0|
|176k|5.6|4.9|
|192k|5.5|4.9|

How is it measured?
===================
To measure roundtrip latency you need reaper (free trial download from http://www.reaper.fm/ ) and a loopback cable for the EMU. The cable loops the EMU output (either TS jack or 1/8" stereo TRS jack) into a line input ( 1/4" TS jack)

 1. Attach the loopback cable.

 2. The direct monitor must be off. Phantom power must be off.
 
 3. The input and output volumes adjusted so that the signal comes back in approximately with the same volume.

 4. Unzip this <a href="3clicks.wav.zip">click track</a>, unzip and load it as reaper track.

 5. Make a 2nd track to record it on (Track menu/Insert new Track) and press the record icon to arm the track for recording.

 6. Set the input channel for the 2nd track to the channel that your loopback cable comes in on (or record both tracks)

 7.   go to prefs. the easiest way is from the menubar where it shows the sample rate, select audio device settings in that menu, and set the settings you want, then in the left hand side of that dialog box go down to "recording" and uncheck "use audio driver reported latency" and make sure the offset fields are also zero.

 8. press record - the audio from track one should go out the output and through the loopback cable and into track two.

 9. zoom in and see how far the recorded signal is behind the original. this is the total latency including the daw buffer.

It can help to see the latency by selecting a time period by dragging with the mouse over a empty bit of screen below the tracks

You can set the time display units by right clicking on the time display at the bottom right of the screen.

Further optimization
--------------------
If you really need the lowest possible latencies, you might consider hacking the installer before running it and replace the 1000 (microseconds) in there with an even lower value. You may have to do you own measurements (see #40) to see how far you can go without distortions. For the 0404 these seem absolute minimum values:

| rate | min latency |
| --- | --- |
| 44k |  0.57ms |
| 48k | 0.58ms |
| 88k | 0.70ms |
| 96k | 0.70ms |
| 176k | 0.58ms |
| 192k | 0.740ms |


Comparison with other cards
--------------------------

The following table shows the EMU performance. All values with 128sample DAW buffer. All latencies are the total roundtrip latency in milliseconds.

| device | latency @44k | latency @48k | latency @96k | latency @192k |
|---|---|---|---|---|
| Windows7 EMU0404 PCIe | 6.1 | - | - | - | 
| RME FireFace UFX USB | 7.8 | - | - | - |
| RME Fireface 800| 9.7 | 8.9 | 6.0 | 5.5 |
| Windows M-Audio 1010lt | 10.3 | 9.5 | 4.7 | - |
| Windows7 Presonus Firestudio Mobile| 10.3 | 9.6 | 6.1 | - |
| Alesis Photon 25 | 10.34 | 10.33 | - | - |
| Roland Duo-Capture EX* | 11.4 | - | - | - |
| EMU0404 (low latency setting) | 11.5 | 10.8 | 7.2 | 5.4 | 
| MOTU Track16 | 11.9 | - | - | - |
| Scarlett 2i2 | 11.1 | 12.5 | - | - | 
| Native instr Kore 1| 12.4 | 11.9 | 7.1 | - |
| Focusrite Clarett 2Pre USB | 13.0 | - | - | - |
| Behringer FCA202 | 13.5 | 12.0 | 7.7 | - |
| PreSonus Studio 192 | 14 | - | - | - |
| Windows7 Line 6 GX | 19.3 | 18.1 | - | - |
| U-Control UCA222 (Windows audio drivers)* | 15.5 | - | - | - |
| Windows7 Creative Sound Blaster X-Fi pci-e | 20.4 | 19.2 | 19.4 | - |
| Novation Audio Hub 2x4 ("FocusRite technology") | 24.8 | - | - | - |

part of this data comes from http://blog.ultimateoutsider.com/2018/04/comparing-usb-audio-interface-latency.html. Other data collected from various sources on the web. Some more on latency with also USB3 and PCI-E cards can be found  https://www.gearslutz.com/board/music-computers/618474-audio-interface-low-latency-performance-data-base.html
