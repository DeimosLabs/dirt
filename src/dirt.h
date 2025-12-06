
/* DIRT - Delt's Impulse Response Tool
 * Version 0.1
 *
 * -----------------------------------------------------------------------------
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 * ----------------------------------------------------------------------------
 * A simple, open-source sinewave sweep generator, recorder, and  deconvolver.
 * Takes two sinewave sweeps, one generated (dry) and the other (wet) recorded
 * through a cab, reverb, etc. and outputs an impulse response.
 *
 * usage examples:
 *
 * dirt --makesweep sweep_dry.wav
 * dirt <sweep_dry.wav> <sweep_wet.wav> <output_ir.wav>
 * dirt <out_jack_port> <in_jack_port> -o <output_ir.wav>
 *
 * See --help text for more info/options
 *
 */
 
#ifndef    __DIRT_H
#define    __DIRT_H
#define __IN_DIRT_H

#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/select.h>

#include <sndfile.h>
#include <fftw3.h>
#ifdef USE_JACK
#include <jack/jack.h>
#define AUDIO_BACKEND "JACK"
class c_jackclient;
#endif

#define MARKER_FREQ                     1000 // in Hz
#define MAX_IR_LEN                      10.0
#define LOWPASS_F                       22000 // gets clamped to nyquist freq
//#define HIGHPASS_F                      40 TODO: fix highpass
#define DEFAULT_SAMPLE_RATE             48000
#define DEFAULT_F1                      20
#define DEFAULT_F2                      22000
#define DEFAULT_JACK_NAME               "Dirt_IR_sweep" // %s becomes argv [0]
#define DEFAULT_IR_SILENCE_THRESH_DB    -72.0
#define DEFAULT_SWEEP_SILENCE_THRESH_DB -60.0
#define DEFAULT_SWEEP_SEC               30
#define DEFAULT_SWEEP_AMPLITUDE_DB      -1
#define DEFAULT_SWEEP_OFFSET_SMP        64
#define DEFAULT_NORMALIZE_AMP           0.9
#define DEFAULT_PREROLL_SEC             1.0
#define DEFAULT_MARKER_SEC              0.1
#define DEFAULT_MARKGAP_SEC             1.0
#define DEFAULT_ZEROPEAK                true // try to zero-align peak?
#define ANSI_VU_METER_MIN_SIZE          64 // for our ascii-art [---- ] meters
#define VU_REDRAW_EVERY            0.03 // in seconds
#define VU_FALL_SPEED                   0.05
#define VU_PEAK_HOLD                    0.5
#define VU_CLIP_HOLD                    1.0
#define VU_XRUN_HOLD                    5.0

//#define THRESH_RELATIVE // relative to peak, comment out for absolute
//#define DISABLE_LEADING_SILENCE_DETECTION // for debugging

extern char   
ANSI_GREY [],      ANSI_BLACK [],
ANSI_RED [],       ANSI_DARK_RED [],
ANSI_GREEN [],     ANSI_DARK_GREEN [],
ANSI_YELLOW [],    ANSI_DARK_YELLOW [],
ANSI_BLUE [],      ANSI_DARK_BLUE [],
ANSI_MAGENTA [],   ANSI_DARK_MAGENTA [],
ANSI_CYAN [],      ANSI_DARK_CYAN [],
ANSI_WHITE [],     ANSI_DARK_GREY [],
ANSI_RESET [];

/* synonyms for some of the above */
#define ANSI_PURPLE        ANSI_MAGENTA
#define ANSI_DARK_PURPLE   ANSI_DARK_MAGENTA
#define ANSI_RED_DARK      ANSI_DARK_RED
#define ANSI_GREEN_DARK    ANSI_DARK_GREEN
#define ANSI_YELLOW_DARK   ANSI_DARK_YELLOW
#define ANSI_BLUE_DARK     ANSI_DARK_BLUE
#define ANSI_MAGENTA_DARK  ANSI_DARK_MAGENTA
#define ANSI_PURPLE_DARK   ANSI_DARK_PURPLE
#define ANSI_CYAN_DARK     ANSI_DARK_CYAN

enum __dirt_ansi_colors {
  COLOR_BLACK,        COLOR_DARK_RED,     COLOR_DARK_GREEN,  COLOR_DARK_YELLOW,
  COLOR_DARK_BLUE,    COLOR_DARK_MAGENTA, COLOR_DARK_CYAN,   COLOR_DARK_GREY,
  COLOR_GREY,         COLOR_RED,          COLOR_GREEN,       COLOR_YELLOW,
  COLOR_BLUE,         COLOR_MAGENTA,      COLOR_CYAN,        COLOR_WHITE,
  COLOR_RESET
};

enum class audio_driver {
  NONE,
  JACK,
  MAX
};

enum class opmode {
  DECONVOLVE,
  MAKESWEEP,
  PLAYSWEEP,
  ERROR, // maybe extend this for diff. error types?
  MAX
};

enum class sig_source {
  NONE,
  FILE,
  JACK,
  GENERATE,
  MAX
};

/*enum sig_channels {
  chn_none,
  chn_mono,
  chn_stereo
};*/

enum class align_method {
  MARKER,
  MARKER_DRY,
  SILENCE,
  MANUAL,
  NONE,
  MAX
};

enum class audiostate {
  NOTREADY,
  IDLE,
  MONITOR,
  PLAY,
  REC,
  PLAYREC,
  PLAYMONITOR,
  MAX
};

void generate_log_sweep (double seconds,
                          double preroll_seconds,
                          double start_marker_seconds,
                          double marker_gap_seconds,
                          int samplerate,
                          float sweep_amp_db,
                          float f1, float f2,
                          std::vector<float> &out);

// our random number generator, entirely inline/header
class c_randomgen {
public:
  inline c_randomgen () { new_seed (); }
  inline void new_seed () {
    // default random seed to microseconds returned by gettimeofday ()
    struct timeval tv;
    //struct timezone tz = { 0, 0 }; /* thanks chatgpt */
    gettimeofday (&tv, NULL/*&tz*/);
    set_seed (tv.tv_sec * 60 + tv.tv_usec);
    next ();
  }
  inline c_randomgen (unsigned int origseed) { set_seed (origseed); }
  
  inline void set_seed (unsigned int newseed) { rnd_seed = newseed; }
  inline uint32_t get_seed () { return rnd_seed; }
  
  // RNG math from:
  // https://www.christianpinder.com/articles/pseudo-random-number-generation/
  inline unsigned int next () {
    int k1;
    int ix = rnd_seed;

    k1 = ix / 127773;
    ix = 16807 * (ix - k1 * 127773) - k1 * 2836;
    if (ix < 0)
      ix += 2147483647;
    rnd_seed = ix;
    
    return rnd_seed;
  }
	inline uint32_t next32 () { return (uint32_t) next (); }
  inline uint64_t next64 () { return (((uint64_t) next ()) << 32) | next (); }
  inline unsigned long int nextlong () { return (unsigned long int) next64 (); }
	/*inline double nextf (double max) {
	  long int intvalue = 0;
		intvalue = next64 () % (long int) (max * 1000000000.0);
		return ((float) intvalue) / 1000000000.0;
		
	}* /
	inline double nextf (double max) {
	  if (max == 0)
		  return 0;
		
    return (next64 () % static_cast <uint64_t> (max)) / static_cast <double> (max);
  }*/
	inline double nextf (double max) {
	  return ((next64 () >> 32) / 4294967296.0f) * max;
	}
  
private:
  // cycle of about 4.2 billion random values... good enough for now
  /*THREAD_SAFE*/ uint32_t rnd_seed;
};

class c_deconvolver;
struct s_prefs;

// this abstract class is mostly "red tape" between
// c_deconvolver and audio backends
class c_audioclient {
public:
  c_audioclient (c_deconvolver *dec);
  virtual ~c_audioclient ();
  virtual bool init () { return false; }
  virtual bool init (std::string clientname = "",
                     int samplerate = -1,
                     bool stereo_out = true) = 0;
  virtual bool shutdown () = 0;
  virtual bool ready () = 0;
  virtual bool play (const std::vector<float> &out,
                     bool block = true) = 0;
  virtual bool play (const std::vector<float> &out_l,
                     const std::vector<float> &out_r,
                     bool block = true) = 0;
  virtual bool arm_record () = 0;
  virtual bool rec () = 0;
  virtual bool playrec (const std::vector<float> &out_l,
                        const std::vector<float> &out_r) = 0;
  virtual bool stop () = 0;
  virtual bool stop_playback () = 0;
  virtual bool stop_record () = 0;
  
  virtual bool init_input (bool stereo) = 0;
  virtual bool init_output (bool stereo) = 0;
  
  virtual int get_input_ports (std::vector<std::string> &v) = 0;
  virtual int get_output_ports (std::vector<std::string> &v) = 0;
  virtual int disconnect_all (jack_port_t *port) = 0;
  virtual bool connect (jack_port_t *port, std::string port_name) = 0;
  //virtual int get_samplerate () = 0;
  //virtual int get_bufsize () = 0;
  //virtual int get_bitdepth () = 0;
  
  
  const std::vector<float> &get_recorded_l () const { return sig_in_l; }
  const std::vector<float> &get_recorded_r () const { return sig_in_r; }
  void clear_recording ();
  bool has_recording () const;
  size_t get_rec_left ();
  size_t get_play_left ();
  
  // call this after reading and displaying peak/clip/error data
  void peak_acknowledge ();
  c_deconvolver *get_deconvolver () { return dec_; }
  
  audio_driver       backend = audio_driver::NONE;
  std::string        backend_name = "default"; // or maybe "unknown"?
  std::vector<float> sig_in_l; // mono/left wet capture (mix of L/R)
  std::vector<float> sig_in_r; //
  std::vector<float> sig_out_l;  // sweep to play
  std::vector<float> sig_out_r;  // sweep to play
  
  float peak_plus_l  = 0;
  float peak_plus_r  = 0;
  float peak_minus_l = 0;
  float peak_minus_r = 0;
  bool clip_l        = false;
  bool clip_r        = false;
  bool audio_error   = false;
  bool peak_new      = false;

  size_t index     = 0;  // playback index
  size_t rec_index = 0;  // record index
  size_t rec_total = 0;  // how many samples to capture

  bool play_go = false;
  bool rec_go  = false;
  bool monitor_only = false;
  bool xrun = false;
  //audio_driver driver = driver_jack;
  
  
  /*std::string portname_dry = "";
  std::string portname_wetL = "";
  std::string portname_wetR = "";*/
  
  bool is_stereo = false;
  int samplerate = 0;
  int bufsize = 256; // sensible default in case we can't determine
  //bool play_done = false;
  audiostate state = audiostate::NOTREADY;

protected:
  c_deconvolver *dec_;
  s_prefs *prefs_;
};

// some static helper functions

bool read_wav (const char *filename, std::vector<float> &left,
                                     std::vector<float> &right, int &sr);
bool write_mono_wav (const char *filename, const std::vector<float> &v,
                     int samplerate);
bool write_stereo_wav (const char *filename, 
                       const std::vector<float> &l,
                       const std::vector<float> &r,
                       int samplerate);

// a few small static inline functions
// ...these "shouldn't" generate dup code

static inline bool file_exists (const std::string &path) {
  struct stat st {};
  return (::stat (path.c_str (), &st) == 0 && S_ISREG (st.st_mode));
}

static inline bool dir_exists (const std::string &path) {
  struct stat st {};
  return (::stat (path.c_str (), &st) == 0 && S_ISREG (st.st_mode));
}

static inline size_t next_pow2 (size_t n) {
  size_t p = 1;
  while (p < n) p <<= 1;
  return p;
}

static inline double db_to_linear (double db) {
  // clamp so we don't get crazy values
  if (db > 200.0f) db = 200.0f;
  if (db < -200.0f) db = -200.0f;
  return std::pow (10.0f, db / 20.0f);
}

static inline double linear_to_db (double v) {
  if (v <= 1e-12f) return -120.0f;
  return 20.0f * std::log10 (v);
}

#undef __IN_DIRT_H
#endif // __DIRT_H
