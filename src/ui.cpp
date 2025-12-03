
#include "dirt.h"
#include "ui.h"
#include "deconvolv.h"

#ifdef DEBUG
#define CMDLINE_DEBUG
#include "cmdline/cmdline.h"
#define debug(...) cmdline_debug(stderr,ANSI_YELLOW,__FILE__,__LINE__,__FUNC__,__VA_ARGS__)
#else
#define debug(...)
#define CP
#define BP
#endif

// c_mainwindow

wxBEGIN_EVENT_TABLE (c_mainwindow, ui_mainwindow)
  //EVT_CLOSE (c_mainwindow::on_close)
  //EVT_SIZE (c_mainwindow::on_resize)
  EVT_BUTTON (wxID_ABOUT, c_mainwindow::on_about)
  EVT_BUTTON (wxID_OK, c_mainwindow::on_ok)
  EVT_BUTTON (wxID_CANCEL, c_mainwindow::on_cancel)
  EVT_RADIOBUTTON (ID_FILE, c_mainwindow::on_radio_file)
  EVT_RADIOBUTTON (ID_MAKESWEEP, c_mainwindow::on_radio_makesweep)
  EVT_RADIOBUTTON (ID_ROUNDTRIP, c_mainwindow::on_radio_roundtrip)
  EVT_RADIOBUTTON (ID_PLAYSWEEP, c_mainwindow::on_radio_playsweep)
  EVT_CHECKBOX (wxID_ANY, c_mainwindow::on_checkbox)
wxEND_EVENT_TABLE ();


c_mainwindow::c_mainwindow (c_deconvolver *d)
: ui_mainwindow (NULL, wxID_ANY) {
  dec = d;
  if (!dec) {
    debug ("No deconvolver given!");
  }
  
  list_backend->Append ("JACK");  
  list_backend->SetSelection (0);
  if (d && d->prefs_)
    set_prefs (d->prefs_);
  CP
  set_mode (ID_FILE);
}

c_mainwindow::~c_mainwindow () {
  CP
}

bool c_mainwindow::init_audio () {
  if (init_audio_done)
    return true;
  CP
  
  if (!dec) {
    debug ("no deconvolver given");
    return false;
  }
  
  if (!dec->audio) {
    debug ("deconvolver has no audio client");
  }
  
  int sr = ::atoi (text_samplerate->GetValue ().c_str ());
  bool st = !chk_forcemono->GetValue ();
  
  std::string jack_name = "asdf";
  if (dec->prefs_) jack_name = dec->prefs_->jack_name;
  std::cout << "name: " << jack_name << "\n";
  CP
  if (!dec->audio->init (jack_name, sr, st)) {
    CP
    debug ("audio init failed");
  }
  
  std::vector<std::string> ports_list;
  
  init_audio_done = true;
  return true;
}

void c_mainwindow::on_ok (wxCommandEvent &ev) {
  CP
}

void c_mainwindow::on_resize (wxSizeEvent &ev) {
  CP
}

void c_mainwindow::on_close (wxCloseEvent &ev) {
  CP
  Close ();
}

void c_mainwindow::on_cancel (wxCommandEvent &ev) {
  CP
  Close ();
}

void c_mainwindow::on_about (wxCommandEvent &ev) {
  CP
  wxMessageBox ("dirt");
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


void c_mainwindow::set_prefs (s_prefs *prefs) {
  spin_dry_length->SetValue (prefs->sweep_seconds);
  spin_dry_f1->SetValue (prefs->sweep_f1);
  spin_dry_f2->SetValue (prefs->sweep_f2);
  spin_dry_preroll->SetValue (prefs->preroll_seconds * 1000);
  spin_dry_marker->SetValue (prefs->marker_seconds * 1000);
  spin_dry_gap->SetValue (prefs->marker_gap_seconds * 1000);
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
}

void c_mainwindow::set_mode (long int mode) {
  struct widget_status *wl;
  
  static struct widget_status widget_status_file [] = {
    // dry sweep tab
    { ID_BACKEND,              false  },
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
    { ID_IR_THR,               true  },
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
    { ID_IR_THR,               true  },
    { ID_CHN_OFFSET,           true  },
    { ID_PROCESS,              true  },
    { -1,                      false }
  },
  
  widget_status_roundtrip [] = {
    // dry sweep tab
    { ID_BACKEND,              true  },
    { ID_FORCEMONO,            true  },
    { ID_SAMPLERATE,           false }, // TODO: might change with other backends
    { ID_DRYFILE,              false },
    { ID_DRYFILE_BROWSE,       false },
    { ID_DRY_LENGTH,           false },
    { ID_DRY_F1,               false },
    { ID_DRY_F2,               false },
    { ID_DRY_PREROLL,          false },
    { ID_DRY_MARKER,           false },
    { ID_DRY_GAP,              false },
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
    { ID_IR_THR,               false },
    { ID_CHN_OFFSET,           false },
    { ID_PROCESS,              false },
    { -1,                      false }
  };
  
  //bool b = chk_forcemono->GetValue();
  //set_enable (list_jack_wet_r, !b);
  //set_enable (spin_chn_offset, !b);
  
  if (mode == ID_ROUNDTRIP) btn_play->SetLabel ("Process");
  else                      btn_play->SetLabel ("Play");
  
  switch (mode) {
    case ID_FILE:
      wl = widget_status_file;
    break;
    
    case ID_MAKESWEEP:
      wl = widget_status_makesweep;
      init_audio ();
    break;
    
    case ID_ROUNDTRIP:
      wl = widget_status_roundtrip;
      init_audio ();
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
  
  if (chk_forcemono->GetValue()) {
    set_enable (list_jack_wet_r, false);
    set_enable (spin_chn_offset, false);
  }
}

void c_mainwindow::set_enable (wxWindow *w, bool b) {
  w->Enable (b);
  w->SetTransparent (b ? 255 : 128);
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
  //wxMessageBox("OnInit!");
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
