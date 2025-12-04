/////////////////////////////////////////////////////////////////////////////
// Name:        mainwindow.cpp
// Purpose:     
// Author:      
// Modified by: 
// Created:     Mon 01 Dec 2025 21:02:58 EST
// RCS-ID:      
// Copyright:   
// Licence:     
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

////@begin includes
#include "wx/imaglist.h"
////@end includes

#include "mainwindow.h"

////@begin XPM images
////@end XPM images


/*
 * ui_mainwindow type definition
 */

IMPLEMENT_CLASS( ui_mainwindow, wxFrame )


/*
 * ui_mainwindow event table definition
 */

BEGIN_EVENT_TABLE( ui_mainwindow, wxFrame )

////@begin ui_mainwindow event table entries
////@end ui_mainwindow event table entries

END_EVENT_TABLE()


/*
 * ui_mainwindow constructors
 */

ui_mainwindow::ui_mainwindow()
{
    Init();
}

ui_mainwindow::ui_mainwindow( wxWindow* parent, wxWindowID id, const wxString& caption, const wxPoint& pos, const wxSize& size, long style )
{
    Init();
    Create( parent, id, caption, pos, size, style );
}


/*
 * ui_mainwindow creator
 */

bool ui_mainwindow::Create( wxWindow* parent, wxWindowID id, const wxString& caption, const wxPoint& pos, const wxSize& size, long style )
{
////@begin ui_mainwindow creation
    wxFrame::Create( parent, id, caption, pos, size, style );

    CreateControls();
    Centre();
////@end ui_mainwindow creation
    return true;
}


/*
 * ui_mainwindow destructor
 */

ui_mainwindow::~ui_mainwindow()
{
////@begin ui_mainwindow destruction
////@end ui_mainwindow destruction
}


/*
 * Member initialisation
 */

void ui_mainwindow::Init()
{
////@begin ui_mainwindow member initialisation
    tab_drysweep = NULL;
    staticbox_drysweep = NULL;
    radio_file = NULL;
    radio_makesweep = NULL;
    radio_roundtrip = NULL;
    radio_playsweep = NULL;
    comb_samplerate = NULL;
    chk_forcemono = NULL;
    list_backend = NULL;
    btn_audio = NULL;
    text_dryfile = NULL;
    btn_dryfile_browse = NULL;
    spin_dry_length = NULL;
    spin_dry_f1 = NULL;
    spin_dry_f2 = NULL;
    btn_generate = NULL;
    spin_dry_preroll = NULL;
    spin_dry_marker = NULL;
    spin_dry_gap = NULL;
    list_jack_dry = NULL;
    list_jack_wet_l = NULL;
    list_jack_wet_r = NULL;
    sizer_meters = NULL;
    btn_play = NULL;
    tab_deconvolv = NULL;
    staticbox_deconvolv = NULL;
    chk_inputdir_recursive = NULL;
    text_inputdir = NULL;
    btn_inputdir_scan = NULL;
    btn_inputdir_browse = NULL;
    btn_inputfiles_clear = NULL;
    btn_inputfiles_add = NULL;
    list_inputfiles = NULL;
    text_outputdir = NULL;
    btn_outputdir_browse = NULL;
    list_align = NULL;
    list_hpf_mode = NULL;
    spin_hpf_freq = NULL;
    list_lpf_mode = NULL;
    spin_lpf_freq = NULL;
    chk_zeroalign = NULL;
    chk_overwrite = NULL;
    chk_debug = NULL;
    spin_sweep_thr = NULL;
    spin_ir_start_thr = NULL;
    spin_ir_end_thr = NULL;
    spin_chn_offset = NULL;
    btn_process = NULL;
    btn_about = NULL;
    btn_ok = NULL;
////@end ui_mainwindow member initialisation
}


/*
 * Control creation for ui_mainwindow
 */

void ui_mainwindow::CreateControls()
{    
////@begin ui_mainwindow content construction
    ui_mainwindow* itemFrame1 = this;

    wxBoxSizer* itemBoxSizer1 = new wxBoxSizer(wxVERTICAL);
    itemFrame1->SetSizer(itemBoxSizer1);

    wxNotebook* itemNotebook1 = new wxNotebook( itemFrame1, ID_NOTEBOOK, wxDefaultPosition, wxDefaultSize, wxBK_DEFAULT );

    tab_drysweep = new wxPanel( itemNotebook1, ID_TAB1, wxDefaultPosition, wxDefaultSize, wxSUNKEN_BORDER|wxTAB_TRAVERSAL );
    tab_drysweep->SetExtraStyle(wxWS_EX_VALIDATE_RECURSIVELY);
    staticbox_drysweep = new wxBoxSizer(wxHORIZONTAL);
    tab_drysweep->SetSizer(staticbox_drysweep);

    wxBoxSizer* itemBoxSizer4 = new wxBoxSizer(wxVERTICAL);
    staticbox_drysweep->Add(itemBoxSizer4, 1, wxGROW|wxALL, 5);
    wxGridSizer* itemGridSizer1 = new wxGridSizer(0, 1, 0, 0);
    itemBoxSizer4->Add(itemGridSizer1, 3, wxGROW|wxALL, 0);
    wxBoxSizer* itemBoxSizer7 = new wxBoxSizer(wxHORIZONTAL);
    itemGridSizer1->Add(itemBoxSizer7, 1, wxGROW|wxALL, 5);
    radio_file = new wxRadioButton( tab_drysweep, ID_FILE, _("File"), wxDefaultPosition, wxDefaultSize, 0 );
    radio_file->SetValue(false);
    itemBoxSizer7->Add(radio_file, 1, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxBoxSizer* itemBoxSizer9 = new wxBoxSizer(wxHORIZONTAL);
    itemGridSizer1->Add(itemBoxSizer9, 1, wxGROW|wxALL, 5);
    radio_makesweep = new wxRadioButton( tab_drysweep, ID_MAKESWEEP, _("Generate"), wxDefaultPosition, wxDefaultSize, 0 );
    radio_makesweep->SetValue(false);
    itemBoxSizer9->Add(radio_makesweep, 1, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxBoxSizer* itemBoxSizer11 = new wxBoxSizer(wxHORIZONTAL);
    itemGridSizer1->Add(itemBoxSizer11, 1, wxGROW|wxALL, 5);
    radio_roundtrip = new wxRadioButton( tab_drysweep, ID_ROUNDTRIP, _("Round-trip"), wxDefaultPosition, wxDefaultSize, 0 );
    radio_roundtrip->SetValue(false);
    itemBoxSizer11->Add(radio_roundtrip, 1, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxBoxSizer* itemBoxSizer6 = new wxBoxSizer(wxHORIZONTAL);
    itemGridSizer1->Add(itemBoxSizer6, 1, wxGROW|wxALL, 5);
    radio_playsweep = new wxRadioButton( tab_drysweep, ID_PLAYSWEEP, _("Audio out"), wxDefaultPosition, wxDefaultSize, 0 );
    radio_playsweep->SetValue(false);
    radio_playsweep->Show(false);
    itemBoxSizer6->Add(radio_playsweep, 1, wxALIGN_CENTER_VERTICAL|wxALL|wxRESERVE_SPACE_EVEN_IF_HIDDEN, 5);

    wxStaticText* itemStaticText4 = new wxStaticText( tab_drysweep, wxID_STATIC, _("Sample rate:"), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer4->Add(itemStaticText4, 0, wxALIGN_LEFT|wxALL, 5);

    wxArrayString comb_samplerateStrings;
    comb_samplerate = new wxComboBox( tab_drysweep, ID_SAMPLERATE, wxEmptyString, wxDefaultPosition, wxDefaultSize, comb_samplerateStrings, wxCB_DROPDOWN );
    itemBoxSizer4->Add(comb_samplerate, 0, wxGROW|wxALL, 5);

    chk_forcemono = new wxCheckBox( tab_drysweep, ID_FORCEMONO, _("Force mono"), wxDefaultPosition, wxDefaultSize, 0 );
    chk_forcemono->SetValue(false);
    itemBoxSizer4->Add(chk_forcemono, 0, wxALIGN_LEFT|wxALL, 5);

    wxStaticText* itemStaticText3 = new wxStaticText( tab_drysweep, wxID_STATIC, _("Driver:"), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer4->Add(itemStaticText3, 0, wxALIGN_LEFT|wxALL, 5);

    wxArrayString list_backendStrings;
    list_backend = new wxChoice( tab_drysweep, ID_BACKEND, wxDefaultPosition, wxDefaultSize, list_backendStrings, 0 );
    list_backend->SetStringSelection(_("0"));
    itemBoxSizer4->Add(list_backend, 0, wxGROW|wxBOTTOM, 5);

    btn_audio = new wxButton( tab_drysweep, ID_AUDIO, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer4->Add(btn_audio, 0, wxGROW|wxALL, 5);

    wxBoxSizer* itemBoxSizer14 = new wxBoxSizer(wxVERTICAL);
    staticbox_drysweep->Add(itemBoxSizer14, 5, wxGROW|wxALL, 5);
    wxBoxSizer* itemBoxSizer15 = new wxBoxSizer(wxHORIZONTAL);
    itemBoxSizer14->Add(itemBoxSizer15, 0, wxGROW|wxALL, 5);
    text_dryfile = new wxTextCtrl( tab_drysweep, ID_DRYFILE, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer15->Add(text_dryfile, 1, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    btn_dryfile_browse = new wxButton( tab_drysweep, ID_DRYFILE_BROWSE, _("Browse..."), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer15->Add(btn_dryfile_browse, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    itemBoxSizer14->Add(5, 5, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 0);

    wxFlexGridSizer* itemFlexGridSizer19 = new wxFlexGridSizer(0, 7, 0, 0);
    itemBoxSizer14->Add(itemFlexGridSizer19, 0, wxGROW|wxALL, 5);
    wxStaticText* itemStaticText20 = new wxStaticText( tab_drysweep, wxID_STATIC, _("Length (seconds):"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer19->Add(itemStaticText20, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    spin_dry_length = new wxSpinCtrl( tab_drysweep, ID_DRY_LENGTH, wxT("0"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 500, 0 );
    itemFlexGridSizer19->Add(spin_dry_length, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxStaticText* itemStaticText22 = new wxStaticText( tab_drysweep, wxID_STATIC, _("Start freq:"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer19->Add(itemStaticText22, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    spin_dry_f1 = new wxSpinCtrl( tab_drysweep, ID_DRY_F1, wxT("0"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 48000, 0 );
    itemFlexGridSizer19->Add(spin_dry_f1, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxStaticText* itemStaticText24 = new wxStaticText( tab_drysweep, wxID_STATIC, _("End freq:"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer19->Add(itemStaticText24, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    spin_dry_f2 = new wxSpinCtrl( tab_drysweep, ID_DRY_F2, wxT("0"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 48000, 0 );
    itemFlexGridSizer19->Add(spin_dry_f2, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    btn_generate = new wxButton( tab_drysweep, ID_GENERATE, _("Generate"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer19->Add(btn_generate, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxStaticText* itemStaticText26 = new wxStaticText( tab_drysweep, wxID_STATIC, _("Preroll (ms):"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer19->Add(itemStaticText26, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    spin_dry_preroll = new wxSpinCtrl( tab_drysweep, ID_DRY_PREROLL, wxT("0"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 1000000, 0 );
    itemFlexGridSizer19->Add(spin_dry_preroll, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxStaticText* itemStaticText28 = new wxStaticText( tab_drysweep, wxID_STATIC, _("Marker (ms):"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer19->Add(itemStaticText28, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    spin_dry_marker = new wxSpinCtrl( tab_drysweep, ID_DRY_MARKER, wxT("0"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 1000000, 0 );
    itemFlexGridSizer19->Add(spin_dry_marker, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxStaticText* itemStaticText30 = new wxStaticText( tab_drysweep, wxID_STATIC, _("Gap (ms):"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer19->Add(itemStaticText30, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    spin_dry_gap = new wxSpinCtrl( tab_drysweep, ID_DRY_GAP, wxT("0"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 1000000, 0 );
    itemFlexGridSizer19->Add(spin_dry_gap, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxButton* itemButton5 = new wxButton( tab_drysweep, ID_DRY_SAVE, _("Save"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer19->Add(itemButton5, 0, wxALIGN_RIGHT|wxALIGN_BOTTOM|wxALL, 5);

    itemFlexGridSizer19->AddGrowableCol(6);

    itemBoxSizer14->Add(5, 5, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

    wxBoxSizer* itemBoxSizer33 = new wxBoxSizer(wxVERTICAL);
    itemBoxSizer14->Add(itemBoxSizer33, 0, wxGROW|wxALL, 0);
    wxBoxSizer* itemBoxSizer34 = new wxBoxSizer(wxHORIZONTAL);
    itemBoxSizer33->Add(itemBoxSizer34, 0, wxGROW|wxALL, 5);
    wxStaticText* itemStaticText35 = new wxStaticText( tab_drysweep, wxID_STATIC, _("\"Round trip\" mode: adjust sweep settings above and select in/out audio ports."), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer34->Add(itemStaticText35, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxFlexGridSizer* itemFlexGridSizer36 = new wxFlexGridSizer(0, 5, 0, 0);
    itemBoxSizer33->Add(itemFlexGridSizer36, 0, wxGROW|wxALL, 5);
    wxStaticText* itemStaticText37 = new wxStaticText( tab_drysweep, wxID_STATIC, _("Play dry"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer36->Add(itemStaticText37, 0, wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxArrayString list_jack_dryStrings;
    list_jack_dry = new wxComboBox( tab_drysweep, ID_JACK_DRY, wxEmptyString, wxDefaultPosition, wxDefaultSize, list_jack_dryStrings, wxCB_DROPDOWN );
    itemFlexGridSizer36->Add(list_jack_dry, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    itemFlexGridSizer36->Add(5, 5, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxStaticText* itemStaticText40 = new wxStaticText( tab_drysweep, wxID_STATIC, _("Record wet L"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer36->Add(itemStaticText40, 0, wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxArrayString list_jack_wet_lStrings;
    list_jack_wet_l = new wxComboBox( tab_drysweep, ID_JACK_WET_L, wxEmptyString, wxDefaultPosition, wxDefaultSize, list_jack_wet_lStrings, wxCB_DROPDOWN );
    itemFlexGridSizer36->Add(list_jack_wet_l, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    itemFlexGridSizer36->Add(5, 5, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    itemFlexGridSizer36->Add(5, 5, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    itemFlexGridSizer36->Add(5, 5, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxStaticText* itemStaticText45 = new wxStaticText( tab_drysweep, wxID_STATIC, _("Record wet R"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer36->Add(itemStaticText45, 0, wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxArrayString list_jack_wet_rStrings;
    list_jack_wet_r = new wxComboBox( tab_drysweep, ID_JACK_WET_R, wxEmptyString, wxDefaultPosition, wxDefaultSize, list_jack_wet_rStrings, wxCB_DROPDOWN );
    itemFlexGridSizer36->Add(list_jack_wet_r, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    itemFlexGridSizer36->AddGrowableCol(1);
    itemFlexGridSizer36->AddGrowableCol(4);

    sizer_meters = new wxBoxSizer(wxVERTICAL);
    itemBoxSizer33->Add(sizer_meters, 0, wxGROW|wxALL, 5);

    itemBoxSizer14->Add(5, 5, 1, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

    btn_play = new wxButton( tab_drysweep, ID_PLAY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer14->Add(btn_play, 0, wxGROW|wxALL, 5);

    itemNotebook1->AddPage(tab_drysweep, _("Dry sweep"));

    tab_deconvolv = new wxPanel( itemNotebook1, ID_TAB2, wxDefaultPosition, wxDefaultSize, wxSUNKEN_BORDER|wxTAB_TRAVERSAL );
    tab_deconvolv->SetExtraStyle(wxWS_EX_VALIDATE_RECURSIVELY);
    staticbox_deconvolv = new wxBoxSizer(wxVERTICAL);
    tab_deconvolv->SetSizer(staticbox_deconvolv);

    staticbox_deconvolv->Add(5, 5, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

    wxBoxSizer* itemBoxSizer8 = new wxBoxSizer(wxHORIZONTAL);
    staticbox_deconvolv->Add(itemBoxSizer8, 0, wxGROW|wxALL, 0);
    wxBoxSizer* itemBoxSizer10 = new wxBoxSizer(wxVERTICAL);
    itemBoxSizer8->Add(itemBoxSizer10, 1, wxGROW|wxALL, 0);
    wxBoxSizer* itemBoxSizer12 = new wxBoxSizer(wxHORIZONTAL);
    itemBoxSizer10->Add(itemBoxSizer12, 0, wxGROW|wxALL, 0);
    wxBoxSizer* itemBoxSizer13 = new wxBoxSizer(wxVERTICAL);
    itemBoxSizer12->Add(itemBoxSizer13, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
    wxStaticText* itemStaticText14 = new wxStaticText( tab_deconvolv, wxID_STATIC, _("Scan directory:"), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer13->Add(itemStaticText14, 0, wxALIGN_LEFT|wxALL, 0);

    chk_inputdir_recursive = new wxCheckBox( tab_deconvolv, ID_INPUTDIR_RECURSIVE, _("Include sub"), wxDefaultPosition, wxDefaultSize, 0 );
    chk_inputdir_recursive->SetValue(false);
    itemBoxSizer13->Add(chk_inputdir_recursive, 0, wxALIGN_LEFT|wxALL, 0);

    text_inputdir = new wxTextCtrl( tab_deconvolv, ID_INPUTDIR, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer12->Add(text_inputdir, 1, wxALIGN_TOP|wxALL, 5);

    itemBoxSizer10->Add(5, 5, 1, wxALIGN_CENTER_HORIZONTAL|wxALL, 1);

    wxBoxSizer* itemBoxSizer21 = new wxBoxSizer(wxHORIZONTAL);
    itemBoxSizer10->Add(itemBoxSizer21, 0, wxGROW|wxALL, 0);
    wxStaticText* itemStaticText23 = new wxStaticText( tab_deconvolv, wxID_STATIC, _("List of sweep files to process:"), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer21->Add(itemStaticText23, 0, wxALIGN_CENTER_VERTICAL|wxLEFT|wxRIGHT|wxBOTTOM, 5);

    wxGridSizer* itemGridSizer24 = new wxGridSizer(0, 2, 0, 0);
    itemBoxSizer8->Add(itemGridSizer24, 0, wxALIGN_CENTER_VERTICAL|wxALL, 0);
    btn_inputdir_scan = new wxButton( tab_deconvolv, ID_INPUTDIR_SCAN, _("Scan"), wxDefaultPosition, wxDefaultSize, 0 );
    itemGridSizer24->Add(btn_inputdir_scan, 0, wxGROW|wxALIGN_BOTTOM|wxALL, 5);

    btn_inputdir_browse = new wxButton( tab_deconvolv, ID_INPUTDIR_BROWSE, _("Browse..."), wxDefaultPosition, wxDefaultSize, 0 );
    itemGridSizer24->Add(btn_inputdir_browse, 0, wxGROW|wxALIGN_BOTTOM|wxALL, 5);

    btn_inputfiles_clear = new wxButton( tab_deconvolv, ID_INPUTFILES_CLEAR, _("Clear"), wxDefaultPosition, wxDefaultSize, 0 );
    itemGridSizer24->Add(btn_inputfiles_clear, 0, wxGROW|wxALIGN_BOTTOM|wxALL, 5);

    btn_inputfiles_add = new wxButton( tab_deconvolv, ID_INPUTFILES_ADD, _("Add files..."), wxDefaultPosition, wxDefaultSize, 0 );
    itemGridSizer24->Add(btn_inputfiles_add, 0, wxGROW|wxALIGN_BOTTOM|wxALL, 5);

    wxFlexGridSizer* itemFlexGridSizer49 = new wxFlexGridSizer(0, 3, 0, 0);
    staticbox_deconvolv->Add(itemFlexGridSizer49, 0, wxGROW|wxALL, 0);
    wxBoxSizer* itemBoxSizer53 = new wxBoxSizer(wxHORIZONTAL);
    itemFlexGridSizer49->Add(itemBoxSizer53, 1, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 0);
    wxBoxSizer* itemBoxSizer54 = new wxBoxSizer(wxHORIZONTAL);
    itemBoxSizer53->Add(itemBoxSizer54, 1, wxGROW|wxALL, 0);

    itemFlexGridSizer49->AddGrowableCol(1);

    wxBoxSizer* itemBoxSizer58 = new wxBoxSizer(wxHORIZONTAL);
    staticbox_deconvolv->Add(itemBoxSizer58, 0, wxGROW|wxLEFT|wxRIGHT|wxTOP, 0);
    itemBoxSizer58->Add(5, 5, 1, wxALIGN_CENTER_VERTICAL|wxALL, 0);

    wxArrayString list_inputfilesStrings;
    list_inputfiles = new wxListBox( tab_deconvolv, ID_INPUTFILES, wxDefaultPosition, wxDefaultSize, list_inputfilesStrings, wxLB_SINGLE );
    staticbox_deconvolv->Add(list_inputfiles, 1, wxGROW|wxLEFT|wxRIGHT|wxBOTTOM, 5);

    wxBoxSizer* itemBoxSizer63 = new wxBoxSizer(wxHORIZONTAL);
    staticbox_deconvolv->Add(itemBoxSizer63, 0, wxGROW|wxALL, 0);
    wxStaticText* itemStaticText64 = new wxStaticText( tab_deconvolv, wxID_STATIC, _("Output directory:"), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer63->Add(itemStaticText64, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    text_outputdir = new wxTextCtrl( tab_deconvolv, ID_OUTPUTDIR, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer63->Add(text_outputdir, 1, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    btn_outputdir_browse = new wxButton( tab_deconvolv, ID_OUTPUTDIR_BROWSE, _("Browse..."), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer63->Add(btn_outputdir_browse, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxBoxSizer* itemBoxSizer67 = new wxBoxSizer(wxHORIZONTAL);
    staticbox_deconvolv->Add(itemBoxSizer67, 0, wxGROW|wxALL, 5);
    wxBoxSizer* itemBoxSizer68 = new wxBoxSizer(wxVERTICAL);
    itemBoxSizer67->Add(itemBoxSizer68, 0, wxALIGN_CENTER_VERTICAL|wxALL, 0);
    wxBoxSizer* itemBoxSizer69 = new wxBoxSizer(wxHORIZONTAL);
    itemBoxSizer68->Add(itemBoxSizer69, 0, wxGROW|wxALL, 0);
    wxStaticText* itemStaticText70 = new wxStaticText( tab_deconvolv, wxID_STATIC, _("Sweep alignment method:"), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer69->Add(itemStaticText70, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxArrayString list_alignStrings;
    list_align = new wxChoice( tab_deconvolv, ID_SWEEP_ALIGN, wxDefaultPosition, wxDefaultSize, list_alignStrings, 0 );
    itemBoxSizer69->Add(list_align, 1, wxGROW|wxALL, 5);

    wxBoxSizer* itemBoxSizer72 = new wxBoxSizer(wxHORIZONTAL);
    itemBoxSizer68->Add(itemBoxSizer72, 0, wxGROW|wxALL, 0);
    wxStaticText* itemStaticText1 = new wxStaticText( tab_deconvolv, wxID_STATIC, _("High-pass filter:"), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer72->Add(itemStaticText1, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    itemBoxSizer72->Add(5, 5, 1, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxArrayString list_hpf_modeStrings;
    list_hpf_mode = new wxChoice( tab_deconvolv, ID_HPF_MODE, wxDefaultPosition, wxDefaultSize, list_hpf_modeStrings, 0 );
    itemBoxSizer72->Add(list_hpf_mode, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    spin_hpf_freq = new wxSpinCtrl( tab_deconvolv, ID_HPF_FREQ, wxT("0"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 11025, 0 );
    itemBoxSizer72->Add(spin_hpf_freq, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxBoxSizer* itemBoxSizer77 = new wxBoxSizer(wxHORIZONTAL);
    itemBoxSizer68->Add(itemBoxSizer77, 0, wxGROW|wxALL, 0);
    wxStaticText* itemStaticText2 = new wxStaticText( tab_deconvolv, wxID_STATIC, _("Low-pass filter:"), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer77->Add(itemStaticText2, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    itemBoxSizer77->Add(5, 5, 1, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxArrayString list_lpf_modeStrings;
    list_lpf_mode = new wxChoice( tab_deconvolv, ID_LPF_MODE, wxDefaultPosition, wxDefaultSize, list_lpf_modeStrings, 0 );
    itemBoxSizer77->Add(list_lpf_mode, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    spin_lpf_freq = new wxSpinCtrl( tab_deconvolv, ID_LPF_FREQ, wxT("0"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 48000, 0 );
    itemBoxSizer77->Add(spin_lpf_freq, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxBoxSizer* itemBoxSizer82 = new wxBoxSizer(wxVERTICAL);
    itemBoxSizer67->Add(itemBoxSizer82, 0, wxALIGN_CENTER_VERTICAL|wxALL, 0);
    chk_zeroalign = new wxCheckBox( tab_deconvolv, ID_ZEROALIGN, _("Zero-align IR peak"), wxDefaultPosition, wxDefaultSize, 0 );
    chk_zeroalign->SetValue(false);
    itemBoxSizer82->Add(chk_zeroalign, 1, wxALIGN_LEFT|wxALL, 5);

    chk_overwrite = new wxCheckBox( tab_deconvolv, ID_OVERWRITE, _("Overwrite existing files"), wxDefaultPosition, wxDefaultSize, 0 );
    chk_overwrite->SetValue(false);
    itemBoxSizer82->Add(chk_overwrite, 0, wxALIGN_LEFT|wxALL, 5);

    chk_debug = new wxCheckBox( tab_deconvolv, ID_DEBUG, _("Debug info"), wxDefaultPosition, wxDefaultSize, 0 );
    chk_debug->SetValue(false);
    itemBoxSizer82->Add(chk_debug, 1, wxALIGN_LEFT|wxALL, 5);

    wxBoxSizer* itemBoxSizer87 = new wxBoxSizer(wxVERTICAL);
    itemBoxSizer67->Add(itemBoxSizer87, 0, wxGROW|wxALL, 0);
    wxBoxSizer* itemBoxSizer88 = new wxBoxSizer(wxHORIZONTAL);
    itemBoxSizer87->Add(itemBoxSizer88, 1, wxGROW|wxALL, 0);
    wxStaticText* itemStaticText89 = new wxStaticText( tab_deconvolv, wxID_STATIC, _("Sweep silence thresh (dB):"), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer88->Add(itemStaticText89, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    itemBoxSizer88->Add(5, 5, 1, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    spin_sweep_thr = new wxSpinCtrl( tab_deconvolv, ID_SWEEP_THR, wxT("0"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, -200, 0, 0 );
    itemBoxSizer88->Add(spin_sweep_thr, 0, wxALIGN_CENTER_VERTICAL|wxALL, 0);

    wxBoxSizer* itemBoxSizer92 = new wxBoxSizer(wxHORIZONTAL);
    itemBoxSizer87->Add(itemBoxSizer92, 1, wxGROW|wxALL, 0);
    wxStaticText* itemStaticText93 = new wxStaticText( tab_deconvolv, wxID_STATIC, _("IR start silence thresh (dB):"), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer92->Add(itemStaticText93, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    itemBoxSizer92->Add(5, 5, 1, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    spin_ir_start_thr = new wxSpinCtrl( tab_deconvolv, ID_IR_START_THR, wxT("0"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, -200, 0, 0 );
    itemBoxSizer92->Add(spin_ir_start_thr, 0, wxALIGN_CENTER_VERTICAL|wxALL, 0);

    wxBoxSizer* itemBoxSizer18 = new wxBoxSizer(wxHORIZONTAL);
    itemBoxSizer87->Add(itemBoxSizer18, 1, wxGROW|wxALL, 0);
    wxStaticText* itemStaticText19 = new wxStaticText( tab_deconvolv, wxID_STATIC, _("IR end silence thresh (dB):"), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer18->Add(itemStaticText19, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    itemBoxSizer18->Add(5, 5, 1, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    spin_ir_end_thr = new wxSpinCtrl( tab_deconvolv, ID_IR_END_THR, wxT("0"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, -200, 0, 0 );
    itemBoxSizer18->Add(spin_ir_end_thr, 0, wxALIGN_CENTER_VERTICAL|wxALL, 0);

    wxBoxSizer* itemBoxSizer96 = new wxBoxSizer(wxHORIZONTAL);
    itemBoxSizer87->Add(itemBoxSizer96, 1, wxGROW|wxALL, 0);
    wxStaticText* itemStaticText97 = new wxStaticText( tab_deconvolv, wxID_STATIC, _("Offset stereo channels (samples):"), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer96->Add(itemStaticText97, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    itemBoxSizer96->Add(5, 5, 1, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    spin_chn_offset = new wxSpinCtrl( tab_deconvolv, ID_CHN_OFFSET, wxT("0"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, -48000, 48000, 0 );
    itemBoxSizer96->Add(spin_chn_offset, 0, wxALIGN_CENTER_VERTICAL|wxALL, 0);

    wxBoxSizer* itemBoxSizer17 = new wxBoxSizer(wxHORIZONTAL);
    staticbox_deconvolv->Add(itemBoxSizer17, 0, wxGROW|wxALL, 5);
    itemBoxSizer17->Add(5, 5, 1, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    btn_process = new wxButton( tab_deconvolv, ID_PROCESS, _("Process"), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer17->Add(btn_process, 5, wxALIGN_BOTTOM|wxALL, 5);

    itemNotebook1->AddPage(tab_deconvolv, _("Deconvolver"));

    wxPanel* itemPanel3 = new wxPanel( itemNotebook1, ID_PANEL1, wxDefaultPosition, wxDefaultSize, wxSUNKEN_BORDER|wxTAB_TRAVERSAL );
    itemPanel3->SetExtraStyle(wxWS_EX_VALIDATE_RECURSIVELY);
    wxBoxSizer* itemBoxSizer20 = new wxBoxSizer(wxVERTICAL);
    itemPanel3->SetSizer(itemBoxSizer20);

    itemBoxSizer20->Add(5, 5, 1, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

    wxStaticText* itemStaticText25 = new wxStaticText( itemPanel3, wxID_STATIC, _("No IR file has been generated yet."), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer20->Add(itemStaticText25, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

    itemBoxSizer20->Add(5, 5, 1, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

    itemNotebook1->AddPage(itemPanel3, _("IR file"));

    wxPanel* itemPanel1 = new wxPanel( itemNotebook1, ID_PANEL, wxDefaultPosition, wxDefaultSize, wxSUNKEN_BORDER|wxTAB_TRAVERSAL );
    itemPanel1->SetExtraStyle(wxWS_EX_VALIDATE_RECURSIVELY);
    wxBoxSizer* itemBoxSizer19 = new wxBoxSizer(wxVERTICAL);
    itemPanel1->SetSizer(itemBoxSizer19);

    itemBoxSizer19->Add(5, 5, 1, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

    wxStaticText* itemStaticText8 = new wxStaticText( itemPanel1, wxID_STATIC, _("(to do)"), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer19->Add(itemStaticText8, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

    itemBoxSizer19->Add(5, 5, 1, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

    itemNotebook1->AddPage(itemPanel1, _("Log"));

    itemBoxSizer1->Add(itemNotebook1, 1, wxGROW|wxALL, 5);

    wxBoxSizer* itemBoxSizer2 = new wxBoxSizer(wxHORIZONTAL);
    itemBoxSizer1->Add(itemBoxSizer2, 0, wxALIGN_RIGHT|wxALL, 5);

    btn_about = new wxButton( itemFrame1, wxID_ABOUT, _("&About"), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer2->Add(btn_about, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    btn_ok = new wxButton( itemFrame1, wxID_EXIT, _("&Quit"), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer2->Add(btn_ok, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

////@end ui_mainwindow content construction
}


/*
 * Should we show tooltips?
 */

bool ui_mainwindow::ShowToolTips()
{
    return true;
}

/*
 * Get bitmap resources
 */

wxBitmap ui_mainwindow::GetBitmapResource( const wxString& name )
{
    // Bitmap retrieval
////@begin ui_mainwindow bitmap retrieval
    wxUnusedVar(name);
    return wxNullBitmap;
////@end ui_mainwindow bitmap retrieval
}

/*
 * Get icon resources
 */

wxIcon ui_mainwindow::GetIconResource( const wxString& name )
{
    // Icon retrieval
////@begin ui_mainwindow icon retrieval
    wxUnusedVar(name);
    return wxNullIcon;
////@end ui_mainwindow icon retrieval
}
