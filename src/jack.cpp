
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
#endif

// our JACK callback, takes care of actual audio i/o buffers
static int jack_process_cb (jack_nframes_t nframes, void *arg) {
  //debug ("start");
  c_jackclient *j = (c_jackclient *) arg;
  
  if (!j) return 0;
  if (!j->port_inL) return 0;
  
  bool stereo = (j->is_stereo && j->port_inR != NULL);
  
  // playback
  if (j->play_go) {
    jack_default_audio_sample_t *port_outL =
      (jack_default_audio_sample_t *) jack_port_get_buffer (j->port_outL, nframes);
    /*jack_default_audio_sample_t *port_outR =
      (jack_default_audio_sample_t *) jack_port_get_buffer (j->port_outR, nframes);*/

    for (jack_nframes_t i = 0; i < nframes; ++i) {
      float v = 0.0f;
      if (j->index < j->sig_out_l.size ()) {
        v = j->sig_out_l [j->index++];
      }
      if (port_outL) port_outL [i] = v;
      //if (port_outR) port_outR [i] = v; // same sweep on both channels
    }
  }
  
  // recording
  if (j->rec_go && j->port_inL && j->rec_index < j->rec_total) {
    jack_default_audio_sample_t *port_inL =
      (jack_default_audio_sample_t *) jack_port_get_buffer (j->port_inL, nframes);
    jack_default_audio_sample_t *port_inR = nullptr;
    if (j->port_inR) {
      port_inR = (jack_default_audio_sample_t *) jack_port_get_buffer (j->port_inR, nframes);
    }
    
    if (j->peak_ack) {
      j->peak_plus_l = 0;
      j->peak_plus_r = 0;
      j->peak_minus_l = 0;
      j->peak_minus_r = 0;
      j->clip_l = false;
      j->clip_r = false;
      j->peak_ack = false;
    }
    
    if (!stereo) {
      j->peak_plus_r = 0;
      j->peak_minus_r = 0;
      j->clip_r = false;
    }
    
    for (jack_nframes_t i = 0; i < nframes; ++i) {
      float vL = port_inL [i];
      float vR = port_inR ? port_inR [i] : vL;

      // always update the input meter
      if (vL > j->peak_plus_l)  j->peak_plus_l  = vL;
      if (vL < j->peak_minus_l) j->peak_minus_l = vL;
      if (fabsf (vL) > 0.99f)   j->clip_l = true;

      if (stereo) {
        if (vR > j->peak_plus_r)  j->peak_plus_r  = vR;
        if (vR < j->peak_minus_r) j->peak_minus_r = vR;
        if (fabsf (vR) > 0.99f)   j->clip_r = true;
      }

      // store data / advance index only when actually recording
      if (!j->monitor_only && j->rec_index < j->rec_total) {
        j->sig_in_l [j->rec_index] = vL;
        if (stereo) {
          j->sig_in_r [j->rec_index] = vR;
        }
        j->rec_index++;
        if (j->rec_index >= j->rec_total) {
          j->rec_done = true;
          j->rec_go   = false;
          break;
        }
      }
    }
    /*debug ("peak L=%f,%f, R=%f,%f",
           j->peak_minus_l, j->peak_plus_l,
           j->peak_minus_r, j->peak_plus_r);*/

  }
  
  //debug ("end");
  return 0;
}

int c_jackclient::get_default_playback (int howmany,
                                       std::vector<std::string> &v) {
  debug ("start");
  //v.clear ();
  if (!client || howmany <= 0) return 0;

  const char **ports = jack_get_ports (
    client,
    nullptr,
    nullptr,
    JackPortIsPhysical | JackPortIsInput   // speakers = physical inputs
  );

  if (!ports)
    return 0;

  int count = 0;

  for (int i = 0; ports[i] && count < howmany; ++i) {
    v.push_back (std::string(ports[i]));
    ++count;
  }

  jack_free (ports);

  debug ("end");
  return count;
}

int c_jackclient::get_default_capture (int howmany,
                                      std::vector<std::string> &v) {
  debug ("start");
  //v.clear ();
  if (!client || howmany <= 0) return 0;

  const char **ports = jack_get_ports (
    client,
    nullptr,
    nullptr,
    JackPortIsPhysical | JackPortIsOutput   // microphones = physical outputs
  );

  if (!ports)
    return 0;

  int count = 0;

  for (int i = 0; ports[i] && count < howmany; ++i) {
    v.push_back (std::string(ports[i]));
    ++count;
  }

  jack_free (ports);
  
  debug ("end");
  return count;
}

c_jackclient::c_jackclient (c_deconvolver *dec)
    : c_audioclient (dec) {
}

c_jackclient::~c_jackclient () {
  if (client) {
    jack_client_close (client); 
    client = NULL;
  }
}

bool c_jackclient::init (std::string clientname,      // = "",
                               int _samplerate,        // = -1, // ignored for now
                               //const char *jack_out_port,
                               //const char *jack_in_port,
                               bool stereo_out) {     // = true) {
  debug ("start");
  if (jack_inited) {
    // already have a client; nothing to do
    return true;
  }
  
  is_stereo = stereo_out;
  jack_status_t status = JackFailure;
  
  // Decide on a client name:
  const char *name = nullptr;
  if (!clientname.empty ()) {
    // explicit from caller (e.g. formatted with argv[0])
    name = clientname.c_str ();
  } else if (prefs_ && !prefs_->jack_name.empty ()) {
    // from prefs (may literally be "%s_ir_sweep" if not formatted)
    name = prefs_->jack_name.c_str ();
  } else {
    // last-chance default
    name = "deconvolver";
  }
  
  client = jack_client_open (name, JackNullOption, &status);
  if (!client) {
    std::cerr << "Error: cannot connect to JACK (client name \""
              << name << "\")\n";
    jack_inited = false;
    return false;
  }
  
  // JACK is alive, get its sample rate
  samplerate = (int) jack_get_sample_rate (client);
  
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
  port_outL = jack_port_register (client, "out",
                            JACK_DEFAULT_AUDIO_TYPE,
                            JackPortIsOutput, 0);
  /*port_outR = jack_port_register (client, "out_R",
                            JACK_DEFAULT_AUDIO_TYPE,
                            JackPortIsOutput, 0);*/
  port_inL  = jack_port_register (client, "in_L",
                            JACK_DEFAULT_AUDIO_TYPE,
                            JackPortIsInput, 0);
  port_inR  = jack_port_register (client, "in_R",
                            JACK_DEFAULT_AUDIO_TYPE,
                            JackPortIsInput, 0);

  if (!port_outL || !port_inL || (is_stereo && !port_inR)) {
    std::cerr << "Error: cannot register JACK ports.\n";
    //jack_deactivate (client);
    jack_client_close (client);
    client  = nullptr;
    jack_inited        = false;
    return false;
  }
  
  jack_set_process_callback (client, jack_process_cb, this);
  
  // 5) Activate client
  if (jack_activate (client) != 0) {
    std::cerr << "Error: cannot activate JACK client.\n";
    jack_client_close (client);
    client = nullptr;
    jack_inited       = false;
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
    if (prefs_->mode == deconv_mode::mode_playsweep && !prefs_->sweepwait) {
      // connect to system default
      std::vector<std::string> strv;
      int n = get_default_playback (1, strv);
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
    if (err) std::cerr << "warning: failed to connect to JACK output port " + 
                          prefs_->portname_wetL << std::endl;
                            
  }
  if (prefs_->portname_wetR.length () > 0) {
    debug ("got portname_wetL");
    // connect to portname_wetL
    int err = jack_connect (client,
                            prefs_->portname_wetR.c_str (),
                            jack_port_name (port_inR));
    if (err) std::cerr << "warning: failed to connect to JACK output port " + 
                          prefs_->portname_wetL << std::endl;
                            
  }
  if (prefs_->portname_wetR.length () > 0) {
    // connect to portname_wetR
  }
  
  jack_inited = true;
  
  // ...wuh duh fuuuh?
  //(void) chan_in;
  //(void) chan_out;
  
  debug ("end");
  return true;
}

bool c_jackclient::shutdown () {
  if (!jack_inited)
    return false;
  jack_deactivate (client);
  jack_client_close (client);
  client = nullptr;
  jack_inited = false;
  
  return true;
}

bool c_jackclient::ready () {
  return jack_inited;
}

bool c_jackclient::rec (std::vector<float> &sig) {
  return false;
}

bool c_jackclient::rec (std::vector<float> &sig_l,
                        std::vector<float> &sig_r) {
  return false;
}

bool c_jackclient::play (const std::vector<float> &sig_l,
                         const std::vector<float> &sig_r) {
  debug ("start");
  if (!jack_inited || !client) {
    return false;
  }

  sig_out_l = sig_l;
  sig_out_r = sig_r;
  index   = 0;
  play_go = false;
  
  size_t limit = sig_out_l.size ();
  if (is_stereo && !sig_out_r.empty ())
      limit = std::min (sig_out_l.size (), sig_out_r.size ());
  
  //std::cout << "Playing sweep via JACK... " << std::flush;
  play_go = true;

  while (index < sig_out_l.size () && ((!is_stereo) || index < sig_out_r.size ())) {
    usleep (10 * 1000); // 10 ms
  }

  std::cout << "done.\n";

  // let caller decide when to deactivate/close
  debug ("end");
  return true;
}

// for just 1 channel... do we play it in 1 channel or in both?
bool c_jackclient::play (const std::vector<float> &sig) {
  std::vector<float> dummy_r;
  return play (sig, dummy_r);
}

bool c_jackclient::playrec (const std::vector<float> &out_l,
                            const std::vector<float> &out_r,
                            std::vector<float> &in_l,
                            std::vector<float> &in_r) {
  const std::vector<float> &sweep = out_l;
  if (sweep.empty ()) {
    std::cerr << "Sweep buffer is empty.\n";
    return false;
  }

  // ensure JACK client
  if (!jack_inited || !client) {
    //if (!jack_init(std::string (), chn_stereo, chn_stereo)) {
      return false;
    //}
  }
  
  // prepare state
  sig_out_l  = out_l;
  sig_out_r  = out_r;
  index    = 0;
  play_go  = false;

  // capture same length + 2 sec. extra tail
  const size_t extra_tail = (size_t) (2.0 * samplerate); // 0.5s
  rec_total = sweep.size () + extra_tail;
  sig_in_l.assign (rec_total, 0.0f);
  rec_index = 0;
  rec_done  = false;
  rec_go    = false;
  is_stereo = (prefs_->portname_wetR.length () > 0);
  if (is_stereo)
    sig_in_r.assign (rec_total, 0.0f);

  std::cout << "Playing + recording sweep via JACK... " << std::flush;
  play_go = true;
  rec_go  = true;

  // block until done, redraw ascii-art level meter
  size_t num_passes = 0;
  float pl, pr;
  char line [256];
  while ((index < sig_out_l.size () && index < sig_out_r.size ()) ||
         (!rec_done)) {
    usleep (10 * 1000); // 10 ms
    num_passes++;
    if (num_passes % 3 == 0) { // redraw at every ~= 30 ms
      //CP
      pl = std::max (std::fabs (peak_plus_l), std::fabs (peak_minus_l));
      pr = std::max (std::fabs (peak_plus_r), std::fabs (peak_minus_r));
      peak_ack = true;
      //std::cout << "peak L=" << pl << ", peak R=" << pr << "\n";
    }
  }

  std::cout << "done.\n";

  // copy captured mono buffer out
  in_l = sig_in_l;
  if (is_stereo)
    in_r = sig_in_r;

  return true;
}

#endif
