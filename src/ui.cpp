
#include "dirt.h"
#include "ui.h"

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
wxEND_EVENT_TABLE ();

c_mainwindow::c_mainwindow ()
: ui_mainwindow (NULL, wxID_ANY) {
  CP
}

c_mainwindow::~c_mainwindow () {
  CP
}

void c_mainwindow::on_about (wxCommandEvent &ev) {
  CP
  wxMessageBox ("dirt");
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

// c_app

c_app::c_app ()
: wxApp::wxApp () {
  CP
}

c_app::~c_app () {
  CP
}

bool c_app::OnInit () {
  debug ("start");
  if (!wxApp::OnInit ()) return false;
  mainwindow = new c_mainwindow;
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

int wx_main (int argc, char **argv) {
  int retval = 0;
  debug ("start");
    
  //check_endianness ();
  
  // THE RIGHT WAY TO DO IT:
  g_app = new c_app ();
  wxApp::SetInstance (g_app);
  retval = wxEntry (argc, argv);
  //delete g_app;
  debug ("end, retval=%d", retval);
  return retval;
}
