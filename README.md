# dirt

DIRT - Delt's Impulse Response Tool
Version 0.1

A simple, open-source sinewave sweep generator, recorder, and deconvolver. In its simplest usage, takes two sinewave sweeps, one generated (dry) and the other (wet) recorded through a cab, reverb, etc. and outputs an impulse response.

compile and install with cmake.

usage examples:

dirt --makesweep sweep_dry.wav
dirt <sweep_dry.wav> <sweep_wet.wav> <output_ir.wav>
dirt <out_jack_port> <in_jack_port> -o <output_ir.wav>

See --help text for more info/options
