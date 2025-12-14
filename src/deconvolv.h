
/* DIRT - Delt's Impulse Response Tool
 * Version 0.1
 *
 * Licensed under the GPL. See dirt.h and dirt.cpp for more info.
 */

#ifndef    __DIRT_DECONVOLV_H
#define    __DIRT_DECONVOLV_H
#define __IN_DIRT_DECONVOLV_H

#include "dirt.h"

struct s_prefs {
  opmode mode = opmode::DECONVOLVE; // this is mainly to guide CLI on what to do

  std::string dry_path; // file, JACK port, or empty (generate one)
  std::string wet_path; // file, or JACK port
  std::string out_path; // file. TODO: decide if we should support JACK output here
  // moved from our s_jackclient struct ...fugly :/
  std::string portname_dry = "";
  std::string portname_wetL = "";
  std::string portname_wetR = "";
  bool gui = false;

  long ir_length_samples            = 0;    // 0 = auto
  bool thresh_relative              = false;
  bool zeropeak = DEFAULT_ZEROPEAK;
  sig_source dry_source             = sig_source::FILE;
  sig_source wet_source             = sig_source::FILE;
  bool jack_autoconnect_dry         = false;
  bool jack_autoconnect_wet         = false;
  bool jack_autoconnect_to_default  = false;
  bool request_stereo               = true;
  //bool using_jack                   = false;
  bool quiet                        = false;
#ifdef DEBUG
  bool verbose                      = true;
  bool debug                        = true;
#else
  bool verbose                      = false;
  bool debug                        = false;
#endif
  bool dump_debug                   = false;
  std::string dump_prefix           = "dirt-debug";
  bool sweepwait                    = false;
  align_method align                = align_method::MARKER_DRY;

  double sweep_seconds         = DEFAULT_SWEEP_SEC;   // default value not really used
  double preroll_seconds       = DEFAULT_PREROLL_SEC;
  double marker_seconds        = DEFAULT_MARKER_SEC;  // alignment marker
  double marker_gap_seconds    = DEFAULT_MARKGAP_SEC;
  //double regularization_db     = -120.0; // "noise floor" - see calc_ir_raw ()
  int64_t sweep_offset_smp     = DEFAULT_SWEEP_OFFSET_SMP;
  int    sweep_sr              = DEFAULT_SAMPLERATE;
  bool   sweep_sr_given        = false; // used for warning user<->file sample rate mismatch
  float  sweep_f1              = DEFAULT_F1;
  float  sweep_f2              = DEFAULT_F2; // DONE: cap to just below nyquist freq
  float  sweep_amp_db          = DEFAULT_SWEEP_AMPLITUDE_DB;
  float  sweep_silence_db      = DEFAULT_SWEEP_SILENCE_THRESH_DB;
  float  ir_silence_db         = DEFAULT_IR_SILENCE_THRESH_DB;
  float  ir_start_silence_db   = DEFAULT_IR_SILENCE_THRESH_DB;
  float  headroom_seconds      = 0.0f;
  float  normalize_amp         = DEFAULT_NORMALIZE_AMP; // TODO: command line opt for this 
#ifdef HIGHPASS_F
  int    hpf                   = HIGHPASS_F;
#endif
#ifdef LOWPASS_F
  int    lpf                   = LOWPASS_F;
#endif
  
  std::string jack_name = DEFAULT_JACK_NAME;
  
  size_t cache_dry_marker_len = 0;
  size_t cache_dry_gap_len    = 0;
};

class c_mainwindow;

class c_deconvolver {
public:
  c_deconvolver (struct s_prefs *prefs);
  ~c_deconvolver ();
  
  void set_prefs (s_prefs *prefs);
  bool set_samplerate (int sr);
  int get_samplerate ();
  void clear (bool clear_dry = true, bool clear_wet = true);
  void clear_dry ();
  void clear_wet ();
  bool set_sweep_dry (c_wavebuffer &v);
  bool set_sweep_wet (c_wavebuffer &wl, c_wavebuffer &wr);
  bool import_file (const char *in_filename,   c_wavebuffer &in_l,
                                               c_wavebuffer &in_r);
  bool export_file (const char *out_filename,  c_wavebuffer &out_l,
                                               c_wavebuffer &out_r);
  //void align_sweeps ();
  bool render_ir (c_wavebuffer &out_l, c_wavebuffer &out_r,
                  size_t max_size = -1);
  bool has_dry ();
  bool has_wet_l ();
  bool has_wet_r ();
  
  s_prefs *prefs = NULL;
  
  void normalize_and_trim_stereo (std::vector<float> &L,
                                  std::vector<float> &R,
                                  bool zeropeak = DEFAULT_ZEROPEAK,
                                  float thr_start = DEFAULT_IR_SILENCE_THRESH_DB,
                                  float thr_end = DEFAULT_IR_SILENCE_THRESH_DB,
                                  float fade_end = 0.05); // %, pre_trim
  
  bool calc_ir_raw (const std::vector<float> &wet,
                    const std::vector<float> &dry,
                    std::vector<float> &out,
                    long int ir_max_samples);
  
//protected:
  
  uint64_t playrec_loop_passes = 0;
  int vu_meter_size = ANSI_VU_METER_MIN_SIZE;
  int samplerate_ = 0;
  std::vector<float> sweep_dry;
  std::vector<float> sweep_wet_l;
  std::vector<float> sweep_wet_r;
  //bool have_dry_ = false;
  //bool have_wet_ = false;
  size_t dry_offset = 0;
  size_t wet_offset = 0;
  s_prefs default_prefs;
  std::string audio_clientname;  
};


bool write_mono_wav (const char *path,
                      const std::vector<float> &data,
                      int samplerate);
                            
bool write_stereo_wav (const char *path,
                        const std::vector<float> &L,
                        const std::vector<float> &R,
                        int samplerate);
                        
bool read_wav (const char *filename,
                std::vector<float> &left,
                std::vector<float> &right,
                int &sr);
                
bool read_wav (const char *filename,
               c_wavebuffer &left,
               c_wavebuffer &right);

#undef  __IN_DIRT_DECONVOLV_H
#endif  // __DIRT_DECONVOLV_H
