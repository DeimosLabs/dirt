
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

#ifdef USE_WXWIDGETS

#include <wx/wx.h>
#include <wx/arrstr.h>

#include "wxwidgets/mainwindow.h"
#include "deconvolv.h"

extern int64_t get_unique_id ();

class ir_entry {
public:
  int64_t id = 0;
  int samplerate = DEFAULT_SAMPLERATE;
  c_wavebuffer l;
  c_wavebuffer r;
  std::string path;
  bool dirty = false;
  ir_entry () { id = ::get_unique_id (); }
};

class c_customwidget;
class c_meterwidget;
class c_deconvolver_gui;

enum class meterwarn {
  REC,
  CLIP,
  XRUN,
  MAX
};

// app
class c_app : public wxApp {
public:
  c_app (c_deconvolver_gui *d);
  ~c_app ();
  bool OnInit ();
  //int OnRun ();
  int OnExit ();

  c_mainwindow *mainwindow = NULL;
  c_deconvolver *dec = NULL;
  c_audioclient *audio = NULL;
  
protected:
  virtual int FilterEvent (wxEvent &ev);
  
private:
};

wxDECLARE_APP (c_app);

// main window
class c_mainwindow : public ui_mainwindow {
public:
  c_mainwindow (c_deconvolver *d = NULL);
  ~c_mainwindow ();
  
  bool init_audio ();
  bool init_audio (int samplerate, bool stereo);
  bool disable_audio ();
  bool is_ready ();
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
  void on_chk_forcemono (wxCommandEvent &ev);
  //void on_prefs (wxCommandEvent &ev);
  //void on_quit (wxCommandEvent &ev);
  //void on_menu_highlight (wxMenuEvent &ev);
  //void on_debug_stuff_toggled (bool b);
  void on_btn_dryfile_browse (wxCommandEvent &ev);
  void on_btn_generate (wxCommandEvent &ev);
  void on_btn_dry_save (wxCommandEvent &ev);
  void on_btn_inputdir_scan (wxCommandEvent &ev);
  void on_btn_inputdir_browse (wxCommandEvent &ev);
  void on_btn_outputdir_browse (wxCommandEvent &ev);
  void on_btn_align_manual (wxCommandEvent &ev);
  void on_btn_inputfiles_add (wxCommandEvent &ev);
  void on_btn_inputfiles_clear (wxCommandEvent &ev);
  void on_btn_play (wxCommandEvent &ev);
  void on_btn_audio (wxCommandEvent &ev);
  void on_btn_process (wxCommandEvent &ev);
  void on_btn_irrename (wxCommandEvent &ev);
  void on_btn_irremove (wxCommandEvent &ev);
  void on_btn_irload (wxCommandEvent &ev);
  void on_btn_irsave (wxCommandEvent &ev);
  void on_port_select (wxCommandEvent &ev);
  void on_timer (wxTimerEvent &ev);
  
  void set_vu_l (float level, float hold, bool clip = false, bool xrun = false);
  void set_vu_r (float level, float hold, bool clip = false, bool xrun = false);
  
  c_deconvolver *dec = NULL;
  c_audioclient *audio = NULL;
  c_wavebuffer drysweep;

private:
  wxDECLARE_EVENT_TABLE ();
  void set_enable (wxWindow *w, bool b);
  void disable (wxWindow *w) { set_enable (w, false); }
  void enable (wxWindow *w) { set_enable (w, true); }
  int add_files (std::vector<std::string> list);
  int add_dir (std::string dir, bool recurs);
  int update_ir_list ();
  bool add_ir (ir_entry &entry);
  
  std::vector <ir_entry> ir_files;
  wxTimer timer;
  std::string cwd;
  bool init_audio_done = false;
  bool last_forcemono = false;
  int last_ir_count = 0;
  long int mode = ID_FILE;
  long int prev_mode = -1;
  audiostate prev_audio_state = (audiostate) -1;
  //c_meterwidget *testwidget = NULL;
};

class c_deconvolver_gui : public c_deconvolver {
public:
  c_deconvolver_gui (struct s_prefs *prefs = NULL)
    : c_deconvolver (prefs) {}
  
  //void set_vu_l (float level, float hold, bool clip, bool xrun);
  //void set_vu_r (float level, float hold, bool clip, bool xrun);

};

/*
 * Custom widget example/template - (usually) base class.
 *
 * Widget class derived directly from wxPanel, where we override/bind
 * paint events etc. to implement our own appearance and behaviour.
 *
 * NOTE TO SELF: New custom widgets should derive from this class, or (if
 * absolutely necessary) copy/paste it and change the name to something
 * else/descriptive/meaningful.
 *
 */

//comment this out for actual use
//#define SHOW_CUSTOMWIDGET_TEST_STUFF

class c_customwidget : public wxPanel/*, public has_slots <>*/ {
public:
  c_customwidget (wxWindow *parent = NULL,
                  int id = -1,//ID_FOREIGN,
                  wxPoint pos = wxDefaultPosition,
                  wxSize size = wxDefaultSize,
                  wxBorder border = wxSIMPLE_BORDER);
  ~c_customwidget ();
  virtual void set_opacity (int opacity);
  virtual void inspect ();
  int width, height;
  
  //signal1 <c_customwidget *> sig_left_clicked;
  //signal1 <c_customwidget *> sig_middle_clicked;
  //signal1 <c_customwidget *> sig_right_clicked;
  //signal3 <c_customwidget *, int, int> sig_mouse_move;
  //signal1 <c_customwidget *> sig_mouse_enter;
  //signal1 <c_customwidget *> sig_mouse_leave;
  
  virtual bool render_base_image ();
  //virtual void render_base_image (wxMemoryDC *dc);
  virtual bool update (wxWindowDC &dc);
  //void schedule_deletion ();
  
  static wxBitmap render_text_label (char *text, wxFont &font, const wxColour &fg, const wxColour &bg, 
                                    int max_width, int max_height, int align, int gradient_h, int gradient_v,
                                    char **r_last_visible_char = NULL);
                                    
  wxColour col_default_bg;
  wxColour col_default_fg;
  wxColour col_bg;
  wxColour col_fg;
  wxColour col_bezel1;
  wxColour col_bezel2;
  wxFont font;
  wxFont smallboldfont;
  
protected:
  virtual bool update ();
  // event handler boilerplate - might be more to come if needed
  virtual void on_paint_event (wxPaintEvent &evt);
  virtual void on_resize_event (wxSizeEvent &evt);
  virtual void on_mousemove (wxMouseEvent &event);
  virtual void on_mousedown_left (wxMouseEvent &event);
  virtual void on_mouseup_left (wxMouseEvent &event);
  virtual void on_mousedown_middle (wxMouseEvent &event);
  virtual void on_mouseup_middle (wxMouseEvent &event);
  virtual void on_mousedown_right (wxMouseEvent &event);
  virtual void on_mouseup_right (wxMouseEvent &event);
  virtual void on_mouseleave (wxMouseEvent &event);
  virtual void on_mousewheel (wxMouseEvent &event);
  //void onkeypress (wxKeyEvent &event);
  //void onkeyrelease (wxKeyEvent&event);
  //void onidle_callback (wxIdleEvent &event);
  virtual void on_visible_callback (wxShowEvent &ev);
  
  inline void get_xy (wxMouseEvent &ev) { mouse_x = ev.GetX (); mouse_y = ev.GetY (); }
  bool check_click_distance (int which_button);
  
#ifdef SHOW_CUSTOMWIDGET_TEST_STUFF
  virtual char *get_example_label_text ();
#endif

  bool visible;
  int opacity;
  int mouse_x;
  int mouse_y;
  int mousedown_x [8];
  int mousedown_y [8];
  int mouse_buttons;
  bool in_paintevent;
  bool base_image_valid;
  bool deleted;
  int click_distance;
  wxWindow *parent;
  wxBitmap img_base;
  wxBitmap img_overlay;
  wxBitmap image;
  wxFontMetrics fm;
  
  DECLARE_EVENT_TABLE ();
};

class c_meterwidget : public c_customwidget {
public:
  c_meterwidget (wxWindow *parent = NULL,
                  int id = -1,//ID_FOREIGN,
                  wxPoint pos = wxDefaultPosition,
                  wxSize size = wxDefaultSize,
                  int wtfisthis = -1);
  ~c_meterwidget () {}
  
  bool render_base_image ();
  bool update (wxWindowDC &dc);
  void on_resize_event (wxSizeEvent &ev);
  void render_gradient_bar ();
  void set_vudata (c_vudata *v);
  c_vudata *get_vudata ();
  bool needs_redraw () { return data ? data->needs_redraw : false; }
  
  //void set_audio_client (c_audioclient *cli);
  void set_stereo (bool b);
  void set_l (float level, float hold, bool clip = false, bool xrun = false);
  void set_r (float level, float hold, bool clip = false, bool xrun = false);

  bool vertical = false;
  //float l       = 0.0;
  //float r       = 0.0;

  wxFont tinyfont;
  //bool  show_rec    = false;  // enable red circle / recording indicator
  //bool  show_clip   = false;  // warn when clipping, (for now) also used for xruns
  int   clip_size    = 0;
  int   rec_size     = 0;
  bool  rec_enabled  = false;
  //bool  needs_redraw = false;
  c_vudata *data     = NULL;
  
protected:
private:
  
  /*void draw_bar (wxDC &dc, int t, int h, bool is_right, float level, float hold, 
                   bool clip = false, bool xrun = false, bool show_rec = false);*/
  void draw_bar (wxDC &dc, int t, int h, bool is_right, float level, float hold);
  
  // drawing related stuff
  // floats for math precision, but these are in pixels
  bool stereo   = true; // TODO: eventually arbitrary num. of channels?
  wxBitmap img_bar;
  wxBitmap img_bar_sub;
  wxBitmap img_warning [(int) meterwarn::MAX];
  /*float center_line  = 0.0;
  float bar_size     = 0.0;
  float center_pos   = 0.0;
  float pos_bar1     = 0.0;
  float pos_bar2     = 0.0;
  float size_bar     = 0.0;*/
  int   met_len      = -1;
  bool  last_stereo  = false;

  
  // length, thickness: x/y size depending if horiz. or vertical
  int ln, th;
  // position of bars along the widget's total thickness
  // left t1->t2, right t3->t4, tp is padding between black and inner bars
  int t1, t2, t3, t4, tp;
};

class c_waveformwidget : public c_customwidget {
public:
  c_waveformwidget (wxWindow *parent = NULL,
                    int id = -1,//ID_FOREIGN,
                    wxPoint pos = wxDefaultPosition,
                    wxSize size = wxDefaultSize,
                    int wtfisthis = -1);
  ~c_waveformwidget () {}
  
  virtual bool render_base_image ();
  //virtual void update ();
  virtual bool update (wxWindowDC &dc);
  
  wxFont tinyfont;
  
protected:
private:
  void draw_border (wxDC &dc);
  c_wavebuffer *wavdata = NULL;
};

int wx_main (int argc, char **argv, c_audioclient *audio);

extern c_app *g_app;

// example derived class from c_customwidget

class c_testwidget : public c_customwidget {
public:
  c_testwidget (wxWindow *parent = NULL,
                int id = -1,//ID_FOREIGN,
                wxPoint pos = wxDefaultPosition,
                wxSize size = wxDefaultSize,
                int border = -1)
  : c_customwidget (parent, id) {}
  
  virtual bool render_base_image ();
   bool update (wxWindowDC &dc);
};


#endif  // USE_WXWIDGETS

#undef  __IN_DIRT_UI_H
#endif  // __DIRT_UI_H

