
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
  virtual bool rec (std::vector<float> &in_l);
  virtual bool rec (std::vector<float> &in_l,
                    std::vector<float> &in_r);
  virtual bool playrec (const std::vector<float> &out_l,
                        const std::vector<float> &out_r,
                        std::vector<float> &in_l,
                        std::vector<float> &in_r);
            
  bool play_go = false;
  bool rec_go  = false;
  bool monitor_only = false;
  
  /*std::string portname_dry = "";
  std::string portname_wetL = "";
  std::string portname_wetR = "";*/
  
  bool is_stereo = false;
  int samplerate = 0;
  jack_client_t *client = NULL;
  jack_port_t   *port_inL  = NULL;
  jack_port_t   *port_inR  = NULL;
  jack_port_t   *port_outL = NULL;
  //jack_port_t   *port_outR = NULL; // not used for now
  
private:
  int get_default_playback (int howmany, std::vector<std::string> &v);
  int get_default_capture (int howmany, std::vector<std::string> &v);
  
  bool jack_inited = false;
};


#endif // USE_JACK

#undef  __IN_DIRT_JACK_H
#endif  // __DIRT_JACK_H
