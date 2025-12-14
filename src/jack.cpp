
/* DIRT - Delt's Impulse Response Tool
 * Version 0.1
 *
 * Licensed under the GPL. See dirt.h and dirt.cpp for more info.
 */

#include "jack.h"

#ifdef USE_JACK

#ifdef DEBUG
#define CMDLINE_DEBUG
#include "cmdline/cmdline.h"
#define debug(...) cmdline_debug(stderr,ANSI_GREEN,__FILE__,__LINE__,__FUNC__,__VA_ARGS__)
#else
#define debug(...)
#define CP
#define BP
#endif

// ...more callback red tape...

static audiostate determine_state_from_flags (c_jackclient *j) {
  if (!j || !j->client)
    return audiostate::NOTREADY;
    
  if (j->force_notready) {
    //j->force_notready = false;
    return audiostate::NOTREADY;
  }
    
  if (!j->play_go && !j->rec_go)
    return audiostate::IDLE;
  
  if (!j->play_go && j->rec_go && j->monitor_only)
    return audiostate::MONITOR;

  if (j->play_go && j->rec_go && j->monitor_only)
    return audiostate::PLAYMONITOR;

  if (j->play_go && !j->rec_go)
    return audiostate::PLAY;

  if (!j->monitor_only && j->rec_go && !j->play_go)
    return audiostate::REC;

  return (!j->monitor_only) ? audiostate::PLAYREC : audiostate::MONITOR;
}

static const char *g_state_names [] = {
  "NOTREADY",
  "IDLE",
  "MONITOR",
  "PLAY",
  "REC",
  "PLAYREC",
  "PLAYMONITOR",
  "MAX",
  NULL
};

// our JACK callback, takes care of actual audio i/o buffers
static int jack_process_cb (jack_nframes_t nframes, void *arg) {
  c_jackclient *j = (c_jackclient *) arg;
  if (!j) return 0;
  //debug ("j=%lx, &j->vu=%lx", (long int) j, (long int) &j->vu);

  audiostate prev_state = j->state;   // from last callback
  audiostate new_state  = determine_state_from_flags (j);
  //debug ("prev_state=%d, new_state=%d", prev_state, new_state);
  //debug ("index=%ld, rec_index=%ld", (long int) index, (long int) rec_index);

  if (new_state != prev_state) {
    debug ("state change: %s (%d) -> %s (%d)",
           g_state_names [(int) prev_state], prev_state,
           g_state_names [(int) new_state], new_state);
    
    // leave previous state
    switch (prev_state) {
      case audiostate::PLAY:        j->on_play_stop ();     break;
      case audiostate::REC:         j->on_record_stop ();   break;
      case audiostate::MONITOR:     j->on_arm_rec_stop ();  break;
      case audiostate::PLAYREC:     j->on_playrec_stop ();  break;
      case audiostate::PLAYMONITOR: j->on_play_stop ();     break;
      default: break;
    }

    // enter new state
    switch (new_state) {
      case audiostate::PLAY:        if (prev_state != audiostate::REC) j->on_play_start();
                                                      break;
      case audiostate::REC:         j->on_record_start ();     break;
      case audiostate::MONITOR:     j->on_arm_rec_start ();    break;
      case audiostate::PLAYREC:     j->on_playrec_start ();    break;
      case audiostate::PLAYMONITOR: j->on_play_start ();       break;
      default: break;
    }

    j->state = new_state;
  }

  // state loop hooks
  switch (j->state) {
    case audiostate::NOTREADY:                            break;
    case audiostate::IDLE:        j->on_idle ();          break;
    case audiostate::PLAY:        j->on_play_loop ();     break;
    case audiostate::REC:         j->on_record_loop ();   break;
    case audiostate::MONITOR:     j->on_arm_rec_loop ();  break;
    case audiostate::PLAYREC:     j->on_playrec_loop ();  break;
    case audiostate::PLAYMONITOR: j->on_play_loop ();     break;
    default: CP break;
  }
  
  bool rec_stereo = (j->stereo_in && j->port_inR != NULL);
  //debug ("j->stereo_in=%d, j->port_inR=%ld, rec_stereo=%d",
  //(int) j->stereo_in, (size_t) j->port_inR, (int) rec_stereo);
  
  // PLAYBACK
  if (j->port_outL) {
    auto *port_outL =
        (jack_default_audio_sample_t *) jack_port_get_buffer (j->port_outL, nframes);
    // auto *port_outR = ... in case we add it later?
    
    if (j->play_go && j->sig_out_l && !j->sig_out_l->empty ()) {
      size_t limit = j->sig_out_l->size ();
      if (j->stereo_in && j->sig_out_r && !j->sig_out_r->empty ()) {
        limit = std::min (j->sig_out_l->size (), j->sig_out_r->size ());
      }
      
      for (jack_nframes_t i = 0; i < nframes; ++i) {
        float vL = 0.0f;
        
        if (j->play_index < limit) {
          vL = (*j->sig_out_l) [j->play_index++]; // our overloaded operator
          // if stereo-out later: vR = j->sig_out_r[j->play_index-1];
        } else {
            vL = 0.0f;
        }
        
        port_outL [i] = vL;
        // if (port_outR) port_outR [i] = vR;
      }
      
      // finished playback?
      if (j->play_index >= limit) {
        j->play_go = false;   // edge for state machine
      }
    } else {
      j->play_go = false;
      j->play_index = 0;
      // not playing: output silence
      for (jack_nframes_t i = 0; i < nframes; ++i) {
        port_outL [i] = 0.0f;
      }
    }
  } else {
    j->play_go = false;
    j->play_index = 0;
  }

  // RECORDING
  if (j->rec_go && j->port_inL /*&& j->sig_in_l*/) {
    auto *port_inL =
        (jack_default_audio_sample_t *) jack_port_get_buffer(j->port_inL, nframes);
    jack_default_audio_sample_t *port_inR = nullptr;
    if (j->port_inR && j->sig_in_r) {
      port_inR = (jack_default_audio_sample_t *)
        jack_port_get_buffer (j->port_inR, nframes);
    }
    
    if (!j->stereo_in || !rec_stereo) {
      j->vu.plus_r  = 0;
      j->vu.minus_r = 0;
      j->vu.clip_r       = false;
    }
    
    float currentbuf_l [nframes];
    float currentbuf_r [nframes];
    size_t rec_n = std::min ((size_t) nframes, (size_t) (j->rec_max - j->rec_index));
    size_t oldsize_l = j->sig_in_l ? j->sig_in_l->size () : 0;
    size_t oldsize_r = 0;
    //debug ("rec_n=%ld, nframes=%ld, oldsize %ld,%ld", (long int) rec_n,
    //       (long int) nframes, (long int) oldsize_l, (long int) oldsize_r);
    
    //debug ("nframes=%ld", (long int) nframes);
    for (jack_nframes_t i = 0; i < nframes; ++i) {
      float vL = port_inL [i];
      float vR = port_inR ? port_inR [i] : vL;
      //debug ("vL=%f, vR=%f", vL, vR);
      j->vu.sample (vL, vR);
      // store data only if actually recording
      if (!j->monitor_only && j->sig_in_l) {
        currentbuf_l [i] = vL;
        if (rec_stereo)
          currentbuf_r [i] = vR;
        else
          currentbuf_r [i] = 0.0;
        
        if (j->rec_index >= j->rec_max) {
          j->rec_go   = false;  // let state machine see state change
          break;
        }
      }
    }
    
    if (j->sig_in_l) {
      // copy this buffer to target c_wavebuffer(s)
      if (rec_stereo)
        oldsize_r = j->sig_in_r->size ();
      
      if (oldsize_r != 0 && oldsize_l != oldsize_r) {
        debug ("Size mismatch between buffers");
      }
      size_t oldsize = std::min (oldsize_l, oldsize_r);
      //sig_in_l->resize (oldsize + rec_n);
      j->sig_in_l->append (currentbuf_l, rec_n);
      if (rec_stereo) {
        //sig_in_r->resize (oldsize + rec_n);
        j->sig_in_r->append (currentbuf_r, rec_n);
      }
    }
    
    j->vu.update (); // the recording one
  } // here would be vu_out.update (); or similar. TODO: implement
  
  //debug ("end");
  return 0;
}

c_jackclient::c_jackclient (s_prefs *prefs)
    : c_audioclient (prefs) {
  backend = audio_driver::JACK;
  backend_name = "JACK";
}

c_jackclient::~c_jackclient () { CP
  debug ("start");
  if (client) {
    CP
    unregister ();
    debug ("CLOSING JACK CLIENT\n");    
    shutdown ();
    jack_client_close (client);
    client = NULL;
    force_notready = true; //state = audiostate::NOTREADY;
  }
  debug ("end");
}

int c_jackclient::disconnect_all (jack_port_t *port) {
  int i;
  CP
  const char **connected = jack_port_get_all_connections (client, port);
  if (connected) {
      for (i = 0; connected [i]; i++) {
          if (jack_port_flags (port) & JackPortIsInput)
            jack_disconnect(client,
                            connected [i],
                            jack_port_name (port));
          else
            jack_disconnect(client,
                            jack_port_name (port),
                            connected [i]);
      }
      jack_free(connected);
  }  
  
  return i;
}

bool c_jackclient::connect (jack_port_t *port, std::string name) {
  if (jack_port_flags (port) & JackPortIsInput)
    return jack_connect (client, name.c_str (), jack_port_name (port));
  else
    return jack_connect (client, jack_port_name (port), name.c_str ());
}

// these two APPEND the number of system/default ports to passed vector
int /*c_jackclient::*/j_get_playback_ports (jack_client_t *client,
                                          int howmany,
                                          std::vector<std::string> &v,
                                          bool default_only) {
  debug ("start");
  //v.clear ();
  if (!client || howmany <= 0) return 0;

  const char **ports = jack_get_ports (
    client,
    nullptr,
    nullptr,
    (default_only ? JackPortIsPhysical : 0) | JackPortIsInput   // speakers = physical inputs
  );

  if (!ports)
    return 0;

  int count = 0;

  for (int i = 0; ports [i] && count < howmany; ++i) {
    v.push_back (std::string (ports[i]));
    ++count;
  }

  jack_free (ports);

  debug ("end");
  return count;
}

int /*c_jackclient::*/j_get_capture_ports (jack_client_t *client,
                                         int howmany,
                                         std::vector<std::string> &v,
                                         bool default_only) {
  debug ("start");
  //v.clear ();
  if (!client || howmany <= 0) return 0;

  const char **ports = jack_get_ports (
    client,
    nullptr,
    nullptr,
    (default_only ? JackPortIsPhysical : 0) | JackPortIsOutput
  );

  if (!ports)
    return 0;

  int count = 0;

  for (int i = 0; ports [i] && count < howmany; ++i) {
    v.push_back (std::string (ports[i]));
    ++count;
  }

  jack_free (ports);
  
  debug ("end");
  return count;
}

int j_get_default_capture (jack_client_t *c, int howmany, std::vector<std::string> &v)
    { CP return j_get_capture_ports (c, howmany, v, false); }
int j_get_default_playback (jack_client_t *c, int howmany, std::vector<std::string> &v)
    { CP return j_get_playback_ports (c, howmany, v, false); }
  

#define MAX_JACK_PORTS 999

int c_jackclient::get_input_ports (std::vector<std::string> &v) {
  CP
  int n = j_get_capture_ports (client, MAX_JACK_PORTS, v);
  debug ("n=%d, v has %d strings", n, v.size ());
  return n;
}

int c_jackclient::get_output_ports (std::vector<std::string> &v) {
  CP
  int n = j_get_playback_ports (client, MAX_JACK_PORTS, v);
  debug ("n=%d, v has %d strings", n, v.size ());
  return n;
}

bool c_jackclient::init (std::string clientname,      // = "",
                         int _samplerate,        // = -1, // ignored for now
                         //const char *jack_out_port,
                         //const char *jack_in_port,
                         bool st) {     // = true) {
  debug ("start");
  if (client) {
    // already have a client, nothing to do
    return true;
  }
  
  stereo_in = st;
  jack_status_t status = JackFailure;
  CP
  // choose a client name:
  const char *name = nullptr;
  if (!clientname.empty ()) {
    // explicit from caller (e.g. formatted with argv [0])
    name = clientname.c_str ();
  } else if (prefs && !prefs->jack_name.empty ()) {
    // from prefs (may literally be "%s_ir_sweep" if not formatted)
    name = prefs->jack_name.c_str ();
  } else {
    // last-chance default
    name = "deconvolver";
  }
  CP
  if (!client) {
    debug ("OPENING JACK CLIENT\n");
    client = jack_client_open (name, JackNullOption, &status);
  }
  if (!client) {
    std::cerr << "Error: cannot connect to JACK (client name \""
              << name << "\")\n";
    return false;
  }
  CP
  // JACK is alive, get its sample rate
  samplerate = (int) jack_get_sample_rate (client);
  bufsize = (int) jack_get_buffer_size (client);
  
  if (prefs) {
    if (!prefs->quiet && prefs->sweep_sr != samplerate) {
        std::cerr << "Note: overriding sample rate (" << prefs->sweep_sr
                  << " Hz) with JACK sample rate " << samplerate << " Hz.\n";
    }
    prefs->sweep_sr = samplerate;
  }
  
  // Reset runtime state
  play_go = false;
  rec_go  = false;
  play_index   = 0;
  //sig_in_l->clear ();
  //sig_in_r->clear ();
  //sig_out_l->clear ();
  //sig_out_r->clear ();
  sig_in_l   = NULL;
  sig_in_r   = NULL;
  sig_out_l  = NULL;
  sig_out_r  = NULL;
  port_inL = port_inR = nullptr;
  //port_outL = port_outR = nullptr;
  // register jack ports
  
  // activate client
  
  jack_set_process_callback (client, jack_process_cb, this);
  
  if (jack_activate (client) != 0) {
    std::cerr << "Error: cannot activate JACK client.\n";
    return false;
  }
  
  /*
  port_outL = jack_port_register (client, "out",
                            JACK_DEFAULT_AUDIO_TYPE,
                            JackPortIsOutput, 0);
  / *port_outR = jack_port_register (client, "out_R",
                            JACK_DEFAULT_AUDIO_TYPE,
                            JackPortIsOutput, 0);* /
  if (is_stereo) {
    port_inL  = jack_port_register (client, "in_L",
                              JACK_DEFAULT_AUDIO_TYPE,
                              JackPortIsInput, 0);
    port_inR  = jack_port_register (client, "in_R",
                              JACK_DEFAULT_AUDIO_TYPE,
                              JackPortIsInput, 0);
  } else {
    port_inL  = jack_port_register (client, "in",
                              JACK_DEFAULT_AUDIO_TYPE,
                              JackPortIsInput, 0);
  }*/
  //CP
  //if (!register_output (false)) return false;
  //CP
  //if (!register_input (is_stereo)) return false;
  CP
  
  force_notready = false; //state = audiostate::IDLE;
  
  // ...wuh duh fuuuh?
  //(void) chan_in;
  //(void) chan_out;
  
  determine_state_from_flags (this);
  
  debug ("end");
  return true;
}

bool c_jackclient::shutdown () {
  if (!jack_deactivate (client) == 0)
    return false;
  
  jack_client_close (client);
  client = NULL;

  return true;
}


int c_jackclient::get_samplerate () {
  return samplerate;
}

int c_jackclient::get_bufsize () {
  return bufsize;
}

int c_jackclient::get_bitdepth () {
  return bitdepth;
}

bool c_jackclient::set_stereo (bool b) {
  debug ("start, b=%d", (int) b);
  if (b != stereo_in) {
    if (!unregister ())               { 
      debug ("unregister failed"); BP
      //return false;
    }
    
    if (!register_input (b)) {
      debug ("register input failed"); BP
      return false;
    }
    
    if (!register_output (false)) {
      debug ("register output failed"); BP
      return false;
    }
    
    connect_ports ();
    
  }
  stereo_in = b;
  
  //if (!b) sig_inR.clear ();
  return true;
}

bool c_jackclient::connect_ports (bool in, bool out) {
  debug ("start, in=%d, out=%d", (int) in, (int) out);
  bool ok = true;
  if (out && !port_outL)               { CP ok = false; }
  if (in && !port_inL)                 { CP ok = false; }
  if (in && stereo_in && !port_inR)    { CP ok = false; }
  
  if (!ok) { CP
  //if (!port_outL || !port_inL || (is_stereo && !port_inR)) {
    std::cerr << "Error: missing JACK ports.\n";
    //jack_deactivate (client);
    return false;
  }
  
  // autoconnect output where it makes sense:
  // any port given: auto connect to it
  // no port given:
  //   playsweep: no -W? auto connect to system default
  //   other: no autoconnect
  if (out) {
    if (prefs->portname_dry.length () > 0) { CP
      debug ("got portname_dry");
      // connect to portname_dry
      int err = jack_connect (client,
                              jack_port_name (port_outL),
                              prefs->portname_dry.c_str ());
      if (err) std::cerr << "warning: failed to connect to JACK input port "
                         << prefs->portname_dry << std::endl;
    } else { CP
      if (prefs->mode == opmode::PLAYSWEEP && !prefs->sweepwait) {
        // connect to system default
        std::vector<std::string> strv;
        int n = j_get_default_playback (client, 1, strv);
        if (n == 1) {
          int err = jack_connect (client,
                                  jack_port_name (port_outL),
                                  strv [0].c_str ());
           if (err) std::cerr << "warning: failed to connect to JACK output port"
                              << strv [0] << std::endl;
        }
      }
    }
  }
  
  // autoconnect input where it makes sense:
  // any port given: auto connect to it
  // no port given: no autoconnect
  if (in) {
    std::cout << "output port L: " << prefs->portname_wetL << std::endl;
    std::cout << "output port R: " << prefs->portname_wetR << std::endl;
    if (prefs->portname_wetL.length () > 0) { CP
      debug ("got portname_wetL");
      // connect to portname_wetL
      int err = jack_connect (client,
                              prefs->portname_wetL.c_str (),
                              jack_port_name (port_inL));
      if (err) std::cerr << "warning: failed to connect to JACK output port " 
                         << prefs->portname_wetL << std::endl;
                              
    }
    if (prefs->portname_wetR.length () > 0) { CP
      debug ("got portname_wetL");
      // connect to portname_wetL
      int err = jack_connect (client,
                              prefs->portname_wetR.c_str (),
                              jack_port_name (port_inR));
      if (err) std::cerr << "warning: failed to connect to JACK output port "
                         << prefs->portname_wetR << std::endl;
                              
    }
  }
  
  return true;
}

bool c_jackclient::register_output (bool st) {
  debug ("start (TODO: fix/complete this)");
  
  if (!client) return false;
  
  force_notready = false;
  debug ("state=%d (%s)", (int) state, g_state_names [(int) state]);
  
  if (port_outL) {
    jack_port_unregister (client, port_outL);
    port_outL = NULL;
  }
  
  // register jack ports
  if (!port_outL) {
    port_outL = jack_port_register (client, "out",
                              JACK_DEFAULT_AUDIO_TYPE,
                              JackPortIsOutput, 0);
  }

  // if (.....)
  /*port_outR = jack_port_register (client, "out_R",
                            JACK_DEFAULT_AUDIO_TYPE,
                            JackPortIsOutput, 0);*/
  if (!port_outL)
    return false;
  /*if (b && !port_outR)
    return false;*/
  if (port_inL || port_inR) {
    debug ("connecting ports by default");
    connect_ports ();
  }
  
  debug ("end");
  return true;
}

bool c_jackclient::register_input (bool st) { CP
  debug ("start, st=%s", st ? "true" : "false");
  //if (st == is_stereo)
  //  return true;
  
  if (!client) return false;
  
  debug ("state=%d (%s)", (int) state, g_state_names [(int) state]);
  
  stereo_in = st;
  
  if (port_inL || port_inR) {
    CP
    unregister ();
  }
  
  if (stereo_in) { CP
    if (!port_inL) port_inL  = jack_port_register (client, "in_L",
                                                  JACK_DEFAULT_AUDIO_TYPE,
                                                  JackPortIsInput, 0);
    if (!port_inR) port_inR  = jack_port_register (client, "in_R",
                                                  JACK_DEFAULT_AUDIO_TYPE,
                                                  JackPortIsInput, 0);
  } else { CP
    if (!port_inL) port_inL  = jack_port_register (client, "in",
                                                  JACK_DEFAULT_AUDIO_TYPE,
                                                  JackPortIsInput, 0);
  }
  
  if (!port_inL) {
    CP
    return false;
  }
  if (stereo_in && !port_inR) {
    CP
    return false;
  }
  
  if (port_outL /*|| port_outR*/) {
    debug ("connecting ports by default");
    connect_ports ();
  }
  
  debug ("end");
  return true;
}

bool c_jackclient::unregister () {
  debug ("start");
  if (!client) {
    CP
    return false;
  }
  
  if (port_outL) jack_port_unregister (client, port_outL);
  if (port_inL) jack_port_unregister (client, port_inL);
  if (port_inR) jack_port_unregister (client, port_inR);
  port_outL = NULL;
  port_inL = NULL;
  port_inR = NULL;
  
  //jack_deactivate (client);
  //jack_client_close (client);
  stop ();
  force_notready = true; //state = audiostate::NOTREADY;
  
  debug ("end");
  return true;
}

bool c_jackclient::ready () {
  BP
  //if (client != NULL) { CP return false;} <--- wtf?
  //if (state != audiostate::NOTREADY) { CP return false;}
  if (!client) { CP return false; }
  if (state == audiostate::NOTREADY) { CP return false; }
   
  return true;
}

bool c_jackclient::arm_record () {
  CP
  if (rec_go /*|| monitor_only*/)
    return false;
  
  rec_go = true;
  monitor_only = true;
  
  //state = audiostate::MONITOR;
  
  return true;
}

bool c_jackclient::play (c_wavebuffer *sig_l,
                         c_wavebuffer *sig_r) {
  debug ("start");
  if (!client) {
    return false;
  }
  
  //dec_->on_play_start ();
  stereo_out = false;
  sig_out_l = sig_l;
  if (sig_out_r) {
    sig_out_r = sig_r;
    stereo_out = true;
  }
  play_index   = 0;
  
  /*size_t limit = sig_out_l.size ();
  if (is_stereo && !sig_out_r.empty ())
    limit = std::min (sig_out_l.size (), sig_out_r.size ());
  */
  play_go = true;
  rec_go = false;
  debug ("state: %d", (int) state);
  //if (block)                         // TODO: fix
  //  while (index < sig_out_l.size () /*&& ((!is_stereo) || index < sig_out_r.size ())*/) {
  //    usleep (10 * 1000); // 10 ms
  //    //dec_->on_play_loop ();
  //  }
  
  //dec_->on_play_stop ();

  debug ("end");
  return true;
}

// for just 1 channel... do we play it in 1 channel or in both?
bool c_jackclient::play (c_wavebuffer *sig) {
  //c_wavebuffer dummy_r;
  return play (sig, NULL/*dummy_r*/);
}

bool c_jackclient::rec (c_wavebuffer *in_l, c_wavebuffer *in_r) {
  debug ("start");
  if (!client || rec_go)
    return false;
  
  //stereo_in = false;
  sig_in_l = in_l;
  sig_in_l->clear ();
  sig_in_l->set_samplerate (samplerate);
  if (sig_in_r) {
    sig_in_r = in_r;
    sig_in_r->clear ();
    sig_in_r->set_samplerate (samplerate);
    stereo_in = true;
  }
  monitor_only = false;
  rec_go = true;
  play_go = false;
  rec_index = 0;
  
  debug ("end");
  return true;
}

// just fixed buffers for now
bool c_jackclient::playrec (c_wavebuffer *out_l,
                            c_wavebuffer *out_r,
                            c_wavebuffer *in_l,
                            c_wavebuffer *in_r) {
  debug ("start");

  // ensure JACK client
  if (!client) {
    //if (!jack_init(std::string (), chn_stereo, chn_stereo)) {
      return false;
    //}
  }
  
  if (!play (out_l, out_r) || !rec (in_l, in_r))
    return false;
  
  // prepare state
  play_index = 0;
  rec_index = 0;
  sig_out_l  = out_l;
  sig_in_l = in_l;

  in_l->clear ();
  if (stereo_in) {
    in_r->clear ();
    sig_in_r = in_r;
  }
  // capture same length + 2 sec. extra tail
  const size_t extra_tail = (size_t) (2.0 * samplerate); // 0.5s
  rec_max = std::max (out_l->size (), out_r->size ()) + extra_tail;
  play_go   = true;
  rec_go    = true;
  monitor_only = false;
  //in_l->assign (rec_max, 0.0f);
  
  //is_stereo = (prefs->portname_wetR.length () > 0); <--- nope
  //dec_->on_playrec_start ();

  // copy captured mono buffer out
  /*in_l = sig_in_l;
  if (is_stereo)
    in_r = sig_in_r;*/

  //dec_->on_playrec_stop ();
  
  debug ("end");
  return true;
}

bool c_jackclient::stop (bool also_stop_monitor) { CP
  return stop_playback () && stop_record (also_stop_monitor);
}

bool c_jackclient::stop_playback () {
  debug ("start");
  if (!play_go)
    return false;
    
  play_go = false;

  debug ("end");
  return true;
}

bool c_jackclient::stop_record (bool also_stop_monitor) {
  debug ("end");
  
  if (also_stop_monitor) {
    rec_go = false;
    monitor_only = false;
    return true;
  } else {
    if (!monitor_only)
      rec_go = false;
  }
    
  debug ("end");
  return true;
}

#undef debug
#define debug(...) cmdline_debug(stderr,ANSI_RED,__FILE__,__LINE__,__FUNC__,__VA_ARGS__)

int jack_test_main (int argc, char **argv) {
  debug ("start");
  s_prefs p;
  c_jackclient j (&p);
  CP
  
  if (!j.init ()) {
    printf ("j.init returned false!\n");
    return 1;
  }
  CP
  if (!j.register_input (true)) {
    printf ("j.register_input returned false!\n");
    return 1;
  }
  CP
  if (!j.register_output (false)) {
    printf ("j.register_output returned false!\n");
    return 1;
  }
  CP
  usleep (1000000);
  return true;
  debug ("end");
}

#endif
