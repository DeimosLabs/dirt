
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
  c_jackclient (c_deconvolver *dec, jack_client_t *jc = NULL);
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
  
  virtual bool init_input (bool stereo);
  virtual bool init_output (bool stereo);
    
  virtual int get_input_ports (std::vector<std::string> &v);
  virtual int get_output_ports (std::vector<std::string> &v);
  //virtual int get_samplerate ();
  //virtual int get_bufsize ();
  //virtual int get_bitdepth ();
  virtual int disconnect_all (jack_port_t *port);
  virtual bool connect (jack_port_t *port, std::string port_name);
  
  jack_client_t *client = NULL;
  jack_port_t   *port_inL  = NULL;
  jack_port_t   *port_inR  = NULL;
  jack_port_t   *port_outL = NULL;
  //jack_port_t   *port_outR = NULL; // not used for now
  
private:
  bool jack_inited = false;
};


int j_get_capture_ports (jack_client_t *c, int howmany,
                            std::vector<std::string> &v, bool default_only = false);
int j_get_playback_ports (jack_client_t *c, int howmany, 
                             std::vector<std::string> &v, bool default_only = false);

int j_get_default_capture (jack_client_t *c, int howmany, std::vector<std::string> &v);
int j_get_default_playback (jack_client_t *c, int howmany, std::vector<std::string> &v);
  

#endif // USE_JACK

#undef  __IN_DIRT_JACK_H
#endif  // __DIRT_JACK_H
