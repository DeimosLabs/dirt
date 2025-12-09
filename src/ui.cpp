
/* DIRT - Delt's Impulse Response Tool
 * Version 0.1
 *
 * Licensed under the GPL. See dirt.h and dirt.cpp for more info.
 */


#ifdef USE_WXWIDGETS

#include <wx/dir.h>
#include <wx/rawbmp.h>

#include "dirt.h"
#include "ui.h"
#include "deconvolv.h"
#ifdef USE_JACK
#include "jack.h"
#endif
//#include "timestamp.h" causes full recompile at each build

#ifdef DEBUG
#define CMDLINE_DEBUG
#include "cmdline/cmdline.h"
#define debug(...) cmdline_debug(stderr,ANSI_YELLOW,__FILE__,__LINE__,__FUNC__,__VA_ARGS__)
#else
#define debug(...)
#define CP
#define BP
#endif

extern char *g_dirt_version;
extern char *g_dirt_build_timestamp;

static std::string g_backend_names [] = {
  "none",
  "JACK",
  "unknown"
};


////////////////////////////////////////////////////////////////////////////////
// c_mainwindow

wxBEGIN_EVENT_TABLE (c_mainwindow, ui_mainwindow)
  //EVT_CLOSE (c_mainwindow::on_close)
  //EVT_SIZE (c_mainwindow::on_resize)
  EVT_BUTTON (wxID_ABOUT, c_mainwindow::on_about)
  EVT_BUTTON (wxID_EXIT, c_mainwindow::on_quit)
  EVT_RADIOBUTTON (ID_FILE, c_mainwindow::on_radio_file)
  EVT_RADIOBUTTON (ID_MAKESWEEP, c_mainwindow::on_radio_makesweep)
  EVT_RADIOBUTTON (ID_ROUNDTRIP, c_mainwindow::on_radio_roundtrip)
  EVT_RADIOBUTTON (ID_PLAYSWEEP, c_mainwindow::on_radio_playsweep)
  EVT_CHECKBOX (ID_FORCEMONO, c_mainwindow::on_chk_forcemono)

  EVT_BUTTON (ID_DRYFILE_BROWSE, c_mainwindow::on_btn_dryfile_browse)
  EVT_BUTTON (ID_GENERATE, c_mainwindow::on_btn_generate)
  EVT_BUTTON (ID_DRY_SAVE, c_mainwindow::on_btn_dry_save)
  EVT_BUTTON (ID_INPUTDIR_SCAN, c_mainwindow::on_btn_inputdir_scan)
  EVT_BUTTON (ID_INPUTDIR_BROWSE, c_mainwindow::on_btn_inputdir_browse)
  EVT_BUTTON (ID_OUTPUTDIR_BROWSE, c_mainwindow::on_btn_outputdir_browse)
  EVT_BUTTON (ID_INPUTFILES_ADD, c_mainwindow::on_btn_inputfiles_add)
  EVT_BUTTON (ID_INPUTFILES_CLEAR, c_mainwindow::on_btn_inputfiles_clear)
  EVT_BUTTON (ID_ALIGN_MANUAL, c_mainwindow::on_btn_align_manual)
  EVT_BUTTON (ID_AUDIO, c_mainwindow::on_btn_audio)
  EVT_BUTTON (ID_PLAY, c_mainwindow::on_btn_play)
  EVT_BUTTON (ID_PROCESS, c_mainwindow::on_btn_process)
  
  EVT_COMBOBOX (ID_JACK_DRY, c_mainwindow::on_port_select)
  EVT_COMBOBOX (ID_JACK_WET_L, c_mainwindow::on_port_select)
  EVT_COMBOBOX (ID_JACK_WET_R, c_mainwindow::on_port_select)

wxEND_EVENT_TABLE ();


c_mainwindow::c_mainwindow (c_deconvolver *d)
: ui_mainwindow (NULL, wxID_ANY) {
  dec = d;
  if (!dec) {
    debug ("No deconvolver given!");
  }
  
  ir_entry ir;
  ir.path = "abcd";
  ir.samplerate = 48000;
  ir.dirty = true;
  add_ir (ir);
  
  comb_samplerate->Append ("44100");
  comb_samplerate->Append ("48000");
  comb_samplerate->Append ("96000");
  comb_samplerate->Append ("192000");
  
  list_backend->Append ("JACK");  
  list_backend->SetSelection (0);
  list_align->Append ("Marker detection");
  list_align->Append ("Marker det. on dry");
  list_align->Append ("Silence detection");
  list_align->Append ("Manual");
  list_align->Append ("None/already aligned");
  
  if (d && d->prefs_)
    set_prefs (d->prefs_);
  CP
  
  Layout ();
  Fit ();
  wxSize sz = GetClientSize ();
  SetSize (sz + wxSize (0, 100));
  SetSizeHints (sz);
  
  set_mode (ID_FILE);
  
  init_audio ();
  timer.Bind (wxEVT_TIMER, &c_mainwindow::on_timer, this);
  timer.Start (32);
}

c_mainwindow::~c_mainwindow () {
  CP
}

bool c_mainwindow::add_ir (ir_entry &ent) { CP
  debug ("got IR entry '%s' size l=%d,r=%d, %dHz, %s", 
         ent.path.c_str (), ent.l.size (), ent.r.size (), ent.samplerate,
         ent.dirty ? "dirty" : "not dirty");
  if (1||ent.path.size () > 0 && ent.l.size () > 0) {
    //ir_entry newir;
    ir_files.push_back (ent);
    //int i = ir_files.size () - 1;
    //ir_files [i].path = ent.path;
    //ir_files [i].l = ent.l;
    //ir_files [i].r = ent.r;
    return true;
  }
  debug ("empty IR, ignoring");
  
  return false;
}

void c_mainwindow::on_timer (wxTimerEvent &ev) {
  static long int num_passes = 0;
  const int update_widgets_every = 4;
  bool do_full_update = false;
  
  num_passes++;
  if (!dec || !dec->audio) return;
  
  // animation stuff etc. that we do every pass
  // nothing yet, our custom widgets update themselves on redraw events
  //pn_meter->Refresh (); // not necessary
  
  // widget stuff: update only every n passes
  if (num_passes % update_widgets_every == 0) {
    audiostate s = dec->audio->state;
    
    if (s != prev_audio_state) {
      debug ("prev_audio_state=%d, s=%d", (int) prev_audio_state, (int) s);
      prev_audio_state = s;
      do_full_update = true;
    } //else {CP}
    if (mode != prev_mode) {
      prev_mode = mode;
      do_full_update = true;
    } //else {CP}
    
    if (s == audiostate::NOTREADY) {    // audio offline
      disable (btn_play);
      btn_audio->SetLabel ("Connect");
      init_audio_done = false;
    } else if (s == audiostate::IDLE || 
               s == audiostate::MONITOR) { // on, not playing
      if (!dec->has_dry ()) {           // ready to play
        disable (btn_play);
      } else {
        //debug ("state %d", dec->audio->state);
        enable (btn_play);
      }
      btn_play->SetLabel ((mode == ID_ROUNDTRIP) ? "Record" : "Play");
      btn_audio->SetLabel ("Disconnect");
    } else if (s == audiostate::PLAY ||
               s == audiostate::PLAYMONITOR ||
               s == audiostate::PLAYREC) {   // something's playing/recording
      if (!dec->has_dry ()) { CP             // uhhh wut
        debug ("audio: playing something but what?");
        disable (btn_play);
      } else {                               // playing dry sweep
        enable (btn_play);
        int sr = dec->prefs_ ? dec->prefs_->sweep_sr : 
                               atoi (comb_samplerate->GetValue ().c_str ());
        size_t sec_left = dec->audio->get_play_remaining () / (size_t) sr;
        char buf [32];
        snprintf (buf, 31, "Stop (%ld)", sec_left);
        //debug ("sr=%d, sec_left=%d", sr, sec_left);
        btn_play->SetLabel (buf);
      }
      btn_audio->SetLabel ("Disconnect");
    } else {
      debug ("audio: no idea what's going on");
    }
    
    wxArrayInt dummy;
    if (list_inputfiles->GetSelections (dummy) > 0)
      btn_inputfiles_clear->SetLabel ("Remove");
    else
      btn_inputfiles_clear->SetLabel ("Clear");
    
    if (list_inputfiles->GetCount ())
      enable (btn_inputfiles_clear);
    else
      disable (btn_inputfiles_clear);
    
    if (text_inputdir->GetValue ().size ())
      enable (btn_inputdir_scan);
    else
      disable (btn_inputdir_scan);
    
    if (list_inputfiles->GetCount () > 0 && dec->has_dry ())
      enable (btn_process);
    else
      {}//disable (btn_process); TEMP!!!!
    
    if (chk_autosave->GetValue ()) {
      enable (chk_overwrite);
    } else {
      disable (chk_overwrite);
    }
  
    int n = ir_files.size ();
    if (n != last_ir_count) {
      debug ("updating ir list (have %d, last count %d)", n, last_ir_count);
      last_ir_count = ir_files.size ();
      CP
      update_ir_list ();
    } else {
      //debug ("ir list up to date, (%d vs %d)", n, last_ir_count);
    }
  }
  if (do_full_update)
    set_mode (mode);
}

bool c_mainwindow::audio_ready () {
  init_audio_done = false;
  
  if (!dec) {
    debug ("no deconvolver");
    return false;
  }
  if (!dec->audio) {
    debug ("deconvolver has no audio client");
    return false;
  }
  if (dec->audio->state == audiostate::IDLE) {
    debug ("deconvolver idle, arming record (monitor mode)");
    dec->audio->arm_record ();
  }
  if (dec->audio->state == audiostate::NOTREADY ||
      dec->audio->state == audiostate::IDLE) {
    debug ("deconvolver audio not ready");
    return false;
  }
  
  CP
  init_audio_done = true;
  return true;
}

bool c_mainwindow::init_audio (int samplerate, bool stereo) { CP
  if (init_audio_done)
    return true;
  debug ("start, samplerate=%d, stereo=%s", samplerate, stereo ? "true" : "false");
  
  if (!dec) {
    debug ("no deconvolver given");
    return false;
  }
  
  if (!dec->audio) {
    dec->audio_init ("DIRT", -1, stereo);
  }
  
  if (!dec->audio) {
    debug ("deconvolver has no audio client");
    return false;
  }
  
  int sr = ::atoi (comb_samplerate->GetValue ().c_str ());
  bool st = !chk_forcemono->GetValue ();
  
  std::string jack_name = "";
  if (dec->prefs_) jack_name = dec->prefs_->jack_name;
  std::cout << "name: " << jack_name << "\n";
  CP
  dec->audio->unregister ();
  if (!dec->audio->register_output (false) ||
      !dec->audio->register_input (dec->prefs_->request_stereo)) {
    CP
    debug ("audio init failed");
    return false;
  }
  
  CP
  dec->audio_arm_record ();
  init_audio_done = true;
  
  debug ("end");
  return true;
}

bool c_mainwindow::init_audio () {
  return init_audio (atoi (comb_samplerate->GetValue ().c_str ()), 
                      !chk_forcemono->GetValue ());
}

bool c_mainwindow::disable_audio () { CP
  if (!dec || !dec->audio) return false;
  if (dec->audio->state == audiostate::NOTREADY) return false;
  
  list_jack_dry->Clear ();
  list_jack_wet_l->Clear ();
  list_jack_wet_r->Clear ();
  init_audio_done = false;
  
  return dec->audio->unregister ();
}

void c_mainwindow::on_quit (wxCommandEvent &ev) {
  CP
  Close ();
}

void c_mainwindow::on_resize (wxSizeEvent &ev) {
  CP
}

void c_mainwindow::on_close (wxCloseEvent &ev) {
  CP
  Close ();
}

void c_mainwindow::on_about (wxCommandEvent &ev) {
  CP
  std::string str = "\nDelt's Impulse Response Tool\n\nVersion " + 
                    std::string (g_dirt_version) + "\nBuild timestamp: " + 
                    std::string (g_dirt_build_timestamp) + "\n";
  show_message (str);
}

void c_mainwindow::on_chk_forcemono (wxCommandEvent &ev) { CP
  if (radio_file->GetValue ()) set_mode (ID_FILE);
  else if (radio_makesweep->GetValue ()) set_mode (ID_MAKESWEEP);
  else if (radio_roundtrip->GetValue ()) set_mode (ID_ROUNDTRIP);
  
  if (!audio_ready ()) // don't re-enable audio on checkbox
    return;
  
  CP
  /*if (!init_audio_done) {
    CP 
    init_audio ();
  }*/
  
  bool a = false;
  bool b = chk_forcemono->GetValue ();
  
  if (!dec->audio->is_stereo == !b)
    a = true;
  
  debug ("a=%d, b=%d", (int) a, (int) b);
  if (b) {
    debug ("force mono");
    dec->set_stereo (false);
  } else {
    debug ("stereo");
    dec->set_stereo (true);
  }
  
  if (a) { 
    debug ("RE-ARMING RECORD"); 
    dec->audio_arm_record ();
  }
}

void c_mainwindow::on_radio_file (wxCommandEvent &ev) { set_mode (ID_FILE); }
void c_mainwindow::on_radio_makesweep (wxCommandEvent &ev) { set_mode (ID_MAKESWEEP); }
void c_mainwindow::on_radio_roundtrip (wxCommandEvent &ev) { set_mode (ID_ROUNDTRIP); }
void c_mainwindow::on_radio_playsweep (wxCommandEvent &ev) { set_mode (ID_PLAYSWEEP); }

struct widget_status {
  long int id = -1;
  bool is_on = true;
};

void c_mainwindow::on_port_select (wxCommandEvent &ev) {
#ifdef USE_JACK
  if (!audio_ready () || dec->audio->backend != audio_driver::JACK) {
    debug ("no JACK client");
    return;
  }
  
  // kludge, but meh who cares
  c_jackclient *jc = (c_jackclient *) dec->audio;
  
  switch (ev.GetId ()) {
    case ID_JACK_DRY: CP
      if (!jc->port_outL) return;
      jc->disconnect_all (jc->port_outL);
      jc->connect (jc->port_outL, std::string (list_jack_dry->GetValue ().c_str ()));
    break;
    
    case ID_JACK_WET_L: CP
      if (!jc->port_inL) return;
      jc->disconnect_all (jc->port_inL);
      jc->connect (jc->port_inL,std::string (list_jack_wet_l->GetValue ().c_str ()));
    break;
    
    case ID_JACK_WET_R: CP
      if (!jc->port_inR) return;
      jc->disconnect_all (jc->port_inR);
      jc->connect (jc->port_inR,std::string (list_jack_wet_r->GetValue ().c_str ()));
    break;
  }
#else
  debug ("compiled without JACK support");
#endif
}

void c_mainwindow::set_prefs (s_prefs *prefs) {
  spin_dry_length->SetValue (prefs->sweep_seconds);
  char buf [32];
  snprintf (buf, 32, "%d", (int) prefs->sweep_sr);
  comb_samplerate->SetValue (std::string (buf));
  spin_dry_f1->SetValue (prefs->sweep_f1);
  spin_dry_f2->SetValue (prefs->sweep_f2);
  spin_dry_preroll->SetValue (prefs->preroll_seconds * 1000.0);
  spin_dry_marker->SetValue (prefs->marker_seconds * 1000.0);
  spin_dry_gap->SetValue (prefs->marker_gap_seconds * 1000.0);
  list_align->SetSelection ((int) prefs->align);
#ifdef HIGHPASS_F
  spin_hpf_freq->SetValue (prefs->hpf);
#endif
#ifdef LOWPASS_F
  spin_lpf_freq->SetValue (prefs->lpf);
#endif
  chk_zeroalign->SetValue (prefs->zeropeak);
  chk_forcemono->SetValue (!prefs->request_stereo);
  chk_debug->SetValue (prefs->dump_debug);
  spin_sweep_thr->SetValue (prefs->sweep_silence_db);
  spin_ir_start_thr->SetValue (prefs->ir_start_silence_db);
  spin_ir_end_thr->SetValue (prefs->ir_silence_db);
}

void c_mainwindow::get_prefs (s_prefs *prefs) {
  prefs->sweep_seconds = spin_dry_length->GetValue ();
  int sr = atoi (comb_samplerate->GetValue ().c_str ());
  if (sr > SAMPLERATE_MIN && sr < SAMPLERATE_MAX)
    prefs->sweep_sr = sr;
  prefs->sweep_f1 = spin_dry_f1->GetValue ();
  prefs->sweep_f2 = spin_dry_f2->GetValue ();
  prefs->preroll_seconds = (float) spin_dry_preroll->GetValue () / 1000.0;
  prefs->marker_seconds = (float) spin_dry_marker->GetValue () / 1000.0;
  prefs->marker_gap_seconds = (float) spin_dry_gap->GetValue () / 1000.0;
  prefs->align = (align_method) list_align->GetSelection ();
#ifdef HIGHPASS_F
  prefs->hpf = spin_hpf_freq->GetValue ();
#endif
#ifdef LOWPASS_F
  prefs->lpf = spin_lpf_freq->GetValue ();
#endif
  prefs->zeropeak = chk_zeroalign->GetValue ();
  prefs->request_stereo = !chk_forcemono->GetValue ();
  prefs->dump_debug = chk_debug->GetValue ();
  prefs->sweep_silence_db = spin_sweep_thr->GetValue ();
  prefs->ir_start_silence_db = spin_ir_start_thr->GetValue ();
  prefs->ir_silence_db = spin_ir_end_thr->GetValue ();
}

void c_mainwindow::set_mode (long int _mode) {
  struct widget_status *wl;
  mode = _mode;
  
  if (!dec) {
    debug ("no deconvolver object");
  }
  dec->prefs_->mode = opmode::GUI;
  
  static struct widget_status widget_status_file [] = {
    // dry sweep tab
    { ID_BACKEND,              true  },
    { ID_FORCEMONO,            true  },
    { ID_SAMPLERATE,           true  },
    { ID_DRYFILE,              true  },
    { ID_DRYFILE_BROWSE,       true  },
    { ID_DRY_LENGTH,           false },
    { ID_DRY_F1,               false },
    { ID_DRY_F2,               false },
    { ID_DRY_PREROLL,          false },
    { ID_DRY_MARKER,           false },
    { ID_DRY_GAP,              false },
    { ID_GENERATE,             false },
    { ID_DRY_SAVE,             false },
    { ID_JACK_DRY,             true },
    { ID_JACK_WET_L,           false },
    { ID_JACK_WET_R,           false },
    { ID_PLAY,                 true  },
    // deconvolver tab
    { ID_INPUTDIR,             true  },
    { ID_INPUTDIR_RECURSIVE,   true  },
    { ID_INPUTDIR_SCAN,        true  },
    { ID_INPUTDIR_BROWSE,      true  },
    { ID_INPUTFILES,           true  },
    { ID_INPUTFILES_CLEAR,     true  },
    { ID_INPUTFILES_ADD,       true  },
    { ID_OUTPUTDIR,            true  },
    { ID_OUTPUTDIR_BROWSE,     true  },
    { ID_SWEEP_ALIGN,          true  },
    { ID_HPF_MODE,             true  },
    { ID_LPF_MODE,             true  },
    { ID_HPF_FREQ,             true  },
    { ID_LPF_FREQ,             true  },
    { ID_ZEROALIGN,            true  },
    { ID_TRIM_START,           true  },
    { ID_TRIM_END,             true  },
    { ID_AUTOSAVE,             true  },
    { ID_OVERWRITE,            true  },
    { ID_DEBUG,                true  },
    { ID_SWEEP_THR,            true  },
    { ID_IR_START_THR,         true  },
    { ID_IR_END_THR,           true  },
    { ID_CHN_OFFSET,           true  },
    { ID_PROCESS,              true  },
    { -1,                      false }
  },
  
  widget_status_makesweep [] = {
    // dry sweep tab
    { ID_BACKEND,              true  },
    { ID_FORCEMONO,            true  },
    { ID_SAMPLERATE,           true  },
    { ID_DRYFILE,              false },
    { ID_DRYFILE_BROWSE,       false },
    { ID_DRY_LENGTH,           true  },
    { ID_DRY_F1,               true  },
    { ID_DRY_F2,               true  },
    { ID_DRY_PREROLL,          true  },
    { ID_DRY_MARKER,           true  },
    { ID_DRY_GAP,              true  },
    { ID_GENERATE,             true  },
    { ID_DRY_SAVE,             true  },
    { ID_JACK_DRY,             true  },
    { ID_JACK_WET_L,           false },
    { ID_JACK_WET_R,           false },
    { ID_PLAY,                 true  },
    // deconvolver tab
    { ID_INPUTDIR,             true  },
    { ID_INPUTDIR_RECURSIVE,   true  },
    { ID_INPUTDIR_SCAN,        true  },
    { ID_INPUTDIR_BROWSE,      true  },
    { ID_INPUTFILES,           true  },
    { ID_INPUTFILES_CLEAR,     true  },
    { ID_INPUTFILES_ADD,       true  },
    { ID_OUTPUTDIR,            true  },
    { ID_OUTPUTDIR_BROWSE,     true  },
    { ID_SWEEP_ALIGN,          true  },
    { ID_HPF_MODE,             true  },
    { ID_LPF_MODE,             true  },
    { ID_HPF_FREQ,             true  },
    { ID_LPF_FREQ,             true  },
    { ID_ZEROALIGN,            true  },
    { ID_TRIM_START,           true  },
    { ID_TRIM_END,             true  },
    { ID_AUTOSAVE,             true  },
    { ID_OVERWRITE,            true  },
    { ID_DEBUG,                true  },
    { ID_SWEEP_THR,            true  },
    { ID_IR_START_THR,         true  },
    { ID_IR_END_THR,           true  },
    { ID_CHN_OFFSET,           true  },
    { ID_PROCESS,              true  },
    { -1,                      false }
  },
  
  widget_status_roundtrip [] = {
    // dry sweep tab
    { ID_BACKEND,              true  },
    { ID_FORCEMONO,            true  },
    { ID_SAMPLERATE,           true  }, // TODO: might change with other backends
    { ID_DRYFILE,              false },
    { ID_DRYFILE_BROWSE,       false },
    { ID_DRY_LENGTH,           false },
    { ID_DRY_F1,               false },
    { ID_DRY_F2,               false },
    { ID_DRY_PREROLL,          false },
    { ID_DRY_MARKER,           false },
    { ID_DRY_GAP,              false },
    { ID_GENERATE,             false },
    { ID_DRY_SAVE,             false },
    { ID_JACK_DRY,             true  },
    { ID_JACK_WET_L,           true  },
    { ID_JACK_WET_R,           true  },
    { ID_PLAY,                 true  },
    // deconvolver tab
    { ID_INPUTDIR,             false },
    { ID_INPUTDIR_RECURSIVE,   false },
    { ID_INPUTDIR_SCAN,        false },
    { ID_INPUTDIR_BROWSE,      false },
    { ID_INPUTFILES,           false },
    { ID_INPUTFILES_CLEAR,     false },
    { ID_INPUTFILES_ADD,       false },
    { ID_OUTPUTDIR,            false },
    { ID_OUTPUTDIR_BROWSE,     false },
    { ID_SWEEP_ALIGN,          false },
    { ID_HPF_MODE,             false },
    { ID_LPF_MODE,             false },
    { ID_HPF_FREQ,             false },
    { ID_LPF_FREQ,             false },
    { ID_ZEROALIGN,            false },
    { ID_TRIM_START,           false },
    { ID_TRIM_END,             false },
    { ID_AUTOSAVE,             false },
    { ID_OVERWRITE,            false },
    { ID_DEBUG,                false },
    { ID_SWEEP_THR,            false },
    { ID_IR_START_THR,         false },
    { ID_IR_END_THR,           false },
    { ID_CHN_OFFSET,           false },
    { ID_PROCESS,              false },
    { -1,                      false }
  };
  
#ifdef USE_JACK
  bool do_audio = true;
#else
  bool do_audio = false;
#endif
  bool fm = chk_forcemono->GetValue ();
  if (last_forcemono != fm) {
    last_forcemono = fm;
  }
  
  //bool b = chk_forcemono->GetValue();
  //set_enable (list_jack_wet_r, !b);
  //set_enable (spin_chn_offset, !b);
  
  /*if (mode == ID_ROUNDTRIP) btn_play->SetLabel ("Process");
  else                      btn_play->SetLabel ("Play");*/
  
  switch (mode) {
    case ID_FILE:
      wl = widget_status_file;
    break;
    
    case ID_MAKESWEEP:
      wl = widget_status_makesweep;
      do_audio = true;
    break;
    
    case ID_ROUNDTRIP:
      wl = widget_status_roundtrip;
      do_audio = true;
    break;
    
    //case ID_PLAYSWEEP:  wl = widget_status_playsweep;  CP; break;
    default:
      std::cout << "invalid mode for window selected: " << mode << "\n";
    break;
  }
  
  for (int i = 0; wl [i].id >= 0; i++) {
    wxWindow *w = tab_drysweep->FindWindow (wl [i].id);
    if (!w)   w = tab_deconvolv->FindWindow (wl [i].id);
    if (w) {
      //debug ("widget %ld %s", wl [i].id, wl [i].is_on ? "on" : "off");
      //w->Enable (wl [i].is_on);
      //w->SetTransparent (wl [i].is_on ? opac_on : opac_off);
      set_enable (w, wl [i].is_on);
    }
  }
  
  if (do_audio) {
    if (dec && dec->audio) {
      char buf [128];
      snprintf (buf, 127, "%d", dec->audio->samplerate);
      comb_samplerate->SetValue (buf);
      CP
      bool s = !chk_forcemono->GetValue ();
      pn_meter->set_stereo (s/*dec->audio->is_stereo*/);
      if ((!s) != (!dec->audio->is_stereo)) { // cheap xor
        CP
        //dec->audio->register_input (s);
        //dec->audio->init_output (false);
      }
      update_audio_ports ();
    }
    
    if (!audio_ready ()) {
      disable (list_jack_dry);
      disable (list_jack_wet_l);
      disable (list_jack_wet_r);
    } /*else {
      enable (list_jack_dry);
      enable (list_jack_wet_l);
      enable (list_jack_wet_r);
    }*/
    
    if (chk_forcemono->GetValue()) {
      disable (list_jack_wet_r);
      disable (spin_chn_offset);
    } else {
      enable (spin_chn_offset);
    }
    
    update_audio_ports ();
  }
}

void c_mainwindow::set_enable (wxWindow *w, bool b) {
  w->Enable (b);
  w->SetTransparent (b ? 255 : 128);
}

void c_mainwindow::update_audio_ports () {
    list_jack_dry->Clear ();
    list_jack_wet_l->Clear ();
    list_jack_wet_r->Clear ();
  if (!audio_ready ()) {
    disable (list_jack_dry);
    disable (list_jack_wet_l);
    disable (list_jack_wet_r);
    return;
  }
  int i;
  bool s = !chk_forcemono->GetValue ();
  
  std::vector <std::string> port_list;
  dec->audio->get_output_ports (port_list);
  for (i = 0; i < port_list.size (); i++) {
    list_jack_dry->Append (port_list [i]);
  }
  
  port_list.clear ();
  dec->audio->get_input_ports (port_list);
  for (i = 0; i < port_list.size (); i++) {
    list_jack_wet_l->Append (port_list [i]);
    if (s) list_jack_wet_r->Append (port_list [i]);
  }
  if (!s) disable (list_jack_wet_r);
}

void c_mainwindow::on_btn_dryfile_browse (wxCommandEvent &ev  ) {
  CP
  wxFileDialog f (this, "Choose dry sweep file",
                  "", "", "*.[Ww][Aa][Vv]", wxFD_OPEN|wxFD_FILE_MUST_EXIST);
  int ret = f.ShowModal ();
  if (ret != wxID_CANCEL) {
    std::string fn = std::string (f.GetPath ());
    text_dryfile->SetValue (fn);
    if (!dec) return;
    bool b = dec->load_sweep_dry (fn.c_str ());
    if (!b) {
      show_message ("Failed to load dry sweep: " + fn);
    }
  }
}

void c_mainwindow::on_btn_generate (wxCommandEvent &ev) { CP
  make_dry_sweep ();
}

bool c_mainwindow::make_dry_sweep (bool load_it) {
  debug ("start");
  if (!dec) return false;
  struct s_prefs *p = dec->prefs_;
  
  get_prefs (dec->prefs_);
  
  generate_log_sweep(p->sweep_seconds,
                     p->preroll_seconds,
                     p->marker_seconds,
                     p->marker_gap_seconds,
                     p->sweep_sr,
                     p->sweep_amp_db,
                     p->sweep_f1,
                     p->sweep_f2,
                     drysweep);

  if (load_it) {
    if (!dec->set_dry_from_buffer (drysweep, p->sweep_sr)) {
      CP
      return false;
    }
  }
  CP
  debug ("end");
  return true;
}

void c_mainwindow::show_message (std::string msg) {
  std::cout << msg;
  // TODO: log_widget->add (msg);
  wxMessageBox ("\n" + msg + "\n", "DIRT");
}

void c_mainwindow::show_error (std::string msg) {
  std::cerr << msg;
  // TODO: log_widget->add ("Error: " + msg);
  wxMessageBox ("\n" + msg + " \n", "DIRT error");
}

void c_mainwindow::on_btn_dry_save (wxCommandEvent &ev) {
  CP
  std::string srtext = std::string (comb_samplerate->GetValue ());
  if (srtext.size () == 0) {
    srtext = "48000"; 
    comb_samplerate->SetValue (srtext);
  }
  wxFileDialog f (this, "Save sweep as .wav file",
                  "", "", "*.[Ww][Aa][Vv]", wxFD_SAVE|wxFD_OVERWRITE_PROMPT);
  int ret = f.ShowModal ();
  if (ret != wxID_CANCEL) {
    CP
    if (!make_dry_sweep ()) {
      show_error ("Deconvolver failed to load our dry sweep\n");
    }
      
    std::string dest = std::string (f.GetPath ());
    // wxWidgets does this for us with wxFD_OVERWRITE_PROMPT flag
    /*bool ok = true;
    if (file_exists (dest.c_str ())) {
      
    } else {
      CP
      ok = true;
    }*/
    int sr = atoi (srtext.c_str ());
    if (sr < SAMPLERATE_MIN || sr > SAMPLERATE_MAX) {
      show_error ("Invalid sample rate:\n" + srtext);
    } else {
      if (!write_mono_wav (dest.c_str (), drysweep, sr)) {
        show_error ("Writing failed:\n" + dest);
      } else {
      }
    }
  }
}

void c_mainwindow::on_btn_inputdir_scan (wxCommandEvent &ev) {
  CP
  wxDir absdir (text_inputdir->GetValue ());
  
  if (!absdir.IsOpened ())
    return;
  wxString wxabspath = absdir.GetName ();
  
  std::string abspath (wxabspath);
  
  int n = add_dir (abspath, chk_inputdir_recursive->GetValue ());
  char buf [32];
  snprintf (buf, 31, "%d", n);
  show_message ("Scanned:\n" + abspath + "\n\nAdded " +
                std::string (buf) + " files\n");
}

void c_mainwindow::on_btn_inputdir_browse (wxCommandEvent &ev) {
  CP
  wxDirDialog f (this, "Choose directory containing input files");
  int ret = f.ShowModal ();
  if (ret != wxID_CANCEL) {
    std::string fn = std::string (f.GetPath ());
    text_inputdir->SetValue (fn);
  }
}

void c_mainwindow::on_btn_outputdir_browse (wxCommandEvent &ev) {
  CP
  wxDirDialog f (this, "Choose destination directory");
  int ret = f.ShowModal ();
  if (ret != wxID_CANCEL) {
    std::string fn = std::string (f.GetPath ());
    text_outputdir->SetValue (fn);
  }
}

void c_mainwindow::on_btn_inputfiles_add (wxCommandEvent &ev) {
  CP
  std::vector<std::string> list;
  std::string s;
  wxFileDialog f (this, "Choose input file(s)",
                  "", "", "*.[Ww][Aa][Vv]", wxFD_OPEN|wxFD_MULTIPLE);
  if (cwd.size () > 0)
    f.SetDirectory (wxString (cwd));
  int ret = f.ShowModal ();
  if (ret != wxID_CANCEL) {
    cwd = f.GetDirectory ();
    wxArrayString wxv;
    f.GetFilenames (wxv);
    for (int i = 0; i < wxv.GetCount (); i++) {
      s = std::string (wxv [i]);
      list_inputfiles->Append (cwd + "/" + s);
      /*std::cout << "got filename: " + s + "\n" << std::flush;
      list.push_back (std::string (s));*/
    }
  }
}

void c_mainwindow::on_btn_inputfiles_clear (wxCommandEvent &ev) {
  wxArrayInt list;
  int count = list_inputfiles->GetSelections (list);
  debug ("count=%d", count);
  if (!count) {
    CP
    list_inputfiles->Clear ();
  } else {
    for (int i = list.GetCount () - 1; i >= 0; i--) {
      debug ("deleting item %d", i);
      list_inputfiles->Delete (list [i]);
    }
  }
}

void c_mainwindow::on_btn_align_manual (wxCommandEvent &ev) {
  CP
  list_align->SetSelection ((int) align_method::MANUAL);
}

void c_mainwindow::on_btn_audio (wxCommandEvent &ev) { CP
  if (!dec || !dec->audio) return;
  
  if (dec->audio->state == audiostate::NOTREADY) {
    if (init_audio ()) {
      update_audio_ports ();
    } else {
      show_error ("Failed to initialize " + 
                    g_backend_names [(int) dec->audio->backend] +
                    "\n...is audio device available?\n");
    }
  } else {
    disable_audio ();
  }
}

void c_mainwindow::on_btn_play (wxCommandEvent &ev) {
  if (!audio_ready ()) return;
  CP
  //init_audio ();
  if (dec->audio->state == audiostate::IDLE ||
      dec->audio->state == audiostate::MONITOR)
    dec->audio->play (dec->dry_, false);
  else
    dec->audio->stop ();
}

void c_mainwindow::on_btn_process (wxCommandEvent &ev) {
  CP
  std::string wetfile;
  int i, n;
  
  if (!dec) {
    printf ("No deconvolver object, can't process! (this shouldn't happen)\n");
    return;
  }
  
  if (!dec->has_dry ()) {
    show_message ("No DRY sweep has been loaded yet");
  }
  
  n = list_inputfiles->GetCount ();
  printf ("got %d files:\n", n);
  
  dec->audio_stop ();
  get_prefs (dec->prefs_);
  debug ("mono: %s", dec->prefs_->request_stereo ? "true" : "false");
  //dec->set_dry_from_buffer (...);
  
  for (i = 0; i < n; i++) {
    wetfile = std::string (list_inputfiles->GetString (i));
    printf ("  %d: '%s'\n", i, wetfile.c_str ());
    bool b = dec->load_sweep_wet (wetfile.c_str ());
    if (!b) {
      printf ("can't load file %s, ", wetfile.c_str ());
      if (chk_abort->GetValue ()) {
        printf ("aborting\n");
        i = n;
      } else {
        printf ("skipping\n");
      }
      break;
    } else {
      debug ("loaded '%s'", wetfile.c_str ());
    }
  }
}

int c_mainwindow::add_files (std::vector<std::string> list) {
  CP
  // not using this, see on_btn_inputfiles_add
  int count = 0;
  return count;
}

int c_mainwindow::update_ir_list () {
  int count, i, n = ir_files.size ();
  debug ("n=%d", n);
  list_irfiles->Clear ();
  
  for (i = 0; i < n; i++) {
    if (1||ir_files [i].path.size () > 0 &&
        ir_files [i].l.size () > 0 &&
        ir_files [i].r.size () > 0) {
      count++;
      list_irfiles->Append (ir_files [i].path);
    }
  }
  debug ("n=%d, count=%d, %d IR entries, %d list entries", 
          n, count, ir_files.size (), list_irfiles->GetCount ());
          
  return count;
}

// could optimize this... 
static bool suffix_match (std::string fn, std::string suffix) {
  int pos = fn.find_last_of ('.');
  
  if (pos < 0)
    return false;
  
  bool retval = true;
  for (int i = 0; fn [pos + i] && suffix [i]; i++) {
    if (tolower (fn [pos +i]) != tolower (suffix [i]))
      retval = false;
  }
  
  //debug ("fn=%s, suffix=%s, pos=%d, retval=%s",
  //       fn.c_str (), suffix.c_str (), pos, retval ? "true" : "false");
  return retval;
}

int c_mainwindow::add_dir (std::string dirname, bool recurs) {
  //CP
  int count = 0;
  
  wxDir dir (dirname);
  wxString str;
  
  if (dir.IsOpened ()) {
    wxString wxfn;
    bool found = dir.GetFirst (&wxfn);
    while (found) {
      std::string fn (wxfn);
      std::string absfn (dirname + "/" + fn.c_str ());
      if (!dir_exists (absfn)) {
        if (recurs) {
          printf ("recurs dir: %s\n", absfn.c_str ());
          count += add_dir (absfn, recurs);
        } else {
          printf ("found dir:  %s\n", absfn.c_str ());
        }
      } else {
        if (suffix_match (absfn, ".wav")) {
          printf ("found file: %s\n", absfn.c_str ());
          list_inputfiles->Append (absfn);
          count++;
        } else {
          printf ("ignoring file: %s\n", absfn.c_str ());
        }
      }
      found = dir.GetNext (&wxfn);
    }
  } else {
    show_error ("can't open directory " + dirname + "\n");
  }
  
  return count;
}

// plumbing for vu data
void c_mainwindow::set_vu_l (float level, float hold, bool clip, bool xrun) 
  { pn_meter->set_l (level, hold, clip, xrun); }
  
void c_mainwindow::set_vu_r (float level, float hold, bool clip, bool xrun)
  { pn_meter->set_r (level, hold, clip, xrun); }

// c_deconvolver_gui

void c_deconvolver_gui::set_vu_pre () { }
void c_deconvolver_gui::set_vu_post () { }

void c_deconvolver_gui::set_vu_l (float level, float hold, bool clip, bool xrun) {
  c_app *app = ((c_app *) wxTheApp);
  if (!app || !app->mainwindow) return;
  app->mainwindow->set_vu_l (level, hold, clip, xrun);
}

void c_deconvolver_gui::set_vu_r (float level, float hold, bool clip, bool xrun) {
  c_app *app = ((c_app *) wxTheApp);
  if (!app || !app->mainwindow) return;
  app->mainwindow->set_vu_r (level, hold, clip, xrun);
}


////////////////////////////////////////////////////////////////////////////////
// c_app

c_app::c_app (c_deconvolver *d)
: wxApp::wxApp () {
  (dec = d); if (!d) { debug ("Warning: no deconvolver given"); }
  CP
}

c_app::~c_app () {
  CP
}

bool c_app::OnInit () {
  debug ("start");
  if (!wxApp::OnInit ()) return false;
  mainwindow = new c_mainwindow (dec);
  SetTopWindow (mainwindow);
  mainwindow->Show ();
  if (!mainwindow) {
    std::cerr << "can't create main window\n";
    exit (1);
  }
  debug ("end");
  return true;
}

/*int c_app::OnRun () {
  debug ("start");
  CP
  int retval = wxApp::OnRun ();
  debug ("end");
  return retval;
}*/

int c_app::OnExit () {
  debug ("start");
  //if (mainwindow) delete mainwindow;
  wxApp::OnExit ();
  debug ("end");
  return 0;
}

int c_app::FilterEvent (wxEvent &ev) {
  return wxApp::FilterEvent (ev);
}

c_app *g_app = NULL;
//wxIMPLEMENT_APP_NO_MAIN (c_app);

int wx_main (int argc, char **argv, c_deconvolver *dec) {
  int retval = 0;
  debug ("start");
    
  //check_endianness ();
  
  // THE RIGHT WAY TO DO IT:
  g_app = new c_app (dec);
  wxApp::SetInstance (g_app);
  retval = wxEntry (argc, argv);
  //delete g_app;
  debug ("end, retval=%d", retval);
  return retval;
}


////////////////////////////////////////////////////////////////////////////////
// c_customwidget - parent class for other widget types

enum {
  ASKYESNO_NO = 0,
  ASKYESNO_YES,
  ASKYESNO_CANCEL
};

enum {
  KEY_LEFT,
  KEY_RIGHT,
  KEY_UP,
  KEY_DOWN,
  KEY_SPACE,
  KEY_ENTER,
  KEY_ESCAPE
};

enum {
  DATETIME_NONE,
  DATETIME_NANOSECONDS,
  DATETIME_MILLISECONDS,
  DATETIME_SECONDS,
  DATETIME_MINUTES,
  DATETIME_HOURS,
  DATETIME_DAYS,
  DATETIME_WEEKS,
  DATETIME_MONTHS,
  DATETIME_YEARS,
  DATETIME_CENTURIES,
  DATETIME_MILLENIA,
  DATETIME_INFINITY
};

enum {
  LABEL_ALIGN_LEFT,
  LABEL_ALIGN_CENTER,
  LABEL_ALIGN_RIGHT
};

 
 
BEGIN_EVENT_TABLE (c_customwidget, wxPanel)
  EVT_PAINT (c_customwidget::on_paint_event)
  EVT_SIZE (c_customwidget::on_resize_event)
  EVT_MOTION (c_customwidget::on_mousemove)
  EVT_LEFT_DOWN (c_customwidget::on_mousedown_left)
  EVT_LEFT_UP (c_customwidget::on_mouseup_left)
  EVT_MIDDLE_DOWN (c_customwidget::on_mousedown_middle)
  EVT_MIDDLE_UP (c_customwidget::on_mouseup_middle)
  EVT_RIGHT_DOWN (c_customwidget::on_mousedown_right)
  EVT_RIGHT_UP (c_customwidget::on_mouseup_right)
  EVT_LEAVE_WINDOW (c_customwidget::on_mouseleave)
  EVT_MOUSEWHEEL (c_customwidget::on_mousewheel)
  /*EVT_KEY_DOWN (c_customwidget::keypress)
  EVT_KEY_UP (c_customwidget::keyrelease)*/
  //EVT_IDLE (c_customwidget::idle_callback)
  EVT_SHOW (c_customwidget::on_visible_callback)
END_EVENT_TABLE ()

c_customwidget::c_customwidget (wxWindow *p_parent,
                                int p_id,
                                wxPoint pos, 
                                wxSize p_size,
                                wxBorder border)
: wxPanel (p_parent, p_id, pos, p_size, border) {
  debug ("begin");
  
  visible = true;
  parent = p_parent;
  int font_height = 16;//fm.height ();
  debug ("font_height=%d", font_height);
  
  //SetMinClientSize (wxSize (128, (int) (font_height * 2.8)));
  //SetMaxClientSize (wxSize (16384, 16384));
  //SetClientSize (p_size);
  
  //col_bg = wxTransparentColour;
  col_default_bg = wxSystemSettings::GetColour (wxSYS_COLOUR_WINDOW);
  col_default_fg = wxSystemSettings::GetColour (wxSYS_COLOUR_WINDOWTEXT);
  col_bg = col_default_bg;
  col_fg = col_default_fg;

  //font = wxSystemSettings::GetFont (wxSYS_SYSTEM_FONT);
  font = GetFont ();
  smallboldfont = font;//.MakeSmaller ().MakeBold (); uhh, this function changes fonts inplace
  smallboldfont.MakeSmaller ().MakeBold ();
  
  mouse_buttons = 0;
  mouse_x = -1;
  mouse_y = -1;
  click_distance = 5; // arbitrary, TODO: get reasonable default from OS/gfx system
  for (int i = 0; i < 8; i++)
    mousedown_x [i] = mousedown_y [i] = -16384; // again arbitrary, but umm... yeah :)
  
  in_paintevent = false;
  
  set_opacity (255);
  debug ("end");
}


c_customwidget::~c_customwidget () {
  debug ("start");
  debug ("end");
}


void c_customwidget::set_opacity (int p_opacity) {
  //char buf [256];
  
  opacity = p_opacity;
  /*sprintf (buf, "* { background-color: rgba(%d,%d,%d,%d); border-width: 0px; }", r, g, b, opacity);
  debug ("buf='%s'", buf);
  setStyleSheet (buf);*/
  
  base_image_valid = false;
  update ();
}


void c_customwidget::inspect () {
  debug ("start");
  debug ("end");
}

/* Typically, this funciton would react on resize, or other event that
 * invalidates the base image. */
void c_customwidget::render_base_image () {
  int i, j, k, x, y;
  //unsigned char *pix, *pixeldata;
  //int rowstride;
  int cr, cg, cb;
  int timeunit, timevalue;
  float alpha;
  wxBitmap /*dummy_bitmap,*/ text_image;
  wxMemoryDC dc;
  
  GetClientSize (&width, &height);
  //debug ("start, width=%d, height=%d", width, height);
  
  /*int w = width - 1;
  int h = height - 1;*/
  int gradwidth = width / 5;
  img_base = wxBitmap (width, height, 32);
  img_overlay = wxBitmap (width, height, 32);
  
  dc.SelectObject (img_base);
  dc.SetBackground (col_bg);
  dc.Clear ();
  dc.SelectObject (wxNullBitmap);
  
  //c_misc::clear_bitmap (&img_overlay, 255, 255, 255, 32);
  
  base_image_valid = true;
  
  //debug ("end");
}

/* Typically, this function would react to minor appearance changes, such
 * as visual feedback of mouse events and so on. */
void c_customwidget::update (wxWindowDC &dc) {
  int x, y, w, h, i, m, c;
  unsigned char new_md5 [16];
  int new_width, new_height;
  
  //debug ("start");
  
  GetClientSize (&new_width, &new_height);
  
  if (!base_image_valid || new_width != width || new_height != height) {
    render_base_image ();
    img_overlay = wxBitmap (width, height, 32);
  }
  
  // ??? keeping this for reference
  //wxGraphicsContext *gc = wxGraphicsContext::Create (dc);
  
  dc.DrawBitmap (img_base, 0, 0);
  //dc.DrawBitmap (img_overlay, 0, 0);
  
  /* Add overlay/highlight indicators etc.
   * we would need to do this AFTER render_base_image () */
  
  //debug ("end");
}

void c_customwidget::update () {
  wxClientDC dc (this);
  update (dc);
}

/*void c_customwidget::schedule_deletion () {
  debug ("start");
  Hide ();
  deleted = true;
  debug ("end");
}

void c_customwidget::idle_callback (wxIdleEvent &ev) {
  //debug ("start");
  if (deleted) {
    debug ("deleted");
    Destroy ();
    deleted = false;
  }
  //debug ("end");
}*/

void c_customwidget::on_visible_callback (wxShowEvent &ev) {
  base_image_valid = false;
  update ();
}

void c_customwidget::on_paint_event (wxPaintEvent &ev) {
  //debug ("start");
  wxPaintDC dc (this);
  update (dc);
  //debug ("end");
}

// TODO: find out in which cases (if any) this x,y < 0 differs from
// mapping EVT_ENTER_WINDOW - see $DOC/classwx_mouse_event.html
void c_customwidget::on_mousemove (wxMouseEvent &ev) {
  //debug ("start");
  
  if (mouse_x < 0 && mouse_y < 0) {
    //debug ("Mouse enter");
    //emit sig_mouse_enter (this);
  }
  //emit sig_mouse_move (this, mouse_x, mouse_y);

  mouse_x = ev.GetX ();
  mouse_y = ev.GetY ();
  //debug ("mouse=%d,%d, buttons=%d", mouse_x, mouse_y, mouse_buttons);
  
  // we do this in derived class before/after calling this function
  //update ();
  
  //debug ("end");
}

bool c_customwidget::check_click_distance (int which) {
  if (which < 0 || which >= 8)
    return false;
  
  if (abs (mouse_x - mousedown_x [which]) > click_distance)
    return false;
  
  if (abs (mouse_y - mousedown_y [which]) > click_distance)
    return false;
  
  return true;
}

void c_customwidget::on_mousedown_left (wxMouseEvent &ev) {
  debug ("start");
  get_xy (ev);
  mousedown_x [0] = mouse_x;
  mousedown_y [0] = mouse_y;
  mouse_buttons |= 1;
  
  /*wxPaintDC dc (this);
  update (dc);*/
  update ();
  debug ("end");
}

void c_customwidget::on_mouseup_left (wxMouseEvent &ev) {
  debug ("start");
  get_xy (ev);
  mouse_buttons &= ~1;
  update ();
  /*if (check_click_distance (0)) {
    debug ("emitting sig_left_clicked");
    emit sig_left_clicked (this);
  }*/
  debug ("end");
}

void c_customwidget::on_mousedown_middle (wxMouseEvent &ev) {
  debug ("start");
  get_xy (ev);
  mousedown_x [1] = mouse_x;
  mousedown_y [1] = mouse_y;
  mouse_buttons |= 2;
  update ();
  debug ("end");
}

void c_customwidget::on_mouseup_middle (wxMouseEvent &ev) {
  debug ("start");
  get_xy (ev);
  mouse_buttons &= ~2;
  update ();
  /*if (check_click_distance (1)) {
    debug ("emitting sig_middle_clicked");
    emit sig_middle_clicked (this);
  }*/
  debug ("end");
}

void c_customwidget::on_mousedown_right (wxMouseEvent &ev) {
  debug ("start");
  get_xy (ev);
  mousedown_x [2] = mouse_x;
  mousedown_y [2] = mouse_y;
  mouse_buttons |= 4;
  update ();
  debug ("end");
}

void c_customwidget::on_mouseup_right (wxMouseEvent &ev) {
  debug ("start");
  get_xy (ev);
  mouse_buttons &= ~4;
  update ();  
  /*if (check_click_distance (2)) {
    debug ("emitting sig_right_clicked");
    emit sig_right_clicked (this);
  }*/
  debug ("end");
}

void c_customwidget::on_mousewheel (wxMouseEvent &ev) {
  //debug ("start");
  update ();
  //debug ("end");
}

void c_customwidget::on_mouseleave (wxMouseEvent &ev) {
  //debug ("start");
  
  mouse_x = -1;
  mouse_y = -1;
  update ();
  //emit sig_mouse_leave (this);
  
  //debug ("end");
}

void c_customwidget::on_resize_event (wxSizeEvent &ev) {
  //debug ("start");
  
  base_image_valid = false;
  update ();
  
  //debug ("end");
}

////////////////////////////////////////////////////////////////////////////////
// c_meterwidget

c_meterwidget::c_meterwidget (wxWindow *parent,
                              int id,
                              wxPoint pos,
                              wxSize size,
                              int wtf)
: c_customwidget (parent, id, pos, size, (wxBorder) wtf) {
  CP
}

void c_meterwidget::on_resize_event (wxSizeEvent &ev) { CP
  c_customwidget::on_resize_event (ev);
}

void c_meterwidget::render_gradient_bar () {
  img_bar = wxBitmap (width, height, 32);
  int bw = img_bar.GetWidth ();
  int bh = img_bar.GetHeight ();
  
  // lookup table for each subpixel, len x 3 array
  int i;
  int len = vertical ? bh : bw;
  int mid = len * 2 / 3;  // point 0..1 where color is exactly yellow
  char lutR [len];
  char lutG [len];
  char lutB [len];
  
  tinyfont = wxSystemSettings::GetFont (wxSYS_SYSTEM_FONT).MakeBold ();
  
  for (i = 0; i < mid; i++) { // from green to yellow
    lutR [i] = i * 255 / mid;
    lutG [i] = 255;
    lutB [i] = 0;
  }
  for (; i < len; i++) {      // from yellow to red
    lutR [i] = 255;
    //float t = (float) ((i - len - 1) - mid) / (float) (mid - len);
    float t = (float)(i - mid) / (float)((len - 1) - mid);
    t = std::clamp(t, 0.0f, 1.0f);
    lutG[i] = 255.0f * (1.0f - t);
    lutB [i] = 0;
  }
  
  // ...now write actual pixels to bitmap
  wxAlphaPixelData pdata (img_bar);
  wxAlphaPixelData::Iterator p (pdata);
  p.MoveTo (pdata, 0, 0);
  for (int y = 0; y < bh; y++) {
    wxAlphaPixelData::Iterator rowstart = p;
    for (int x = 0; x < bw; x++, p++) {
      int i = vertical ? (bh - 1 - y) : x;
      p.Red ()                 = lutR [i];
      p.Green ()               = lutG [i];
      p.Blue ()                = lutB [i];
      p.Alpha ()               = 255;
    }
    p = rowstart;
    p.OffsetY (pdata, 1);
  }
}

void c_meterwidget::render_base_image () {
  //debug ("start");
  c_customwidget::render_base_image ();
  
  //if (rec_size < 0) rec_size = (vertical ? width : height);
  //if (clip_size < 0) clip_size = (vertical ? rec_size / 2 : rec_size * 2);
  
  met_len = (vertical ? height : width) - clip_size - rec_size;

  if (width < height) {
    vertical = true;
    ln = height;
    th = width;
  } else {
    vertical = false;
    ln = width;
    th = height;
  }
  
  wxMemoryDC dc; 
  dc.SelectObject (img_base);
  
  int crk = 0;
  
  if (stereo) {
    t1 = 0;
    crk = th / 50;
    if (crk < 1) crk = 1;
    t2 = (th / 2) - crk;
    t4 = th - t1;
    t3 = th - t2;
    tp = (th / 20);
    if (tp <= 1) tp = 1;
  } else {
    t1 = th * 0.1;
    t2 = th - t1;
    t3 = -1;
    t4 = -1;
    tp = (th / 20) + 1;
  }
  
  dc.SetPen (wxPen (*wxBLACK));
  dc.SetBrush (wxBrush (*wxBLACK));
  //debug ("(%dx%d) t1=%d, t2=%d, t3=%d, t4=%d, crk=%d", width, height, t1, t2, t3, t4, crk);
  if (vertical) {
    dc.DrawRectangle (t1, clip_size, t2 - t1, met_len);
    dc.DrawRectangle (t3, clip_size, t4 - t3, met_len);
  } else {
    dc.DrawRectangle (rec_size, t1, met_len, t2 - t1);
    dc.DrawRectangle (rec_size, t3, met_len, t4 - t3);
  }
  
  // debug outline
  //dc.SetBrush (wxBrush ());
  //dc.SetPen (wxPen (wxColour (255, 255, 255, 64)));
  //dc.DrawRectangle (0, 0, width, height);
  
  // meter lines
  struct ln {
    float pos;
    int r, g, b, a;
  } lines [] = {
    // pos    r    g    b    a 
    //{ 0.0,     0, 255,   0, 255 },
    { 0.5,     0, 255,   0, 255 },
    { 0.75,  255, 255,   0, 255 },
    { 0.875, 255, 128,   0, 255 },
    { 1.0,   255,   0,   0, 255 },
    { -1.0,    0,   0,   0, 255 },
  };
  
  for (int i = 0; lines [i].pos >= 0; i++) {
    ln * l = &lines [i];
    //debug ("line: %f, rgb (%d,%d,%d)", l->pos, l->r, l->g, l->b);
    dc.SetPen (wxPen (wxColour (l->r, l->g, l->b)));
    if (vertical) {
      int ppos = (height - clip_size - rec_size) * l->pos;
      if (ppos > height - 1) ppos = height - 1;
      dc.DrawLine (0, height - ppos - rec_size, width, height - ppos - rec_size);
    } else {
      int ppos = (width - clip_size - rec_size) * l->pos;
      if (ppos > width - 1) ppos = width - 1;
      dc.DrawLine (ppos + rec_size, 0, ppos + rec_size, height);
    }
  }
  
  render_gradient_bar ();
  
  // "clip" and "xrun" indicators
  wxSize meterwarnsize [(int) meterwarn::MAX];
  wxString meterwarntext [] = {
    "REC",
    "CLIP",
    "XRUN",
    ""
  };
  for (int i = 0; i < (int) meterwarn::MAX; i++) {
    meterwarnsize [i] = dc.GetTextExtent (meterwarntext [i]);
  }
  dc.SelectObject (wxNullBitmap);
  if (!vertical) {
    float ps = (float) std::min (width, height);
    ps /= 1.6;
    tinyfont.SetPointSize (ps);
    dc.SetFont (tinyfont);
    for (int i = 0; i < (int) meterwarn::MAX; i++) {
      int w = meterwarnsize [i].x;
      int h = meterwarnsize [i].y;
      debug ("new bitmap, w=%d, h=%d", w, h);
      img_warning [i] = wxBitmap (w * 4 / 3, h, 32);
      dc.SelectObject (img_warning [i]);
      dc.SetTextForeground (wxColour (192, 0, 0));
      dc.SetBackground (wxColour (0, 0, 0, 0));
      dc.Clear ();
      dc.DrawText (meterwarntext [i], 0, 0);
      dc.SelectObject (wxNullBitmap);
    }
  }
             
  //debug ("end");
}

// at: x start pos if vertical, y pos if horiz
// ie. distance between (0,0) and meter bar
void c_meterwidget::draw_bar (wxDC &dc, int at, int th, bool is_r,
                              float level, float hold) {
  if (level < 0) level = 0;
  if (level > 1) level = 1;
  int bar_len = met_len * level;
  int x, y, w, h;
  
  if (bar_len > 0) {
    dc.SetPen (wxPen (*wxBLACK));
    dc.SetBrush (wxBrush (*wxGREEN));
    if (vertical) {
      
      int meter_top    = clip_size;
      int meter_bottom = height - rec_size;
      int meter_height = meter_bottom - meter_top; // == met_len
      int bar_len = static_cast<int>(meter_height * level);
      int bar_top = meter_bottom - bar_len;   // grow from bottom up
      
      x = at + tp;
      y = bar_top + tp;//height - bar_len - rec_size - clip_size + tp;
      w = th - (tp * 2);
      h = bar_len /*+ clip_size*/ - (tp * 2);
    } else {
      x = rec_size + tp;
      y = at + tp;
      w = bar_len/* + rec_size*/ - (tp * 2);
      h = th - (tp * 2);
      //dc.DrawRectangle (x, y, w, h);
    }
    //dc.DrawRectangle (x, y, w, h); //top
    img_bar_sub = img_bar.GetSubBitmap (wxRect (x, y, w, h));
    dc.DrawBitmap (img_bar_sub, x, y);
  }
  
  // peak hold indicator
  int holdpos = hold * met_len;
  if (holdpos > 0 && holdpos < met_len) {
    dc.SetPen (wxPen (wxColour (128, 128, 0)));
    if (vertical) {
      holdpos -= clip_size;
      dc.DrawLine (at, y + h - holdpos, at + th, y + h - holdpos);
    } else {
      holdpos += rec_size;
      dc.DrawLine (x + holdpos, at, x + holdpos, at + th);
    }
  }
}

void c_meterwidget::update (wxWindowDC &dc) {
  c_customwidget::update (dc);
  
  // meter bars
  if (stereo) {
    draw_bar (dc, t1, t2 - t1, false, l, hold_l);
    draw_bar (dc, t3, t4 - t3, true,  r, hold_r);
  } else {
    draw_bar (dc, t1, t2 - t1, false, l, hold_l);
  }
  
  // clip indicator
  dc.SetBrush (wxBrush (*wxRED));
  dc.SetPen (wxPen (*wxRED));
  if ((clip_l || clip_r) && clip_size > 0) {
    if (vertical) {
      dc.DrawRectangle (0, 0, width, clip_size);
    } else {
      dc.DrawRectangle (width - clip_size, 0, clip_size, height);
    }
  } else if ((clip_l || clip_r) && !vertical) {
    int idx = (int) meterwarn::CLIP;
    int w = img_warning [idx].GetWidth ();
    int h = img_warning [idx].GetHeight ();
    dc.DrawBitmap (img_warning [idx], width - w - 1, (height - h) / 2);
  }
  
  // recording indicator (red circle)
  int rp = (vertical ? width : height) / 4;
  if (rec_size > 0 && rec) {
    if (vertical)
      dc.DrawEllipse (rp, height - rec_size + rp, width - (rp * 2), rec_size - (rp * 2));
    else
      dc.DrawEllipse (rp, rp, rec_size - (rp * 2), height - (rp * 2));
  } else if (1||rec && !vertical) {
    int idx = (int) meterwarn::REC;
    int h = img_warning [idx].GetHeight ();
    dc.DrawBitmap (img_warning [idx], 1, (height - h) / 2);
  }
}

void c_meterwidget::set_stereo (bool b) {
  stereo = b;
  base_image_valid = false;
  Refresh ();
}

void c_meterwidget::set_l (float level, float hold,
                           bool clip, bool xr, bool _rec) {
  //debug ("lev:%f, hold=%f, clip=%s, xrun=%s", level, hold,
  //       clip ? "true" : "false", xrun ? "true": "false");
  bool needs_update = false;
  if (l != level) needs_update = true;
  l = level;
  hold_l = hold;
  clip_l = clip;
  rec = _rec;
  xrun = xr;
  if (needs_update) Refresh ();
}

void c_meterwidget::set_r (float level, float hold,
                           bool clip, bool xr, bool _rec) {
  //debug ("lev:%f, hold=%f, clip=%s, xrun=%s", level, hold,
  //       clip ? "true" : "false", xrun ? "true": "false");
  bool needs_update = false;
  if (r != level) needs_update = true;
  r = level;
  hold_r = hold;
  clip_r = clip;
  rec = _rec;
  xrun = xr;
  if (needs_update) Refresh ();
}


////////////////////////////////////////////////////////////////////////////////
// c_waveformwidget

c_waveformwidget::c_waveformwidget (wxWindow *parent,
                              int id,
                              wxPoint pos,
                              wxSize size,
                              int wtf)
: c_customwidget (parent, id, pos, size, (wxBorder) wtf) {
  CP
  // for now, hard-coded colors for our waveform view
  col_bezel1 = wxColour (0, 0, 0, 128);
  col_bezel2 = wxColour (255, 255, 255, 128);
  col_bg = wxColour (0, 0, 0);
  col_fg = wxColour (0, 128, 32);
}

void c_waveformwidget::render_base_image () {
  c_customwidget::render_base_image ();
  // draw grid
  int w2 = width / 2;
  int h2 = height / 2;
  wxMemoryDC dc;
  dc.SelectObject (img_base);
  
  if (!wavdata) {
    dc.SetPen (wxPen (wxTransparentColor));
    dc.SetBrush (wxBrush (col_default_bg));
    dc.DrawRectangle (0, 0, width, height);
    dc.SetPen (col_default_fg);
    dc.SetTextForeground (col_default_fg);
        
    wxFont font = GetFont ();
    std::string msg = "(no impulse response file)";
    wxSize sz = dc.GetTextExtent (msg);
    dc.DrawText (msg, w2 - sz.x / 2, h2 - sz.y / 2);
    return;
  }
  
  // background
  dc.SetBrush (wxBrush (col_bg));
  dc.SetPen (wxPen (col_bg));
  dc.DrawRectangle (0, 0, width, height);
  
  // baseline
  dc.SetPen (wxPen (col_fg));
  dc.DrawLine (2, 1 + height / 2, 1 + width - 4, 1 + height / 2);
  
  draw_frame (dc);
  dc.SelectObject (wxNullBitmap);
}

void c_waveformwidget::draw_frame (wxDC &dc) {
  // bezel/frame, this gives a nice gradient effect: line at a slight
  // angle, sandwitched between two lines of system background color
  dc.SetPen (wxPen (col_bezel1));
  dc.DrawLine (1, 1, width * 2, -1);
  dc.DrawLine (1, 1, -1, height * 2);
  dc.SetPen (wxPen (col_bezel2));
  dc.DrawLine (width - 1, height * -3 / 2, width - 2, height);
  dc.DrawLine (width - 2, height - 2, 1 + (width * -3 / 2), height - 1);
  
  dc.SetPen (wxPen (col_default_bg));
  //dc.DrawLine (2, 2, width -3, 2);
  //dc.DrawLine (2, 2, 2, height - 3);
  dc.DrawLine (width - 3, 2, width - 3, height - 3);
  dc.DrawLine (width - 3, height - 3, 3, height - 3);
  dc.DrawLine (0, 0, width -1, 0);
  dc.DrawLine (0, 0, 0, height - 1);
  dc.DrawLine (width - 1, 0, width - 1, height - 1);
  dc.DrawLine (width - 1, height - 1, 0, height - 1);
}

void c_waveformwidget::update (wxWindowDC &dc) {
  c_customwidget::update (dc);
  dc.SetPen (wxPen (col_default_fg));
}


////////////////////////////////////////////////////////////////////////////////
// test/minimal example usage of c_customwidget derived class

void c_testwidget::render_base_image () { CP
  c_customwidget::render_base_image ();
  wxMemoryDC dc;
  dc.SelectObject (img_base);
  std::string msg = "This is an example use of c_customwidget";
  wxSize sz = dc.GetTextExtent (msg);
  dc.SetTextForeground (col_default_fg);
  dc.DrawText (msg, (width - sz.x) / 2, (height - sz.y) / 2);
  dc.SelectObject (wxNullBitmap);
}

void c_testwidget::update (wxWindowDC &dc) {
  c_customwidget::update (dc);
  dc.SetPen (wxPen (*wxRED));
  dc.DrawLine (0, 0, width, height);
}

#endif // USE_WXWIDGETS

