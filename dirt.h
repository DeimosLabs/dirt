
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
 * compile:
 * g++ -O3 -std=c++17 dirt.cpp -o dirt -lsndfile -lfftw3 -ljack
 * (or just use the included build.sh script)
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
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <unistd.h>
#include <sys/time.h>
#include <sys/stat.h>

#include <sndfile.h>
#include <fftw3.h>
#include <jack/jack.h>

#define MARKER_FREQ                    1000 // in Hz
#define DEFAULT_SAMPLE_RATE            48000
#define DEFAULT_JACK_NAME              "%s_ir_sweep" // %s becomes argv [0]
#define DEFAULT_SILENCE_THRESH_DB      -80.0
#define DEFAULT_SWEEP_SEC              30
#define DEFAULT_SWEEP_AMPLITUDE_DB     -1
#define DEFAULT_MARKER_SEC             0.1
#define DEFAULT_PREROLL_SEC            1.0
#define DEFAULT_MARKGAP_SEC            1.0

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

struct s_prefs {
  deconv_mode mode = deconv_mode::mode_deconvolve;

  std::string dry_path; // file, JACK port, or empty (generate one)
  std::string wet_path; // file, or JACK port
  std::string out_path; // file  TODO: decide if we should support JACK output here

  long ir_length_samples = 0;    // 0 = auto
#ifdef THRESH_RELATIVE
  bool thresh_relative = true;
#else
  bool thresh_relative = false;
#endif
  sig_source dry_source = sig_source::src_file;
  sig_source wet_source = sig_source::src_file;
  bool quiet = false;
  bool verbose = false;
  bool sweepwait = false;

  double sweep_seconds      = DEFAULT_SWEEP_SEC;   // default not really used
  double preroll_seconds    = DEFAULT_PREROLL_SEC;
  double marker_seconds     = DEFAULT_MARKER_SEC;  // alignment marker
  double marker_gap_seconds = DEFAULT_MARKGAP_SEC;
  int    sweep_sr           = DEFAULT_SAMPLE_RATE;
  float  sweep_f1           = 20.0f;
  float  sweep_f2           = 20000.0f;
  float  sweep_amp_db       = DEFAULT_SWEEP_AMPLITUDE_DB;
  float  headroom_seconds   = 0.0f;
 
  float silence_thresh  = -1;
  std::string jack_name = DEFAULT_JACK_NAME;
  std::string jack_portname;
  
  size_t cache_dry_marker_len = 0;
  size_t cache_dry_gap_len    = 0;
  bool early_jack_init        = false;
};

struct s_jackclient {
  bool play_go = false;
  bool rec_go  = false;

  jack_client_t *client = NULL;
  jack_port_t   *inL  = NULL;
  jack_port_t   *inR  = NULL;
  jack_port_t   *outL = NULL;
  jack_port_t   *outR = NULL;

  std::vector<float> sig_in;   // mono wet capture (mix of L/R)
  std::vector<float> sig_out;  // sweep to play

  size_t index     = 0;  // playback index
  size_t rec_index = 0;  // record index
  size_t rec_total = 0;  // how many samples to capture

  bool rec_done = false;
};

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
                                   bool trim_end = true,
                                   bool trim_start = false);

  bool set_dry_from_buffer(const std::vector<float>& buf, int sr);
  bool set_wet_from_buffer(const std::vector<float>& bufL,
                           const std::vector<float>& bufR,
                           int sr);

  bool init_jack (const std::string clientname,
                  sig_channels chan_in = sig_channels::chn_stereo,
                  sig_channels chan_out = sig_channels::chn_stereo);
  bool jack_play (std::vector<float> &buf, int samplerate, const char *portname);
  bool jack_rec  (std::vector<float> &buf, int samplerate, const char *portname);
  
  bool jack_playrec_sweep(const std::vector<float> &sweep,
                          int samplerate,
                          const char *jack_out_port,
                          const char *jack_in_port,
                          std::vector<float> &captured);
private:
  bool set_samplerate_if_needed (int sr);
  
  s_jackclient jackclient;
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
