
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

class c_ir_entry {
public:
  int64_t id = 0;
  int64_t timestamp = 0;
  c_wavebuffer l;
  c_wavebuffer r;
  std::string name;
  std::string dir;
  bool loaded = false;
  bool dirty = false;
  c_ir_entry ();
  size_t size ();
};

class c_customwidget;
class c_meterwidget;
class c_deconvolver_gui;
class c_waveformview;

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
  
  bool shift = false;
  bool alt =   false;
  bool ctrl =  false;
  
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
  bool process_one_file (std::string str);
  void set_statustext (const wxString str);
  void set_mode (long int mode);
  void set_prefs (s_prefs *prefs);
  void get_prefs (s_prefs *prefs);
  void update_audio_ports ();
  bool make_dry_sweep (bool load_it = true);
  bool load_ir_file (std::string file);
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
  void on_btn_zoomfull (wxCommandEvent &ev);
  void on_port_select (wxCommandEvent &ev);
  void on_irfile_select (wxListEvent &ev);
  void on_irfile_unselect (wxListEvent &ev);
  void on_timer (wxTimerEvent &ev);
  void on_recording_done ();
  
  void set_vu_l (float level, float hold, bool clip = false, bool xrun = false);
  void set_vu_r (float level, float hold, bool clip = false, bool xrun = false);
  
  c_deconvolver *dec = NULL;
  c_audioclient *audio = NULL;
  c_wavebuffer drysweep;
  c_wavebuffer wetsweep_l;
  c_wavebuffer wetsweep_r;

private:
  wxDECLARE_EVENT_TABLE ();
  void set_enable (wxWindow *w, bool b);
  void disable (wxWindow *w) { set_enable (w, false); }
  void enable (wxWindow *w) { set_enable (w, true); }
  int add_dir (std::string dir, bool recurs);
  int update_ir_list ();
  bool add_ir (c_ir_entry &entry);
  bool ask_overwrite (std::string filename);
  
  std::vector <c_ir_entry> ir_files;
  wxTimer timer;
  std::string cwd;
  bool init_audio_done = false;
  bool last_forcemono = false;
  int last_ir_count = 0;
  long int mode = ID_FILE;
  long int prev_mode = -1;
  audiostate prev_audio_state = (audiostate) -1;
  bool busy = false;
  bool filedir_scanning = false;
  bool filedir_error = false;
  int num_files_ok = 0;
  int num_files_error = 0;
  //c_meterwidget *testwidget = NULL;
};

class c_deconvolver_gui : public c_deconvolver {
public:
  c_deconvolver_gui (struct s_prefs *prefs = NULL)
    : c_deconvolver (prefs) {}
  
  //void set_vu_l (float level, float hold, bool clip, bool xrun);
  //void set_vu_r (float level, float hold, bool clip, bool xrun);

};

class c_irlist : public wxListCtrl {
public:
  c_irlist (wxWindow *parent = NULL,
            int id = -1,
            wxPoint pos = wxDefaultPosition,
            wxSize size = wxDefaultSize,
            int border = wxSIMPLE_BORDER);
            
  void clear ();
  void append (c_ir_entry &ir);
  int get_count ();
  int64_t get_selected_id ();
  
private:
};


/*
 * Custom widget example/template - (usually) base class.
 *
 * Widget class derived directly from wxPanel, where we override/bind
 * paint events etc. to implement our own appearance and behaviour.
 *
 * New custom widget classes should derive from this class, and override
 * the required virtual functions for drawing and event handling.
 *
 */

class c_customwidget : public wxPanel/*, public has_slots <>*/ {
public:
  c_customwidget (wxWindow *parent = NULL,
                  int id = -1,//ID_FOREIGN,
                  wxPoint pos = wxDefaultPosition,
                  wxSize size = wxDefaultSize,
                  int wtf = 0);
                  //wxBorder border = wxSIMPLE_BORDER);
  ~c_customwidget () = default;
  virtual void set_opacity (int opacity);
  virtual void inspect ();
  int width, height;
  
  // signal/slot stuff. requires sigslot.h - not including it in this project.
  //signal1 <c_customwidget *> sig_left_clicked;
  //signal1 <c_customwidget *> sig_middle_clicked;
  //signal1 <c_customwidget *> sig_right_clicked;
  //signal3 <c_customwidget *, int, int> sig_mouse_move;
  //signal1 <c_customwidget *> sig_mouse_enter;
  //signal1 <c_customwidget *> sig_mouse_leave;
  
protected:
  virtual void draw_base (wxDC &dc);
  
  // override THESE, not the functions that take wx*Evt & paramters
  virtual void on_resize (int w, int h) {}
  virtual void on_mousemove (int x, int y) {}
  virtual void on_mousedown_left () {}
  virtual void on_mouseup_left () {}
  virtual void on_mousedown_middle () {}
  virtual void on_mouseup_middle () {}
  virtual void on_mousedown_right () {}
  virtual void on_mouseup_right () {}
  virtual void on_mouseleave () {}
  virtual void on_mousewheel () {}
  // we manage these, just override from derived class
  virtual void on_mousewheel_v (int howmuch);
  virtual void on_mousewheel_h (int howmuch);
  virtual void on_keypress (wxChar unicode_key, int keycode,
                            wxUint32 rawkeycode, bool is_keyrepeat) {}
  virtual void on_keyrelease (wxChar unicode_key, int keycode,
                              wxUint32 rawkeycode) {}
  virtual void on_idle () {}
  virtual void on_visible () {}
  virtual void on_paint (wxDC &dc) {}
  //virtual void update ();
  virtual void render_overlay (wxDC &dc);
  virtual void invalidate_base ();
  virtual void invalidate_overlay ();
  virtual void invalidate_overlay_rect (const wxRect &r);
  //virtual void render_base_image (wxMemoryDC *dc);
  ////virtual bool update (wxWindowDC &dc);
  //void schedule_deletion ();
  
  wxColour col_default_bg;
  wxColour col_default_fg;
  wxColour col_bg;
  wxColour col_fg;
  wxColour col_bezel1;
  wxColour col_bezel2;
  wxFont font;
  wxFont smallboldfont;
  
  inline void get_xy (wxMouseEvent &ev) { mouse_x = ev.GetX (); mouse_y = ev.GetY (); }
  bool check_click_distance (int which_button);
  
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
  
private:
  bool render_base_image ();
  // event handler boilerplate - might be more to come if needed
  void on_paint (wxPaintEvent &evt);
  void on_resize (wxSizeEvent &evt); // calls API func on_resized ()
  void on_mousemove (wxMouseEvent &event);
  void on_mousedown_left (wxMouseEvent &event);
  void on_mouseup_left (wxMouseEvent &event);
  void on_mousedown_middle (wxMouseEvent &event);
  void on_mouseup_middle (wxMouseEvent &event);
  void on_mousedown_right (wxMouseEvent &event);
  void on_mouseup_right (wxMouseEvent &event);
  void on_mouseleave (wxMouseEvent &event);
  void on_mousewheel (wxMouseEvent &event);
  // we manage these, just override from derived class
  void on_keypress (wxKeyEvent &event);
  void on_keyrelease (wxKeyEvent&event);
  void on_idle (wxIdleEvent &event);
  void on_visible (wxShowEvent &ev);
    
  DECLARE_EVENT_TABLE ();
};


// c_testwidget (minimal WORKING) example derived class from c_customwidget

class c_testwidget : public c_customwidget {
public:
  c_testwidget (wxWindow *parent = NULL,
                int id = -1,//ID_FOREIGN,
                wxPoint pos = wxDefaultPosition,
                wxSize size = wxDefaultSize,
                int border = -1)
  : c_customwidget (parent, id) {}
  
  void draw_base (wxDC &dc);
  void on_paint (wxDC &dc);
};


// c_meterwidget (vu meters)

class c_meterwidget : public c_customwidget {
public:
  c_meterwidget (wxWindow *parent = NULL,
                  int id = -1,//ID_FOREIGN,
                  wxPoint pos = wxDefaultPosition,
                  wxSize size = wxDefaultSize,
                  int wtfisthis = -1);
  ~c_meterwidget () {}
  
  void set_db_scale (float f);
  void on_paint (wxDC &dc);
  void draw_base (wxDC &dc);
  void on_resize (wxSizeEvent &ev);
  void render_gradient_bar ();
  void set_vudata (c_vudata *v);
  c_vudata *get_vudata ();
  bool needs_redraw () { return data ? data->needs_redraw : false; }
  
  void set_stereo (bool b);
  void set_l (float level, float hold, bool clip = false, bool xrun = false);
  void set_r (float level, float hold, bool clip = false, bool xrun = false);

  bool vertical = false;
  //float l       = 0.0;
  //float r       = 0.0;

  int       clip_size    = 0;
  int       rec_size     = 0;
  bool      rec_enabled  = false;
  c_vudata  *data        = NULL;
  float     db_scale     = DEFAULT_VU_DB;
  wxFont    tinyfont;
  
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
  int   met_len      = -1;
  bool  last_stereo  = false;

  // length, thickness: x/y size depending if horiz. or vertical
  int ln, th;
  // position of bars along the widget's total thickness
  // left t1->t2, right t3->t4, tp is padding between black and inner bars
  int t1, t2, t3, t4, tp;
};

struct sxy {
  int s;  // sample
  int x;  // on screen x rel. to widget
  int y;  //       " " y " " 
};

// c_waveformwidget: combines 1 or more c_wwaveformview's
class c_waveformwidget : public c_customwidget {
public:
  c_waveformwidget (wxWindow *parent = NULL,
                    int id = -1,//ID_FOREIGN,
                    wxPoint pos = wxDefaultPosition,
                    wxSize size = wxDefaultSize,
                    int i = -1);
  ~c_waveformwidget ();
  
  bool stereo ();

  void draw_base (wxDC &dc);
  void on_paint (wxDC &dc);
  
  // event handlers, these mostly pass through to waveformview objects
  void on_mousedown_left ();
  void on_mousedown_right ();
  void on_mousewheel_h (int howmuch);
  void on_mousewheel_v (int howmuch);
  void on_mousemove (int x, int y);
  void on_idle ();
  void on_resize (int x, int y);

  bool select_ir (c_ir_entry *entry);
  bool unselect_ir ();
  c_ir_entry *get_selected ();
  void   zoom_full_x ();
  void   zoom_full_y ();
  
  c_ir_entry *ir;  
  
private:
  void draw_border (wxDC &dc, int x = -1, int y = -1, int w = -1, int h = -1);
  inline bool mouse_in (wxRect &rect) { return rect.Contains (mouse_x, mouse_y); }
  
  c_waveformview *l = NULL;
  c_waveformview *r = NULL;
  
  bool have_l ();
  bool have_r ();
};


////////////////////////////////////////////////////////////////////////////////
// c_waveformview (audio waveform viewer/editor)

class c_waveformview {
public:
  c_waveformview (c_customwidget *parent = NULL);
  ~c_waveformview () {}

  // these should be called from container class
  void on_paint ();
  void on_resize (int w, int h);
  void on_mousedown_left ();
  void on_mousedown_right ();
  void on_idle ();
  void on_mousewheel_h (int howmuch);
  void on_mousewheel_v (int howmuch);
  void on_mousemove (int x, int y);
  
  // TODO: this will eventually be select_wavebuffer, and 2
  // instances of this class will be used for editing stereo files.
  bool select_waveform (c_wavebuffer *buf);
  bool unselect_waveform ();
  c_wavebuffer *get_selected ();
  
  int width   = -1;
  int height  = -1;
  int mouse_x = -1;
  int mouse_y = -1;
  
  // zoom/scroll functions
  size_t get_scroll_pos ();
  size_t get_scroll_visible ();
  size_t set_zoom (size_t pos = 0, size_t sz = -1);
  void   zoom_x (float ratio);
  void   zoom_in () { zoom_x (1.05); }
  void   zoom_out () { zoom_x (-1.05); }
  void   zoom_full_x ();
  void   zoom_full_y ();
  void   zoom_y (float ratio, int around_px);
  void   zoom_full_y (float ratio);
  void   zoom_in_y (float ratio);
  void   zoom_out_y (float ratio);
  void   scroll_left_by (int samples);
  void   scroll_right_by (int samples);
  void   scroll_y_by (float offset);
  void   scroll_up_by (float offset);
  void   scroll_down_by (float offset);
  void   scroll_x_to (size_t pos);
  void   resize_x_to (size_t sz);
  void   scroll_y_to (float pos);
  void   resize_y_to (float size);
  int    get_x_zoom ();
  int    get_x_pos ();
  float  get_y_zoom ();
  float  get_y_pos ();
  void   update_scrollbars ();
  float  y_to_amplitude (int y);
  int    amplitude_to_y (float y);
  size_t x_to_samplepos (int x);
  int    samplepos_to_x (size_t s);
  
  wxFont tinyfont;
  c_customwidget *parent;
  wxRect rect;
  
//private:
  void draw_grid (wxDC &dc);
  void draw_waveform (wxDC &dc, c_wavebuffer &buf);
  void on_paint (wxDC &dc);
  //void draw_cursor (wxDC &dc, int x, int y, int len);
  int get_dot_at (int x, int y);
  size_t get_dot_under_mouse ();
  size_t get_sample_at (int x);
  float get_amplitude_at (int y);
  void clamp_coords ();

  // selection
  size_t cursor = 0;
  size_t selection = -1;
  size_t last_dragged_handle = -1;
  
  //c_wavebuffer *wavdata = NULL;
  c_wavebuffer *wb = NULL;
  std::vector<sxy> dothandles;
  bool    dotedit      = false;
  
protected:
  // zoom / position
  int64_t viewpos_x      = 0;   // visible waveform pos/size in samples
  int64_t viewsize_x     = -1;
  int64_t min_viewsize_x = 32;
  float   viewpos_y      = 1.0; // sample values multiplied by 1/this
  float   viewsize_y     = 0.0; // offset, -1 means baseline on top of screen
  float   min_viewsize_y = 0.00001;
  int     handlesize    = 8;
  
  wxColour col_wave_fg;
  wxColour col_wave_bg;
  wxColour col_waveselect_fg;
  wxColour col_waveselect_bg;
  wxColour col_cursor;
  wxColour col_dothandle;
  wxColour col_dothighlight;
  wxColour col_wavezoom;  
  wxColour col_grid;
  wxColour col_baseline;
};

int wx_main (int argc, char **argv, c_audioclient *audio);

extern c_app *g_app;


#endif  // USE_WXWIDGETS

#undef  __IN_DIRT_UI_H
#endif  // __DIRT_UI_H

