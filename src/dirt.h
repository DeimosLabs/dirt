
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
#include <cstddef>
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

#ifdef DEBUG
#ifdef BP
#undef BP
#endif
void __bp ();
void __die ();
#define BP {debug("\x1B[1;31m\x1B[5m____BREAKPOINT____\x1B[0;37m\x1B\25[m");__bp();}
#define DIE {debug("\x1B[1;31m\x1B[5m____!!!DIE!!!____\x1B[1;37m\x1B\25[m");__die();}
#else
#define BP
#define CP
#endif

#define DEFAULT_SAMPLERATE             48000
#define SAMPLERATE_MIN                  8000
#define SAMPLERATE_MAX                  192000
#define MARKER_FREQ                     1000 // in Hz
#define MAX_IR_LEN                      10.0
#define LOWPASS_F                       22000 // gets clamped to nyquist freq
//#define HIGHPASS_F                      40 TODO: fix highpass
#define DEFAULT_F1                      20
#define DEFAULT_F2                      22000
#define DEFAULT_JACK_NAME               "Dirt_IR_sweep" // %s becomes argv [0]
#define DEFAULT_IR_SILENCE_THRESH_DB    -72.0
#define DEFAULT_SWEEP_SILENCE_THRESH_DB -60.0
#define DEFAULT_SWEEP_SEC               30
#define DEFAULT_SWEEP_AMPLITUDE_DB      -1
#define DEFAULT_SWEEP_OFFSET_SMP        64
#define DEFAULT_NORMALIZE_AMP           0.9
#define DEFAULT_PREROLL_SEC             0.4
#define DEFAULT_MARKER_SEC              0.1
#define DEFAULT_MARKGAP_SEC             0.5
#define DEFAULT_ZEROPEAK                true // try to zero-align peak?
#define ANSI_VU_METER_MIN_SIZE          64 // for our ascii-art [====--] meters
#define VU_REDRAW_EVERY                 0.03 // in seconds
#define VU_FALL_SPEED                   0.1
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

enum class waverror {
  NONE,
  OPEN,
  LOAD,
  SAVE,
  READ,
  WRITE,
  NAME,
  SAMPLERATE,
  CHANNELS,
  BITDEPTH
};

enum class audio_driver {
  NONE,
  JACK,
  MAX
};

enum class opmode {
  DECONVOLVE,
  ROUNDTRIP,
  MAKESWEEP,
  PLAYSWEEP,
  GUI,
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
  UNKNOWN,
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

class c_wavebuffer;
class c_audioclient;
class c_deconvolver;
struct s_prefs;

void generate_log_sweep (double seconds,
                          double preroll_seconds,
                          double start_marker_seconds,
                          double marker_gap_seconds,
                          int samplerate,
                          float sweep_amp_db,
                          float f1, float f2,
                          c_wavebuffer &out);

// simple API class for manipulating wave data
// just uses static vectors for now
class c_wavebuffer {
public:
  using sample_t = float;
  
  c_wavebuffer () = default;
  
  void clear () { return samples_.clear (); }
  size_t size () { return samples_.size (); }
  bool empty () { return (size () == 0); }
  void append (const sample_t *data, size_t count);
  void append (const std::vector<sample_t> &vec);
  void resize (const size_t sz) { samples_.resize (sz); }
  void insert (size_t pos, const sample_t *data, size_t count);
  void insert (size_t pos, const std::vector<sample_t> &vec);
  void erase (size_t start, size_t len);
  void import_from (const sample_t *data, size_t count);
  void import_from (const std::vector<sample_t> &vec);
  void export_to (sample_t *out, size_t max) const;
  void export_to (std::vector<sample_t> &vec) const;
  
  int get_samplerate () { return samplerate_; }
  void set_samplerate (int sr) { samplerate_ = sr; }
  
  sample_t &operator[] (size_t i) { return samples_ [i]; }
  const sample_t &operator[] (size_t i) const { return samples_ [i]; }
  const sample_t *data () const { return samples_.data (); }
  sample_t *data () { return samples_.data (); } // c++ actually lets me do this??
  
private:
  std::vector<sample_t> samples_;
  int samplerate_;
};

class c_vudata {
public:
  bool sample (float l, float r);
  bool update (); // normally called once per buffer from rt thread
  void acknowledge (); // from ui: tells it to reset peak/hold data
  //c_audioclient *audio ();
  
  // set/get these at each frame & call update () from audio thread
  float abs_l   = 0;
  float abs_r   = 0;
  float minus_l = 0; // keeping both + and - for drawing waveforms from vu data
  float minus_r = 0;
  float plus_l  = 0;
  float plus_r  = 0;
  
  float hold_l = 0;
  float hold_r = 0;
  bool  clip_l        = false;
  bool  clip_r        = false;
  bool  xrun          = false;
  bool  peak_new      = false;
  bool  is_stereo     = true;
  bool  acknowledged  = false;
  bool  needs_redraw  = true;
  
  size_t timestamp_now = 0;
  size_t timestamp_hold_l = 0;
  size_t timestamp_hold_r = 0;
  size_t timestamp_clip_l = 0;
  size_t timestamp_clip_r = 0;
  size_t timestamp_xrun = 0;
  
  int samplerate  = 48000; // reasonable defaults, audioclient should set these
  int bufsize     = 128;
  size_t bufcount = 0;

protected:
  int bufs_per_sec = 1;
  int redraw_every = 0;
};

// this abstract class is mostly "red tape" between
// c_deconvolver and audio backends
class c_audioclient {
public:
  c_audioclient (s_prefs *prefs);
  virtual ~c_audioclient ();
  virtual bool init () { return false; }
  virtual bool init (std::string clientname = "",
                     int samplerate = -1,
                     bool stereo_out = true) = 0;
  virtual bool shutdown () = 0;
  virtual bool register_input (bool stereo) = 0;
  virtual bool register_output (bool stereo) = 0;
  virtual bool set_stereo (bool stereo) = 0;
  
  virtual bool unregister () = 0;
  virtual bool ready () = 0;
  virtual bool play (c_wavebuffer *out) = 0;
  virtual bool play (c_wavebuffer *out_l,
                     c_wavebuffer *out_r) = 0;
  virtual bool arm_record () = 0;
  //virtual bool rec () = 0;
  virtual bool rec (c_wavebuffer *in_l,
                    c_wavebuffer *in_r) = 0;
  virtual bool playrec (c_wavebuffer *out_l,
                        c_wavebuffer *out_r,
                        c_wavebuffer *in_l,
                        c_wavebuffer *in_r) = 0;
  virtual bool stop (bool also_stop_monitor = false) = 0;
  virtual bool stop_playback () = 0;
  virtual bool stop_record (bool also_stop_monitor = false) = 0;
  
  virtual int get_input_ports (std::vector<std::string> &v) = 0;
  virtual int get_output_ports (std::vector<std::string> &v) = 0;
  /*virtual int disconnect_all (jack_port_t *port) = 0;
  virtual bool connect (jack_port_t *port, std::string port_name) = 0;*/
  virtual int get_samplerate () = 0;
  virtual int get_bufsize () = 0;
  virtual int get_bitdepth () = 0;
  
  // UI hooks for derived classes, audio backends should call these
  // override them for UI other than textmode
  virtual int on_idle () { return 0; }
  virtual int on_play_start (void *data = NULL) { return 0; }
  virtual int on_play_loop (void *data = NULL) { return 0; }
  virtual int on_play_stop (void *data = NULL) { return 0; }
  virtual int on_arm_rec_start (void *data = NULL) { return 0; }
  virtual int on_arm_rec_loop (void *data = NULL) { return 0; }
  virtual int on_arm_rec_stop (void *data = NULL) { return 0; }
  virtual int on_record_start (void *data = NULL) { return 0; }
  virtual int on_record_loop (void *data = NULL) { return 0; }
  virtual int on_record_stop (void *data = NULL) { return 0; }
  virtual int on_playrec_start (void *data = NULL) { return 0; }
  virtual int on_playrec_loop (void *data = NULL) { return 0; }
  virtual int on_playrec_stop (void *data = NULL) { return 0; }
  
  //virtual void set_vu_pre ();
  //virtual void set_vu_l (float min, float max, float hold, bool clip, bool xrun);
  //virtual void set_vu_r (float min, float max, float hold, bool clip, bool xrun);
  //virtual void set_vu_post ();
  
  c_wavebuffer *get_recorded_l () { return sig_in_l;  }
  c_wavebuffer *get_recorded_r () { return sig_in_r;  }
  c_wavebuffer *get_played_l ()   { return sig_out_l; }
  c_wavebuffer *get_played_r ()   { return sig_out_r; }
  //void clear_recording ();
  //bool has_recording () const;
  size_t get_rec_remaining ();
  size_t get_play_remaining ();
  
  audio_driver       backend = audio_driver::NONE;
  std::string        backend_name = "default"; // or maybe "unknown"?
  c_wavebuffer       *sig_in_l   = NULL; // mono/left wet capture (mix of L/R)
  c_wavebuffer       *sig_in_r   = NULL; //
  c_wavebuffer       *sig_out_l  = NULL;  // sweep to play
  c_wavebuffer       *sig_out_r  = NULL;  // sweep to play
  
  c_vudata vu_in, vu_out;
  //float peak_plus_l  = 0;
  //float peak_plus_r  = 0;
  //float peak_minus_l = 0;
  //float peak_minus_r = 0;
  //bool clip_l        = false;
  //bool clip_r        = false;
  //bool audio_error   = false;
  //bool peak_new      = false;

  size_t play_index  = 0;  // playback index
  size_t rec_index   = 0;  // record index
  size_t rec_max     = 0;  // how many samples to capture

  bool play_go = false;
  bool rec_go  = false;
  bool monitor_only = false;
  bool xrun = false;
  //audio_driver driver = driver_jack;
  
  bool stereo_in = false;
  bool stereo_out = false;
  int samplerate = 0; // 0 means not determined yet / use backend default
  int bufsize = 256;  // sensible defaults in case we can't determine
  int bitdepth = 24; 
  //bool play_done = false;
  audiostate state = audiostate::NOTREADY;
  
protected:
  //c_deconvolver *dec_;
  s_prefs *prefs;
};

// some static helper functions

/*bool read_wav (const char *filename, c_wavebuffer &left,
                                     c_wavebuffer &right, int &sr);
bool write_mono_wav (const char *filename, const c_wavebuffer &v,
                     int samplerate);
bool write_stereo_wav (const char *filename, 
                       const c_wavebuffer &l,
                       const c_wavebuffer &r,
                       int samplerate);*/

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

#undef __IN_DIRT_H
#endif // __DIRT_H
