# dirt

DIRT - Delt's Impulse Response Tool

A simple, open-source sinewave sweep generator, recorder, and deconvolver. In its simplest usage, takes two sinewave sweeps, one generated (dry) and the other (wet) recorded through a cab, reverb, etc. and outputs an impulse response.

Compiles and installs with cmake.
Required libraries: libsndfile, libfftw3, (for JACK support) libjack

If you have libjack installed, to explicitly disable it use:
```
cmake -DDONT_USE_JACK=on wherver/is/src/dirt 
```
Usage examples:
```
dirt --makesweep sweep_dry.wav

dirt <sweep_dry.wav> <sweep_wet.wav> <output_ir.wav>

dirt <out_jack_port> <in_jack_port> -o <output_ir.wav>
```
See --help text for more info/options

## License

DIRT is licensed under the GNU General Public License v3.0 (GPL-3.0-or-later).
See the COPYING file for full license text.
