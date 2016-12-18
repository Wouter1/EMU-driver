<img align="right" width="400" src="E-MU_0404_USB.jpg"/>



Acceptance Test
===================

This describes the procedure to check that the driver is playing and recording correctly.

The following tests should succeed for all available rates and all driver versions but in practice I often skip the 88k and 176k rates because of time constraints (the full test takes more than an hour).

There are two ways to do this. One is with reaper, one with Audacity. Both tests need the loopback cable (see <a href="Latency.md">latency test</a> attached and volume adjusted. Make sure phantom power is off.

The Audacity version goes as follows

1. Set the test rate in the EMU Control Panel
2. Set audacity project rate to the test rate
3. Generate a 10 minute 440Hz sine tone, amplitude 0.8 (Generate/Tone)
4. export the project as wav
5. open the wav in quicktime 7
6. in system preferences/audio select the EMU as OUTPUT
7. clean up audacity (remove that generated track)
8. select the EMU as input in audacity 
9. set input and output volume to around 0.9
10. start recording in audacity
11. start playing in quicktime
12. When the track is fully played after 10 minutes, stop audacity recording
13. Apply in Audacity the Effect/Notch Filter. Set 440Hz and Q=11. 
14. Check if there are any clicks between the moment of start and end of the quicktime playing. Zoom in a few times to make sure you do not miss clicks.


The reaper version goes as follows:

1. open <a href="sinetest.RPP">the test setting</a> in reaper
2. set the test rate in the reaper preferences/Audio/Device. For this test, use block size 128 or larger. 64 is too small for the slower computers.
3. press "record"
4. After 10 minutes, stop recording and "Save all"
5. right click on the "recording trac" and select "Apply track FX to items as new take"
6. Check if there are any clicks between the moment of start and end of the track. Zoom in a few times and scale up the vertical size to make sure you do not miss clicks.

In both cases, the test succeeded for the selected rate if you did not find any clicks.

