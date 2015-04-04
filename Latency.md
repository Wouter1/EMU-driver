<img align="right" width="400" src="E-MU_0404_USB.jpg"/>

Latency
=======
The default latency of the driver is set to 4.2ms. This is the 'high quality' setting, 
to ensure that not even the highest jitter in the USB system affects the audio quality.
However for some applications, low latency is more important than an occasional click in 
the audio. Therefore, the latency can be adjusted to 1.0ms (good low latency value) as follows:

1. Run (double click) the lowLatency script.
2. Run the installation procedure.

Only run this script if you want to use the low latency version of the driver. You can use this low-latency version of the driver and increase your application's buffer size to get a more flexible way of adjusting the latency.

The total latency is the sum of (1) the driver latency, (2) your application's latency (buffer setting) and (3) the EMU internal latency. The EMU one-way internal latency typically is 2.2ms (96k) to 2.7ms (44k), the exact value depends on the selected sample rate and the type of EMU device that you have. The roundtrip will be twice this. If you select the low latency driver and e.g. a 64 sample buffer in your application, this gives you around 8 ms (96k) to 10 ms (44k) total roundtrip latency. The driver reports the estimated latency to the system, and eg reaper shows it in its menu bar (including its own buffer latency). 

How is it measured?
===================
To measure roundtrip latency you need reaper (free trial download from http://www.reaper.fm/ ) and a loopback cable for the EMU. The cable loops the EMU output (either TS jack or 1/8" stereo TRS jack) into a line input ( 1/4" TS jack)

 1. Attach the loopback cable.

 2. The direct monitor must be off
 
 3. The input and output volumes adjusted so that the signal comes back in approximately with the same volume.

 4. Unzip this <a href="3clicks.wav.zip">click track</a>, unzip and load it as reaper track.

 5. Make a 2nd track to record it on (Track menu/Insert new Track) and press the record icon to arm the track for recording.

 6. Set the input channel for the 2nd track to the channel that your loopback cable comes in on (or record both tracks)

 7.   go to prefs. the easiest way is from the menubar where it shows the sample rate, select audio device settings in that menu, and set the settings you want, then in the left hand side of that dialog box go down to "recording" and uncheck "use audio driver reported latency" and make sure the offset fields are also zero.

 8. press record - the audio from track one should go out the output and through the loopback cable and into track two.

 9. zoom in and see how far the recorded signal is behind the original. this is the total latency including the daw buffer.

It can help to see the latency by selecting a time period by dragging with the mouse over a empty bit of screen below the tracks

You can set the time display units by right clicking on the time display at the bottom right of the screen.

