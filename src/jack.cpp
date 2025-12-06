
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
  if (!j)
    return audiostate::NOTREADY;
    
  if (!j->client)
    return audiostate::NOTREADY;
    
  if (!j->play_go && !j->rec_go)
    return audiostate::IDLE;
  
  if (j->rec_go && j->monitor_only && !j->play_go)
      return audiostate::MONITOR;

  if (j->play_go && !j->rec_go && j->monitor_only)
      return audiostate::PLAYMONITOR;

  if (j->play_go && !j->rec_go)
      return audiostate::PLAY;

  if (!j->monitor_only && j->rec_go && !j->play_go)
      return audiostate::REC;

  return (!j->monitor_only) ? audiostate::PLAYREC : audiostate::MONITOR;
}

// our JACK callback, takes care of actual audio i/o buffers
static int jack_process_cb (jack_nframes_t nframes, void *arg) {
  c_jackclient *j = (c_jackclient *) arg;
  if (!j) return 0;
  c_deconvolver *d = j->get_deconvolver ();
  if (!d) return 0;

  audiostate prev_state = j->state;   // stored from last callback
  audiostate new_state  = determine_state_from_flags (j);
  /*if (prev_state != new_state) {
      debug ("prev_state=%d, new_state=%d", prev_state, new_state);
  }*/

  if (new_state != prev_state) {
    // leave previous state
    switch (prev_state) {
      case audiostate::PLAY:        d->on_play_stop ();     break;
      case audiostate::REC:         d->on_record_stop ();   break;
      case audiostate::MONITOR:     d->on_arm_rec_stop ();  break;
      case audiostate::PLAYREC:     d->on_playrec_stop ();  break;
      case audiostate::PLAYMONITOR: d->on_play_stop ();     break;
      default: break;
    }

    // enter new state
    switch (new_state) {
      case audiostate::PLAY:        if (prev_state != audiostate::REC) d->on_play_start();
                                                      break;
      case audiostate::REC:         d->on_record_start ();     break;
      case audiostate::MONITOR:     d->on_arm_rec_start ();    break;
      case audiostate::PLAYREC:     d->on_playrec_start ();    break;
      case audiostate::PLAYMONITOR: d->on_play_start ();       break;
      default: break;
    }

    j->state = new_state;
  }

  // state loop hooks
  switch (j->state) {
    case audiostate::NOTREADY:                            break;
    case audiostate::IDLE:        d->on_audio_idle ();    break;
    case audiostate::PLAY:        d->on_play_loop ();     break;
    case audiostate::REC:         d->on_record_loop ();   break;
    case audiostate::MONITOR:     d->on_arm_rec_loop ();  break;
    case audiostate::PLAYREC:     d->on_playrec_loop ();  break;
    case audiostate::PLAYMONITOR: d->on_play_loop ();     break;
    default: CP break;
  }
  
  bool rec_stereo = (j->is_stereo && j->port_inR != NULL);
  
  // PLAYBACK
  if (j->port_outL) {
    auto *port_outL =
        (jack_default_audio_sample_t*) jack_port_get_buffer(j->port_outL, nframes);
    // auto *port_outR = ... in case we add it later?

    if (j->play_go && !j->sig_out_l.empty ()) {
      size_t limit = j->sig_out_l.size ();
      if (j->is_stereo && !j->sig_out_r.empty ()) {
        limit = std::min (j->sig_out_l.size (), j->sig_out_r.size ());
      }

      for (jack_nframes_t i = 0; i < nframes; ++i) {
        float vL = 0.0f;

        if (j->index < limit) {
          vL = j->sig_out_l [j->index++];
          // if stereo-out later: vR = j->sig_out_r[j->index-1];
        } else {
            vL = 0.0f;
        }

        port_outL [i] = vL;
        // if (port_outR) port_outR[i] = vR;
      }

      // finished the sweep?
      if (j->index >= limit) {
        j->play_go = false;   // edge for state machine
      }
    } else {
      j->play_go = false;
      j->index = 0;
      // not playing: output silence
      for (jack_nframes_t i = 0; i < nframes; ++i) {
        port_outL [i] = 0.0f;
      }
    }
  } else {
    j->play_go = false;
    j->index = 0;
  }

  // RECORDING
  if (j->rec_go && j->port_inL /*&& j->rec_index < j->rec_total*/) {
    auto *port_inL =
        (jack_default_audio_sample_t *) jack_port_get_buffer(j->port_inL, nframes);
    jack_default_audio_sample_t *port_inR = nullptr;
    if (j->port_inR) {
      port_inR = (jack_default_audio_sample_t *)
        jack_port_get_buffer (j->port_inR, nframes);
    }

    if (!j->is_stereo) {
      j->peak_plus_r  = 0;
      j->peak_minus_r = 0;
      j->clip_r       = false;
    }

    for (jack_nframes_t i = 0; i < nframes; ++i) {
      float vL = port_inL [i];
      float vR = port_inR ? port_inR [i] : vL;
      //debug ("vL=%f, vR=%f", vL, vR);

      // meters
      if (vL > j->peak_plus_l)  j->peak_plus_l  = vL;
      if (vL < j->peak_minus_l) j->peak_minus_l = vL;
      if (fabsf (vL) > 0.99f)   j->clip_l = true;

      if (rec_stereo) {
        if (vR > j->peak_plus_r)  j->peak_plus_r  = vR;
        if (vR < j->peak_minus_r) j->peak_minus_r = vR;
        if (fabsf (vR) > 0.99f)   j->clip_r = true;
      }

      // store data only if actually recording
      if (!j->monitor_only) {
        if (j->rec_index < j->rec_total) {
          j->sig_in_l [j->rec_index] = vL;
          if (rec_stereo) {
            j->sig_in_r [j->rec_index] = vR;
          }
          j->rec_index++;
        }

        if (j->rec_index >= j->rec_total) {
          //j->rec_done = true;
          j->rec_go   = false;  // let state machine see the edge
          break;
        }
      }
    }
    /*debug ("state=%d, peak L %f/%f, peak R %f/%f", j->state, j->peak_plus_l, j->peak_minus_l,
                                         j->peak_plus_r, j->peak_minus_r);*/
  }
  
  //debug ("end");
  return 0;
}

c_jackclient::c_jackclient (c_deconvolver *dec)
    : c_audioclient (dec) {
  backend = audio_driver::JACK;
  backend_name = "JACK";
}

c_jackclient::~c_jackclient () { CP
  debug ("start");
  if (client) {
    CP
    unregister ();
    debug ("CLOSING JACK CLIENT\n");    
    jack_client_close (client);
    client = NULL;
  }
  debug ("end");
}

int c_jackclient::disconnect_all (jack_port_t *port) {
  int i;
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
    { return j_get_capture_ports (c, howmany, v, false); }
int j_get_default_playback (jack_client_t *c, int howmany, std::vector<std::string> &v)
    { return j_get_playback_ports (c, howmany, v, false); }
  

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
                         bool stereo_out) {     // = true) {
  debug ("start");
  if (client) {
    // already have a client, nothing to do
    return true;
  }
  
  is_stereo = stereo_out;
  jack_status_t status = JackFailure;
  CP
  // choose a client name:
  const char *name = nullptr;
  if (!clientname.empty ()) {
    // explicit from caller (e.g. formatted with argv [0])
    name = clientname.c_str ();
  } else if (prefs_ && !prefs_->jack_name.empty ()) {
    // from prefs (may literally be "%s_ir_sweep" if not formatted)
    name = prefs_->jack_name.c_str ();
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
  
  if (prefs_) {
    if (!prefs_->quiet && prefs_->sweep_sr != samplerate) {
        std::cerr << "Note: overriding sample rate (" << prefs_->sweep_sr
                  << " Hz) with JACK sample rate " << samplerate << " Hz.\n";
    }
    prefs_->sweep_sr = samplerate;
  }
  
  // Reset runtime state
  play_go = false;
  rec_go  = false;
  index   = 0;
  sig_in_l.clear ();
  sig_in_r.clear ();
  sig_out_l.clear ();
  sig_out_r.clear ();
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

  state = audiostate::IDLE;
  
  // ...wuh duh fuuuh?
  //(void) chan_in;
  //(void) chan_out;
  
  debug ("end");
  return true;
}

bool c_jackclient::connect_ports () {
  if (!port_outL || !port_inL || (is_stereo && !port_inR)) {
    std::cerr << "Error: cannot register JACK ports.\n";
    //jack_deactivate (client);
    return false;
  }
  
  // autoconnect output where it makes sense:
  // any port given: auto connect to it
  // no port given:
  //   playsweep: no -W? auto connect to system default
  //   other: no autoconnect
  if (prefs_->portname_dry.length () > 0) {
    debug ("got portname_dry");
    // connect to portname_dry
    int err = jack_connect (client,
                            jack_port_name (port_outL),
                            prefs_->portname_dry.c_str ());
    if (err) std::cerr << "warning: failed to connect to JACK input port "
                       << prefs_->portname_dry << std::endl;
  } else {
    if (prefs_->mode == opmode::PLAYSWEEP && !prefs_->sweepwait) {
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
  
  // autoconnect input where it makes sense:
  // any port given: auto connect to it
  // no port given: no autoconnect
  std::cout << "output port L: " << prefs_->portname_wetL << std::endl;
  std::cout << "output port R: " << prefs_->portname_wetR << std::endl;
  if (prefs_->portname_wetL.length () > 0) {
    debug ("got portname_wetL");
    // connect to portname_wetL
    int err = jack_connect (client,
                            prefs_->portname_wetL.c_str (),
                            jack_port_name (port_inL));
    if (err) std::cerr << "warning: failed to connect to JACK output port " 
                       << prefs_->portname_wetL << std::endl;
                            
  }
  if (prefs_->portname_wetR.length () > 0) {
    debug ("got portname_wetL");
    // connect to portname_wetL
    int err = jack_connect (client,
                            prefs_->portname_wetR.c_str (),
                            jack_port_name (port_inR));
    if (err) std::cerr << "warning: failed to connect to JACK output port "
                       << prefs_->portname_wetR << std::endl;
                            
  }
  
  return true;
}

bool c_jackclient::register_output (bool st) {
  debug ("start (TODO: fix this)");
  
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
  debug ("connecting ports by default");
  connect_ports ();
  
  debug ("end");
  return true;
}

bool c_jackclient::register_input (bool st) {
  debug ("start, st=%s", st ? "true" : "false");
  //if (st == is_stereo)
  //  return true;
    
  is_stereo = st;
  
  if (port_inL || port_inR) {
    CP
    unregister ();
  }
  
  if (is_stereo) {
    if (!port_inL) port_inL  = jack_port_register (client, "in_L",
                                                  JACK_DEFAULT_AUDIO_TYPE,
                                                  JackPortIsInput, 0);
    if (!port_inR) port_inR  = jack_port_register (client, "in_R",
                                                  JACK_DEFAULT_AUDIO_TYPE,
                                                  JackPortIsInput, 0);
  } else {
    if (!port_inL) port_inL  = jack_port_register (client, "in",
                                                  JACK_DEFAULT_AUDIO_TYPE,
                                                  JackPortIsInput, 0);
  }
  
  if (!port_inL) {
    CP
    return false;
  }
  if (is_stereo && !port_inR) {
    CP
    return false;
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
  //state = audiostate::NOTREADY;
  
  debug ("end");
  return true;
}

bool c_jackclient::ready () {
  return client != NULL && state != audiostate::NOTREADY;
}

bool c_jackclient::arm_record () {
  CP
  if (rec_go /*|| monitor_only*/)
    return false;
  
  rec_go = true;
  monitor_only = true;
  
  return true;
}

bool c_jackclient::rec () {
  debug ("start");
  if (rec_go)
    return false;
  
  //sig_in_l = sig_l;
  //sig_in_r = sig_r;
  monitor_only = false;
  rec_go = true;
  
  debug ("end");
  return true;
}

bool c_jackclient::play (const std::vector<float> &sig_l,
                         const std::vector<float> &sig_r,
                         bool block) {
  debug ("start, %s", block ? "block" : "!block");
  if (!client) {
    return false;
  }
  
  //dec_->on_play_start ();

  sig_out_l = sig_l;
  sig_out_r = sig_r;
  index   = 0;
  play_go = false;
  
  /*size_t limit = sig_out_l.size ();
  if (is_stereo && !sig_out_r.empty ())
    limit = std::min (sig_out_l.size (), sig_out_r.size ());
  */
  play_go = true;
  CP
  if (block)                         // TODO: fix
    while (index < sig_out_l.size () /*&& ((!is_stereo) || index < sig_out_r.size ())*/) {
      usleep (10 * 1000); // 10 ms
      //dec_->on_play_loop ();
    }
  
  //dec_->on_play_stop ();

  debug ("end");
  return true;
}

// for just 1 channel... do we play it in 1 channel or in both?
bool c_jackclient::play (const std::vector<float> &sig, bool block) {
  std::vector<float> dummy_r;
  return play (sig, dummy_r, block);
}

bool c_jackclient::playrec (const std::vector<float> &out_l,
                            const std::vector<float> &out_r) {
  debug ("start, is_stereo=%s", (is_stereo ? "true" : "false"));

  // ensure JACK client
  if (!client) {
    //if (!jack_init(std::string (), chn_stereo, chn_stereo)) {
      return false;
    //}
  }
  
  // prepare state
  sig_out_l  = out_l;
  sig_out_r  = out_r;
  index    = 0;

  // capture same length + 2 sec. extra tail
  const size_t extra_tail = (size_t) (2.0 * samplerate); // 0.5s
  rec_total = std::max (out_l.size (), out_r.size ()) + extra_tail;
  sig_in_l.assign (rec_total, 0.0f);
  rec_index = 0;
  
  //is_stereo = (prefs_->portname_wetR.length () > 0); <--- nope
  if (is_stereo)
    sig_in_r.assign (rec_total, 0.0f);
  
  //dec_->on_playrec_start ();
  play_go   = true;
  //rec_done  = false;
  rec_go    = true;
  monitor_only = false;

  // copy captured mono buffer out
  /*in_l = sig_in_l;
  if (is_stereo)
    in_r = sig_in_r;*/

  //dec_->on_playrec_stop ();
  
  debug ("end");
  return true;
}

bool c_jackclient::stop () {
  return stop_playback () && stop_record ();
}

bool c_jackclient::stop_playback () {
  debug ("start");
  if (!play_go)
    return false;
    
  play_go = false;

  debug ("end");
  return true;
}

bool c_jackclient::stop_record () {
  debug ("end");
  if (!rec_go)
    return false;
    
  rec_go = false;
  
  debug ("end");
  return true;
}

#undef debug
#define debug(...) cmdline_debug(stderr,ANSI_RED,__FILE__,__LINE__,__FUNC__,__VA_ARGS__)

int jack_test_main (int argc, char **argv) {
  debug ("start");
  c_deconvolver dec;
  c_jackclient j (&dec);
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
