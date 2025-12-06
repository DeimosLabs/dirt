
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
  opmode mode = opmode::DECONVOLVE;

  std::string dry_path; // file, JACK port, or empty (generate one)
  std::string wet_path; // file, or JACK port
  std::string out_path; // file. TODO: decide if we should support JACK output here
  // moved from our s_jackclient struct ...fugly :/
  std::string portname_dry = "";
  std::string portname_wetL = "";
  std::string portname_wetR = "";
  bool gui = false;

  long ir_length_samples = 0;    // 0 = auto
#ifdef THRESH_RELATIVE
  bool thresh_relative = true;
#else
  bool thresh_relative = false;
#endif
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
#else
  bool verbose                      = false;
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
  int    sweep_sr              = DEFAULT_SAMPLE_RATE;
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
friend c_audioclient;
#ifdef USE_WXWIDGETS
friend c_mainwindow;
#endif
public:
  //c_deconvolver () = default;
  c_deconvolver (struct s_prefs *prefs = NULL, std::string name = "");
  virtual ~c_deconvolver ();
  
  void set_prefs (s_prefs *prefs);
  void set_name (std::string str);
  bool set_stereo (bool b);
  bool load_sweep_dry (const char *in_filename);
  bool load_sweep_wet (const char *in_filename);
  bool output_ir (const char *out_filename, long int ir_length_samples = 0);
  int samplerate ();
  bool has_dry ();
  bool has_wet ();
  
  // passthrough functions for c_audioclient & derived
  // these just call hooks and corresponding c_audioclient functions 
  virtual bool audio_init (std::string clientname = "",
                     int samplerate = -1,
                     bool stereo_out = true);
  virtual bool audio_shutdown ();
  virtual bool audio_ready ();
  virtual bool audio_playback_done ();
  virtual audiostate get_audio_state ();
  virtual bool audio_play (const std::vector<float> &out);
  virtual bool audio_play (const std::vector<float> &out_l,
                     const std::vector<float> &out_r);
  virtual bool audio_arm_record ();
  virtual bool audio_rec ();
  virtual bool audio_playrec (const std::vector<float> &out_l,
                              const std::vector<float> &out_r);
  virtual bool audio_stop ();
  virtual bool audio_stop_playback ();
  virtual bool audio_stop_record ();
  
  // UI hooks for derived classes, audio backends should call these
  // override them for UI other than textmode
  virtual int on_audio_idle () { return 0; }
  virtual int on_play_start (void *data = NULL);
  virtual int on_play_loop (void *data = NULL);
  virtual int on_play_stop (void *data = NULL);
  virtual int on_arm_rec_start (void *data = NULL);
  virtual int on_arm_rec_loop (void *data = NULL);
  virtual int on_arm_rec_stop (void *data = NULL);
  virtual int on_record_start (void *data = NULL);
  virtual int on_record_loop (void *data = NULL);
  virtual int on_record_stop (void *data = NULL);
  virtual int on_playrec_start (void *data = NULL);
  virtual int on_playrec_loop (void *data = NULL);
  virtual int on_playrec_stop (void *data = NULL);
  
  virtual void set_vu_pre ();
  virtual void set_vu_l (float level, float hold, bool clip, bool xrun);
  virtual void set_vu_r (float level, float hold, bool clip, bool xrun);
  virtual void set_vu_post ();
  
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
  /*bool jack_init     (const std::string clientname,
                      int samplerate, bool stereo);
  bool jack_shutdown ();
  bool jack_play     (std::vector<float> &buf);
  bool jack_playrec  (const std::vector<float> &buf,
                      std::vector<float> &in_l,
                      std::vector<float> &in_r);*/

  /*bool jack_playrec_sweep (const std::vector<float> &sweep,
                           int samplerate,
                           const char *jack_out_port,
                           const char *jack_in_port,
                           std::vector<float> &captured);*/
#endif
  bool set_samplerate_if_needed (int sr);
  void update_peak_data ();

  c_audioclient *audio = NULL;
  
//protected:
  
  uint64_t playrec_loop_passes = 0;
  int vu_meter_size = ANSI_VU_METER_MIN_SIZE;
  int samplerate_ = 0;
  std::vector<float> dry_;
  std::vector<float> wet_L_;
  std::vector<float> wet_R_;
  bool have_dry_ = false;
  bool have_wet_ = false;
  size_t dry_offset_ = 0;
  size_t wet_offset_ = 0;
  s_prefs *prefs_ = NULL;
  s_prefs default_prefs_;
  std::string audio_clientname;
};

bool write_mono_wav (const char *path,
                      const std::vector<float> &data,
                      int samplerate);
                            
bool write_stereo_wav (const char *path,
                        const std::vector<float> &L,
                        const std::vector<float> &R,
                        int samplerate);

#undef  __IN_DIRT_DECONVOLV_H
#endif  // __DIRT_DECONVOLV_H
