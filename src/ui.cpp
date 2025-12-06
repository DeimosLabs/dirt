
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
  SetSize (GetSize () + wxSize (0, 100));
  SetSizeHints (GetSize ());
  
  set_mode (ID_FILE);
  
  init_audio ();
  timer.Bind (wxEVT_TIMER, &c_mainwindow::on_timer, this);
  timer.Start (32);
  pn_meter2->vertical = true;
}

c_mainwindow::~c_mainwindow () {
  CP
}

void c_mainwindow::on_timer (wxTimerEvent &ev) {
  static long int num_passes = 0;
  const int update_widgets_every = 4;
  num_passes++;
  if (!dec || !dec->audio) return;
  
  // animation stuff etc. that we do every pass
  // nothing yet, our custom widgets update themselves on redraw events
  //pn_meter->Refresh (); // not necessary
  
  // widget stuff: update only every n passes
  if (num_passes % update_widgets_every == 0) {
    
    bool do_full_update = false;
    audiostate s = dec->audio->state;
    
    if (s != prev_audio_state) {
      prev_audio_state = s;
      do_full_update = true;
    }
    if (mode != prev_mode) {
      prev_mode = mode;
      do_full_update = true;
    }
    
    if (s == audiostate::NOTREADY) {
      disable (btn_play);
      btn_audio->SetLabel ("Connect");
    } else if (s == audiostate::IDLE) {
      if (!dec->has_dry ()) {
        disable (btn_play);
      } else {
        enable (btn_play);
      }
      btn_play->SetLabel ((mode == ID_ROUNDTRIP) ? "Record" : "Play");
      btn_audio->SetLabel ("Disconnect");
    } else {
      if (!dec->has_dry ()) {
        disable (btn_play);
      } else {
        enable (btn_play);
        int sr = dec->prefs_ ? dec->prefs_->sweep_sr : 
                               atoi (comb_samplerate->GetValue ().c_str ());
        size_t sec_left = dec->audio->get_play_left () / (size_t) sr;
        char buf [32];
        snprintf (buf, 31, "Stop (%ld)", sec_left);
        //debug ("sr=%d, sec_left=%d", sr, sec_left);
        btn_play->SetLabel (buf);
        btn_audio->SetLabel ("Disconnect");
      }
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
  }
  /*if (do_full_update)
    set_mode (mode);*/
}

bool c_mainwindow::audio_ready () {
  if (!dec) {
    //debug ("no deconvolver");
    return false;
  }
  if (!dec->audio) {
    //debug ("deconvolver has no audio client");
    return false;
  }
  if (dec->audio->state == audiostate::NOTREADY) {
    //debug ("deconvolver audio not ready");
    return false;
  }
  
  return true;
}

bool c_mainwindow::init_audio () { CP
  return init_audio (atoi (comb_samplerate->GetValue ().c_str ()), 
                      !chk_forcemono->GetValue ());
}

bool c_mainwindow::init_audio (int samplerate, bool stereo) {
  if (init_audio_done)
    return true;
  debug ("start, samplerate=%d, stereo=%s", samplerate, stereo ? "true" : "false");
  CP
  
  if (!dec) {
    debug ("no deconvolver given");
    return false;
  }
  
  if (!dec->audio) {
    CP
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
  if (!dec->audio->init (jack_name, sr, st)) {
    CP
    debug ("audio init failed");
    return false;
  }
  
  dec->audio_arm_record ();
  init_audio_done = true;
  
  debug ("end");
  return true;
}

bool c_mainwindow::shutdown_audio () { CP
  if (!dec || !dec->audio) return false;
  if (dec->audio->state == audiostate::NOTREADY) return false;
  
  list_jack_dry->Clear ();
  list_jack_wet_l->Clear ();
  list_jack_wet_r->Clear ();
  init_audio_done = false;
  
  return dec->audio->shutdown ();
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

void c_mainwindow::on_chk_forcemono (wxCommandEvent &ev) {
  if (radio_file->GetValue ()) set_mode (ID_FILE);
  else if (radio_makesweep->GetValue ()) set_mode (ID_MAKESWEEP);
  else if (radio_roundtrip->GetValue ()) set_mode (ID_ROUNDTRIP);
  
  if (chk_forcemono->GetValue ()) {
    debug ("force mono");
    dec->audio->is_stereo = false;
  } else {
    debug ("stereo");
    dec->audio->is_stereo = true;
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
  if (sr > 0 && sr < 192000)
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
    pn_meter->stereo = pn_meter2->stereo = !fm;
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
    init_audio ();
    if (!audio_ready ()) {
      std::cerr << "Failed to initialize audio!\n";
      return;
    }
    char buf [128];
    snprintf (buf, 127, "%d", dec->audio->samplerate);
    comb_samplerate->SetValue (buf);
    CP
    bool s = !chk_forcemono->GetValue ();
    if ((!s) != (!dec->audio->is_stereo)) { // cheap xor
      CP
      dec->audio->init_input (s);
      //dec->audio->init_output (false);
    }
  }
  update_audio_ports ();
  
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
    dec->load_sweep_dry (fn.c_str ());
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
    if (sr < 0 || sr > 192000) {
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
    shutdown_audio ();
  }
}

void c_mainwindow::on_btn_play (wxCommandEvent &ev) {
  if (!audio_ready ()) return;
  CP
  init_audio ();
  if (dec->audio->state == audiostate::IDLE)
    dec->audio->play (dec->dry_, false);
  else
    dec->audio->stop ();
}

void c_mainwindow::on_btn_process (wxCommandEvent &ev) {
  CP
}

int c_mainwindow::add_files (std::vector<std::string> list) {
  CP
  // not using this, see on_btn_inputfiles_add
  int count = 0;
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

void c_mainwindow::set_vu_l (float level, float hold, bool clip, bool xrun) 
  { pn_meter->set_l (level, hold, clip, xrun);
    pn_meter2->set_l (level, hold, clip, xrun); }
  
void c_mainwindow::set_vu_r (float level, float hold, bool clip, bool xrun)
  { pn_meter->set_r (level, hold, clip, xrun); 
    pn_meter2->set_r (level, hold, clip, xrun); }  

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


/*
 * Custom widget example/template - (usually) base class.
 *
 * Widget class derived directly from wxPanel, where we override/bind
 * paint events etc. to implement our own appearance and behaviour.
 */


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


#ifdef SHOW_CUSTOMWIDGET_TEST_STUFF
char *c_customwidget::get_example_label_text () {
  return "This is an instance of c_customwidget.";
}
#endif


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
  
  GetSize (&width, &height);
  //debug ("start, width=%d, height=%d", width, height);
  
  /*int w = width - 1;
  int h = height - 1;*/
  int gradwidth = width / 5;
  base_image = wxBitmap (width, height, 32);
  img_overlay = wxBitmap (width, height, 32);
  
  dc.SelectObject (base_image);
  dc.SetBackground (col_bg);
  dc.Clear ();
  dc.SelectObject (wxNullBitmap);
  
  //c_misc::clear_bitmap (&img_overlay, 255, 255, 255, 32);
  
#ifdef SHOW_CUSTOMWIDGET_TEST_STUFF
  
  /*wxBitmap img_line1 = c_misc::render_text_label ("Custom widget",
                                                  smallboldfont,
                                                  col_fg,
                                                  col_bg,
                                                  width,
                                                  height,
                                                  LABEL_ALIGN_LEFT,
                                                  gradwidth,
                                                  0);
  wxBitmap img_line2 = c_misc::render_text_label (get_example_label_text (),
                                                  font,
                                                  col_fg,
                                                  col_bg,
                                                  width,
                                                  height,
                                                  LABEL_ALIGN_LEFT,
                                                  gradwidth,
                                                  0);

  dc.SelectObject (base_image);
  dc.DrawBitmap (img_line1, (width / 2) - (img_line1.GetWidth () / 2), 0);
  dc.DrawBitmap (img_line2, (width / 2) - (img_line2.GetWidth () / 2), 
                            (height / 2) - (img_line1.GetHeight () / 2));*/
  
  /* now the lines around the border*/
  dc.SelectObject (base_image);
  dc.SetPen (wxPen (col_fg));
  dc.DrawLine (0, 0, width, 0);
  dc.DrawLine (0, 0, 0, height);
  dc.DrawLine (0, height - 1, width - 1, height - 1);
  dc.DrawLine (width - 1, 0, width - 1, height - 1);
  dc.SelectObject (wxNullBitmap);
  
#endif
  
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
  
  GetSize (&new_width, &new_height);
  
  if (!base_image_valid || new_width != width || new_height != height) {
    render_base_image ();
    img_overlay = wxBitmap (width, height, 32);
  }
  
  // ??? keeping this for reference
  //wxGraphicsContext *gc = wxGraphicsContext::Create (dc);
  
  dc.DrawBitmap (base_image, 0, 0);
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

void c_meterwidget::render_base_image () {
  //debug ("start");
  c_customwidget::render_base_image ();
  wxMemoryDC dc; 
  dc.SelectObject (base_image);
  
  GetSize (&width, &height);
  if (vertical) {
    clip_width = width;
    clip_height = 3;
  } else {
    int fontsize = (width / 125);
    tinyfont.SetPointSize (fontsize);
    tinyfont.MakeBold ();
    dc.SetFont (tinyfont);
    wxSize fsz_clip = dc.GetTextExtent ("CLIP");
    wxSize fsz_xrun = dc.GetTextExtent ("XRUN");
    clip_width = std::max (fsz_clip.GetWidth (), fsz_xrun.GetWidth ()) * 1.5;
    clip_height = std::max (fsz_xrun.GetHeight (), fsz_xrun.GetHeight ());
  }
  
  // keep a bit of space for clip/xrun indicator
  int len, th, w, h, w2, h2;
  if (vertical) {
    len = height - clip_height;
    th = height;// - 1;
    h2 = height - 1;
    w2 = width - 1;
  } else {
    len = width - clip_width;
    th = width;// - 1;
    h2 = height - 1;
    w2 = width - 1;
  }
  
  if (stereo) {
    h2 = height / 2;
    w2 = width / 2;
  }
  
  dc.SetBrush (wxBrush (*wxBLACK));
  if (vertical)
    dc.DrawRectangle (0, 0, th, len);
  else
    dc.DrawRectangle (0, 0, len, th);
  
  // draw grid
  if (vertical) {
    dc.SetPen (wxPen (col_default_bg));
    dc.DrawLine (0, th - 1, w, th -1);               // (0, 0, 0, h); //left
    dc.DrawLine (w2, 0, w2, len);                    // (0, h2, len, h2); // bottom
    dc.SetPen (wxPen (*wxGREEN));
    dc.DrawLine (0, len / 2, th, len / 2);           //(len / 2, 0, len / 2, th); // middle marker
    dc.DrawLine (0, len - (len * 3 / 4), th, len - (len * 3 / 4));  //(len * 3 / 4, 0, len * 3 / 4, th); // 3/4 marker
    dc.SetPen (wxPen (*wxYELLOW));
    dc.DrawLine (0, len - (len * 7 / 8), th, len - (len * 7 / 8)); //(len * 7 / 8, 0, len * 7 / 8, th); //yellow marker
    //dc.SetPen (wxPen (wxColour (255,128,0)));
    //dc.DrawLine (len * 15 / 16, 0, len * 15 / 16, h);
    dc.SetPen (wxPen (*wxRED));
    dc.DrawLine (0, 0, th, 0); // right (red marker)
  } else {
    dc.SetPen (wxPen (col_default_bg));
    dc.DrawLine (0, 0, 0, th);
    dc.DrawLine (0, h2, len, h2);
    dc.SetPen (wxPen (*wxGREEN));
    dc.DrawLine (len / 2, 0, len / 2, th);
    dc.DrawLine (len * 3 / 4, 0, len * 3 / 4, th);
    dc.SetPen (wxPen (*wxYELLOW));
    dc.DrawLine (len * 7 / 8, 0, len * 7 / 8, th);
    //dc.SetPen (wxPen (wxColour (255,128,0)));
    //dc.DrawLine (len * 15 / 16, 0, len * 15 / 16, h);
    dc.SetPen (wxPen (*wxRED));
    dc.DrawLine (len, 0, len, th);
  }
  
  // also render gradient bar
  { // shadow variable names above
    img_bar = wxBitmap (width, height, 32);
    int w = img_bar.GetWidth ();
    int h = img_bar.GetHeight ();
    int r = (vertical ? h : w) - (vertical ? clip_height : clip_width);
    int mid = r * 2 / 3;
    //if (vertical) mid = h - mid;
    int th = vertical ? img_bar.GetWidth () : img_bar.GetHeight ();
    int x, y;
    // render bar gradient
    wxAlphaPixelData pdata (img_bar);
    wxAlphaPixelData::Iterator p (pdata);
    p.MoveTo (pdata, 0, 0);
    
    if (vertical) {
      for (y = 0; y < h; ++y) {
        wxAlphaPixelData::Iterator rowstart = p;
        for (x = 0; x < th; ++x, ++p) {
          //a = ((float) p.Alpha ()) * alpha_factor;
          // TODO: sort out alpha premultiply stuff
          if (h-y < mid) {
            p.Red () = ((h-y) * 255) / mid;
            p.Green () = 255;
            p.Blue () = 0;
            p.Alpha () = 255;
          } else if (h-y < r) {
            p.Red () = 255;
            float t = float ((h-y) - mid) / float (mid - h);
            p.Green () = 255.0 * (t);
            p.Blue () = 0;
            p.Alpha () = 255;
          } else {
            p.Alpha () = 0;
          }
        }
        p = rowstart;
        p.OffsetY (pdata, 1);
      }
    } else {
      for (y = 0; y < th; ++y) {
        wxAlphaPixelData::Iterator rowstart = p;
        for (x = 0; x < w; ++x, ++p) {
          //a = ((float) p.Alpha ()) * alpha_factor;
          // TODO: sort out alpha premultiply stuff
          if (x < mid) {
            p.Red () = (x * 255) / mid;
            p.Green () = 255;
            p.Blue () = 0;
            p.Alpha () = 255;
          } else if (x < r) {
            p.Red () = 255;
            float t = float (x - mid) / float (w - mid);
            p.Green () = 255.0 * (1.0 - t);
            //p.Green () = (255 * (x + 1 - mid));
            p.Blue () = 0;
            p.Alpha () = 255;
          } else {
            p.Alpha () = 0;
          }
        }
        p = rowstart;
        p.OffsetY (pdata, 1);
      }
    }
  }
  //dc.DrawBitmap (img_bar, 0, 0);
  dc.SelectObject (wxNullBitmap);
  //debug ("end");
}

void c_meterwidget::draw_bar (wxDC &dc, int t, int o, bool is_r,
                                float level, float hold, bool clip, bool xrun) {
  //dc.Clear ();
  dc.SetPen (wxPen (wxColour ()));
  dc.SetBrush (wxBrush (wxColour (255, 0, 0)));
  
  int m; // usable meter length
  if (level > 1) level = 1;
  if (vertical)
    m = height - clip_height;
  else
    m = width - clip_width;
  int len = m * level;
  if (len > 0) {
    int ulim = m - 3;
    if (len > ulim) len = ulim;
    /*debug ("%s o=%d, m=%d, len=%d, width=%d, height=%d",
           vertical ? "ver" : "hor", o, m, len, width, height);*/
    if (vertical) {
      img_bar_sub = img_bar.GetSubBitmap (wxRect (t, m - len, o, len));
      dc.DrawBitmap (img_bar_sub, t, m - len - 2);
      //dc.SetBrush (wxBrush (*wxGREEN));
      //dc.DrawRectangle (t, m - len, o, len);
    } else {
      img_bar_sub = img_bar.GetSubBitmap (wxRect (0, 0, len, o));
      dc.DrawBitmap (img_bar_sub, 2, t);
    }
  }
  int holdpos = m * hold;
  if (holdpos > 0 && holdpos < m) {
    dc.SetPen (wxColour (192, 192, 128));
    if (vertical)
      dc.DrawLine (t, m - holdpos, t + o, m - holdpos);
    else
      dc.DrawLine (holdpos, t, holdpos, t + o - 1);
  }
}

void c_meterwidget::update (wxWindowDC &dc) {
  //wxMemoryDC dc;
  ////if (!img_overlay.IsOk ()) { CP; return; }
  //dc.SelectObject (img_overlay);
  
  if (stereo != last_stereo) {
    last_stereo = stereo;
    base_image_valid = false;
  }
  c_customwidget::update (dc);
  dc.SetPen (wxPen (*wxRED));
  dc.SetBrush (wxBrush (*wxRED));
  //debug ("lev %f,%f  hold %f,%f  clip %s,%s  xrun %s,%s",
  //       l, r, hold_l, hold_r,
  //       clip_l ? "t" : "f", clip_r ? "t" : "f",
  //       xrun_l ? "t" : "f", xrun_r ? "t" : "f");
 
  float t, h;
  bool clipany = clip_l||clip_r;
  
  if (stereo) {
    if (vertical) {
      t = width / 8;
      if (t < 1) t = 1;
      h = width / 4;
      if (h < 1) h = 1;
    } else {
      t = height / 8;
      if (t < 1) t = 1;
      h = height / 4;
      if (h < 1) h = 1;
    }
    
    draw_bar (dc, t, h, false, l, hold_l, clipany, xrun);
    
    if (vertical)
      t = width - h - t;
    else
      t = height - h - t;
    draw_bar (dc, t, h, true,  r, hold_r, clipany, xrun);
  } else {
    if (vertical) {
      t = width / 4;
      h = width - (t * 2) - 1;
    } else {
      t = height / 4;
      h = height - (t * 2) - 1;
    }
    
    if (height >= 0) height = 1;
    //if (t < 0 || t > height - 1) t = 0;
    //draw_bar (dc, t, h, false, l, hold_l, clip_l, xrun_l);
    draw_bar (dc, t, h, false, l, hold_l, clip_l, xrun);
  }
  //debug ("w/h %d,%d", width, height);
  int clipx, clipy;
  if (vertical) {
    clipx = ((width - clip_width) / 2) - 2;
    clipy = height - clip_height + 2;
  } else {
    clipx = width - clip_width + 2;
    clipy = ((height - clip_height) / 2) - 2;
  }
  if (clipany || xrun) {
    if (vertical) {
      dc.SetPen (wxPen (*wxRED));
      dc.SetBrush (wxBrush (*wxRED));
      dc.DrawRectangle (0, 0, clip_width, clip_height);
    } else {
      dc.SetTextForeground (*wxRED);
      dc.DrawText (xrun? "XRUN" : "CLIP", clipx, clipy);
    }
  } else {
    dc.SetPen (wxPen ());
    dc.SetBrush (wxBrush (col_default_bg));
    dc.DrawRectangle (clipx, clipy, clip_width, clip_height);
    
  }
}


void c_meterwidget::set_l (float level, float hold, bool clip, bool xr) {
  //debug ("lev:%f, hold=%f, clip=%s, xrun=%s", level, hold,
  //       clip ? "true" : "false", xrun ? "true": "false");
  bool needs_update = false;
  if (l != level) needs_update = true;
  l = level;
  hold_l = hold;
  clip_l = clip;
  xrun = xr;
  if (needs_update) Refresh ();
}

void c_meterwidget::set_r (float level, float hold, bool clip, bool xr) {
  //debug ("lev:%f, hold=%f, clip=%s, xrun=%s", level, hold,
  //       clip ? "true" : "false", xrun ? "true": "false");
  bool needs_update = false;
  if (r != level) needs_update = true;
  r = level;
  hold_r = hold;
  clip_r = clip;
  xrun = xr;
  if (needs_update) Refresh ();
}

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
  dc.SelectObject (base_image);
  
  if (0&&!wavdata) {
    dc.SetPen (wxPen (wxTransparentColor));
    dc.SetBrush (wxBrush (col_default_bg));
    dc.DrawRectangle (0, 0, width, height);
    dc.SetPen (col_default_fg);
    dc.SetTextForeground (col_default_fg);
        
    wxFont font = GetFont ();
    std::string msg = "No impulse response file has been generated.";
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
}


#endif // USE_WXWIDGETS

