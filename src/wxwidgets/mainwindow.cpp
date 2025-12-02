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
    staticbox_drysweep = NULL;
    radio_file = NULL;
    radio_generate = NULL;
    radio_roundtrip = NULL;
    radio_play = NULL;
    list_backend = NULL;
    text_dryfile = NULL;
    btn_dryfile_browse = NULL;
    spin_dry_length = NULL;
    spin_dry_f1 = NULL;
    spin_dry_f2 = NULL;
    spin_dry_preroll = NULL;
    spin_dry_marker = NULL;
    spin_dry_gap = NULL;
    list_jack_dry = NULL;
    list_jack_wet_l = NULL;
    chk_jack_mono = NULL;
    list_jack_wet_r = NULL;
    sizer_meters = NULL;
    staticbox_deconvolv = NULL;
    chk_inputdir_recursive = NULL;
    text_inputdir = NULL;
    btn_inputdir_scan = NULL;
    btn_inputdir_browse = NULL;
    btn_inputfiles_add = NULL;
    list_inputfiles = NULL;
    text_outputdir = NULL;
    btn_outputdir_browse = NULL;
    list_alignmethod = NULL;
    list_hpf_mode = NULL;
    spin_hpf_freq = NULL;
    list_lpf_mode = NULL;
    spin_lpf_freq = NULL;
    chk_zeroalign = NULL;
    chk_forcemono = NULL;
    chk_overwrite = NULL;
    chk_debug = NULL;
    spin_sweep_thr = NULL;
    spin_ir_thresh = NULL;
    spin_chn_offset = NULL;
    btn_about = NULL;
    btn_cancel = NULL;
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

    wxPanel* itemPanel2 = new wxPanel( itemNotebook1, ID_PANEL, wxDefaultPosition, wxDefaultSize, wxSUNKEN_BORDER|wxTAB_TRAVERSAL );
    itemPanel2->SetExtraStyle(wxWS_EX_VALIDATE_RECURSIVELY);
    staticbox_drysweep = new wxBoxSizer(wxHORIZONTAL);
    itemPanel2->SetSizer(staticbox_drysweep);

    wxBoxSizer* itemBoxSizer4 = new wxBoxSizer(wxVERTICAL);
    staticbox_drysweep->Add(itemBoxSizer4, 0, wxGROW|wxALL, 5);
    wxGridSizer* itemGridSizer1 = new wxGridSizer(0, 1, 0, 0);
    itemBoxSizer4->Add(itemGridSizer1, 0, wxGROW|wxALL, 0);
    wxBoxSizer* itemBoxSizer7 = new wxBoxSizer(wxHORIZONTAL);
    itemGridSizer1->Add(itemBoxSizer7, 1, wxGROW|wxALL, 5);
    radio_file = new wxRadioButton( itemPanel2, ID_RADIOBUTTON, _("File"), wxDefaultPosition, wxDefaultSize, 0 );
    radio_file->SetValue(false);
    itemBoxSizer7->Add(radio_file, 1, wxALIGN_TOP|wxALL, 1);

    wxBoxSizer* itemBoxSizer9 = new wxBoxSizer(wxHORIZONTAL);
    itemGridSizer1->Add(itemBoxSizer9, 1, wxGROW|wxALL, 5);
    radio_generate = new wxRadioButton( itemPanel2, ID_RADIOBUTTON1, _("Generate"), wxDefaultPosition, wxDefaultSize, 0 );
    radio_generate->SetValue(false);
    itemBoxSizer9->Add(radio_generate, 1, wxALIGN_TOP|wxALL, 5);

    wxBoxSizer* itemBoxSizer11 = new wxBoxSizer(wxHORIZONTAL);
    itemGridSizer1->Add(itemBoxSizer11, 1, wxGROW|wxALL, 5);
    radio_roundtrip = new wxRadioButton( itemPanel2, ID_RADIOBUTTON2, _("Round-\ntrip"), wxDefaultPosition, wxDefaultSize, 0 );
    radio_roundtrip->SetValue(false);
    itemBoxSizer11->Add(radio_roundtrip, 1, wxALIGN_TOP|wxALL, 5);

    wxBoxSizer* itemBoxSizer6 = new wxBoxSizer(wxVERTICAL);
    itemGridSizer1->Add(itemBoxSizer6, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5);
    wxArrayString itemChoice1Strings;
    wxChoice* itemChoice1 = new wxChoice( itemPanel2, ID_CHOICE4, wxDefaultPosition, wxDefaultSize, itemChoice1Strings, 0 );
    itemChoice1->Show(false);
    itemBoxSizer6->Add(itemChoice1, 0, wxALIGN_LEFT|wxALL|wxRESERVE_SPACE_EVEN_IF_HIDDEN, 0);

    radio_play = new wxRadioButton( itemPanel2, ID_RADIOBUTTON3, _("Play"), wxDefaultPosition, wxDefaultSize, 0 );
    radio_play->SetValue(false);
    itemBoxSizer6->Add(radio_play, 1, wxALIGN_LEFT|wxALL, 5);

    wxArrayString list_backendStrings;
    list_backend = new wxChoice( itemPanel2, ID_CHOICE1, wxDefaultPosition, wxDefaultSize, list_backendStrings, 0 );
    itemBoxSizer6->Add(list_backend, 0, wxALIGN_LEFT|wxALL, 0);

    itemGridSizer1->Add(5, 5, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxBoxSizer* itemBoxSizer20 = new wxBoxSizer(wxVERTICAL);
    itemBoxSizer4->Add(itemBoxSizer20, 1, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

    wxBoxSizer* itemBoxSizer14 = new wxBoxSizer(wxVERTICAL);
    staticbox_drysweep->Add(itemBoxSizer14, 1, wxGROW|wxALL, 5);
    wxBoxSizer* itemBoxSizer15 = new wxBoxSizer(wxHORIZONTAL);
    itemBoxSizer14->Add(itemBoxSizer15, 0, wxGROW|wxALL, 5);
    text_dryfile = new wxTextCtrl( itemPanel2, ID_TEXTCTRL3, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer15->Add(text_dryfile, 1, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    btn_dryfile_browse = new wxButton( itemPanel2, ID_BUTTON5, _("Browse..."), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer15->Add(btn_dryfile_browse, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    itemBoxSizer14->Add(5, 5, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

    wxFlexGridSizer* itemFlexGridSizer19 = new wxFlexGridSizer(0, 6, 0, 0);
    itemBoxSizer14->Add(itemFlexGridSizer19, 0, wxGROW|wxALL, 5);
    wxStaticText* itemStaticText20 = new wxStaticText( itemPanel2, wxID_STATIC, _("Length (seconds):"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer19->Add(itemStaticText20, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    spin_dry_length = new wxSpinCtrl( itemPanel2, ID_SPINCTRL, wxT("0"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 100, 0 );
    itemFlexGridSizer19->Add(spin_dry_length, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxStaticText* itemStaticText22 = new wxStaticText( itemPanel2, wxID_STATIC, _("Start freq:"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer19->Add(itemStaticText22, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    spin_dry_f1 = new wxSpinCtrl( itemPanel2, ID_SPINCTRL1, wxT("0"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 100, 0 );
    itemFlexGridSizer19->Add(spin_dry_f1, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxStaticText* itemStaticText24 = new wxStaticText( itemPanel2, wxID_STATIC, _("End freq:"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer19->Add(itemStaticText24, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    spin_dry_f2 = new wxSpinCtrl( itemPanel2, ID_SPINCTRL2, wxT("0"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 100, 0 );
    itemFlexGridSizer19->Add(spin_dry_f2, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxStaticText* itemStaticText26 = new wxStaticText( itemPanel2, wxID_STATIC, _("Preroll (ms):"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer19->Add(itemStaticText26, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    spin_dry_preroll = new wxSpinCtrl( itemPanel2, ID_SPINCTRL3, wxT("0"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 100, 0 );
    itemFlexGridSizer19->Add(spin_dry_preroll, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxStaticText* itemStaticText28 = new wxStaticText( itemPanel2, wxID_STATIC, _("Marker (ms):"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer19->Add(itemStaticText28, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    spin_dry_marker = new wxSpinCtrl( itemPanel2, ID_SPINCTRL4, wxT("0"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 100, 0 );
    itemFlexGridSizer19->Add(spin_dry_marker, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxStaticText* itemStaticText30 = new wxStaticText( itemPanel2, wxID_STATIC, _("Gap (ms):"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer19->Add(itemStaticText30, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    spin_dry_gap = new wxSpinCtrl( itemPanel2, ID_SPINCTRL5, wxT("0"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 100, 0 );
    itemFlexGridSizer19->Add(spin_dry_gap, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    itemBoxSizer14->Add(5, 5, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

    wxBoxSizer* itemBoxSizer33 = new wxBoxSizer(wxVERTICAL);
    itemBoxSizer14->Add(itemBoxSizer33, 0, wxGROW|wxALL, 0);
    wxBoxSizer* itemBoxSizer34 = new wxBoxSizer(wxHORIZONTAL);
    itemBoxSizer33->Add(itemBoxSizer34, 0, wxGROW|wxALL, 5);
    wxStaticText* itemStaticText35 = new wxStaticText( itemPanel2, wxID_STATIC, _("\"Round trip\" mode: adjust sweep settings above and select in/out audio ports."), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer34->Add(itemStaticText35, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxFlexGridSizer* itemFlexGridSizer36 = new wxFlexGridSizer(0, 5, 0, 0);
    itemBoxSizer33->Add(itemFlexGridSizer36, 0, wxGROW|wxALL, 5);
    wxStaticText* itemStaticText37 = new wxStaticText( itemPanel2, wxID_STATIC, _("Play dry"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer36->Add(itemStaticText37, 0, wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxArrayString list_jack_dryStrings;
    list_jack_dry = new wxComboBox( itemPanel2, ID_COMBOBOX, wxEmptyString, wxDefaultPosition, wxDefaultSize, list_jack_dryStrings, wxCB_DROPDOWN );
    itemFlexGridSizer36->Add(list_jack_dry, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    itemFlexGridSizer36->Add(5, 5, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxStaticText* itemStaticText40 = new wxStaticText( itemPanel2, wxID_STATIC, _("Record wet L"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer36->Add(itemStaticText40, 0, wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxArrayString list_jack_wet_lStrings;
    list_jack_wet_l = new wxComboBox( itemPanel2, ID_COMBOBOX1, wxEmptyString, wxDefaultPosition, wxDefaultSize, list_jack_wet_lStrings, wxCB_DROPDOWN );
    itemFlexGridSizer36->Add(list_jack_wet_l, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    itemFlexGridSizer36->Add(5, 5, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    chk_jack_mono = new wxCheckBox( itemPanel2, ID_CHECKBOX6, _("Mono"), wxDefaultPosition, wxDefaultSize, 0 );
    chk_jack_mono->SetValue(false);
    itemFlexGridSizer36->Add(chk_jack_mono, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    itemFlexGridSizer36->Add(5, 5, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxStaticText* itemStaticText45 = new wxStaticText( itemPanel2, wxID_STATIC, _("Record wet R"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer36->Add(itemStaticText45, 0, wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxArrayString list_jack_wet_rStrings;
    list_jack_wet_r = new wxComboBox( itemPanel2, ID_COMBOBOX2, wxEmptyString, wxDefaultPosition, wxDefaultSize, list_jack_wet_rStrings, wxCB_DROPDOWN );
    itemFlexGridSizer36->Add(list_jack_wet_r, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    itemFlexGridSizer36->AddGrowableCol(1);
    itemFlexGridSizer36->AddGrowableCol(4);

    sizer_meters = new wxBoxSizer(wxVERTICAL);
    itemBoxSizer33->Add(sizer_meters, 0, wxGROW|wxALL, 5);

    itemNotebook1->AddPage(itemPanel2, _("Dry sweep"));

    wxPanel* itemPanel47 = new wxPanel( itemNotebook1, ID_PANEL1, wxDefaultPosition, wxDefaultSize, wxSUNKEN_BORDER|wxTAB_TRAVERSAL );
    itemPanel47->SetExtraStyle(wxWS_EX_VALIDATE_RECURSIVELY);
    staticbox_deconvolv = new wxBoxSizer(wxVERTICAL);
    itemPanel47->SetSizer(staticbox_deconvolv);

    staticbox_deconvolv->Add(5, 5, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

    wxBoxSizer* itemBoxSizer8 = new wxBoxSizer(wxHORIZONTAL);
    staticbox_deconvolv->Add(itemBoxSizer8, 0, wxGROW|wxALL, 0);
    wxBoxSizer* itemBoxSizer10 = new wxBoxSizer(wxVERTICAL);
    itemBoxSizer8->Add(itemBoxSizer10, 1, wxGROW|wxALL, 0);
    wxBoxSizer* itemBoxSizer12 = new wxBoxSizer(wxHORIZONTAL);
    itemBoxSizer10->Add(itemBoxSizer12, 0, wxGROW|wxALL, 0);
    wxBoxSizer* itemBoxSizer13 = new wxBoxSizer(wxVERTICAL);
    itemBoxSizer12->Add(itemBoxSizer13, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
    wxStaticText* itemStaticText14 = new wxStaticText( itemPanel47, wxID_STATIC, _("Scan directory:"), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer13->Add(itemStaticText14, 0, wxALIGN_LEFT|wxALL, 0);

    chk_inputdir_recursive = new wxCheckBox( itemPanel47, ID_CHECKBOX3, _("Include sub"), wxDefaultPosition, wxDefaultSize, 0 );
    chk_inputdir_recursive->SetValue(false);
    itemBoxSizer13->Add(chk_inputdir_recursive, 0, wxALIGN_LEFT|wxALL, 0);

    text_inputdir = new wxTextCtrl( itemPanel47, ID_TEXTCTRL1, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer12->Add(text_inputdir, 1, wxALIGN_TOP|wxALL, 5);

    itemBoxSizer10->Add(5, 5, 1, wxALIGN_CENTER_HORIZONTAL|wxALL, 1);

    wxBoxSizer* itemBoxSizer21 = new wxBoxSizer(wxHORIZONTAL);
    itemBoxSizer10->Add(itemBoxSizer21, 0, wxGROW|wxALL, 0);
    wxStaticText* itemStaticText23 = new wxStaticText( itemPanel47, wxID_STATIC, _("List of sweep files to process:"), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer21->Add(itemStaticText23, 0, wxALIGN_CENTER_VERTICAL|wxLEFT|wxRIGHT|wxBOTTOM, 5);

    wxGridSizer* itemGridSizer24 = new wxGridSizer(0, 2, 0, 0);
    itemBoxSizer8->Add(itemGridSizer24, 0, wxALIGN_CENTER_VERTICAL|wxALL, 0);
    btn_inputdir_scan = new wxButton( itemPanel47, ID_BUTTON2, _("Scan"), wxDefaultPosition, wxDefaultSize, 0 );
    itemGridSizer24->Add(btn_inputdir_scan, 0, wxGROW|wxALIGN_TOP|wxALL, 5);

    btn_inputdir_browse = new wxButton( itemPanel47, ID_BUTTON3, _("Browse..."), wxDefaultPosition, wxDefaultSize, 0 );
    itemGridSizer24->Add(btn_inputdir_browse, 0, wxGROW|wxALIGN_TOP|wxALL, 5);

    itemGridSizer24->Add(5, 5, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    btn_inputfiles_add = new wxButton( itemPanel47, ID_BUTTON, _("Add files..."), wxDefaultPosition, wxDefaultSize, 0 );
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
    list_inputfiles = new wxListBox( itemPanel47, ID_LISTBOX, wxDefaultPosition, wxDefaultSize, list_inputfilesStrings, wxLB_SINGLE );
    staticbox_deconvolv->Add(list_inputfiles, 1, wxGROW|wxLEFT|wxRIGHT|wxBOTTOM, 5);

    wxBoxSizer* itemBoxSizer63 = new wxBoxSizer(wxHORIZONTAL);
    staticbox_deconvolv->Add(itemBoxSizer63, 0, wxGROW|wxALL, 0);
    wxStaticText* itemStaticText64 = new wxStaticText( itemPanel47, wxID_STATIC, _("Output directory:"), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer63->Add(itemStaticText64, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    text_outputdir = new wxTextCtrl( itemPanel47, ID_TEXTCTRL, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer63->Add(text_outputdir, 1, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    btn_outputdir_browse = new wxButton( itemPanel47, ID_BUTTON1, _("Browse..."), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer63->Add(btn_outputdir_browse, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxBoxSizer* itemBoxSizer67 = new wxBoxSizer(wxHORIZONTAL);
    staticbox_deconvolv->Add(itemBoxSizer67, 0, wxGROW|wxALL, 5);
    wxBoxSizer* itemBoxSizer68 = new wxBoxSizer(wxVERTICAL);
    itemBoxSizer67->Add(itemBoxSizer68, 0, wxALIGN_CENTER_VERTICAL|wxALL, 0);
    wxBoxSizer* itemBoxSizer69 = new wxBoxSizer(wxHORIZONTAL);
    itemBoxSizer68->Add(itemBoxSizer69, 0, wxGROW|wxALL, 0);
    wxStaticText* itemStaticText70 = new wxStaticText( itemPanel47, wxID_STATIC, _("Sweep alignment method:"), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer69->Add(itemStaticText70, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxArrayString list_alignmethodStrings;
    list_alignmethod = new wxChoice( itemPanel47, ID_CHOICE, wxDefaultPosition, wxDefaultSize, list_alignmethodStrings, 0 );
    itemBoxSizer69->Add(list_alignmethod, 1, wxGROW|wxALL, 5);

    wxBoxSizer* itemBoxSizer72 = new wxBoxSizer(wxHORIZONTAL);
    itemBoxSizer68->Add(itemBoxSizer72, 0, wxGROW|wxALL, 0);
    wxStaticText* itemStaticText1 = new wxStaticText( itemPanel47, wxID_STATIC, _("High-pass filter:"), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer72->Add(itemStaticText1, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    itemBoxSizer72->Add(5, 5, 1, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxArrayString list_hpf_modeStrings;
    list_hpf_mode = new wxChoice( itemPanel47, ID_CHOICE2, wxDefaultPosition, wxDefaultSize, list_hpf_modeStrings, 0 );
    itemBoxSizer72->Add(list_hpf_mode, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    spin_hpf_freq = new wxSpinCtrl( itemPanel47, ID_SPINCTRL6, wxT("0"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 100, 0 );
    itemBoxSizer72->Add(spin_hpf_freq, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxBoxSizer* itemBoxSizer77 = new wxBoxSizer(wxHORIZONTAL);
    itemBoxSizer68->Add(itemBoxSizer77, 0, wxGROW|wxALL, 0);
    wxStaticText* itemStaticText2 = new wxStaticText( itemPanel47, wxID_STATIC, _("Low-pass filter:"), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer77->Add(itemStaticText2, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    itemBoxSizer77->Add(5, 5, 1, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxArrayString list_lpf_modeStrings;
    list_lpf_mode = new wxChoice( itemPanel47, ID_CHOICE3, wxDefaultPosition, wxDefaultSize, list_lpf_modeStrings, 0 );
    itemBoxSizer77->Add(list_lpf_mode, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    spin_lpf_freq = new wxSpinCtrl( itemPanel47, ID_SPINCTRL7, wxT("0"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 100, 0 );
    itemBoxSizer77->Add(spin_lpf_freq, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxBoxSizer* itemBoxSizer82 = new wxBoxSizer(wxVERTICAL);
    itemBoxSizer67->Add(itemBoxSizer82, 0, wxALIGN_CENTER_VERTICAL|wxALL, 0);
    chk_zeroalign = new wxCheckBox( itemPanel47, ID_CHECKBOX, _("Zero-align IR peak"), wxDefaultPosition, wxDefaultSize, 0 );
    chk_zeroalign->SetValue(false);
    itemBoxSizer82->Add(chk_zeroalign, 1, wxALIGN_LEFT|wxALL, 5);

    chk_forcemono = new wxCheckBox( itemPanel47, ID_CHECKBOX1, _("Force mono output"), wxDefaultPosition, wxDefaultSize, 0 );
    chk_forcemono->SetValue(false);
    itemBoxSizer82->Add(chk_forcemono, 1, wxALIGN_LEFT|wxALL, 5);

    chk_overwrite = new wxCheckBox( itemPanel47, ID_CHECKBOX7, _("Overwrite existing files"), wxDefaultPosition, wxDefaultSize, 0 );
    chk_overwrite->SetValue(false);
    itemBoxSizer82->Add(chk_overwrite, 0, wxALIGN_LEFT|wxALL, 5);

    chk_debug = new wxCheckBox( itemPanel47, ID_CHECKBOX2, _("Dump debug sweeps"), wxDefaultPosition, wxDefaultSize, 0 );
    chk_debug->SetValue(false);
    itemBoxSizer82->Add(chk_debug, 1, wxALIGN_LEFT|wxALL, 5);

    wxBoxSizer* itemBoxSizer87 = new wxBoxSizer(wxVERTICAL);
    itemBoxSizer67->Add(itemBoxSizer87, 0, wxGROW|wxALL, 0);
    wxBoxSizer* itemBoxSizer88 = new wxBoxSizer(wxHORIZONTAL);
    itemBoxSizer87->Add(itemBoxSizer88, 1, wxGROW|wxALL, 0);
    wxStaticText* itemStaticText89 = new wxStaticText( itemPanel47, wxID_STATIC, _("Sweep silence thresh (dB):"), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer88->Add(itemStaticText89, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    itemBoxSizer88->Add(5, 5, 1, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    spin_sweep_thr = new wxSpinCtrl( itemPanel47, ID_SPINCTRL8, wxT("0"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 100, 0 );
    itemBoxSizer88->Add(spin_sweep_thr, 0, wxALIGN_CENTER_VERTICAL|wxALL, 0);

    wxBoxSizer* itemBoxSizer92 = new wxBoxSizer(wxHORIZONTAL);
    itemBoxSizer87->Add(itemBoxSizer92, 1, wxGROW|wxALL, 0);
    wxStaticText* itemStaticText93 = new wxStaticText( itemPanel47, wxID_STATIC, _("IR silence thresh (dB):"), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer92->Add(itemStaticText93, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    itemBoxSizer92->Add(5, 5, 1, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    spin_ir_thresh = new wxSpinCtrl( itemPanel47, ID_SPINCTRL10, wxT("0"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 100, 0 );
    itemBoxSizer92->Add(spin_ir_thresh, 0, wxALIGN_CENTER_VERTICAL|wxALL, 0);

    wxBoxSizer* itemBoxSizer96 = new wxBoxSizer(wxHORIZONTAL);
    itemBoxSizer87->Add(itemBoxSizer96, 1, wxGROW|wxALL, 0);
    wxStaticText* itemStaticText97 = new wxStaticText( itemPanel47, wxID_STATIC, _("(stereo) Offset channels"), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer96->Add(itemStaticText97, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    itemBoxSizer96->Add(5, 5, 1, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    spin_chn_offset = new wxSpinCtrl( itemPanel47, ID_SPINCTRL9, wxT("0"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 100, 0 );
    itemBoxSizer96->Add(spin_chn_offset, 0, wxALIGN_CENTER_VERTICAL|wxALL, 0);

    itemNotebook1->AddPage(itemPanel47, _("Deconvolver"));

    itemBoxSizer1->Add(itemNotebook1, 1, wxGROW|wxALL, 5);

    wxBoxSizer* itemBoxSizer2 = new wxBoxSizer(wxHORIZONTAL);
    itemBoxSizer1->Add(itemBoxSizer2, 0, wxALIGN_RIGHT|wxALL, 5);

    btn_about = new wxButton( itemFrame1, wxID_ABOUT, _("&About"), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer2->Add(btn_about, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    itemBoxSizer2->Add(5, 5, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    itemBoxSizer2->Add(5, 5, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    btn_cancel = new wxButton( itemFrame1, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer2->Add(btn_cancel, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    btn_ok = new wxButton( itemFrame1, wxID_OK, _("OK"), wxDefaultPosition, wxDefaultSize, 0 );
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
