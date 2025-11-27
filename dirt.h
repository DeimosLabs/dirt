
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
 
#ifndef __DECONVOLVER_H
#define __DECONVOLVER_H

#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <unistd.h>
#include <sys/time.h>
#include <sys/stat.h>

// these are set by cmake
//#define DEBUG
//#define USE_JACK

#include <sndfile.h>
#include <fftw3.h>
#ifdef USE_JACK
#include <jack/jack.h>
#endif

#define MARKER_FREQ                     1000 // in Hz
#define MAX_IR_LEN                      10.0
#define LOWPASS_F                       24000 // gets clamped to nyquist freq
//#define HIGHPASS_F                      40 TODO: fix highpass
#define DEFAULT_SAMPLE_RATE             48000
#define DEFAULT_F1                      20
#define DEFAULT_F2                      22000
#define DEFAULT_JACK_NAME               "%s_ir_sweep" // %s becomes argv [0]
#define DEFAULT_IR_SILENCE_THRESH_DB    -72.0
#define DEFAULT_SWEEP_SILENCE_THRESH_DB -60.0
#define DEFAULT_SWEEP_SEC               30
#define DEFAULT_SWEEP_AMPLITUDE_DB      -1
#define DEFAULT_SWEEP_OFFSET_SMP        64
#define DEFAULT_NORMALIZE_AMP           0.9
#define DEFAULT_PREROLL_SEC             1.0
#define DEFAULT_MARKER_SEC              1.0
#define DEFAULT_MARKGAP_SEC             1.0
#define DEFAULT_ZEROPEAK                true // try to zero-align peak?

//#define THRESH_RELATIVE // relative to peak, comment out for absolute
//#define DISABLE_LEADING_SILENCE_DETECTION // for debugging

enum deconv_mode {
  mode_deconvolve,
  mode_makesweep,
  mode_playsweep,
  mode_error // maybe extend this for diff. error types?
};

enum sig_source {
  src_none,
  src_file,
  src_jack,
  src_generate
};

enum sig_channels {
  chn_none,
  chn_mono,
  chn_stereo
};

enum align_method {
  align_marker,
  align_marker_dry,
  align_silence,
  align_none,
  align_method_max
};

struct s_prefs {
  deconv_mode mode = deconv_mode::mode_deconvolve;

  std::string dry_path; // file, JACK port, or empty (generate one)
  std::string wet_path; // file, or JACK port
  std::string out_path; // file. TODO: decide if we should support JACK output here
  // moved from our s_jackclient struct ...fugly :/
  std::string portname_dry = "";
  std::string portname_wetL = "";
  std::string portname_wetR = "";
  

  long ir_length_samples = 0;    // 0 = auto
#ifdef THRESH_RELATIVE
  bool thresh_relative = true;
#else
  bool thresh_relative = false;
#endif
  bool zeropeak = DEFAULT_ZEROPEAK;
  sig_source dry_source             = sig_source::src_file;
  sig_source wet_source             = sig_source::src_file;
  bool jack_autoconnect_dry         = false;
  bool jack_autoconnect_wet         = false;
  bool jack_autoconnect_to_default  = false;
  //bool using_jack                   = false;
  bool quiet                        = false;
#ifdef DEBUG
  bool verbose                      = true;
#else
  bool verbose                      = false;
#endif
  bool dump_debug                   = false;
  std::string dump_prefix           = "dirt-debug";
  bool sweepwait                    = false;
  align_method align                = align_none;

  double sweep_seconds      = DEFAULT_SWEEP_SEC;   // default value not really used
  double preroll_seconds    = DEFAULT_PREROLL_SEC;
  double marker_seconds     = DEFAULT_MARKER_SEC;  // alignment marker
  double marker_gap_seconds = DEFAULT_MARKGAP_SEC;
  //double regularization_db  = -120.0; // "noise floor" - see calc_ir_raw ()
  size_t sweep_offset_smp   = DEFAULT_SWEEP_OFFSET_SMP;
  int    sweep_sr           = DEFAULT_SAMPLE_RATE;
  bool   sweep_sr_given     = false; // used for warning user<->file sample rate mismatch
  float  sweep_f1           = DEFAULT_F1;
  float  sweep_f2           = DEFAULT_F2; // DONE: cap to just below nyquist freq
  float  sweep_amp_db       = DEFAULT_SWEEP_AMPLITUDE_DB;
  float  sweep_silence_db   = DEFAULT_SWEEP_SILENCE_THRESH_DB;
  float  ir_silence_db      = DEFAULT_IR_SILENCE_THRESH_DB;
  float  ir_start_silence_db   = DEFAULT_IR_SILENCE_THRESH_DB;
  float  headroom_seconds   = 0.0f;
  float  normalize_amp      = DEFAULT_NORMALIZE_AMP; // TODO: command opt line for this 
  
  std::string jack_name = DEFAULT_JACK_NAME;
  //std::string jack_portname;
  
  size_t cache_dry_marker_len = 0;
  size_t cache_dry_gap_len    = 0;
};

#ifdef USE_JACK
struct s_jackclient {
  bool play_go = false;
  bool rec_go  = false;
  
  /*std::string portname_dry = "";
  std::string portname_wetL = "";
  std::string portname_wetR = "";*/
  
  int samplerate = 0;
  jack_client_t *client = NULL;
  jack_port_t   *port_inL  = NULL;
  jack_port_t   *port_inR  = NULL;
  jack_port_t   *port_outL = NULL;
  //jack_port_t   *port_outR = NULL; // not used for now

  std::vector<float> sig_in_L; // mono/left wet capture (mix of L/R)
  std::vector<float> sig_in_R; //
  std::vector<float> sig_out;  // sweep to play

  size_t index     = 0;  // playback index
  size_t rec_index = 0;  // record index
  size_t rec_total = 0;  // how many samples to capture

  bool rec_done = false;
};
#endif

class c_deconvolver {
public:
  //c_deconvolver () = default;
  c_deconvolver (struct s_prefs *prefs = NULL);
  //~c_deconvolver ();
  
  void set_prefs (s_prefs *prefs);
  void set_name (std::string str);
  bool load_sweep_dry (const char *in_filename);
  bool load_sweep_wet (const char *in_filename);
  bool output_ir (const char *out_filename, long ir_length_samples = 0);
  int samplerate ();
  
  /*static bool calc_ir_raw (const std::vector<float> &wet,
                          const std::vector<float>  &dry,
                          std::vector<float>        &ir_raw,
                          long                      ir_length_samples = 0);*/
  bool calc_ir_raw (const std::vector<float> &wet,
                     const std::vector<float> &dry,
                     std::vector<float> &ir_raw,
                     long ir_length_samples);
                   
  void normalize_and_trim_stereo (std::vector<float> &L,
                                  std::vector<float> &R,
                                  bool zeropeak = DEFAULT_ZEROPEAK,
                                  float thr_start = DEFAULT_IR_SILENCE_THRESH_DB,
                                  float thr_end = DEFAULT_IR_SILENCE_THRESH_DB,
                                  float fade_end = 0.05); // last 5% of IR pre-trim

  bool set_dry_from_buffer (const std::vector<float>& buf, int sr);
  bool set_wet_from_buffer (const std::vector<float>& bufL,
                            const std::vector<float>& bufR,
                            int sr);
#ifdef USE_JACK
  bool jack_init     (const std::string clientname,
                      int samplerate, bool stereo);
  bool jack_shutdown ();
  bool jack_play     (std::vector<float> &buf);
  bool jack_playrec  (const std::vector<float> &buf,
                      std::vector<float> &in_l,
                      std::vector<float> &in_r);

  /*bool jack_playrec_sweep (const std::vector<float> &sweep,
                           int samplerate,
                           const char *jack_out_port,
                           const char *jack_in_port,
                           std::vector<float> &captured);*/
#endif
private:
  bool set_samplerate_if_needed (int sr);

#ifdef USE_JACK
  s_jackclient jackclient;
#endif
  int samplerate_ = 0;
  std::vector<float> dry_;
  std::vector<float> wet_L_;
  std::vector<float> wet_R_;
  bool have_dry_ = false;
  bool have_wet_ = false;
  bool jack_inited = false;
  size_t dry_offset_ = 0;
  size_t wet_offset_ = 0;
  s_prefs *prefs_ = NULL;
  s_prefs default_prefs_;
};

#endif // __DECONVOLVER_H
