
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

#include "wx/wx.h"
#include "wxwidgets/mainwindow.h"

class c_mainwindow : public ui_mainwindow {
public:
  c_mainwindow ();
  ~c_mainwindow ();
  
  //void on_debug (wxCommandEvent &ev);
  void on_close (wxCloseEvent &ev);
  //void on_keydown (wxKeyEvent &ev);
  void on_resize (wxSizeEvent &ev);
  void on_Close (wxCloseEvent &ev);
  void on_about (wxCommandEvent &ev);
  void on_ok (wxCommandEvent &ev);
  void on_cancel (wxCommandEvent &ev);
  //void on_prefs (wxCommandEvent &ev);
  //void on_quit (wxCommandEvent &ev);
  //void on_menu_highlight (wxMenuEvent &ev);
  //void on_debug_stuff_toggled (bool b);
  
  void set_statustext (const char *str, ...);

private:
  wxDECLARE_EVENT_TABLE ();
};

class c_app : public wxApp {
public:
  c_app ();
  ~c_app ();
  bool OnInit ();
  //int OnRun ();
  int OnExit ();

  c_mainwindow *mainwindow = NULL;
protected:
  //virtual int FilterEvent (wxEvent &ev);
  
private:
};

wxDECLARE_APP (c_app);
