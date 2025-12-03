
/* DIRT - Delt's Impulse Response Tool
 * Version 0.1
 *
 * Licensed under the GPL. See dirt.h and dirt.cpp for more info.
 */

#ifndef    __DIRT_JACK_H
#define    __DIRT_JACK_H
#define __IN_DIRT_JACK_H

#ifdef USE_JACK

#include "dirt.h"
#include "deconvolv.h"

class c_jackclient : public c_audioclient {
public:
  c_jackclient (c_deconvolver *dec);
  ~c_jackclient ();

  virtual bool init (std::string clientname = "",
                     int samplerate = -1,
                     bool stereo = true);
  virtual bool shutdown ();
  virtual bool ready ();
  virtual bool play (const std::vector<float> &out);
  virtual bool play (const std::vector<float> &out_l,
                     const std::vector<float> &out_r);
  virtual bool arm_record ();
  virtual bool rec ();
  virtual bool playrec (const std::vector<float> &out_l,
                        const std::vector<float> &out_r);
  virtual bool stop ();
  virtual bool stop_playback ();
  virtual bool stop_record ();
            
  jack_client_t *client = NULL;
  jack_port_t   *port_inL  = NULL;
  jack_port_t   *port_inR  = NULL;
  jack_port_t   *port_outL = NULL;
  //jack_port_t   *port_outR = NULL; // not used for now
  
  int get_capture_ports (int howmany, std::vector<std::string> &v, bool default_only);
  int get_playback_ports (int howmany, std::vector<std::string> &v, bool default_only);
  
  int get_default_capture (int howmany, std::vector<std::string> &v)
      { return get_capture_ports (howmany, v, false); }
  int get_default_playback (int howmany, std::vector<std::string> &v)
      { return get_playback_ports (howmany, v, false); }
  
private:
  bool jack_inited = false;
};


#endif // USE_JACK

#undef  __IN_DIRT_JACK_H
#endif  // __DIRT_JACK_H
