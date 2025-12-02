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
////@end includes

/*!
 * Forward declarations
 */

////@begin forward declarations
class wxBoxSizer;
class wxSpinCtrl;
////@end forward declarations

/*!
 * Control identifiers
 */

////@begin control identifiers
#define ID_UI_MAINWINDOW 10000
#define ID_NOTEBOOK 10038
#define ID_PANEL 10039
#define ID_RADIOBUTTON 10013
#define ID_RADIOBUTTON1 10016
#define ID_RADIOBUTTON2 10012
#define ID_CHOICE4 10043
#define ID_RADIOBUTTON3 10037
#define ID_CHOICE1 10023
#define ID_TEXTCTRL3 10014
#define ID_BUTTON5 10015
#define ID_SPINCTRL 10017
#define ID_SPINCTRL1 10018
#define ID_SPINCTRL2 10019
#define ID_SPINCTRL3 10020
#define ID_SPINCTRL4 10021
#define ID_SPINCTRL5 10022
#define ID_COMBOBOX 10024
#define ID_COMBOBOX1 10025
#define ID_CHECKBOX6 10032
#define ID_COMBOBOX2 10026
#define ID_PANEL1 10040
#define ID_CHECKBOX3 10006
#define ID_TEXTCTRL1 10007
#define ID_BUTTON2 10005
#define ID_BUTTON3 10008
#define ID_BUTTON 10001
#define ID_LISTBOX 10002
#define ID_TEXTCTRL 10010
#define ID_BUTTON1 10011
#define ID_CHOICE 10003
#define ID_CHOICE2 10028
#define ID_SPINCTRL6 10033
#define ID_CHOICE3 10029
#define ID_SPINCTRL7 10034
#define ID_CHECKBOX 10004
#define ID_CHECKBOX1 10009
#define ID_CHECKBOX7 10036
#define ID_CHECKBOX2 10027
#define ID_SPINCTRL8 10035
#define ID_SPINCTRL10 10042
#define ID_SPINCTRL9 10041
#define SYMBOL_UI_MAINWINDOW_STYLE wxCAPTION|wxRESIZE_BORDER|wxSYSTEM_MENU|wxCLOSE_BOX
#define SYMBOL_UI_MAINWINDOW_TITLE _("DIRT")
#define SYMBOL_UI_MAINWINDOW_IDNAME ID_UI_MAINWINDOW
#define SYMBOL_UI_MAINWINDOW_SIZE wxSize(960, 512)
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
    wxBoxSizer* staticbox_drysweep;
    wxRadioButton* radio_file;
    wxRadioButton* radio_generate;
    wxRadioButton* radio_roundtrip;
    wxRadioButton* radio_play;
    wxChoice* list_backend;
    wxTextCtrl* text_dryfile;
    wxButton* btn_dryfile_browse;
    wxSpinCtrl* spin_dry_length;
    wxSpinCtrl* spin_dry_f1;
    wxSpinCtrl* spin_dry_f2;
    wxSpinCtrl* spin_dry_preroll;
    wxSpinCtrl* spin_dry_marker;
    wxSpinCtrl* spin_dry_gap;
    wxComboBox* list_jack_dry;
    wxComboBox* list_jack_wet_l;
    wxCheckBox* chk_jack_mono;
    wxComboBox* list_jack_wet_r;
    wxBoxSizer* sizer_meters;
    wxBoxSizer* staticbox_deconvolv;
    wxCheckBox* chk_inputdir_recursive;
    wxTextCtrl* text_inputdir;
    wxButton* btn_inputdir_scan;
    wxButton* btn_inputdir_browse;
    wxButton* btn_inputfiles_add;
    wxListBox* list_inputfiles;
    wxTextCtrl* text_outputdir;
    wxButton* btn_outputdir_browse;
    wxChoice* list_alignmethod;
    wxChoice* list_hpf_mode;
    wxSpinCtrl* spin_hpf_freq;
    wxChoice* list_lpf_mode;
    wxSpinCtrl* spin_lpf_freq;
    wxCheckBox* chk_zeroalign;
    wxCheckBox* chk_forcemono;
    wxCheckBox* chk_overwrite;
    wxCheckBox* chk_debug;
    wxSpinCtrl* spin_sweep_thr;
    wxSpinCtrl* spin_ir_thresh;
    wxSpinCtrl* spin_chn_offset;
    wxButton* btn_about;
    wxButton* btn_cancel;
    wxButton* btn_ok;
////@end ui_mainwindow member variables
};

#endif
    // _MAINWINDOW_H_
