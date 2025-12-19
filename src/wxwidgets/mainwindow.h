/////////////////////////////////////////////////////////////////////////////
// Name:        mainwindow.h
// Purpose:     
// Author:      
// Modified by: 
// Created:     Mon 01 Dec 2025 21:02:58 EST
// RCS-ID:      
// Copyright:   
// Licence:     
/////////////////////////////////////////////////////////////////////////////

#ifndef _MAINWINDOW_H_
#define _MAINWINDOW_H_


/*!
 * Includes
 */

////@begin includes
#include "wx/frame.h"
#include "wx/notebook.h"
#include "wx/spinctrl.h"
#include "wx/listctrl.h"
#include "wx/scrolbar.h"
#include "wx/statline.h"
////@end includes

/*!
 * Forward declarations
 */

////@begin forward declarations
class wxNotebook;
class wxBoxSizer;
class wxSpinCtrl;
class c_meterwidget;
class c_irlist;
class c_waveformwidget;
////@end forward declarations

/*!
 * Control identifiers
 */

////@begin control identifiers
#define ID_UI_MAINWINDOW 10000
#define ID_NOTEBOOK 10038
#define ID_TAB1 10039
#define ID_FILE 10013
#define ID_MAKESWEEP 10016
#define ID_ROUNDTRIP 10012
#define ID_PLAYSWEEP 10037
#define ID_SAMPLERATE 10044
#define ID_FORCEMONO 10009
#define ID_BACKEND 10023
#define ID_AUDIO 10048
#define ID_DRYFILE 10014
#define ID_DRYFILE_BROWSE 10015
#define ID_DRY_LENGTH 10017
#define ID_DRY_F1 10018
#define ID_DRY_F2 10019
#define ID_GENERATE 10049
#define ID_DRY_PREROLL 10020
#define ID_DRY_MARKER 10021
#define ID_DRY_GAP 10022
#define ID_DRY_SAVE 10046
#define ID_JACK_DRY 10024
#define ID_JACK_WET_L 10025
#define ID_JACK_WET_R 10026
#define wxID_DRYINFO 10071
#define ID_DRYINFO 10073
#define ID_STATICTEXT 10072
#define ID_METER_OUT 10051
#define ID_PLAY 10031
#define ID_TAB2 10040
#define ID_INPUTDIR_RECURSIVE 10006
#define ID_INPUTDIR 10007
#define wxID_INPUTHEADER 10074
#define ID_INPUTDIR_BROWSE 10008
#define ID_INPUTDIR_SCAN 10005
#define ID_INPUTFILES_ADD 10001
#define ID_INPUTFILES_CLEAR 10030
#define ID_INPUTFILES 10002
#define ID_OUTPUTDIR 10010
#define ID_OUTPUTDIR_BROWSE 10011
#define ID_SWEEP_ALIGN 10003
#define ID_ALIGN_MANUAL 10050
#define ID_HPF_MODE 10028
#define ID_HPF_FREQ 10033
#define ID_LPF_MODE 10029
#define ID_LPF_FREQ 10034
#define ID_ZEROALIGN 10004
#define ID_TRIM_START 10036
#define ID_TRIM_END 10055
#define ID_ABORT 10036
#define ID_AUTOSAVE 10067
#define ID_OVERWRITE 10036
#define ID_IGNORESR 10068
#define ID_DEBUG 10027
#define ID_SPINCTRL 10075
#define ID_SWEEP_THR 10035
#define ID_IR_START_THR 10042
#define ID_IR_END_THR 10043
#define ID_CHN_OFFSET 10041
#define ID_PROCESS 10045
#define ID_PANEL1 10047
#define ID_IRFILES 10056
#define ID_IR_RENAME 10057
#define ID_IR_REMOVE 10058
#define ID_IR_LOAD 10069
#define ID_IR_SAVE 10070
#define ID_WAVEFORM 10052
#define ID_SCROLLBAR1 10054
#define ID_SCROLLBAR 10053
#define ID_ZOOMFULL -1
#define ID_IR_PLAY 10059
#define ID_IR_TRIM 10060
#define ID_IR_BACKWARDS 10061
#define ID_IR_DCFLIP 10062
#define ID_IR_DCOFFSET 10063
#define ID_IR_AMPLIFY 10064
#define ID_IR_STEREO 10065
#define ID_PANEL 10032
#define ID_LOG 10076
#define wxID_STATUSBAR 10066
#define SYMBOL_UI_MAINWINDOW_STYLE wxCAPTION|wxRESIZE_BORDER|wxSYSTEM_MENU|wxCLOSE_BOX
#define SYMBOL_UI_MAINWINDOW_TITLE _("DIRT")
#define SYMBOL_UI_MAINWINDOW_IDNAME ID_UI_MAINWINDOW
#define SYMBOL_UI_MAINWINDOW_SIZE wxSize(1024, 640)
#define SYMBOL_UI_MAINWINDOW_POSITION wxDefaultPosition
////@end control identifiers


/*!
 * ui_mainwindow class declaration
 */

class ui_mainwindow: public wxFrame
{    
    DECLARE_CLASS( ui_mainwindow )
    DECLARE_EVENT_TABLE()

public:
    /// Constructors
    ui_mainwindow();
    ui_mainwindow( wxWindow* parent, wxWindowID id = SYMBOL_UI_MAINWINDOW_IDNAME, const wxString& caption = SYMBOL_UI_MAINWINDOW_TITLE, const wxPoint& pos = SYMBOL_UI_MAINWINDOW_POSITION, const wxSize& size = SYMBOL_UI_MAINWINDOW_SIZE, long style = SYMBOL_UI_MAINWINDOW_STYLE );

    bool Create( wxWindow* parent, wxWindowID id = SYMBOL_UI_MAINWINDOW_IDNAME, const wxString& caption = SYMBOL_UI_MAINWINDOW_TITLE, const wxPoint& pos = SYMBOL_UI_MAINWINDOW_POSITION, const wxSize& size = SYMBOL_UI_MAINWINDOW_SIZE, long style = SYMBOL_UI_MAINWINDOW_STYLE );

    /// Destructor
    ~ui_mainwindow();

    /// Initialises member variables
    void Init();

    /// Creates the controls and sizers
    void CreateControls();

////@begin ui_mainwindow event handler declarations

////@end ui_mainwindow event handler declarations

////@begin ui_mainwindow member function declarations

    /// Retrieves bitmap resources
    wxBitmap GetBitmapResource( const wxString& name );

    /// Retrieves icon resources
    wxIcon GetIconResource( const wxString& name );
////@end ui_mainwindow member function declarations

    /// Should we show tooltips?
    static bool ShowToolTips();

////@begin ui_mainwindow member variables
    wxNotebook* note_tabs;
    wxPanel* tab_drysweep;
    wxBoxSizer* staticbox_drysweep;
    wxRadioButton* radio_file;
    wxRadioButton* radio_makesweep;
    wxRadioButton* radio_roundtrip;
    wxRadioButton* radio_playsweep;
    wxComboBox* comb_samplerate;
    wxCheckBox* chk_forcemono;
    wxChoice* list_backend;
    wxButton* btn_audio;
    wxTextCtrl* text_dryfile;
    wxButton* btn_dryfile_browse;
    wxSpinCtrl* spin_dry_length;
    wxSpinCtrl* spin_dry_f1;
    wxSpinCtrl* spin_dry_f2;
    wxButton* btn_generate;
    wxSpinCtrl* spin_dry_preroll;
    wxSpinCtrl* spin_dry_marker;
    wxSpinCtrl* spin_dry_gap;
    wxComboBox* list_jack_dry;
    wxComboBox* list_jack_wet_l;
    wxComboBox* list_jack_wet_r;
    wxBoxSizer* sizer_meters;
    wxStaticText* text_dryinfo;
    c_meterwidget* pn_meter_out;
    c_meterwidget* pn_meter_in;
    wxButton* btn_play;
    wxPanel* tab_deconvolv;
    wxBoxSizer* staticbox_deconvolv;
    wxCheckBox* chk_inputdir_recursive;
    wxTextCtrl* text_inputdir;
    wxStaticText* text_inputheader;
    wxButton* btn_inputdir_browse;
    wxButton* btn_inputdir_scan;
    wxButton* btn_inputfiles_add;
    wxButton* btn_inputfiles_clear;
    wxListBox* list_inputfiles;
    wxTextCtrl* text_outputdir;
    wxButton* btn_outputdir_browse;
    wxChoice* list_align;
    wxButton* btn_align_manual;
    wxChoice* list_hpf_mode;
    wxSpinCtrl* spin_hpf_freq;
    wxChoice* list_lpf_mode;
    wxSpinCtrl* spin_lpf_freq;
    wxCheckBox* chk_zeroalign;
    wxCheckBox* chk_trim_start;
    wxCheckBox* chk_trim_end;
    wxCheckBox* chk_abort;
    wxCheckBox* chk_autosave;
    wxCheckBox* chk_overwrite;
    wxCheckBox* chk_ignore_sr;
    wxCheckBox* chk_debug;
    wxSpinCtrl* spin_sweep_thr;
    wxSpinCtrl* spin_ir_start_thr;
    wxSpinCtrl* spin_ir_end_thr;
    wxSpinCtrl* spin_chn_offset;
    wxButton* btn_process;
    c_irlist* list_irfiles;
    wxButton* btn_ir_rename;
    wxButton* btn_ir_remove;
    wxButton* btn_ir_load;
    wxButton* btn_ir_save;
    c_waveformwidget* pn_waveform;
    wxButton* btn_zoomfull;
    wxButton* btn_ir_play;
    wxButton* btn_ir_trim;
    wxButton* btn_ir_backwards;
    wxButton* btn_ir_dcflip;
    wxButton* btn_ir_dcoffset;
    wxButton* btn_ir_amplify;
    wxButton* btn_ir_stereo;
    wxTextCtrl* text_log;
    wxStaticText* text_statusbar;
    wxButton* btn_about;
    wxButton* btn_ok;
////@end ui_mainwindow member variables
};

#endif
    // _MAINWINDOW_H_
