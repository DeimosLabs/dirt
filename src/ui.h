
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
 * See header file and --help text for more info
 */
 
 
#ifndef    __DIRT_UI_H
#define    __DIRT_UI_H
#define __IN_DIRT_UI_H

#include "wx/wx.h"
#include "wxwidgets/mainwindow.h"
#include "deconvolv.h"

class c_mainwindow : public ui_mainwindow {
public:
  c_mainwindow (c_deconvolver *d = NULL);
  ~c_mainwindow ();
  
  bool init_audio ();
  bool init_audio (int samplerate, bool stereo);
  bool shutdown_audio ();
  bool audio_ready ();
  void set_statustext (const char *str, ...);
  void set_mode (long int mode);
  void set_prefs (s_prefs *prefs);
  void get_prefs (s_prefs *prefs);
  void update_audio_ports ();
  bool make_dry_sweep (bool load_it = true);
  void show_error (std::string str);
  void show_message (std::string str);
  
  // event handlers
  //void on_debug (wxCommandEvent &ev);
  void on_close (wxCloseEvent &ev);
  //void on_keydown (wxKeyEvent &ev);
  void on_resize (wxSizeEvent &ev);
  void on_about (wxCommandEvent &ev);
  void on_quit (wxCommandEvent &ev);
  void on_radio_file (wxCommandEvent &ev);
  void on_radio_playsweep (wxCommandEvent &ev);
  void on_radio_makesweep (wxCommandEvent &ev);
  void on_radio_roundtrip (wxCommandEvent &ev);
  void on_checkbox (wxCommandEvent &ev);
  //void on_prefs (wxCommandEvent &ev);
  //void on_quit (wxCommandEvent &ev);
  //void on_menu_highlight (wxMenuEvent &ev);
  //void on_debug_stuff_toggled (bool b);
  void on_btn_dryfile_browse (wxCommandEvent &ev);
  void on_btn_generate (wxCommandEvent &ev);
  void on_btn_dry_save (wxCommandEvent &ev);
  void on_btn_inputdir_scan (wxCommandEvent &ev);
  void on_btn_inputdir_browse (wxCommandEvent &ev);
  void on_btn_inputfiles_add (wxCommandEvent &ev);
  void on_btn_inputfiles_clear (wxCommandEvent &ev);
  void on_btn_play (wxCommandEvent &ev);
  void on_btn_audio (wxCommandEvent &ev);
  void on_btn_process (wxCommandEvent &ev);
  void on_port_select (wxCommandEvent &ev);
  void on_timer (wxTimerEvent &ev);
  
  c_deconvolver *dec = NULL;
  std::vector <float> drysweep;

private:
  wxDECLARE_EVENT_TABLE ();
  void set_enable (wxWindow *w, bool b);
  void disable (wxWindow *w) { set_enable (w, false); }
  void enable (wxWindow *w) { set_enable (w, true); }
  
  wxTimer timer;
  bool init_audio_done = false;
  int mode = ID_FILE;
  int prev_mode = -1;
  int prev_audio_state = -1;
};

class c_deconvolver_gui : public c_deconvolver {
public:
  c_deconvolver_gui (struct s_prefs *prefs = NULL, std::string name = "")
    : c_deconvolver (prefs, name) {}
};

class c_app : public wxApp {
public:
  c_app (c_deconvolver *d);
  ~c_app ();
  bool OnInit ();
  //int OnRun ();
  int OnExit ();

  c_mainwindow *mainwindow = NULL;
  c_deconvolver *dec = NULL;
  
protected:
  //virtual int FilterEvent (wxEvent &ev);
  
private:
};

wxDECLARE_APP (c_app);

int wx_main (int argc, char **argv, c_deconvolver *p);

extern c_app *g_app;

#undef  __IN_DIRT_UI_H
#endif  // __DIRT_UI_H

