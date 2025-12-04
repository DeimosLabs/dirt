
#ifdef USE_WXWIDGETS

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
  EVT_CHECKBOX (wxID_ANY, c_mainwindow::on_checkbox)

  EVT_BUTTON (ID_DRYFILE_BROWSE, c_mainwindow::on_btn_dryfile_browse)
  EVT_BUTTON (ID_GENERATE, c_mainwindow::on_btn_generate)
  EVT_BUTTON (ID_DRY_SAVE, c_mainwindow::on_btn_dry_save)
  EVT_BUTTON (ID_INPUTDIR_SCAN, c_mainwindow::on_btn_inputdir_scan)
  EVT_BUTTON (ID_INPUTDIR_BROWSE, c_mainwindow::on_btn_inputdir_browse)
  EVT_BUTTON (ID_INPUTFILES_ADD, c_mainwindow::on_btn_inputfiles_add)
  EVT_BUTTON (ID_INPUTFILES_CLEAR, c_mainwindow::on_btn_inputfiles_clear)
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
  list_align->Append ("None/manually aligned");
  
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
}

c_mainwindow::~c_mainwindow () {
  CP
}

void c_mainwindow::on_timer (wxTimerEvent &ev) {
  if (!dec || !dec->audio) return;
  
  bool do_full_update = false;
  int s = dec->audio->state;
  
  if (s != prev_audio_state) {
    prev_audio_state = s;
    do_full_update = true;
  }
  if (mode != prev_mode) {
    prev_mode = mode;
    do_full_update = true;
  }
  
  if (s == ST_NOTREADY) {
    disable (btn_play);
    btn_audio->SetLabel ("Connect");
  } else if (s == ST_IDLE) {
    if (!dec->has_dry ()) { CP
      disable (btn_play);
    } else { CP
      enable (btn_play);
    }
    btn_play->SetLabel ((mode == ID_ROUNDTRIP) ? "Process" : "Play");
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
  if (dec->audio->state == ST_NOTREADY) {
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
  
  init_audio_done = true;
  
  debug ("end");
  return true;
}

bool c_mainwindow::shutdown_audio () { CP
  if (!dec || !dec->audio) return false;
  if (dec->audio->state == ST_NOTREADY) return false;
  
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

void c_mainwindow::on_checkbox (wxCommandEvent &ev) {
  if (radio_file->GetValue ()) set_mode (ID_FILE);
  else if (radio_makesweep->GetValue ()) set_mode (ID_MAKESWEEP);
  else if (radio_roundtrip->GetValue ()) set_mode (ID_ROUNDTRIP);
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
  if (!audio_ready () || dec->audio->backend != driver_jack) {
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
  spin_dry_f1->SetValue (prefs->sweep_f1);
  spin_dry_f2->SetValue (prefs->sweep_f2);
  spin_dry_preroll->SetValue (prefs->preroll_seconds * 1000.0);
  spin_dry_marker->SetValue (prefs->marker_seconds * 1000.0);
  spin_dry_gap->SetValue (prefs->marker_gap_seconds * 1000.0);
  list_align->SetSelection (prefs->align);
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
    { ID_GENERATE,             true },
    { ID_DRY_SAVE,             true },
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
    { ID_OVERWRITE,            true  },
    { ID_DEBUG,                true  },
    { ID_SWEEP_THR,            true  },
    { ID_IR_START_THR,         true },
    { ID_IR_END_THR,           true },
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
  wxFileDialog f (this, "Save sweep as .wav file",
                  "", "", "*.[Ww][Aa][Vv]", wxFD_SAVE);
  int ret = f.ShowModal ();
  if (ret != wxID_CANCEL) {
    CP
    if (!make_dry_sweep ()) {
      show_error ("Deconvolver failed to load our dry sweep\n");
    }
      
    std::string dest = std::string (f.GetPath ());
  }
}

void c_mainwindow::on_btn_inputdir_scan (wxCommandEvent &ev) {
  CP
}

void c_mainwindow::on_btn_inputdir_browse (wxCommandEvent &ev) {
  CP
}

void c_mainwindow::on_btn_inputfiles_add (wxCommandEvent &ev) {
  CP
}

void c_mainwindow::on_btn_inputfiles_clear (wxCommandEvent &ev) {
  CP
}

void c_mainwindow::on_btn_audio (wxCommandEvent &ev) { CP
  if (!dec || !dec->audio) return;
  
  if (dec->audio->state == ST_NOTREADY) {
    if (init_audio ()) {
      update_audio_ports ();
    } else {
      show_error ("Failed to initialize " + 
                    g_backend_names [dec->audio->backend] +
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
  if (dec->audio->state == ST_IDLE)
    dec->audio->play (dec->dry_, false);
  else
    dec->audio->stop ();
}

void c_mainwindow::on_btn_process (wxCommandEvent &ev) {
  CP
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

#endif // USE_WXWIDGETS
