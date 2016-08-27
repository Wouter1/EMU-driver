<img align="right" width="400" src="E-MU_0404_USB.jpg"/>


Technical Specifications
========================


General
------

 * Sample Rates: 44.1, 48, 88.2, 96, 176.4, 192kHz from internal crystal (no sample rate conversion)
  * Bit Depth: 24-bit I/O, 32-bit floating point processing
 * USB 2.0 Hi-Speed
  * Full 24-bit resolution at all sample rates
  * 4in/4 out channels from 44.1-96kHz
  * 2 in/2 out channels from 176.4-192kHz
 * Latency:
  * "Zero-latency" (~4.5ms latency roundtrip) direct hardware monitoring (disabled at 176.4-192kHz)
  * One-way latency of driver+EMU internal: down to 5.5 ms (default) or 2.3 ms (low latency setting) at 96kHz rate. 
 * Apple CoreAudio and Apple CoreMIDI.
  * AC3 and DTS Passthru supported (not tested if this works)
 * Anti-Pop speaker protection minimizes noise during power on/off
 * Ultra-low jitter clock subsystem: < 500ps RMS in PLL mode (48kHz, Coaxial S/PDIF Sync)
   
Combo Microphone Preamplifier/Hi-Z/Line Inputs (2)
------

 * Type: E-MU XTC™ combo mic preamplifier and Hi-Z/line input w/ Soft Limiter
 * A/D converter: AK5385A
 * Gain Range: +60dB
 * Frequency Response (min gain, 20Hz-20kHz): +0.0/-0.16dB
 * Stereo Crosstalk (1kHz min gain, -1dBFS): < -110dB
 * Hi-Z Line Input:

  * Input Impedance: 1M&Omega;
  * Max Level: +12dBV (14.2dBu)
  * Dynamic Range (A-weighted, 1kHz, min gain): 113dB
  * Signal-to-Noise Ratio (A-weighted, min gain): 113dB
  * THD+N (1kHz at -1dBFS, min gain): -101dB (.0009%)
 * Microphone Preamplifier:

  * Input Impedance: 1.5k&Omega;
  * Max Level: +6dBV (+8.2dBu)
  * EIN (20Hz-20kHz, 150&Omega;, unweighted): -127dBu
  * Signal-to-Noise Ratio (A-weighted, min gain): 112.5dB
  * THD+N (1kHz at -1dBFS, min gain): -101dB (.0009%)
  * Phantom Power: 48V
  * Soft Limiter: 5dB max compression (software selectable)
  
Analog Line Outputs (2)
------

 * Type: balanced, AC-coupled, 2-pole low-pass differential filter
 * D/A converter: AK4396
 * Level (auto detect):

  * Professional: +12dBV max (balanced)
  * Consumer: +6dBV max (unbalanced)
 * Frequency Response (20Hz - 20kHz): 0.06/-.035dB
 * Dynamic Range (1kHz, A-weighted): 117dB
 * Signal-to-Noise Ratio (A-weighted): 117dB
 * THD+N (1kHz at -1dBFS): -100dB (.001%)
 * Stereo Crosstalk (1kHz at -1dBFS): < -114.5dB
 * Balanced/Unbalanced Output Impedance: 560&Omega;
 
Headphone Amplifier
------

 * Type: Class-A power amplifier
 * D/A converter: AK4396 (shared with Line Out)
 * Gain Range: 60dB
 * Maximum Output Power: 20mW
 * Output Impedance: 22&Omega;
 * Frequency Response (20Hz-20kHz): +0.06/-0.035dB
 * Dynamic Range (A-weighted): 114dB
 * Signal-to-Noise Ratio (A-weighted): 114dB
 * THD+N (1kHz, max gain): 600&Omega; load: -95.5dB (.0018%)
 * Stereo Crosstalk (1kHz at -1dBFS, 600&Omega; load): < -85dB
 
Digital I/O
------

 * S/PDIF*:
  * 2 in/2 out coaxial (transformer coupled)
  * 2 in/2 out optical
  * AES/EBU or S/PDIF format (software selectable)
 * MIDI
  * 1 in, 1 out
  
Synchronization
------

 * Internal crystal sync at 44.1, 48, 88.2, 96, 176.4, 192kHz
 * External sample rate sync via
  * Optical S/PDIF (44.1 - 96kHz)
  * Coaxial S/PDIF (44.1 - 96kHz)
  
System Requirements
------

 * Apple Macintosh 
 * Apple Macintosh OS X Mavericks (10.9) 64 bits or higher
 * 1 available (Hi-speed) USB 2.0 port
