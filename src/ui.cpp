
/* DIRT - Delt's Impulse Response Tool
 * Version 0.1
 *
 * Licensed under the GPL. See dirt.h and dirt.cpp for more info.
 */


#ifdef USE_WXWIDGETS

#include <optional>
#include <wx/dcbuffer.h>
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

static std::string basename (std::string name) {
  std::string ret;
  int a = name.find_last_of ("/");
  if (a >= 0 && a < name.size ())
    ret = name.substr (a + 1, name.size () - a);
  else
    ret = name;
  
  return ret;
}

std::string dirname (std::string name) {
  std::string ret;
  int a = name.find_last_of ("/");
  if (a >= 0 && a < name.size ())
    ret = name.substr (0, a);
  else
    ret = name;
  
  return ret;
}

std::string remove_suffix (std::string name) {
  std::string ret;
  int a = name.find_last_of (".");
  if (a >= 1 && a < name.size () - 1)
    ret = name.substr (0, a);
  else
    ret = name;
  
  return ret;
}

int main_test_string_shenanigans () {
  std::string s = "abc/def/gh.wav";
  debug ("%s basename %s", s.c_str(), basename (s).c_str ());
  debug ("%s dirname  %s", s.c_str(),  dirname (s).c_str ());
  debug ("%s remove suffix  %s", s.c_str(),  remove_suffix (s).c_str ());
  return 0;
}

c_ir_entry::c_ir_entry () { id = get_unique_id (); }

size_t c_ir_entry:: size () {
  size_t ls = l.size ();
  size_t rs = r.size ();
  
  if (ls && rs)
    return std::min (l.size (), r.size ());  
  else
    return std::max (l.size (), r.size ());  
}

// helper for wxDC clip/origin shenanigans
/*struct dc_scope {
  wxDC &dc;
  wxPoint origin;
  dc_scope (wxDC &d, const wxRect &r) : dc (d), origin (d.GetDeviceOrigin ()) {
    debug ("rect %d,%d,%d,%d", r.x, r.y, r.width, r.height);
    //dc.DestroyClippingRegion ();
    dc.SetClippingRegion (r.x, r.y, r.width, r.height);
    dc.SetLogicalOrigin (origin.x + r.x, origin.y + r.y);
  }
  ~dc_scope () {
    debug ("restoring origin: %d,%d", origin.x, origin.y);
    dc.SetLogicalOrigin(origin.x, origin.y);
    dc.DestroyClippingRegion ();
  }
};*/

// (new version)
struct dc_scope {
  wxDC& dc;
  wxPoint old_logical;
  std::optional<wxDCClipper> clip;

  dc_scope(wxDC& d, const wxRect& r)
    : dc(d), old_logical(d.GetLogicalOrigin())
  {
    // shift logical origin so child draws in local coords
    dc.SetLogicalOrigin(old_logical.x - r.x, old_logical.y - r.y);
    dc.DestroyClippingRegion ();
    dc.SetClippingRegion (0, 0, r.width, r.height);

    // clip in parent coords (or make it local; see below)
    //clip.emplace(dc, r);
  }

  ~dc_scope() {
    // clip restores automatically
    dc.DestroyClippingRegion ();
    dc.SetLogicalOrigin(old_logical.x, old_logical.y);
  }
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
  EVT_BUTTON (ID_AUDIO, c_mainwindow::on_btn_audio)
  EVT_BUTTON (ID_PLAY, c_mainwindow::on_btn_play)
  EVT_COMBOBOX (ID_JACK_DRY, c_mainwindow::on_port_select)
  EVT_COMBOBOX (ID_JACK_WET_L, c_mainwindow::on_port_select)
  EVT_COMBOBOX (ID_JACK_WET_R, c_mainwindow::on_port_select)
  
  EVT_BUTTON (ID_INPUTDIR_BROWSE, c_mainwindow::on_btn_inputdir_browse)
  EVT_BUTTON (ID_INPUTDIR_SCAN, c_mainwindow::on_btn_inputdir_scan)
  EVT_BUTTON (ID_INPUTFILES_ADD, c_mainwindow::on_btn_inputfiles_add)
  EVT_BUTTON (ID_INPUTFILES_CLEAR, c_mainwindow::on_btn_inputfiles_clear)
  EVT_BUTTON (ID_OUTPUTDIR_BROWSE, c_mainwindow::on_btn_outputdir_browse)
  EVT_BUTTON (ID_ALIGN_MANUAL, c_mainwindow::on_btn_align_manual)
  EVT_BUTTON (ID_PROCESS, c_mainwindow::on_btn_process)
  
  EVT_BUTTON (ID_IR_RENAME, c_mainwindow::on_btn_irrename)
  EVT_BUTTON (ID_IR_REMOVE, c_mainwindow::on_btn_irremove)
  EVT_BUTTON (ID_IR_LOAD, c_mainwindow::on_btn_irload)
  EVT_BUTTON (ID_IR_SAVE, c_mainwindow::on_btn_irsave)
  
  EVT_BUTTON (ID_ZOOMFULL, c_mainwindow::on_btn_zoomfull)
  EVT_LIST_ITEM_SELECTED (ID_IRFILES, c_mainwindow::on_irfile_select)
  EVT_LIST_ITEM_DESELECTED (ID_IRFILES, c_mainwindow::on_irfile_unselect)

wxEND_EVENT_TABLE ();

c_mainwindow::c_mainwindow (c_deconvolver *d)
: ui_mainwindow (NULL, wxID_ANY) {
  dec = d;
  if (!dec) {
    debug ("No deconvolver given!");
  }
  
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
  
  list_hpf_mode->Append ("Off");
  list_hpf_mode->Append ("Normal");
  list_lpf_mode->Append ("Off");
  list_lpf_mode->Append ("Normal");
  
  if (d && d->prefs)
    set_prefs (d->prefs);
  CP
  
  Layout ();
  Fit ();
  wxSize sz = GetClientSize ();
  SetSize (sz + wxSize (0, 100));
  SetSizeHints (sz);
  
  text_log->SetBackgroundColour (*wxBLACK);
  
  set_mode (ID_FILE);
  
  init_audio ();
  timer.Bind (wxEVT_TIMER, &c_mainwindow::on_timer, this);
  timer.Start (32);
}

c_mainwindow::~c_mainwindow () {
  CP
}

void c_mainwindow::on_btn_zoomfull (wxCommandEvent &ev) {
  if (!pn_waveform) return;
  
  pn_waveform->zoom_full_x ();
  pn_waveform->zoom_full_y ();
}

bool c_mainwindow::add_ir (c_ir_entry &ent) { CP
  debug ("got IR entry '%s' size l=%d,r=%d, %dHz, %s", 
         ent.name.c_str (), ent.l.size (), ent.r.size (), ent.l.get_samplerate (),
         ent.dirty ? "dirty" : "not dirty");
         
  if (ent.name.size () <= 0)
    ent.name = readable_timestamp (epoch_sec ());
  if (ent.l.size () > 0) {
    //c_ir_entry newir;
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

void c_mainwindow::on_irfile_select (wxListEvent &ev) { CP
  int64_t id =  list_irfiles->get_selected_id ();
  CP
  debug ("got selected ID %ld", id);
  int n = ir_files.size ();
  CP
  for (int i = 0; i < n; i++)
    if (ir_files [i].id == id) {
      debug ("IR found");
      CP
      pn_waveform->select_ir (&ir_files [i]);
      CP
      return;
    }
  debug ("no IR found");
}

void c_mainwindow::on_irfile_unselect (wxListEvent &ev) {
  CP
  pn_waveform->unselect_ir ();
}

void c_mainwindow::on_recording_done () { CP
  c_ir_entry ir;
  
  if (!dec->set_sweep_wet (wetsweep_l, wetsweep_r)) {
    show_error ("Deconvolver failed to load wet sweep");
  } else if (!dec->render_ir (ir.l, ir.r)) {
    show_error ("Deconvolver failed to render IR");
  } else {
    debug ("done rendering IR, yayyy");
    ir.timestamp = epoch_sec ();
    ir.dirty = true;
    ir.loaded = true;
    add_ir (ir);
  }
}

void c_mainwindow::on_timer (wxTimerEvent &ev) {
  //static int64_t num_passes = 0;
  const int update_widgets_every = 8;
  bool do_full_update = false;
  char buf [256];
  
  //debug ("drysweep: %ld, wetsweep: %ld,%ld",
  //       drysweep.size (), wetsweep_l.size (), wetsweep_r.size ());
  //num_passes++;
  g_app->frame_counter++;
  if (!dec || !audio) return;
  
  audiostate s = audio->state;
  
  if (s != prev_audio_state) {
    debug ("prev_audio_state=%d, s=%d", (int) prev_audio_state, (int) s);
    do_full_update = true;
    
    if (prev_audio_state == audiostate::PLAYREC &&
        s == audiostate::REC) {
      audio->stop ();
    } else if ((prev_audio_state == audiostate::PLAYREC ||
         prev_audio_state == audiostate::REC) &&
        (s == audiostate::IDLE || s == audiostate::MONITOR)) {
      
      on_recording_done ();
    }
    prev_audio_state = s;
  }
  
  pn_meter_out->on_ui_timer ();
  pn_meter_in->on_ui_timer ();
  pn_waveform->on_ui_timer ();
  
  // widget stuff: update only every n passes
  //if (num_passes % update_widgets_every == 0) {
  if (g_app->frame_counter % update_widgets_every == 0) {
    if (mode != prev_mode) {
      prev_mode = mode;
      do_full_update = true;
    } //else {CP}
    
    if (dec->has_dry ()) {
      enable (radio_roundtrip);
    } else {
      disable (radio_roundtrip);
      if (mode == ID_ROUNDTRIP)
        set_mode (ID_GENERATE);
    }
    
    if (s == audiostate::NOTREADY) {    // audio offline
      disable (btn_play);
      btn_audio->SetLabel ("Connect");
      init_audio_done = false;
    } else if (s == audiostate::IDLE || 
               s == audiostate::MONITOR) { // on, not playing
      if (!dec->has_dry ()) {           // ready to play
        disable (btn_play);
      } else {
        //debug ("state %d", audio->state);
        enable (btn_play);
      }
      btn_play->SetLabel ((mode == ID_ROUNDTRIP) ? "Record" : "Play");
      btn_audio->SetLabel ("Disconnect");
      pn_meter_in->rec_enabled = false;
      pn_meter_out->rec_enabled = false;
    } else if (s == audiostate::PLAY ||
               s == audiostate::PLAYMONITOR ||
               s == audiostate::PLAYREC ||
               s == audiostate::REC) {   // something's playing/recording
      if (!dec->has_dry ()) {              // uhhh wut
        debug ("audio: playing something but what?");
        disable (btn_play);
      } else {                               // playing dry sweep
        enable (btn_play);
        int sr = dec->prefs ? dec->prefs->sweep_sr : 
                               atoi (comb_samplerate->GetValue ().c_str ());
        size_t sec_left = audio->get_play_remaining () / (size_t) sr;
        
        snprintf (buf, 127, "Stop (%ld)", sec_left);
        
        //debug ("sr=%d, sec_left=%d", sr, sec_left);
        btn_play->SetLabel (buf);
        if (sr == (int) audiostate::PLAYREC ||
            sr == (int) audiostate::REC)
          pn_meter_in->rec_enabled = true;
        else
          pn_meter_in->rec_enabled = false;
      }
      btn_audio->SetLabel ("Disconnect");
    } else {
      debug ("audio: no idea what's going on");
    }
    
    int numfiles = list_inputfiles->GetCount ();
    
    //std::cout << "selected tab: " << note_tabs->GetSelection () << "\n";
    
    if (note_tabs->GetSelection () == 2) {
      
      c_ir_entry *ir = pn_waveform->get_selected ();
      
      if (!ir) {
        set_statustext ("No file selected.");
      } else {
        char buf [256];
        snprintf (buf, 255, "%ld samples, %s.",
                   ir->size (), (ir->r.size () ? "stereo" : "mono"));
        // TODO: figure out wtf is wrong with this (segfault if incl. name):
        //snprintf (buf, 255, "%s: %ld samples, %s.", ir->name.c_str (),
        //           ir->size (), (ir->r.size () ? "stereo" : "mono"));
                
        
        set_statustext (wxString (buf));
        
      } 
    } else if (filedir_scanning) { 
      btn_inputdir_scan->SetLabel ("Cancel");
      set_statustext ("Scanning...");
    } else if (num_files_ok || num_files_error) { 
      snprintf (buf, 255, "Files processed: %d, errors: %d",
                num_files_ok, num_files_error);
                
      set_statustext (std::string (buf));
    } else {
      btn_inputdir_scan->SetLabel ("Scan");
      if (drysweep.size () == 0) {
        set_statustext ("Ready: Please load or generate a dry sweep");
      } else if (list_inputfiles->GetCount () == 0) {
        set_statustext ("Ready: Please load or record wet sweep(s) to process");
      } else if (!busy) { 
        snprintf (buf, 255, "Ready to process %d %s", 
                  numfiles, numfiles == 1 ? "file" : "files"); 
        set_statustext (std::string (buf));
      } else {
        set_statustext ("Deconvolving...");
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
    
    if (list_inputfiles->GetCount () > 0 && dec->has_dry ())
      enable (btn_process);
    else
      disable (btn_process);
    
    if (chk_autosave->GetValue ()) {
      enable (chk_overwrite);
    } else {
      disable (chk_overwrite);
    }
    
    if (list_hpf_mode->GetSelection () == 0)
      disable (spin_hpf_freq);
    else
      enable (spin_hpf_freq);
    if (list_lpf_mode->GetSelection () == 0)
      disable (spin_lpf_freq);
    else
      enable (spin_lpf_freq);
    
  
    int n = ir_files.size ();
    if (n != last_ir_count) {
      debug ("updating ir list (have %d, last count %d)", n, last_ir_count);
      last_ir_count = ir_files.size ();
      update_ir_list ();
    } else {
      //debug ("ir list up to date, (%d vs %d)", n, last_ir_count);
    }
  }
  if (do_full_update)
    set_mode (mode);
}

void c_mainwindow::set_statustext (const wxString str) {
  text_statusbar->SetLabel (str);
}

bool c_mainwindow::is_ready () {
  init_audio_done = false;
  
  if (!dec) {
    debug ("no deconvolver");
    return false;
  }
  if (!audio) {
    debug ("deconvolver has no audio client");
    return false;
  }
  if (audio->state == audiostate::IDLE) {
    debug ("deconvolver idle, arming record (monitor mode)");
    audio->arm_record ();
  }
  if (audio->state == audiostate::NOTREADY ||
      audio->state == audiostate::IDLE) {
    debug ("deconvolver audio not ready");
    return false;
  }
  
  init_audio_done = true;
  return true;
}

bool c_mainwindow::init_audio (int samplerate, bool stereo) { CP
  if (init_audio_done)
    return true;
  
  if (!dec)
    return false;
  
  debug ("start, samplerate=%d, stereo=%s", samplerate, stereo ? "true" : "false");
  
  audio = g_app->audio;
  if (!audio) {
    audio = new c_jackclient (dec->prefs);
  }

  pn_meter_in->set_vudata (&audio->vu_in);
  pn_meter_out->set_vudata (&audio->vu_out);
  audio->init ("DIRT", -1, stereo);
  
  if (!dec) {
    debug ("no deconvolver given");
    return false;
  }
  
  if (!audio) {
    debug ("deconvolver has no audio client");
    return false;
  }
  
  int sr = ::atoi (comb_samplerate->GetValue ().c_str ());
  bool st = !chk_forcemono->GetValue ();
  
  std::string jack_name = "";
  if (dec->prefs) jack_name = dec->prefs->jack_name;
  std::cout << "name: " << jack_name << "\n";
  CP
  audio->unregister ();
  if (!audio->register_output (false) ||
      !audio->register_input (dec->prefs->request_stereo)) {
    CP
    debug ("audio init failed");
    return false;
  }
  
  CP
  audio->arm_record ();
  init_audio_done = true;
  
  debug ("end");
  return true;
}

bool c_mainwindow::init_audio () {
  return init_audio (atoi (comb_samplerate->GetValue ().c_str ()), 
                      !chk_forcemono->GetValue ());
}

bool c_mainwindow::disable_audio () { CP
  if (!dec || !audio) return false;
  if (audio->state == audiostate::NOTREADY) return false;
  
  list_jack_dry->Clear ();
  list_jack_wet_l->Clear ();
  list_jack_wet_r->Clear ();
  init_audio_done = false;
  
  return audio->unregister ();
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
  
  if (!is_ready ()) // don't re-enable audio on checkbox
    return;
  
  CP
  /*if (!init_audio_done) {
    CP 
    init_audio ();
  }*/
  
  bool a = false;
  bool b = chk_forcemono->GetValue ();
  
  if (!audio->stereo_in == !b)
    a = true;
  
  debug ("a=%d, b=%d", (int) a, (int) b);
  if (b) {
    debug ("force mono");
    audio->set_stereo (false);
  } else {
    debug ("stereo");
    audio->set_stereo (true);
  }
  
  if (a) { 
    debug ("RE-ARMING RECORD"); 
    audio->arm_record ();
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
  if (!is_ready () || audio->backend != audio_driver::JACK) {
    debug ("no JACK client");
    return;
  }
  
  // kludge, but meh who cares
  c_jackclient *jc = (c_jackclient *) audio;
  
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
  list_hpf_mode->SetSelection (prefs->hpf_mode);
  list_lpf_mode->SetSelection (prefs->lpf_mode);
  spin_hpf_freq->SetValue (prefs->hpf);
  spin_lpf_freq->SetValue (prefs->lpf);
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
  prefs->hpf_mode = list_hpf_mode->GetSelection ();
  prefs->hpf_mode = list_lpf_mode->GetSelection ();
  prefs->hpf = spin_hpf_freq->GetValue ();
  prefs->lpf = spin_lpf_freq->GetValue ();
}

void c_mainwindow::set_mode (long int _mode) {
  struct widget_status *wl;
  mode = _mode;
  
  if (!dec) {
    debug ("no deconvolver object");
  }
  dec->prefs->mode = opmode::GUI;
  
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
    if (dec && audio) {
      char buf [128];
      snprintf (buf, 127, "%d", audio->samplerate);
      comb_samplerate->SetValue (buf);
      CP
      bool s = !chk_forcemono->GetValue ();
      pn_meter_in->set_stereo (s/*audio->is_stereo*/);
      pn_meter_out->set_stereo (false);
      if ((!s) != (!audio->stereo_in)) { // cheap xor
        CP
        //audio->register_input (s);
        //audio->init_output (false);
      }
      update_audio_ports ();
    }
    
    if (!is_ready ()) {
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
  if (!b != !w->IsEnabled ()) {
    get_unique_id ();
    w->Enable (b);
    w->SetTransparent (b ? 255 : 128);
  }
}

void c_mainwindow::update_audio_ports () {
    list_jack_dry->Clear ();
    list_jack_wet_l->Clear ();
    list_jack_wet_r->Clear ();
  if (!is_ready ()) {
    disable (list_jack_dry);
    disable (list_jack_wet_l);
    disable (list_jack_wet_r);
    return;
  }
  int i;
  bool s = !chk_forcemono->GetValue ();
  
  std::vector <std::string> port_list;
  audio->get_output_ports (port_list);
  for (i = 0; i < port_list.size (); i++) {
    list_jack_dry->Append (port_list [i]);
  }
  
  port_list.clear ();
  audio->get_input_ports (port_list);
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
    int sr;
    c_wavebuffer dummy_r;
    if (!read_wav (f.GetPath ().c_str (), drysweep, dummy_r))
      show_message ("Failed to load file: " + fn);
    else if (!dec->set_sweep_dry (drysweep))
      show_message ("Deconvolver refused dry sweep: " + fn);
  }
}

void c_mainwindow::on_btn_generate (wxCommandEvent &ev) { CP
  make_dry_sweep ();
  text_dryfile->Clear ();
}

void c_mainwindow::on_btn_irremove (wxCommandEvent &ev) { CP
}

void c_mainwindow::on_btn_irrename (wxCommandEvent &ev) { CP
}

void c_mainwindow::on_btn_irload (wxCommandEvent &ev) { CP
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
    f.GetPaths (wxv);
    for (int i = 0; i < wxv.GetCount (); i++) {
      //list_irfiles->Append (wxv [i]);
      if (!load_ir_file (std::string (wxv [i]))) {
        show_error ("can't load file: " + std::string (wxv [i]));
      }
    }
  }
}

bool c_mainwindow::load_ir_file (std::string filename) { CP
  c_ir_entry ir;
  ir.dir = dirname (filename);
  if (read_wav (filename.c_str (), ir.l, ir.r)) {
    std::string bname = basename (filename);
    ir.name = bname;
    ir.id = get_unique_id ();
    ir.timestamp = epoch_sec ();
    ir.loaded = true;
    ir.dirty = false;
    add_ir (ir);
    return true;
  } else {
    return false;
  }
}

void c_mainwindow::on_btn_irsave (wxCommandEvent &ev) { CP
}

bool c_mainwindow::make_dry_sweep (bool load_it) {
  debug ("start");
  if (!dec) return false;
  struct s_prefs *p = dec->prefs;
  
  get_prefs (p);
  
  generate_log_sweep (p->sweep_seconds,
                      p->preroll_seconds,
                      p->marker_seconds,
                      p->marker_gap_seconds,
                      p->sweep_sr,
                      p->sweep_amp_db,
                      p->sweep_f1,
                      p->sweep_f2,
                      drysweep);
  
  if (load_it) {
    if (!dec->set_sweep_dry (drysweep)) {
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
  wxMessageBox ("\n" + msg + "\n", "DIRT", wxICON_INFORMATION);
}

void c_mainwindow::show_error (std::string msg) {
  std::cerr << msg;
  // TODO: log_widget->add ("Error: " + msg);
  wxMessageBox ("\n" + msg + " \n", "DIRT error", wxICON_ERROR);
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
      std::vector<float> v;
      drysweep.export_to (v);
      if (!write_mono_wav (dest.c_str (), v, sr)) {
        show_error ("Writing failed:\n" + dest);
      } else {
      }
    }
  }
}

void c_mainwindow::on_btn_inputdir_scan (wxCommandEvent &ev) {
  CP
  wxDir absdir (text_inputdir->GetValue ());
  int n = 0;
  
  if (!absdir.IsOpened ())
    return;
  wxString wxabspath = absdir.GetName ();
  
  std::string abspath (wxabspath);
  if (filedir_scanning) {
    filedir_error = true;
    return;
  } else {
    filedir_scanning = true;
    n = add_dir (abspath, chk_inputdir_recursive->GetValue ());
    filedir_scanning = false;
  }

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
    f.GetPaths (wxv);
    for (int i = 0; i < wxv.GetCount (); i++) {
      list_inputfiles->Append (wxv [i]);
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
  if (!dec || !audio) return;
  
  if (audio->state == audiostate::NOTREADY) {
    if (init_audio ()) {
      update_audio_ports ();
    } else {
      show_error ("Failed to initialize " + 
                    g_backend_names [(int) audio->backend] +
                    "\n...is audio device available?\n");
    }
  } else {
    disable_audio ();
  }
}

void c_mainwindow::on_btn_play (wxCommandEvent &ev) { CP
  if (!is_ready ()) return;
  
  //init_audio ();
  debug ("audio->state=%d", audio->state);
  if (audio->state == audiostate::IDLE ||
    audio->state == audiostate::MONITOR) {
      if (mode == ID_ROUNDTRIP) {
      wetsweep_l.resize (drysweep.size ());
      wetsweep_r.resize (drysweep.size ());
      //audio->play (&drysweep);
      //audio->record (&wetsweep_l, &wetsweep_r);
      c_wavebuffer dummy;
      dummy.resize (drysweep.size ());
      audio->playrec (&drysweep, &dummy, &wetsweep_l, &wetsweep_r);
    } else {
      audio->play (&drysweep);
    }
  }
    //audio->play (&dec->sweep_dry);
  else
    audio->stop ();
}

void c_mainwindow::on_btn_process (wxCommandEvent &ev) {
  CP
  std::string wetfile;
  int i, n;
  
  if (!dec) {
    printf ("No deconvolver object, can't process! (this shouldn't happen)\n");
    return;
  }
  
  std::string dirname = std::string (text_outputdir->GetValue ());
  debug ("dirname='%s'", dirname.c_str ());
  if (chk_autosave->GetValue () && !dir_exists (dirname)) {
    show_error ("Please untick auto-save or select an output directory.\n");
    return;
  }
  
  if (!dec->has_dry ()) {
    show_message ("No DRY sweep has been loaded yet");
  }
  
  n = list_inputfiles->GetCount ();
  printf ("got %d files:\n", n);
  
  audio->stop ();
  get_prefs (dec->prefs);
  debug ("mono: %s", dec->prefs->request_stereo ? "true" : "false");
  //dec->set_dry_from_buffer (...);
  
  num_files_ok = 0;
  num_files_error = 0;
  busy = true;
  
  for (i = 0; i < list_inputfiles->GetCount ();) {
    wetfile = std::string (list_inputfiles->GetString (i));
    printf ("  %d: '%s'\n", i, wetfile.c_str ());
    wxYield ();
    if (process_one_file (wetfile)) {
      list_inputfiles->Delete (i);
    } else {
      debug ("Failed to process file: %s", wetfile.c_str ());
      if (chk_abort->GetValue ()) {
        i = n; // skip to end
      } else {
        i++;   // error, skip this file (leave it in list)
      }        // else stay at same index (next file)
    }
  }
  if (list_inputfiles->GetCount () > 0) {
    text_inputheader->SetLabel ("There were errors processing these files:");
  }
  busy = false;
}

bool c_mainwindow::process_one_file (std::string filename) {
  bool ret = true;
  debug ("start, filename='%s'", filename.c_str());
  
  set_statustext ("Processing file: " + filename);
  usleep (10000);
  
  c_wavebuffer wl, wr, ir_l, ir_r;
  
  std::string basename;
  std::string strippedname;
  std::string outputname;
  
  int a = filename.find_last_of ("/");
  if (a > 0)
    basename = filename.substr (a + 1, filename.size () - 1 - a);
  else
    basename = filename;
    
  int b = basename.find_last_of (".");
  if (b > 1)
    strippedname = basename.substr (0, b);
  else
    strippedname = basename;
  
  std::string outputdir = std::string (text_outputdir->GetValue ());  
  std::string outputbasename = strippedname + "-ir.wav";
  
  if (outputdir.size () > 0)
    if (outputdir [outputdir.size () - 1] == '/')
      outputname = outputdir + outputbasename;
    else
      outputname = outputdir + "/" + outputbasename;
  else
    outputname = strippedname;
    
  debug ("filename: %s", filename.c_str ());
  debug ("basename: %s", basename.c_str ());
  debug ("strippedname: %s", strippedname.c_str ());
  debug ("outputname: %s", outputname.c_str ());
  
  if (!read_wav (filename.c_str (), wl, wr)) {
    set_statustext ("Failed to open " + filename);
    ret = false;
  } else if (!dec->set_sweep_wet (wl, wr)) {
    debug ("deconvolver refused file: %s", filename.c_str ());
    ret = false;
  } else if (!dec->render_ir (ir_l, ir_r)) {
      debug ("deconvolver failed to process: %s", filename.c_str ());
      ret = false;
  } else if (!ir_l.size ()) {
    debug ("deconvolver output zero length IR: %s", filename.c_str ());
  } else if (chk_autosave->GetValue () && ask_overwrite (outputname)) {
    if (ir_r.size ()) {
      if (!write_stereo_wav (outputname.c_str (), ir_l, ir_r)) {
        debug ("can't write stereo output file: %s", outputname.c_str ());
        ret = false;
      }
    } else {
      if (!write_mono_wav (outputname.c_str (), ir_l)) {
        debug ("can't write mono output file: %s", outputname.c_str ());
        ret = false;
      }
    }
  }
  
  
  if (ret) {
    num_files_ok++;
    
    auto now = std::chrono::system_clock::now();
    time_t epoch = std::chrono::system_clock::to_time_t(now);
    
    c_ir_entry ir;
    ir.timestamp = epoch;
    ir.l = ir_l;
    ir.r = ir_r;
    ir.name = outputbasename;
    ir.dir = outputdir;
    //ir.samplerate = ir_l.get_samplerate ();
    ir.dirty = true;
    ir.loaded = true;
    add_ir (ir);
    
  } else {
    num_files_error++;
  }
  
  debug ("end, ret=%d", (int) ret);
  return ret;
}

bool c_mainwindow::ask_overwrite (std::string filename) {
  if (!file_exists (filename))
    return true;
  
  if (chk_overwrite->GetValue ())
    return true;
  
  wxMessageDialog msg (this, std::string ("File exists:\n" + filename + "\n"),
                       "\nOverwrite?", wxCANCEL|wxYES_NO|wxICON_QUESTION);
  msg.SetOKCancelLabels ("Yes", "All");
  
  int ret = msg.ShowModal ();
  
  switch (ret) {
    case wxID_OK:
    case wxID_YES:
      debug ("wxID_YES");
      return true;
    break;
    
    case wxID_CANCEL: // "all" button
      debug ("wxID_CANCEL");
      chk_overwrite->SetValue (true);
      return true;
    break;
    
    default: CP
      debug ("default (wxID_NO or other)");
      return false;
    break;
  }
    
  return false;
}

int c_mainwindow::update_ir_list () {
  int count = 0, i, n = ir_files.size ();
  debug ("n=%d", n);
  list_irfiles->clear ();
  
  for (i = 0; i < n; i++) {
    
    if (ir_files [i].name.size () > 0 &&
       (ir_files [i].l.size () > 0 ||
        ir_files [i].r.size () > 0)) {
      count++;
      
      debug ("adding entry '%s'", ir_files [i].name.c_str ());
      //list_irfiles->Append (wxString (shortname));
      list_irfiles->append (ir_files [i]);
    } else {
      debug ("ignoring empty entry '%s'", ir_files [i].name.c_str ());
    }
    
    printf ("IR %d:\n",             i);
    printf (" ID:         %ld\n",   (int64_t) ir_files [i].id);
    printf (" timestamp:  %ld",     (int64_t) ir_files [i].timestamp);
    printf (" (%s)\n",              readable_timestamp
        (ir_files [i].timestamp).c_str ());
    printf (" name:       %s\n",    ir_files [i].name.c_str ());
    printf (" dir:        %s\n",    ir_files [i].dir.c_str ());
    printf (" size L:     %ld\n",   (int64_t) ir_files [i].l.size ());
    printf (" size R:     %ld\n",   (int64_t) ir_files [i].r.size ());
    printf (" dirty:      %s\n",    ir_files [i].dirty ? "yes" : "no");
    printf (" loaded:     %s\n\n",  ir_files [i].loaded ? "yes" : "no");
  }
  debug ("n=%d, count=%d, %d IR entries, %d list entries", 
          n, count, ir_files.size (), list_irfiles->get_count ());
  
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
  bool err = false;
  
  wxDir dir (dirname);
  wxString str;
  
  filedir_error = false;
  
  if (dir.IsOpened ()) {
    wxString wxfn;
    bool found = dir.GetFirst (&wxfn);
    
    while (found && !filedir_error) {
      std::string fn (wxfn);
      std::string absfn (dirname + "/" + fn.c_str ());
      if (dir_exists (absfn)) {
        if (recurs) {
          printf ("recurs dir: %s\n", absfn.c_str ());
          int c = add_dir (absfn, recurs);
          if (c != -1) {
            count += c;
          } else {
            filedir_error = true;
            return 0;
          }
        } else {
          printf ("found dir:  %s\n", absfn.c_str ());
        }
      } else {
        if (suffix_match (absfn, ".wav")) {
          printf ("found file: %s\n", absfn.c_str ());
          list_inputfiles->Append (absfn);
          text_inputheader->SetLabel ("List of sweep files to process:");
          count++;
        } else {
          printf ("ignoring file: %s\n", absfn.c_str ());
        }
      }
      found = dir.GetNext (&wxfn);
    }
    wxYield ();
  } else {
    //show_error ("can't open directory " + dirname + "\n");
    wxMessageDialog msg (this, std::string ("Can't open directory " + dirname + "\n"),
                         "Read error", wxOK|wxCANCEL|wxICON_ERROR);
    msg.SetOKCancelLabels ("Skip", "Cancel");
    int ret = msg.ShowModal ();
    if (ret != wxID_OK && ret != wxID_YES) {
      debug ("ret=%d", ret);
      err = true;
      return -1;
    } else {
      debug ("ret=%d", ret);
    }
  }
  
  return count;
}

////////////////////////////////////////////////////////////////////////////////
// c_app

c_app::c_app (c_deconvolver_gui *d)
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
  bool keystate = false;
  int evttype = ev.GetEventType ();
  
  if (evttype == wxEVT_KEY_DOWN)
    keystate = true;
    
  if (evttype == wxEVT_KEY_UP || evttype == wxEVT_KEY_DOWN) {
    switch (((wxKeyEvent&)ev).GetKeyCode ()) {
      case WXK_SHIFT:
        shift = keystate;
        debug ("shift=%d", (int) shift);
      break;
      
      case WXK_ALT: CP
        alt = keystate;
        debug ("alt=%d", (int) alt);
      break;
      
      case WXK_CONTROL: CP
        ctrl = keystate;
        debug ("ctrl=%d", (int) ctrl);
      break;
    }
  }
  
  return wxApp::FilterEvent (ev);
}

c_app *g_app = NULL;
//wxIMPLEMENT_APP_NO_MAIN (c_app);

int wx_main (int argc, char **argv, c_audioclient *audio) {
  int retval = 0;
  c_deconvolver_gui dec;

  debug ("start");
  g_app = new c_app (&dec);
  g_app->audio = audio;
  wxApp::SetInstance (g_app);
  retval = wxEntry (argc, argv);
  if (0&&g_app)
    delete g_app;
  debug ("end, retval=%d", retval);
  return retval;
}


////////////////////////////////////////////////////////////////////////////////
// c_irlist

c_irlist::c_irlist (wxWindow *parent, int id, 
                    wxPoint pos, wxSize size, int border)
: wxListCtrl (parent, id, pos, size, border) { CP
  clear ();
}

void c_irlist::clear () { CP
  ClearAll ();
  AppendColumn ("Generated IR files:", wxLIST_FORMAT_LEFT, wxLIST_AUTOSIZE_USEHEADER);
  //SetColumnWidth (0, GetSize ().x);
}

int c_irlist::get_count () { CP
  return GetItemCount ();
}

void c_irlist::append (c_ir_entry &ir) { CP
  char buf [64];
  snprintf (buf, 31, "%d", (int) ir.id);
  
  long int item = InsertItem (GetItemCount (), ir.name);
  SetItemData (item, ir.id);
}

int64_t c_irlist::get_selected_id () {
  long int item = -1;
  item = GetNextItem (item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
  
  if (item >= 0) {
    return GetItemData (item);
  } else
    return 0;
}


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


////////////////////////////////////////////////////////////////////////////////
// c_customwidget - parent class for other widget types 
 
BEGIN_EVENT_TABLE (c_customwidget, wxPanel)
  EVT_PAINT (c_customwidget::on_paint)
  EVT_SIZE (c_customwidget::on_resize)
  EVT_MOTION (c_customwidget::on_mousemove)
  EVT_LEFT_DOWN (c_customwidget::on_mousedown_left)
  EVT_LEFT_UP (c_customwidget::on_mouseup_left)
  EVT_MIDDLE_DOWN (c_customwidget::on_mousedown_middle)
  EVT_MIDDLE_UP (c_customwidget::on_mouseup_middle)
  EVT_RIGHT_DOWN (c_customwidget::on_mousedown_right)
  EVT_RIGHT_UP (c_customwidget::on_mouseup_right)
  EVT_LEAVE_WINDOW (c_customwidget::on_mouseleave)
  EVT_MOUSEWHEEL (c_customwidget::on_mousewheel)
  EVT_KEY_DOWN (c_customwidget::on_keydown)
  EVT_KEY_UP (c_customwidget::on_keyup)
  EVT_IDLE (c_customwidget::on_idle)
  EVT_SHOW (c_customwidget::on_visible)
END_EVENT_TABLE ()

c_customwidget::c_customwidget (wxWindow *p_parent,
                                int p_id,
                                wxPoint pos, 
                                wxSize p_size,
                                int wtf)
                                //wxBorder border)
: wxPanel (p_parent, p_id, pos, p_size, wtf) {
  debug ("start");
  
  visible = true;
  parent = p_parent;
  int font_height = 16;//fm.height ();
  debug ("font_height=%d", font_height);
  
  SetBackgroundStyle(wxBG_STYLE_PAINT);
  
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
  
  in_paintevent = false;  
  base_image_valid = false;
  mouse_buttons = 0;
  mouse_x = -1;
  mouse_y = -1;
  click_distance = 5; // arbitrary, TODO: get reasonable default from OS/gfx system
  for (int i = 0; i < 8; i++)
    mousedown_x [i] = mousedown_y [i] = -16384; // again arbitrary, but umm... yeah :)
  
  set_opacity (255);
  debug ("end");
}


void c_customwidget::set_opacity (int p_opacity) {
  //char buf [256];
  
  opacity = p_opacity;
  /*sprintf (buf, "* { background-color: rgba(%d,%d,%d,%d); border-width: 0px; }", r, g, b, opacity);
  debug ("buf='%s'", buf);
  setStyleSheet (buf);*/
  
  base_image_valid = false;
  Refresh (false);
}


void c_customwidget::inspect () {
  debug ("start");
  debug ("end");
}

/* Typically, this funciton would react on resize, or other event that
 * invalidates the base image. */
/*bool c_customwidget::render_base_image () { CP
  //debug ("start");
  GetClientSize (&width, &height);
  if (width <= 0 || height <= 0) return false;
  
  img_base = wxBitmap (width, height, 32);
  
  if (!img_base.IsOk ()) return false;
  
  wxMemoryDC dc;
  dc.SelectObject (img_base);
  dc.SetBackground (col_bg);
  dc.SetTextForeground (col_default_fg);
  std::string msg = "c_customwidget::render_base_image";
  wxSize sz = dc.GetTextExtent (msg);
  dc.DrawText (msg, (width - sz.x) / 2, (height - sz.y) / 2);
  //dc.Clear ();
  dc.SelectObject (wxNullBitmap);
  
  base_image_valid = true;
  
  //debug ("end");
  return true;
}*/




bool c_customwidget::render_base_image ()
{
  GetClientSize (&width, &height);
  if (width <= 0 || height <= 0) return false;

  if (!img_base.IsOk () || img_base.GetWidth () != width || 
      img_base.GetHeight () != height)
    img_base = wxBitmap (width, height, 32);

  if (!img_base.IsOk ()) return false;

  wxMemoryDC dc;
  dc.SelectObject (img_base);
  dc.SetBackground (wxBrush(col_bg));
  dc.Clear ();

  render_base (dc);              // <-- virtual hook (no need to call parent)
  
  dc.SelectObject (wxNullBitmap);

  base_image_valid = true;
  return true;
}


void c_customwidget::render_base (wxDC &dc) {
  dc.SetBackground (col_bg);
  dc.SetTextForeground (col_default_fg);
  std::string msg = "Base class, override\nc_customwidget::render_base_image";
  wxSize sz = dc.GetTextExtent (msg);
  dc.DrawText (msg, (width - sz.x) / 2, (height - sz.y) / 2);
}



// public API, override for drawing overlay
void c_customwidget::render_overlay (wxDC &dc) {
}

/* Typically, this function would react to minor appearance changes, such
 * as visual feedback of mouse events and so on. */
/*bool c_customwidget::update (wxWindowDC &dc) {
  int x, y, w, h, i, m, c;
  unsigned char new_md5 [16];
  int new_width, new_height;
  
  //debug ("start");
  
  GetClientSize (&new_width, &new_height);
  if (new_width <= 0 || new_height <= 0)
    return false;
  
  if (!base_image_valid || new_width != width || new_height != height) {
    width = new_width;
    height = new_height;
    render_base_image ();
    img_overlay = wxBitmap (width, height, 32);
  }
  
  dc.DrawBitmap (img_base, 0, 0);
  //dc.DrawBitmap (img_overlay, 0, 0);
  
  // Add overlay/highlight indicators etc.
  // we would need to do this AFTER render_base_image ()
  
  //debug ("end");
  return true;
}*/

/*void c_customwidget::update () { // <--- called to schedule a repaint
  Refresh (false);
}*/

void c_customwidget::invalidate_base () {
  base_image_valid = false;
  Refresh (false);
}

void c_customwidget::invalidate_overlay () {
  Refresh (false);
}

void c_customwidget::invalidate_overlay_rect (const wxRect &r) {
  RefreshRect (r, false);
}

void c_customwidget::on_paint (wxPaintEvent &ev) {
  wxAutoBufferedPaintDC dc (this);

  // always sync size here; paint can happen without EVT_SIZE in some cases.
  int new_w, new_h;
  GetClientSize (&new_w, &new_h);
  if (new_w <= 0 || new_h <= 0) return;

  if (new_w != width || new_h != height) {
    width = new_w; height = new_h;
    base_image_valid = false;
  }
  
  if (!base_image_valid || !img_base.IsOk ()) {
    render_base_image ();
  }

  // Compose: base bitmap then overlay primitives.
  if (img_base.IsOk ())
    dc.DrawBitmap (img_base, 0, 0, false);

  on_paint (dc); // <--- the api hook

  render_overlay (dc);
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
  
  on_mousemove (mouse_x, mouse_y);
  
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

void c_customwidget::on_keydown (wxKeyEvent &ev) {
  int keycode = ev.GetKeyCode ();
  bool is_keyrepeat = ev.IsAutoRepeat ();
  on_keydown (keycode, is_keyrepeat);
  on_keydown (ev.GetUnicodeKey (), keycode,
               ev.GetRawKeyCode (), ev.IsAutoRepeat ());
}

void c_customwidget::on_keyup (wxKeyEvent &ev)  {
  int keycode = ev.GetKeyCode ();
  on_keyup (keycode);
  on_keyup (ev.GetUnicodeKey (), keycode,
                 ev.GetRawKeyCode ());
}
void c_customwidget::on_idle (wxIdleEvent &ev) { 
  on_idle ();
}
void c_customwidget::on_visible (wxShowEvent &ev) {
  on_visible ();
}

void c_customwidget::on_mousedown_left (wxMouseEvent &ev) {
  //debug ("start");
  get_xy (ev);
  
  SetFocus ();
  ev.Skip ();
  
  mousedown_x [0] = mouse_x;
  mousedown_y [0] = mouse_y;
  mouse_buttons |= 1;
  
  on_mousedown (0);
  on_mousedown_left ();
  
  Refresh (false);
  ev.Skip ();
  //debug ("end");
}

void c_customwidget::on_mouseup_left (wxMouseEvent &ev) {
  //debug ("start");
  SetFocus ();
  get_xy (ev);
  mouse_buttons &= ~1;
  
  on_mouseup (0);
  on_mouseup_left ();
  
  Refresh (false);
  ev.Skip ();
}

void c_customwidget::on_mousedown_middle (wxMouseEvent &ev) {
  //debug ("start");
  get_xy (ev);
  mousedown_x [1] = mouse_x;
  mousedown_y [1] = mouse_y;
  mouse_buttons |= 2;
  
  on_mousedown (1);
  on_mousedown_middle ();
  
  Refresh (false);
  ev.Skip ();
  //debug ("end");
}

void c_customwidget::on_mouseup_middle (wxMouseEvent &ev) {
  //debug ("start");
  get_xy (ev);
  mouse_buttons &= ~2;
  
  on_mouseup (1);
  on_mouseup_middle ();
  
  Refresh (false);
  ev.Skip ();
  //debug ("end");
}

void c_customwidget::on_mousedown_right (wxMouseEvent &ev) {
  //debug ("start");
  get_xy (ev);
  mousedown_x [2] = mouse_x;
  mousedown_y [2] = mouse_y;
  mouse_buttons |= 4;
  
  on_mousedown (2);
  on_mousedown_right ();
  
  Refresh (false);
  ev.Skip ();
  //debug ("end");
}

void c_customwidget::on_mouseup_right (wxMouseEvent &ev) {
  //debug ("start");
  get_xy (ev);
  mouse_buttons &= ~4;
  
  on_mouseup (2);
  on_mouseup_right ();
  
  Refresh (false);
  ev.Skip ();
}

void c_customwidget::on_mousewheel (wxMouseEvent &ev) {
  //debug ("start");
  
  int n = ev.GetWheelRotation ();
  if (ev.IsWheelInverted ()) n *= -1;
  if (ev.GetWheelAxis () == wxMOUSE_WHEEL_HORIZONTAL) {
    on_mousewheel_h (n);
  } else {
    on_mousewheel_v (n);
  }
  
  Refresh (false);
  // no skip (would break scrolling)
  //debug ("end");
}

// this is triggered by sideways scrolling on a drag pad or similar
void c_customwidget::on_mousewheel_h (int howmuch) {
  //debug ("howmuch=%d", howmuch);
}

void c_customwidget::on_mousewheel_v (int howmuch) {
  //debug ("howmuch=%d", howmuch);
}

void c_customwidget::on_mouseleave (wxMouseEvent &ev) {
  //debug ("start");
  
  mouse_x = -1;
  mouse_y = -1;
  
  on_mouseleave ();
  
  Refresh (false);
  //emit sig_mouse_leave (this);
  
  //debug ("end");
}

// EVENT HANDLER (on_resize vs on_resized)
void c_customwidget::on_resize (wxSizeEvent &ev) {
  //debug ("start");
  
  base_image_valid = false;
  int new_w = ev.GetSize ().x;
  int new_h = ev.GetSize ().y;
  
  on_resize (new_w, new_h);

  // schedule repaint (no direct drawing)
  Refresh (false);

  ev.Skip (); // allow wx to continue layout/propagation
  
  //debug ("end");
}

////////////////////////////////////////////////////////////////////////////////
// test/minimal example usage of c_customwidget derived class

void c_testwidget::render_base (wxDC &dc) {
  //if (!c_customwidget::render_base_image ()) return false;
  dc.DrawLine (0, 0, width, height);
  dc.DrawLine (0, height, width, 0);
}

void c_testwidget::on_paint (wxDC &dc) { CP
  std::string msg = "This is an example use of c_customwidget";
  wxSize sz = dc.GetTextExtent (msg);
  dc.SetTextForeground (col_default_fg);
  dc.DrawText (msg, (width - sz.x) / 2, (height - sz.y) / 2);
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
  set_db_scale (-40);
}

void c_meterwidget::set_vudata (c_vudata *v) {
  data = v;
  set_db_scale (db_scale);
}

c_vudata *c_meterwidget::get_vudata () {
  return data;
}

void c_meterwidget::render_gradient_bar () {
  //debug ("start");
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
  
  //debug ("end");
}

void c_meterwidget::set_db_scale (float f) {
  db_scale = f;
  if (data)
    data->set_db_scale (f);
}

void  c_meterwidget::render_base (wxDC &dc) {
  //debug ("start");
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
    { 0.5,     0, 255,   0, 128 },
    { 0.75,  255, 255,   0, 128 },
    { 0.875, 255, 128,   0, 128 },
    { 1.0,   255,   0,   0, 128 },
    { -1.0,    0,   0,   0, 128 },
  };
  
  for (int i = 0; lines [i].pos >= 0; i++) {
    ln *l = &lines [i];
    //debug ("line: %f, rgb (%d,%d,%d)", l->pos, l->r, l->g, l->b);
    dc.SetPen (wxPen (wxColour (l->r, l->g, l->b, l->a)));
    if (vertical) {
      int ppos = (height - clip_size - rec_size) * l->pos;
      if (ppos > height - 1) ppos = height - 1;
      dc.DrawLine (t1, height - ppos - rec_size, t2 - 1, height - ppos - rec_size);
      if (stereo)
      dc.DrawLine (t3, height - ppos - rec_size, t4 - 1, height - ppos - rec_size);
    } else {
      int ppos = (width - clip_size - rec_size) * l->pos;
      if (ppos > width - 1) ppos = width - 1;
      dc.DrawLine (ppos + rec_size, t1, ppos + rec_size, t2 - 1);
      if (stereo)
        dc.DrawLine (ppos + rec_size, t3, ppos + rec_size, t4 - 1);
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
  wxMemoryDC mdc;
  
  for (int i = 0; i < (int) meterwarn::MAX; i++) {
    meterwarnsize [i] = mdc.GetTextExtent (meterwarntext [i]);
  }
  mdc.SelectObject (wxNullBitmap);
  if (!vertical) {
    float ps = (float) std::min (width, height);
    ps /= 1.6;
    tinyfont.SetPointSize (ps);
    mdc.SetFont (tinyfont);
    for (int i = 0; i < (int) meterwarn::MAX; i++) {
      int w = meterwarnsize [i].x;
      int h = meterwarnsize [i].y;
      //debug ("new bitmap, w=%d, h=%d", w, h);
      img_warning [i] = wxBitmap (w * 4 / 3, h, 32);
      mdc.SelectObject (img_warning [i]);
      mdc.SetTextForeground (wxColour (192, 0, 0));
      mdc.SetBackground (wxColour (0, 0, 0, 0));
      mdc.Clear ();
      mdc.DrawText (meterwarntext [i], 0, 0);
      mdc.SelectObject (wxNullBitmap);
    }
  }
             
  //debug ("end");
}

// at: x start pos if vertical, y pos if horiz
// ie. distance between (0,0) and meter bar
void c_meterwidget::draw_bar (wxDC &dc, int at, int th, bool is_r,
                              float level, float hold) {
  //debug ("start");
  //debug ("at=%d, th=%d, %s, level=%f, hold=%f", at, th,
  //       is_r ? "is_r" : "!is_r", level, hold);
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
    if (img_bar.IsOk () && w > 0 && h > 0) {
      //debug ("x=%d y=%d, w=%d h=%d", x, y, w, h);
      img_bar_sub = img_bar.GetSubBitmap (wxRect (x, y, w, h));
      if (!img_bar_sub.IsOk ()) return;
      dc.DrawBitmap (img_bar_sub, x, y);
    }
  }
  
  // peak hold indicator
  int holdpos = hold * met_len;
  if (holdpos > 1 && holdpos < met_len) {
    dc.SetPen (wxPen (wxColour (255, 255, 255, 128)));
    if (vertical) {
      holdpos -= clip_size;
      dc.DrawLine (at, y + h - holdpos, at + th - 1, y + h - holdpos);
      dc.DrawLine (at, y + h - holdpos - 1, at + th - 1, y + h - holdpos - 1);
    } else {
      holdpos += rec_size;
      dc.DrawLine (x + holdpos, at, x + holdpos, at + th - 1);
      dc.DrawLine (x + holdpos - 1, at, x + holdpos - 1, at + th - 1);
    }
  }
  //debug ("end");
}

bool c_meterwidget::needs_redraw () {
  return data->needs_redraw.exchange (false);
}

void c_meterwidget::on_ui_timer () {
  if (needs_redraw ()) Refresh ();
}

void c_meterwidget::on_paint (wxDC &dc) {
  //debug ("start");
  if (!data) return;
  
  // meter bars
  if (stereo) {
    draw_bar (dc, t1, t2 - t1, false, data->l (), data->peak_l ());
    draw_bar (dc, t3, t4 - t3, true,  data->r (), data->peak_r ());
  } else {
    draw_bar (dc, t1, t2 - t1, false, data->l (), data->peak_l ());
  }
  
  // clip indicator
  dc.SetBrush (wxBrush (*wxRED));
  dc.SetPen (wxPen (*wxRED));
  if ((data->clip_l || data->clip_r) && clip_size > 0) {
    if (vertical) {
      dc.DrawRectangle (0, 0, width, clip_size);
    } else {
      dc.DrawRectangle (width - clip_size, 0, clip_size, height);
    }
  } else if ((data->clip_l || data->clip_r) && !vertical) {
    int idx = (int) meterwarn::CLIP;
    int w = img_warning [idx].GetWidth ();
    int h = img_warning [idx].GetHeight ();
    dc.DrawBitmap (img_warning [idx], width - w - 1, (height - h) / 2);
  }
  
  // recording indicator (red circle)
  int rp = (vertical ? width : height) / 4;
  if (rec_enabled && rec_size > 0) {
    if (vertical)
      dc.DrawEllipse (rp, height - rec_size + rp, width - (rp * 2), rec_size - (rp * 2));
    else
      dc.DrawEllipse (rp, rp, rec_size - (rp * 2), height - (rp * 2));
  } else if (rec_enabled && !vertical) {
    int idx = (int) meterwarn::REC;
    int h = img_warning [idx].GetHeight ();
    dc.DrawBitmap (img_warning [idx], 1, (height - h) / 2);
  }
  
  data->acknowledge ();
  //debug ("end");
  return;
}

void c_meterwidget::set_stereo (bool b) {
  stereo = b;
  base_image_valid = false;
  Refresh ();
}


////////////////////////////////////////////////////////////////////////////////
// c_waveformwidget

c_waveformwidget::c_waveformwidget (wxWindow *parent,
                                    int id,
                                    wxPoint pos,
                                    wxSize size,
                                    int i) // <-- WXWIDGETS PLZ DECIDE WTF YOU
                                           //     WANT HERE, INT OR WXBORDER
: c_customwidget (parent, id, pos, size, (wxBorder) i) { CP
  l = new c_waveformchannel (this);
  r = new c_waveformchannel (this);
  col_bezel1 = wxColour (64, 64, 64, 128);
  col_bezel2 = wxColour (255, 255, 255, 128);
}

c_waveformwidget::~c_waveformwidget () { CP
  if (l) delete l;
  if (r) delete r;
}

bool c_waveformwidget::stereo () {
  if (!ir || ir->r.size () <= 0)
    return false;
    
  return true;
}

// event handlers: mostly passthrough for c_waveformchannel

// nice macros.. don't we all love writing boilerplate
#define PASS_HANDLER(x,handler) {                      \
  if (x) { uint64_t f = x->handler;                   \
  if (f & UI_NEEDS_FULL_REDRAW) invalidate_base ();   \
  if (f & UI_NEEDS_REDRAW)      sched_redraw (); }    \
}
#define MOUSEIN(x) (x && x->rect.Contains (mouse_x, mouse_y))
#define PASS_MOUSEHANDLER(x,hnd) { if (MOUSEIN(x)) PASS_HANDLER(x,hnd); }

void c_waveformwidget::on_resize (int x, int y) {
  //PASS_HANDLER (l, on_resize (x, y));
  //PASS_HANDLER (r, on_resize (x, y));
}

void c_waveformwidget::on_idle () {
  PASS_HANDLER (l, on_idle ());
  PASS_HANDLER (r, on_idle ());
}

void c_waveformwidget::on_mousewheel_h (int howmuch) { CP
  PASS_MOUSEHANDLER (l, on_mousewheel_h (howmuch));
  PASS_MOUSEHANDLER (r, on_mousewheel_h (howmuch));
}

void c_waveformwidget::on_mousewheel_v (int howmuch) { CP
  PASS_MOUSEHANDLER (l, on_mousewheel_v (howmuch));
  PASS_MOUSEHANDLER (r, on_mousewheel_v (howmuch));
}

void c_waveformwidget::on_mousedown_left () {
  PASS_MOUSEHANDLER (l, on_mousedown_left ());
  PASS_MOUSEHANDLER (r, on_mousedown_left ());
}

void c_waveformwidget::on_mouseup_left () { CP
  PASS_MOUSEHANDLER (l, on_mouseup_left ());
  PASS_MOUSEHANDLER (r, on_mouseup_left ());
}

void c_waveformwidget::on_mousedown_right () { CP
  PASS_MOUSEHANDLER (l, on_mousedown_right ());
  PASS_MOUSEHANDLER (r, on_mousedown_right ());
}

void c_waveformwidget::on_mouseup_right () { CP
  PASS_MOUSEHANDLER (l, on_mouseup_right ());
  PASS_MOUSEHANDLER (r, on_mouseup_right ());
}

void c_waveformwidget::on_mousemove (int x, int y) {
  PASS_MOUSEHANDLER (l, on_mousemove (mouse_x - l->rect.x, mouse_y - l->rect.y));
  PASS_MOUSEHANDLER (r, on_mousemove (mouse_x - r->rect.x, mouse_y - r->rect.y));
}

// rest of passthrough functions

bool c_waveformwidget::select_ir (c_ir_entry *entry) { CP
  ir = entry;
  bool ret = true;

  if (!ir)
    return false;
  
  if (ir->l.size () > 0) {
    if (l) ret = l->select_waveform (&ir->l);
    if (!ret) return false;
  }

  if (ir->r.size () > 0) {
    if (r) r->select_waveform (&ir->r);
  }
  
  base_image_valid = false;
  Refresh ();
  
  return true;
}

bool c_waveformwidget::unselect_ir () { CP
  l->unselect_waveform ();
  r->unselect_waveform ();
  
  ir = NULL;
  
  Refresh ();
  return true;
}

c_ir_entry *c_waveformwidget::get_selected () {
  return ir;
}

void c_waveformwidget::zoom_full_x () { 
  if (l && l->zoom_full_x ()) sched_redraw ();
  if (r && r->zoom_full_x ()) sched_redraw ();
}

void c_waveformwidget::zoom_full_y () { 
  if (l && l->zoom_full_y ()) sched_redraw ();
  if (r && r->zoom_full_y ()) sched_redraw ();
}

bool c_waveformwidget::have_l () {
  if (!l) return false; 
  if (!l->wb) return false; 
  if (l->wb->size () <= 0) return false;
  return true;
}

bool c_waveformwidget::have_r () {
  if (!r) return false;
  if (!r->wb) return false;
  if (r->wb->size () <= 0) return false;
  return true;
}

void c_waveformwidget::draw_border (wxDC &dc, wxRect &rect) {
  int x = rect.x - 1;
  int y = rect.y - 1;
  int w = rect.width + 2;
  int h = rect.height + 2;
  //if (x < 0) x = 0;
  //if (y < 0) y = 0;
  //if (w < 0) w = width;
  //if (h < 0) h = height;
  // bezel/frame, this gives a nice gradient effect: line at a slight
  // angle, sandwitched between two lines of system background color
  dc.SetPen (wxPen (col_bezel1));
  dc.DrawLine (x, y, x + w - 1, y);             // top
  dc.DrawLine (x, y, x, y + h - 1);             // left
  dc.SetPen (wxPen (col_bezel2));
  dc.DrawLine (x + 1, y + h, x + w - 1, y + h - 1);     // bottom, 1 pixel angle
  dc.DrawLine (x + w, y + 1, x + w - 1, y + h - 1);     // right, 1 pixel angle
  dc.SetPen (wxPen (col_default_bg));
  dc.DrawLine (x + 1, y + h, x + w - 1, y + h);         // bottom inner
  dc.DrawLine (x + w, y + 1, x + w, y + h);             // right inner
}

void c_waveformwidget::render_base (wxDC &dc) {
  debug ("width=%d, height=%d", width, height);
  wxRect rect_l, rect_r;
  
  if (ir) { 
    if (have_l () && !have_r ()) { debug ("havel_l && !have_r");
      rect_l = wxRect (4, 4, width - 8, height - 8);
    } else if (have_r ()) { debug ("havel_l && have_r");
      rect_l = wxRect (4, 4, width - 8, height / 2 - 8);
      rect_r = wxRect (4, height / 2 + 6 , width - 8, height / 2 - 8); 
    } else { return; }
    
    if (have_l ()) { 
      debug ("l->rect %d,%d,%d,%d", l->rect.x, l->rect.y, l->rect.width, l->rect.height);
      if (rect_l != l->rect) {
        l->rect = rect_l;
        l->on_resize (rect_l.width, rect_l.height);
      }
      dc_scope scope_l (dc, rect_l);
      l->render_base (dc);
    }
    
    if (have_r ()) { debug ("have_r");
      debug ("r->rect %d,%d,%d,%d", r->rect.x, r->rect.y, r->rect.width, r->rect.height);
      if (rect_r != r->rect) {
        r->rect = rect_r;
        r->on_resize (rect_r.width, rect_r.height);
      }
      dc_scope scope_r (dc, rect_r);
      //dc.SetClippingRegion (l->rect);
      //dc.SetDeviceOrigin (r->rect.x, r->rect.y);
      r->render_base (dc);
    }
    
    dc.SetDeviceOrigin (0, 0);
    dc.DestroyClippingRegion ();
    if (l) draw_border (dc, l->rect);
    if (r) draw_border (dc, r->rect);
  } else {
    debug ("no IR");
  }
}

void c_waveformwidget::on_paint (wxDC &dc) {
  //wxPoint old_origin = dc.GetDeviceOrigin ();
  
  if (have_l ()) { 
    dc_scope scope_l (dc, l->rect);
    l->on_paint (dc);
  }
  
  if (have_r ()) {
    dc_scope scope_r (dc, r->rect);
    r->on_paint (dc);
  }
}

void c_waveformwidget::on_ui_timer () {
  uint64_t fl = l->ui_flags.load ();
  uint64_t fr = r->ui_flags.load ();
  
  //debug ("fl=%lx, fr=%lx", (long int) fl, (long int) fr);
  
  if ((fl | fr) & UI_NEEDS_FULL_REDRAW) { debug ("invalidate_base");
    invalidate_base ();
  } else if ((fl | fr) & UI_NEEDS_REDRAW) { debug ("Refresh");
    Refresh ();
  }
  
  fl &= ~(UI_NEEDS_REDRAW|UI_NEEDS_FULL_REDRAW);
  fr &= ~(UI_NEEDS_REDRAW|UI_NEEDS_FULL_REDRAW);
  
  l->ui_flags.exchange (fl);
  r->ui_flags.exchange (fr);
}

void c_waveformwidget::on_keydown (int keycode, bool keyrepeat) {
  debug ("got keycode %d,%s keyrepeat", keycode, keyrepeat ? "" : " not");
}

void c_waveformwidget::sync_state_to (c_waveformchannel *src) {
  c_waveformchannel *dest = l;
  if (src == dest) dest = r;
  
  
}

////////////////////////////////////////////////////////////////////////////////
// c_waveformchannel

c_waveformchannel::c_waveformchannel (c_customwidget *_parent) { 
  
  parent = _parent;
  
  col_wave_fg        = wxColour (  0, 255,   0, 255);
  col_wave_bg        = wxColour (  0,   0,   0, 255);
  col_wave_select_fg = wxColour (  0, 192,   0, 255);
  col_wave_select_bg = wxColour (  0,  40,   0, 255);
  col_cursor         = wxColour (255, 255,   0, 255);
  col_dothandle      = wxColour (  0, 255,   0, 255);
  col_dothighlight   = wxColour (255, 255,   0, 255);
  col_wavezoom       = wxColour (  0, 128,   0, 255);
  col_baseline       = wxColour (255, 255, 255, 255);
  col_grid           = wxColour ( 20,  32,  64, 255);
  
  init ();
}

void c_waveformchannel::init () {
  viewpos_x       = 0;
  viewsize_x      = -1;
  min_viewsize_x  = 32;
  viewpos_y       = 1.0;
  viewsize_y      = 0.0;
  min_viewsize_y  = 0.00001;
  
  ui_flags        = 0;
  baseline_px     = -1;
  pps             = -1;
  cursor_px       = -1;
  sel_begin_px    = -1;
  sel_end_px      = -1;
}

void c_waveformchannel::get_state (s_waveformstate *s) {
  s->cursor = cursor;
  s->selection = selection;
  s->viewpos_x = viewpos_x;
  s->viewsize_x = viewsize_x;
  s->viewpos_y = viewpos_y;
  s->viewsize_y = viewsize_y;
}

void c_waveformchannel::set_state (s_waveformstate *s) {
  set_cursor (s->cursor);
  zoom_x_to (s->viewpos_x, s->viewsize_x);
  zoom_y_to (s->viewpos_y, s->viewsize_y);
}

uint64_t c_waveformchannel::on_idle () {
  int framecycle = g_app->frame_counter % CURSOR_BLINK_SPEED;
  
  if (framecycle == 0 || framecycle == CURSOR_BLINK_SPEED / 2) 
    return UI_NEEDS_REDRAW;
  else
    return 0;
}

uint64_t c_waveformchannel::on_resize (int w, int h) {
  width = w;
  height = h;
  return UI_NEEDS_FULL_REDRAW;
}

bool c_waveformchannel::begin_drag_selection (bool extend) {
  debug ("start, extend=%d", (int) extend);
  select_mode_extend = extend;
  if (extend) {
    cursor = x_to_smp (mouse_x);
    if (cursor < 0) cursor = 0;
    if (cursor >= wb->size ()) cursor = wb->size () - 1;
  }
  ui_flags |= UI_DRAG_SELECT;
  if (extend)
    return extend_selection ();
  else
    return set_selection_from_cursor ();
  return true;
}

// extends from (extend ? middle of selection : cursor) either left or right
// selection = end being dragged, cursor = other end
bool c_waveformchannel::extend_selection () { 
  size_t sel_left, sel_right, sel_middle, mouse_pos;
  sel_left = std::min (cursor, selection);
  sel_right = std::max (cursor, selection);
  sel_middle = (sel_left + sel_right) / 2;
  mouse_pos = x_to_smp (mouse_x);
  if (mouse_pos < 0 && mouse_pos > dothandles.size ()) {
    debug ("mouse position out of range");
    return false;
  }
  
  if (mouse_pos >= 0 && mouse_pos < wb->size ()) {
    if (mouse_pos < sel_middle) {
      if (sel_right >= 0 && sel_right < wb->size ()) {
        selection = mouse_pos;
        cursor = sel_right;// = cursor;
      }
    } else {
      if (sel_left >= 0 && sel_left < wb->size ()) {
        selection = mouse_pos;
        cursor = sel_left;// = cursor;
      }
    }
  }
  //selection = mouse_pos;
  
  ui_flags |= UI_SELECTION_CHANGE;
  
  debug ("cursor=%ld, selection=%ld, mouse_pos=%ld", cursor, selection, mouse_pos);
  return true;
}

bool c_waveformchannel::set_selection_from_cursor () {
  bool ret = false;
  size_t new_sel = x_to_smp (mouse_x);
  if (new_sel != selection) {
    ret = true;
    ui_flags |= UI_SELECTION_CHANGE;
    selection = new_sel;
  }
  
  return ret;
}

bool c_waveformchannel::end_drag (bool cancel) { CP
  bool ret = false;
  size_t smp = -1;
  float v = y_to_amp (mouse_y);
  
  if (ui_flags & UI_DRAG_SELECT) {
    // this gets updated at each mousemove while dragging
    debug ("selection drag end");
    return ret;
  }
  
  if (selected_dot >= 0 && selected_dot < dothandles.size ())
    smp = dothandles [selected_dot].s;
  
  if (smp < 0) return false;
  if (smp >= wb->size ()) return false;
  
  if (ui_flags & UI_DRAG_HANDLE) {
    if (ui_flags & UI_DRAG_SELECT) {
      debug ("warning: dragging both selection and dot handle");
    }
    // commit vertical position to sample being dragged
    if (!cancel) {
      register_undo (UI_DRAG_HANDLE);
      if (v >= -1.0 && v <= 1.0) { 
        debug ("committing value %f to dot %d (smp %ld)", v, selected_dot, smp);
        c_wavebuffer &buf = *wb;
        buf [smp] = v;
      } else {
        debug ("value %f out of range", v);
      }
      selected_dot = -1;
    }
    ret = true;
  }
  
  return ret;
}

bool c_waveformchannel::begin_drag_dothandle () { CP
  bool ret = false;
  selected_dot = get_dot_under_mouse ();
  if (selected_dot < 0)
    ret = false;
  
  debug ("selected_dot=%d", selected_dot);
  return true;
}

void c_waveformchannel::set_cursor (size_t newcurs) {
  if (cursor != newcurs)
    ui_flags |= UI_NEEDS_REDRAW;
  
  cursor = newcurs;
}

void c_waveformchannel::set_selection (size_t newsel) {
  if (selection != newsel)
    ui_flags |= UI_NEEDS_FULL_REDRAW;
  
  selection = newsel;
}

bool c_waveformchannel::set_cursor_and_unselect () {
  bool ret = true;
  
  /*if (selection >= 0)*/ ui_flags = UI_SELECTION_CHANGE;
  //ui_flags |= UI_CURSOR_CHANGE;
  size_t smp = x_to_smp (mouse_x);
  if (smp < 0 || smp >= wb->size ()) {
    ret = false;
    selection = -1;
  } else {
    selection = smp;
  }
  cursor = smp;
  return ret;
}

uint64_t c_waveformchannel::on_mousedown_left () { CP
  
  if (ui_flags & UI_DRAG) {
    end_drag (true); // mouse-up happened outside of widget
    ui_flags |= UI_NEEDS_FULL_REDRAW;
  } else {
    if (kb_shift ()) { debug ("SHIFT, DRAGGING SELECTION");
      begin_drag_selection (selection > -1);
    } else {
      int dot = get_dot_under_mouse ();
      if (dot >= 0 && dot < dothandles.size ()) {
        ui_flags |= begin_drag_dothandle () ? UI_DRAG_HANDLE : 0;
      } else {
        ui_flags |= set_cursor_and_unselect () ? UI_DRAG_SELECT : 0;
      }
    }
  }
  ui_flags |= UI_MOUSEBUTTON_LEFT;
  return ui_flags;
}

uint64_t c_waveformchannel::on_mouseup_left () { CP

  ui_flags |= end_drag () ? UI_NEEDS_FULL_REDRAW : 0;
  
  ui_flags &= ~(UI_MOUSEBUTTON_LEFT|UI_DRAG_HANDLE|UI_DRAG);
  return ui_flags;
}

uint64_t c_waveformchannel::on_mouseup_right () { CP
  ui_flags &= ~UI_MOUSEBUTTON_LEFT;
  return 0;
}

uint64_t c_waveformchannel::on_mousedown_right () { CP
  ui_flags &= ~UI_MOUSEBUTTON_RIGHT;
  return false;
}

// TODO: horizontal scrolling on sideways drag pad movement
uint64_t c_waveformchannel::on_mousewheel_h (int howmuch) {
  debug ("howmuch=%d", howmuch);
  return false;
}

uint64_t c_waveformchannel::on_mousewheel_v (int howmuch) {
  bool redraw = false;
  debug ("howmuch=%d, viewsize_x=%d", howmuch, viewsize_x);
  
  if (kb_ctrl () && kb_shift ()) {
    debug ("ctrl+shift+mousewheel");
    if (howmuch > 0)
      redraw |= scroll_down_by (0.02);
    else
      redraw |= scroll_up_by (0.02);
  } else if (kb_ctrl ()) {
    debug ("ctrl+mousewheel");
    if (howmuch < 0)
      redraw |= zoom_y (1.0 / 1.1, mouse_y);
    else
      redraw |= zoom_y (1.1, mouse_y);
  } else if (kb_shift ()) {
    debug ("shift+mousewheel");
    if (howmuch < 0)
      redraw |= scroll_right_by (viewsize_x / 16);
    else
      redraw |= scroll_left_by (viewsize_x / 16);
  } else if (kb_alt ()) {
    debug ("alt+mousewheel");
  } else {
    debug ("mousewheel with no ctrl/alt/shift");
    if (howmuch > 0)
      redraw |= zoom_x (1.1);
    else
      redraw |= zoom_x (-1.1);
  }
  return redraw ? UI_NEEDS_FULL_REDRAW : 0;
}

bool c_waveformchannel::drag_selection () {
  ui_flags |= UI_SELECTION_CHANGE;
  if (select_mode_extend)
    return extend_selection ();
  else
    return set_selection_from_cursor ();
}

uint64_t c_waveformchannel::on_mousemove (int x, int y) {
  uint64_t ret = 0;
  mouse_x = x;
  mouse_y = y;
  int64_t smp = x_to_smp (mouse_x);
  int64_t len = wb->size ();
  float sampleamp = y_to_amp (mouse_y);
  
  //debug ("ui_flags=%lx", ui_flags.load ());
  
  if (ui_flags & UI_DRAG_HANDLE && 
      smp >= 0 && smp < len) { 
    //debug ("dragging dot handle %d to %d", selected_dot, mouse_y);
    ret |= UI_NEEDS_FULL_REDRAW;
  } else if (ui_flags & UI_DRAG_SELECT) {
    //debug ("extending selection");
    ret |= drag_selection ();
  } else { // just hovering, check for sample dot/handle to highlight
    //debug ("hovering");
    int dot_old = highlighted_dot;
    highlighted_dot = get_dot_under_mouse ();
    if (dot_old != highlighted_dot) { CP ret |= UI_NEEDS_REDRAW; }
  }
  debug ("cur=%ld, sel=%ld, pps=%d, sel_px=%d,%d",
         cursor, selection, pps, sel_begin_px, sel_end_px);
  return ret;
}

bool c_waveformchannel::zoom_x_to (size_t newpos, size_t newsize) {
  bool ret = false;
  if (newpos != viewpos_x || newsize != viewsize_x) {
    ui_flags |= UI_SELECTION_CHANGE;
    ret = true;
  }
  
  viewpos_x = newpos;
  viewsize_x = newsize;
  
  return clamp_coords () || ret;
}

bool c_waveformchannel::zoom_y_to (float newpos, float newsize) { CP
  bool ret = false;
  if (newpos != viewpos_y || newsize != viewsize_y) {
    ui_flags |= UI_SELECTION_CHANGE;
    ret = true;
  }
  
  viewpos_y = newpos;
  viewsize_y = newsize;
  
  return clamp_coords () || ret;
}

bool c_waveformchannel::zoom_full_x () { CP
  if (!wb) return 0;
  //viewsize_x = wb->size ();
  //viewpos_x = 0;
  //return true;
  return zoom_x_to (0, wb->size ());
}

bool c_waveformchannel::zoom_full_y () { CP
  if (!wb) return false;
  
  //viewsize_y = 2.0;
  //viewpos_y = 1.0;
  //return true;
  return zoom_y_to (1.0, 2.0);
}

bool c_waveformchannel::scroll_left_by (int howmuch) { CP
  bool ret = true;
  
  if (!wb) return false;
  viewpos_x -= howmuch;
  int max = wb->size () - viewsize_x;
  if (viewpos_x < 0) { ret = true; viewpos_x = 0; }
  if (viewpos_x > max) { ret = true; viewpos_x = max; }
  
  ui_flags |= UI_SCROLL_H;
  
  debug ("viewpos=%d", viewpos_x);
  return ret;
}

bool c_waveformchannel::scroll_right_by (int howmuch) { CP
  return scroll_left_by (howmuch * -1);
}

bool c_waveformchannel::scroll_up_by (float howmuch) { CP
  if (!wb) return false;
  viewpos_y += howmuch;
  /*float max_scroll_y = 1.0 - viewsize_y;// - (1 - (1/viewpos_y));
  
  if (viewpos_y < -1) viewpos_y = -1;
  if (viewpos_y > max_scroll_y) viewpos_y = max_scroll_y;*/
  ui_flags |= UI_SCROLL_V;
  
  return clamp_coords ();
}

bool c_waveformchannel::scroll_down_by (float howmuch) { return scroll_up_by (-1 * howmuch); }

bool c_waveformchannel::clamp_coords () { CP
  bool ret = false;
  
  if (!wb) return false;
  
  if (viewsize_y < min_viewsize_y)   { CP ret = true; viewsize_y = min_viewsize_y; }
  if (viewsize_y > 2.0)              { CP ret = true; viewsize_y = 2.0; }
  if (viewpos_y < -1.0 + viewsize_y) { CP ret = true; viewpos_y = -1.0 + viewsize_y; }
  if (viewpos_y >  1.0)              { CP ret = true; viewpos_y =  1.0;}
  debug ("viewpos_y=%f", viewpos_y);

  int sz = wb->size ();
  if (viewsize_x < 32)               { ret = true; viewsize_x = 32; }
  if (viewsize_x > sz)               { ret = true; viewsize_x = sz; }
  if (viewpos_x < 0)                 { ret = true; viewpos_x = 0;   }
  if (viewpos_x > sz - viewsize_x)   { ret = true; viewpos_x = sz - viewsize_x; }
  
  return ret;
}

bool c_waveformchannel::zoom_x (float ratio) { CP
  if (!wb) return false;
  float vs = viewsize_x;
  float oldviewsize_x = viewsize_x;
  size_t newsize = viewsize_x;
  size_t newpos = viewpos_x;
  debug ("ratio=%f", ratio);
  
  if (ratio < 0)
  ratio = -1 / ratio;
  
  newsize = viewsize_x / ratio;
  
  float xpos = (float) mouse_x / (float) width;
  debug ("xpos=%f", xpos);
  newpos = viewpos_x + (oldviewsize_x - newsize) * xpos;
  
  bool ret = zoom_x_to (newpos, newsize);
  ret |= clamp_coords ();
  return ret;
}

bool c_waveformchannel::zoom_y (float ratio, int around_px) { CP
  if (!wb) return false;
  float min_size_y = 0.0001;
  float t = (float) around_px / (float) (height - 1); // TODO: fix this (height) for stereo
  float anchor_at = viewpos_y - t * viewsize_y;

  // apply zoom
  float new_size = viewsize_y / ratio;

  // solve: anchor_at = newViewpos - t * new_size
  float newpos = anchor_at + t * new_size;
  float newsize = new_size;
  
  return zoom_y_to (newpos, newsize);

  //return clamp_coords ();
}


int c_waveformchannel::get_dot_under_mouse () {
  int thr = 8, i;
  struct sxy *h = NULL;
  int s = dothandles.size ();
  
  for (i = 0; i < s; i++) {
    if (dothandles [i].s < 0) return -1;
    if (mouse_x > dothandles [i].x - thr && mouse_x < dothandles [i].x + thr &&
        mouse_y > dothandles [i].y - thr && mouse_y < dothandles [i].y + thr) {
      return i;
    }
  }
  
  return -1;
}

void c_waveformchannel::calculate_positions () {
  
  baseline_px = amp_to_y (0);
  
  pps = width / viewsize_x;
  
  sel_begin_px = -1;
  sel_end_px = -1;
  
  // selection
  //if (cursor >= 0 && selection < viewsize_x + 1 && selection >= 0) {
  if (cursor >= 0 && selection < wb->size () && selection >= 0) {
    // samples where to draw selection rect
    sel_begin = std::min (cursor, selection);
    sel_end = std::max (cursor, selection);

    // clamp to visible
    sel_begin = std::max ((size_t) sel_begin, (size_t) viewpos_x);
    sel_end = std::min ((size_t) sel_end, (size_t) (viewpos_x + viewsize_x));
    
    // convert to pixel coords
    sel_begin_px = smp_to_x (sel_begin);
    sel_end_px = smp_to_x (sel_end);
    if (sel_begin_px < 0) sel_begin_px = 0;
    if (sel_end_px > width) sel_end_px = width;
  }
}

void c_waveformchannel::on_paint (wxDC &dc) {
  calculate_positions ();
  bool do_repaint = false;
  //debug ("width=%d, height=%d", width, height);
  int thr = 8, i, dot;
  int dotsz = 12;
  int draw_y = -1;
  struct sxy *h = NULL;
  wxRect rect;
  
  //dc_scope scope (dc, rect);
  
  if (ui_flags & UI_DRAG_HANDLE && mouse_x > -1 && mouse_y > -1) {
    
    dot = selected_dot;
    h = &dothandles [dot];
    if (dot >= 0 || dot < dothandles.size ()) {
      draw_y = mouse_y;
      dc.SetPen (wxPen (col_wave_fg));
      int prev_y = baseline_px;
      int next_y = baseline_px;
      if (dot >= 1) {
        prev_y = dothandles [dot - 1].y;
        draw_curve (dc, h->x, draw_y, dothandles [dot - 1].x, dothandles [dot - 1].y);
        // dc.DrawLine (h->x, draw_y, dothandles [dot - 1].x, dothandles [dot - 1].y);
      }
      if (dot < dothandles.size () - 1) {
        next_y = dothandles [dot + 1].y;
        draw_curve (dc, h->x, draw_y, dothandles [dot + 1].x, dothandles [dot + 1].y);
        // dc.DrawLine (h->x, draw_y, dothandles [dot + 1].x, dothandles [dot + 1].y);
      }
    }
  } else {
    dot = get_dot_under_mouse ();
    h = &dothandles [dot];
    if (dot >= 0 || dot < dothandles.size ()) {
      draw_y = h->y;
    }
  }
  
  // dot? draw it
  if (dot >= 0 && dot < dothandles.size ()) {
    //debug ("handle: %d,%d,%d", h->x, h->y, h->s);
    rect = wxRect (h->x - dotsz / 2, draw_y - dotsz / 2, dotsz, dotsz);
    dc.SetBrush (wxBrush ());
    dc.SetPen (wxPen (col_dothighlight));
    dc.DrawRectangle (rect);
  }
  if (g_app->frame_counter % CURSOR_BLINK_SPEED < CURSOR_BLINK_SPEED / 2) {
    draw_cursor (dc);
  }
  ui_flags &= ~UI_NEEDS_REDRAW;
}

void c_waveformchannel::render_base (wxDC &dc) { CP
  calculate_positions ();
  draw_bg (dc);
  draw_select_bg (dc);
  draw_waveform (dc);
  draw_baseline (dc);
}

void c_waveformchannel::draw_grid (wxDC & dc) {
  int yt, yb, last_yt = 0, last_yb = 0;
  float l;
  int distance_thresh = height / 64;
  
  dc.SetPen (wxPen (col_grid));
  for (int i = 0; i < 12; i += 3) {
    l = db_to_linear (i * -1);
    yt = amp_to_y (l);
    yb = amp_to_y (l * -1);
    //debug ("i=%d, l=%f, yt=%d, yb=%d", i, l, yt, yb);
    if (yt >= 0 && yt < height) {
      if (abs (yt - last_yt) > distance_thresh) {
        dc.DrawLine (0, yt, width, yt);
        last_yt = yt;
      }
    }
    if (yb >= 0 && yb < height) {
      if (abs (yb - last_yb) > distance_thresh) {
        dc.DrawLine (0, yb, width, yb);
        last_yb = yb;
      }
    }
  }
}

void c_waveformchannel::draw_baseline (wxDC & dc) {
  baseline_px = amp_to_y (0);
  if (baseline_px > 0 && baseline_px < height) {
    dc.SetPen (wxColour (col_baseline));
    dc.DrawLine (0, baseline_px, width, baseline_px);
    ui_flags &= ~UI_NEEDS_FULL_REDRAW;
  } else
    baseline_px = -1;
}

void c_waveformchannel::draw_bg (wxDC &dc) {
  // black background
  dc.SetBrush (wxBrush (col_wave_bg));
  dc.SetPen (wxPen (col_wave_bg));
  dc.DrawRectangle (0, 0, width, height);
  
  // select
  dc.SetBrush (wxBrush (col_wave_select_bg));
}

void c_waveformchannel::draw_select_bg (wxDC &dc) {
  debug ("sel_begin/end=%ld,%ld  px=%d,%d", sel_begin, sel_end, sel_begin_px, sel_end_px);
  /*size_t total = wb->size ();
  if (selection < 0 || cursor < 0) return
  if (selection >= total || cursor >= total) return;
  if (selection == cursor) {
    selection = -1;
    return;
  }
  
  size_t sel_begin = std::min (selection, cursor);
  size_t sel_end = std::max (slection, cursor);
  
  sel_begin = 
  */
  if (selection == cursor)
    selection = -1;
  
  if (selection < 0 || selection >= wb->size ())
    return;
  
  dc.SetBrush (wxBrush (col_wave_select_bg));
  dc.SetPen (wxPen (col_wave_select_bg));
  dc.DrawRectangle (sel_begin_px/* - pps / 2*/, 1, 
                    sel_end_px - sel_begin_px/* + pps*/, height - 2);
}

// draws waveform to dc, and calculates positions & sample mapping of dot handles
void c_waveformchannel::draw_waveform (wxDC &dc) {
  if (!wb || wb->size () <= 0 || viewsize_x <= 0 || width < 4 || height < 4)
    return; // nothing to draw / not enough space
  
  c_wavebuffer &buf = *wb;
  
  debug ("width=%d, height=%d", width, height);
  debug ("viewpos_x=%ld, viewsize_x=%ld, viewpos_y=%f, viewsize_y=%f",
         viewpos_x, viewsize_x, viewpos_y, viewsize_y);
  
  int dotmode_thr = 16; // min. pixels between samples when zoomed way in
  int sz = viewsize_x + 1;
  int pos = viewpos_x;
  size_t sel_begin, sel_end;
  //int sel_begin_px, sel_end_px;
  int w = width - 1;
  int h = height - 1;
  int i, j, wl, wr, si, lx, ly, x, y, dh;
  float hi, lo, hi_min, hi_max, lo_min, lo_max, spos, xpos, v, z;
  /*int*/ cursor_px = -1;
  bool oob_hi = false, oob_lo = false;
  bool over = false, under = false;
  
  calculate_positions ();
  
  if (viewsize_x <= 0) viewsize_x = buf.size ();
  
  if (sz < 0) sz = 0;
  if (sz > buf.size ()) sz = buf.size ();
  if (pos < 0) pos = 0;
  if (pos > buf.size () - sz) pos = buf.size () - sz;
  if (pos < 0 || pos > buf.size () - sz) return;
  
  if (dothandles.size () > 0) {
    dothandles [0].x = -1;
    dothandles [0].y = -1;
    dothandles [0].s = -1;
  }
  
  draw_grid (dc);
  //debug ("cursor=%ld, cursor_px=%d, pps=%d", cursor, cursor_px, pps);
  
  dc.SetPen (wxPen (col_wave_fg));
  
  // now we decide how to draw depending on zoom level:
  //   A) process each block of samples for given pixel
  //   B) draw a line to each pixel from last
  //   C) draw a line to each sample from last
  //   D) "dot edit" mode: like C except add "handle" for each visible sample
  
  if (sz > w * 2) { // more than 2 samples per pixel: per block from wl to wr
    debug ("branch A: >= 1 samples per pixels");
    selected_dot = highlighted_dot = -1;
    for (i = 1; i < w; i++) { // this one was a handful!!!
      over = under = oob_hi = oob_lo = false;
      spos = (float) (i - 1) / (float) w;
      wl = (float) spos * (float) sz;
      spos = (float) i / (float) w;
      wr = (float) spos * (float) sz;
      hi_max = -1;
      lo_max = -1;
      lo_min = 1;
      lo_max = 1;
      
      if (wl < 0) wl = 0;
      if (wl > sz) wl = sz;
      if (wr < wl) wr = wl;
      if (wr > sz) wr = sz;
      for (j = wl; j < wr; j++) { // find min/max in this block
        if (buf [pos + j] > hi_max) hi_max = buf [pos + j];
        if (buf [pos + j] > hi_min) hi_min = buf [pos + j];
        if (buf [pos + j] < lo_max) lo_max = buf [pos + j];
        if (buf [pos + j] < lo_min) lo_min = buf [pos + j];
      }
      
      // calculate screen coords relative to top left of waveform
      hi_max = amp_to_y (hi_max);
      hi_min = amp_to_y (hi_min);
      lo_max = amp_to_y (lo_max);
      lo_min = amp_to_y (lo_min);
      
      if (i == sel_begin_px)
        dc.SetPen (wxPen (col_wave_select_fg));
      else if (i == sel_end_px + 1)
        dc.SetPen (wxPen (col_wave_fg));
      /*if (sz < w * 4) {
        hi_max = std::min ((int) baseline_px, (int) hi_max);
        lo_min = std::max ((int) baseline_px, (int) lo_min);
      }*/
      draw_curve (dc, i, hi_max, i, lo_min);
      // dc.DrawLine (i, hi_max, i, lo_min);
    }
  } else if (sz > w / 2) { // zigzag between pixels
    debug ("branch B: sz > w (1 to 2 samples per pixel)");
    selected_dot = highlighted_dot = -1;
    y = amp_to_y (buf [viewpos_y]);
    for (i = 0; i < w - 1; i++) {
      spos = (float) (i - 1) / (float) w;
      si = spos * (float) sz;
      v = buf [pos + si];
      y = amp_to_y (v);
      
      if (i == sel_begin_px)
        dc.SetPen (wxPen (col_wave_select_fg));
      else if (i == sel_end_px + 1)
        dc.SetPen (wxPen (col_wave_fg));

      //if (y < h && y >= 0 && i > 0) {
      if (i > 0)
        draw_curve (dc, i, ly, i + 1, y);
        // dc.DrawLine (i, ly, i + 1, y);
      //}
      ly = y;
    }
  } else if (sz > w / dotmode_thr) { // zigzag bewidtheen samples
    debug ("branch C: sz > w / dotmode_thr (1 to 2 samples per pixel)");
    selected_dot = highlighted_dot = -1;
    lx = smp_to_x (pos);
    ly = amp_to_y (buf [pos]);
    for (i = 1; i < sz; i++) {
      xpos = (float) (i - 1) / (float) viewsize_x;
      v = buf [i + pos];
      x = smp_to_x (i + pos) - pps / 2;
      y = amp_to_y (v);
      
      if (i == sel_begin)
        dc.SetPen (wxPen (col_wave_select_fg));
      else if (i == sel_end + 1)
        dc.SetPen (wxPen (col_wave_fg));

      //if (y < h && y >= 0)
      if (i > 0)
        draw_curve (dc, lx, ly, x, y);
        // dc.DrawLine (lx, ly, x, y);
      lx = x;
      ly = y;
    }
  } else { // >= dotmode_thr pixels per sample: "dot edit" mode
    debug ("branch D: dot edit mode");
    int pxps = w / sz;
    int nh = 1 + (w / pxps);
    int dot_skip = -1;
    if (dothandles.size () != nh)
      dothandles.resize (nh);
    dc.SetPen (wxPen (col_wavezoom));
    dh = 0;
    lx = smp_to_x (pos);
    ly = amp_to_y (buf [pos]);
    for (i = 0; i < sz; i++) {
      v = buf [i + pos];
      x = smp_to_x (i + pos) + pps / 2;
      
      if (selected_dot >= 1 && selected_dot < dothandles.size () &&
                dothandles [selected_dot].s == pos) {
        y = mouse_y;
        debug ("dothandles [%d].s == %ld - using mouse_y to draw line",
               selected_dot, dothandles [selected_dot].s);
      } else {
        y = amp_to_y (v);
      }
      
      /*if (i + pos == sel_begin)
        dc.SetPen (wxPen (col_wave_select_fg));
      else if (i + pos == sel_end + 1)
        dc.SetPen (wxPen (col_wave_fg));*/
      
      //if (y >= 0 && y < h)
        if (!(ui_flags & UI_DRAG_HANDLE) || (i != selected_dot &&
            i != selected_dot + 1)) 
          draw_curve (dc, lx, ly, x, y);
          // dc.DrawLine (lx, ly, x, y);
        else
          dot_skip = i - 1;
        
      if (dh < nh) {
        dothandles [dh].x = x;
        dothandles [dh].y = y;
        dothandles [dh].s = i + pos;
        dh++;
      }
      lx = x;
      ly = y;
    }
    dc.SetPen (wxPen (col_dothandle));
    dc.SetBrush (wxBrush (col_dothandle));
    for (i = 0; i < dh; i++) {
      if (!(dot_skip == i && ui_flags & UI_DRAG_HANDLE))
        dc.DrawRectangle (dothandles [i].x - (handlesize / 2),
                          dothandles [i].y - (handlesize / 2),
                          handlesize, handlesize);
    }
    // clear rest of handles
    int n = dothandles.size ();
    for (; i < n; i++) {
      dothandles [i].x = -1;
      dothandles [i].y = -1;
      dothandles [i].s = -1;
    }
  }
}

void c_waveformchannel::draw_cursor (wxDC &dc) {
  //debug ("cursor=%ld, selection=%ld", cursor, selection);
  if (cursor > wb->size () || cursor < 0)
    cursor = 0;
  if (selection == cursor)
    selection = -1;
  
  if (cursor >= viewpos_x && cursor < viewsize_x + viewpos_x && selection < 0) {
    //if (pps <= 0) pps = 1;
    cursor_px = smp_to_x (cursor);// - pps / 2;
    //debug ("viewpos_x=%d, viewsize_x=%d, pps=%d, cursor=%ld, cursor_px=%d",
    //       viewpos_x, viewsize_x, pps, cursor, cursor_px);
    dc.SetPen (wxPen (wxColour (col_cursor)));
    for (int i = 0; i < height / 2; i += 2) {
      dc.DrawLine (cursor_px, i * 2, cursor_px, 1 + i * 2);
    }
  }
}

bool c_waveformchannel::select_waveform (c_wavebuffer *entry) { CP
  if (!entry) { 
    unselect_waveform ();
    return false;
  }
  wb = entry;
  
  zoom_full_x ();
  zoom_full_y ();
  
  ui_flags |= UI_NEEDS_FULL_REDRAW;
  return true;
}

bool c_waveformchannel::unselect_waveform () { CP
  wb = NULL;
  ui_flags |= UI_NEEDS_FULL_REDRAW;
  return true;
}

c_wavebuffer *c_waveformchannel::get_selected () { CP
  return wb;
}


#endif // USE_WXWIDGETS

