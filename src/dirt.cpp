
/* DIRT - Delt's Impulse Response Tool
 * Version 0.1
 *
 * -----------------------------------------------------------------------------
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 * ----------------------------------------------------------------------------
 * A simple, open-source sinewave sweep generator, recorder, and  deconvolver.
 * Takes two sinewave sweeps, one generated (dry) and the other (wet) recorded
 * through a cab, reverb, etc. and outputs an impulse response.
 *
 * See header file and --help text for more info
 */

#include <ctime>
#include <cstdint>
#include <time.h>
#include "dirt.h"
#include "deconvolv.h"
#include "jack.h"
#include "ui.h"
//#include "timestamp.h" nope - causes full recompile at each build

#ifdef DEBUG
#define CMDLINE_IMPLEMENTATION
#define CMDLINE_DEBUG
#include "cmdline/cmdline.h"
#define debug(...) cmdline_debug(stderr,ANSI_RED,__FILE__,__LINE__,__FUNC__,__VA_ARGS__)
#else
#define debug(...)
#define CP
#define BP
#endif


std::string readable_timestamp (int64_t epoch) {
  std::time_t tt = static_cast<std::time_t>(epoch);
  struct tm tm_buf;

#ifdef _WIN32
  localtime_s (&tm_buf, &tt);        // Windows safe version
#else
  localtime_r (&tt, &tm_buf);        // POSIX safe version
#endif

  char buf [64];
  std::snprintf (
    buf, sizeof (buf),
    "%04d-%02d-%02d_%02d:%02d:%02d",
    tm_buf.tm_year + 1900,
    tm_buf.tm_mon + 1,
    tm_buf.tm_mday,
    tm_buf.tm_hour,
    tm_buf.tm_min,
    tm_buf.tm_sec
  );

  return std::string (buf);
}


//#define DONT_USE_ANSI

#ifdef DONT_USE_ANSI

#ifdef DEBUG
#define ANSI_DUMMY //CP
#else
#define ANSI_DUMMY
#endif

static void print_vu_meter (float level, float hold,
                             bool clip, bool xrun)
                                            { printf ("level=%f, hold=%f\n", level, hold); }
//                                            { ANSI_DUMMY }

static void ansi_cursor_move_x (int n)                { ANSI_DUMMY }
static void ansi_cursor_move_to_x (int n)             { ANSI_DUMMY }
static void ansi_cursor_move_y (int n)                { ANSI_DUMMY }
static void ansi_cursor_hide ()                       { ANSI_DUMMY }
static void ansi_cursor_show ()                       { ANSI_DUMMY }
static void ansi_cursor_save ()                       { ANSI_DUMMY }
static void ansi_cursor_restore ()                    { ANSI_DUMMY }
static void ansi_clear_screen ()                      { ANSI_DUMMY }
static void ansi_clear_to_endl ()                     { ANSI_DUMMY }

#define FUCK char*//(char*)std::string("") //const

char *g_ansi_colors [32] = { NULL };
  //FUCK, FUCK, FUCK, FUCK, FUCK, FUCK, FUCK, FUCK, 
  //FUCK, FUCK, FUCK, FUCK, FUCK, FUCK, FUCK, FUCK, 
  //FUCK, FUCK, FUCK, FUCK, FUCK, FUCK, FUCK, FUCK, 
  //FUCK, FUCK, FUCK, FUCK, FUCK, FUCK, FUCK, FUCK };

#else

#ifndef __CMDLINE_H
char ANSI_BLACK [] =          "\x1B[0;30m";  //  0
char ANSI_DARK_RED [] =       "\x1B[0;31m";  //  1
char ANSI_DARK_GREEN [] =     "\x1B[0;32m";  //  2
char ANSI_DARK_YELLOW [] =    "\x1B[0;33m";  //  3
char ANSI_DARK_BLUE [] =      "\x1B[0;34m";  //  4
char ANSI_DARK_MAGENTA [] =   "\x1B[0;35m";  //  5
char ANSI_DARK_CYAN [] =      "\x1B[0;36m";  //  6
char ANSI_GREY [] =           "\x1B[0;37m";  //  7
char ANSI_DARK_GREY [] =      "\x1B[1;30m";  //  8
char ANSI_RED [] =            "\x1B[1;31m";  //  9
char ANSI_GREEN [] =          "\x1B[1;32m";  // 10
char ANSI_YELLOW [] =         "\x1B[1;33m";  // 11
char ANSI_BLUE [] =           "\x1B[1;34m";  // 12
char ANSI_MAGENTA [] =        "\x1B[1;35m";  // 13
char ANSI_CYAN [] =           "\x1B[1;36m";  // 14
char ANSI_WHITE [] =          "\x1B[1;37m";  // 15
char ANSI_RESET [] =          "\x1B[0m";
#endif

char *g_ansi_colors [] = {
  ANSI_BLACK,       ANSI_DARK_RED,       ANSI_DARK_GREEN,     ANSI_DARK_YELLOW,
  ANSI_DARK_BLUE,   ANSI_DARK_MAGENTA,   ANSI_DARK_CYAN,      ANSI_DARK_GREY,
  ANSI_GREY,        ANSI_RED,            ANSI_GREEN,          ANSI_YELLOW,
  ANSI_BLUE,        ANSI_MAGENTA,        ANSI_CYAN,           ANSI_WHITE,
  ANSI_RESET  
};

static void ansi_cursor_move_x (int n) {
  if (n == 0)
    printf ("\x1b[G"); // special case: start of line
  else if (n < 0)
    printf ("\x1b[%dD", -n); // left
  else
    printf ("\x1b[%dC", n); // right
}

static void ansi_cursor_move_y (int n) {
  if (n < 0)
    printf ("\x1b[%dA", -n);
  else if (n > 0)
    printf ("\x1b[%dB", n);
}

static void ansi_cursor_move_to_x (int n) { printf ("\x1b[%dG", n); }
static void ansi_clear_screen ()    { printf ("\x1b[2J");   }
static void ansi_clear_to_endl ()   { printf ("\x1b[K");    }
static void ansi_cursor_hide ()     { printf ("\x1b[?25l"); }
static void ansi_cursor_show ()     { printf ("\x1b[?25h"); }
static void ansi_cursor_save ()     { printf ("\x1b[s");    }
static void ansi_cursor_restore ()  { printf ("\x1b[u");    }

static void print_vu_meter (float level, float hold, bool clip, bool xrun) {
  ansi_clear_to_endl ();
  if (level < 0) level = 0;
  if (level > 1) level = 1;
  if (hold > 1) hold = 1;
  //debug ("level=%f hold=%f, %s, %s", level, hold,
  //       clip ? "clip" : "!clip", xrun ? "xrun" : "!xrun");
  //return;
  int i, size = ANSI_VU_METER_MIN_SIZE;
  char buf [size];
  for (i = 0; i < size; i++) buf [i] = ' ';
  char colors [size];
  for (i = 0; i < size; i++) colors [i] = 8;
  int right = size - 6;
  int yellow = (right * 2) / 3;
  int red = (right * 5) / 6;
  int n = (int) ((float) (level) * (float) (right));
  if (n > size) n = size;
  if (n < 0) n = 0;
  
  for (i = 1; i < n && i < yellow; i++)    { buf [i] = '='; colors [i] = 10; }
  for (; i < n && i < red; i++)            { buf [i] = '='; colors [i] = 11; }
  for (; i < n && i < right; i++)          { buf [i] = '='; colors [i] = 9; }
  for (i = n; i < yellow; i++)             { buf [i] = '-'; colors [i] = 2; }   
  for (; i < red; i++)                     { buf [i] = '-'; colors [i] = 3; }
  for (; i < right; i++)                   { buf [i] = '-'; colors [i] = 1; }
  //for (; i < size - 5; i++)  { buf [i] = '-'; colors [i] = 0x07; }
  int holdpos = (int) (hold * (float) right);
  if (holdpos > right - 1) holdpos = right - 1;
  if (holdpos > 0) { 
    if (holdpos > 0 && holdpos < right) {
      buf [holdpos] = (holdpos == right - 1) ? '!' : '|';
      colors [holdpos] = (holdpos == right - 1) ? 9 : 16;
    }
  }

  
  // lazyyyyyy... who cares
  if (xrun) {
    buf [right + 1] = 'X';
    buf [right + 2] = 'R';
    buf [right + 3] = 'U';
    buf [right + 4] = 'N';
  } else if (clip) {
    buf [right + 1] = 'C';
    buf [right + 2] = 'L';
    buf [right + 3] = 'I';
    buf [right + 4] = 'P';
  } else  {
    buf [right + 1] = ' ';
    buf [right + 2] = 'O';
    buf [right + 3] = 'K';
    buf [right + 4] = ' ';
    colors [right + 1] = 10;
    colors [right + 2] = 10;
    colors [right + 3] = 10;
    colors [right + 4] = 10;
  }
  if (xrun || clip) {
    colors [right + 1] = 9;
    colors [right + 2] = 9;
    colors [right + 3] = 9;
    colors [right + 4] = 9;
  }

  buf [0] = '[';
  buf [right] = ']';
  colors [0] = 16;
  colors [right] = 16;
  
  buf [size - 1] = 0;
  
  std::string output = ""; 
  int col = -1;
  for (i = 0; buf [i]; i++) {
    if (colors [i] != col) {
      output += g_ansi_colors [colors [i]];
      col = colors [i];
    }
    output += buf [i];
  }
  
  output += ANSI_RESET;
    
  //printf ("%s", buf);
  std::cout << output << " \n" << std::flush;
}

#endif // DONT_USE_ANSI

static void vu_wait (c_vudata &vu, std::string str) {
  ansi_cursor_move_x (0);
  int move_up = 2;
  print_vu_meter (vu.abs_l, vu.hold_l, vu.clip_l, vu.xrun);
  if (vu.is_stereo) {
    move_up++;
    print_vu_meter (vu.abs_r, vu.hold_r, vu.clip_r, vu.xrun);
  }
  ansi_clear_to_endl ();
  std::cout << str << std::endl;
  ansi_cursor_move_y (-1 * move_up);
  vu.acknowledge ();
  usleep (33333); 
}

extern char *g_dirt_build_timestamp;
extern char *g_dirt_version;

/*extern*/ int64_t get_unique_id () { CP
  static int64_t unique_id = 0;
  return ++unique_id;
}

void __bp () {
  fflush (stdin);
  getchar ();
}

void __die () {
  for (;;)
    usleep (10000);
}

////////////////////////////////////////////////////////////////////////////////
// c_wavebuffer

using sample_t = float;

void c_wavebuffer::append (const sample_t *data, size_t count) {
  if (!data || count == 0) return;
  samples_.insert (samples_.end (), data, data + count);
}

void c_wavebuffer::append (const std::vector<sample_t> &vec) {
    append (vec.data (), vec.size ());
}

void c_wavebuffer::insert (size_t pos, const sample_t *data, size_t count) {
  if (!data || count == 0) return;

  if (pos > samples_.size ())
    pos = samples_.size ();

  samples_.insert (samples_.begin () + pos, data, data + count);
}

void c_wavebuffer::insert (size_t pos, const std::vector<sample_t> &vec) {
    insert (pos, vec.data (), vec.size ());
}

void c_wavebuffer::erase (size_t start, size_t len) {
  if (len == 0 || samples_.empty ())
    return;

  if (start >= samples_.size ())
    return; // nothing to delete

  size_t end = start + len;
  if (end > samples_.size ())
    end = samples_.size ();

  samples_.erase (samples_.begin () + start,
                  samples_.begin () + end);
}

void c_wavebuffer::import_from (const sample_t *data, size_t count) {
  samples_.assign (data, data + count);
}

void c_wavebuffer::import_from (const std::vector<sample_t> &vec) {
  samples_ = vec;  // deep copy
}

void c_wavebuffer::export_to (sample_t *out, size_t max) const {
  // TODO
  //std::copy (samples_.begin (), std::min (max, samples_.end ()), out);
}

void c_wavebuffer::export_to (std::vector<sample_t> &vec) const {
  vec = samples_;
}


////////////////////////////////////////////////////////////////////////////////
// c_vudata

// called for each input sample.
bool c_vudata::sample (float l, float r) {
  bool ret = false;
  
  //debug ("registering %f,%f", l, r);
  
  if (l > plus_l)    { plus_l   = l; ret = true; }
  if (l < minus_l)   { minus_l  = l; ret = true; }
  
  if (r > plus_r)    { plus_r   = r; ret = true; }
  if (r < minus_r)   { minus_r  = r; ret = true; }
  
  return ret;
}

// called for each buffer
bool c_vudata::update () {
  //debug ("this=%lx", (long int) this);
  bool ret = false;
  int bufs_sec = 0;
  if (bufsize > 0)
    bufs_sec = samplerate / bufsize;
  
  if (bufsize == 0) return -1;
  
  const float sec_per_redraw = VU_REDRAW_EVERY;
  int redraw_every = (int) (sec_per_redraw * bufs_sec);
  if (redraw_every < 1)
    redraw_every = 1;
  
  if (bufcount % redraw_every == 0) {
    if (!acknowledged) ret = true;
    int peak_hold_frames = (VU_PEAK_HOLD * bufs_sec / redraw_every);
    int clip_hold_frames = (VU_CLIP_HOLD * bufs_sec / redraw_every);
    int xrun_hold_frames = (VU_XRUN_HOLD * bufs_sec / redraw_every);
    
    abs_l = std::max (std::fabs (plus_l), std::fabs (minus_l));
    abs_r = std::max (std::fabs (plus_r), std::fabs (minus_r));
    uint32_t now = bufcount / redraw_every;
    
    if (abs_l < 0) abs_l = 0;
    if (abs_l > 1) abs_l = 1;
    if (abs_r < 0) abs_r = 0;
    if (abs_r > 1) abs_r > 1;
    if (now - timestamp_hold_l > peak_hold_frames) hold_l = 0;
    if (now - timestamp_hold_r > peak_hold_frames) hold_r = 0;
    if (abs_l > hold_l) { hold_l = abs_l; timestamp_hold_l = now; }
    if (abs_r > hold_r) { hold_r = abs_r; timestamp_hold_r = now; }
    if (abs_l > 0.999) timestamp_clip_l = now;
    if (abs_r > 0.999) timestamp_clip_r = now;
    if (xrun) timestamp_xrun = now;
    
    //debug ("hold %f,%f", hold_l, hold_r);
    
    // YES, this looks backwards
    if (timestamp_clip_l && now - timestamp_clip_l < clip_hold_frames) clip_l = true;
    if (timestamp_clip_r && now - timestamp_clip_r < clip_hold_frames) clip_r = true;
    if (timestamp_xrun && now - timestamp_xrun < xrun_hold_frames)     xrun = true;
  }
  
  //debug ("levels %f,%f, peaks %f,%f", abs_l, abs_r, hold_l, hold_r);
  bufcount++;
  return ret; 
}

void c_vudata::set_db_scale (float f) {
  db_scale = f;
}

float c_vudata::db_scaled (float f) {
  float ret = 0;
  if (db_scale == 0) {
    ret = f;
  } else if (db_scale > 0) {
    ret = f; // TODO: math for exp. curve
  } else {
    float db = linear_to_db (f);
    db += db_scale;
    ret = 2 - (db / db_scale);
  }
  
  if (ret < 0) ret = 0;
  if (ret > 1) ret = 1;
  
  //debug ("db_scale=%f, ret=%f", db_scale, ret);
  return ret;
}

float c_vudata::l () {
  return db_scaled (abs_l);
}

float c_vudata::r () {
  return db_scaled (abs_r);
}

float c_vudata::peak_l () {
  return db_scaled (hold_l);
}

float c_vudata::peak_r () {
  return db_scaled (hold_r);
}

void c_vudata::acknowledge () {
  peak_new = false;
  xrun = false;
  clip_l = false;
  clip_r = false;
  
  abs_l -= VU_FALL_SPEED;
  abs_r -= VU_FALL_SPEED;
  if (abs_l < 0) abs_l = 0;
  if (abs_r < 0) abs_r = 0;
  needs_redraw = true;
  
  plus_l = 0;
  plus_r = 0;
  minus_l = 0;
  minus_r = 0;
}

////////////////////////////////////////////////////////////////////////////////
// c_audioclient

c_audioclient::c_audioclient (s_prefs *_prefs) { CP
  debug ("start");
  prefs = _prefs;
  //vudata = new c_vudata;
  debug ("end");
}

c_audioclient::~c_audioclient () { CP
  debug ("start");
  //if (vudata) delete vudata;
  debug ("end");
}

/*void c_audioclient::clear_recording () {
  sig_in_l.clear ();
  sig_in_r.clear ();
  rec_index = 0;
  rec_total = 0;
  //rec_done  = false;
}

/*bool c_audioclient::has_recording () const {
  return (sig_in_l.size () > 0 || sig_in_r.size () > 0) && state == audiostate::IDLE;
}*/

size_t c_audioclient::get_rec_remaining () {
  if (rec_index <= 0)
    return 0;
    
  if (state == audiostate::REC || state == audiostate::PLAYREC) {
    return rec_max - rec_index;
  }
  
  return 0;
}

size_t c_audioclient::get_play_remaining () {
  if (play_index <= 0)
    return 0;
    
  if (play_go) {
    /*size_t a = sig_in_l.size ();
    size_t b = sig_in_r.size ();
    size_t c = sig_out.size ();*/
    //debug ("a=%ld, b=%ld, index=%ld", (long int) a, (long int) b, (long int) index);
    //return std::max (a, b) - index;
    
    size_t sz_l = 0, sz_r = 0;
    if (sig_out_l) sz_l = sig_out_l->size ();
    if (sig_out_r) sz_r = sig_out_r->size ();
    return std::max (sz_l, sz_r) - play_index;
  }
  
  return 0;
}

static bool stdin_has_enter()   /* totally not copy-pasted from chatgpt */
{
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(STDIN_FILENO, &rfds);

    struct timeval tv;
    tv.tv_sec  = 0;
    tv.tv_usec = 0; // non-blocking

    int ret = select(STDIN_FILENO + 1, &rfds, nullptr, nullptr, &tv);
    if (ret <= 0) return false;  // no data or error

    if (FD_ISSET(STDIN_FILENO, &rfds))
    {
        char buf[64];
        ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
        if (n > 0)
        {
            for (ssize_t i = 0; i < n; ++i)
            {
                if (buf[i] == '\n' || buf[i] == '\r')
                    return true;
            }
        }
    }
    return false;
}

// for now just check if given name contains a ":"
static bool looks_like_jack_port (const std::string &s) {
#ifdef USE_JACK
  if ((s.find(':') != std::string::npos) &&
     (s [0] >= 'A' && s [0] <= 'z'))
    return true;
#endif

  return false;
}

void generate_log_sweep (double seconds,
                        double preroll_seconds,
                        double start_marker_seconds,
                        double marker_gap_seconds,
                        int samplerate,
                        float sweep_amp_db,
                        float f1, float f2,
                        c_wavebuffer &buf_out) {
  const size_t N      = (size_t) (seconds * samplerate);          // sweep
  const size_t NPRE   = (size_t) (preroll_seconds * samplerate);  // NEW
  const size_t NM     = (size_t) (start_marker_seconds * samplerate);
  const size_t NGAP   = (size_t) (marker_gap_seconds * samplerate);
  size_t n = 0;
  
  if (samplerate <= 0) {
    std::cout << "can't have a sample rate of zero!\n";
    return;
  }
  
  std::vector<float> out;

  out.resize (NPRE + NM + NGAP + N);

  std::cerr << "Generating sweep, "   << samplerate << " Hz sample rate:\n"
            << "  Length:           " << seconds << ((seconds == 1) ?
                                         " second\n" : " seconds\n")
            << "  Frequency range:  " << f1 << "Hz to " << f2 << " Hz\n"
            << "  Preroll:          " << NPRE << " samples\n"
            << "  Marker:           " << NM << " samples\n"
            << "  Gap after marker: " << NGAP << " samples\n\n";

  // preroll silence
  for (n = 0; n < NPRE; ++n) {
    out [n] = 0.0f;
  }

  // marker (square wave)
  float sweep_amp_linear = db_to_linear (sweep_amp_db);
  
  const int period = samplerate / MARKER_FREQ; // 1 kHz etc.
  for (; n < NPRE + NM; ++n) {
    size_t k = n - NPRE; // so marker indexing still starts at 0
    out [n] = ((k / (period / 2)) & 1) ? sweep_amp_linear : (-1 * sweep_amp_linear);
  }

  // silence gap
  for (; n < NPRE + NM + NGAP; ++n) {
    out [n] = 0.0f;
  }

  // sweep
  for (; n < NPRE + NM + NGAP + N; ++n) {
    double t     = (double)(n - (NPRE + NM + NGAP)) / samplerate;
    double T     = seconds;
    double w1    = 2.0 * M_PI * f1;
    double w2    = 2.0 * M_PI * f2;
    double L     = std::log (w2 / w1);
    double phase = w1 * T / L * (std::exp (t * L / T) - 1.0);
    out [n]       = (float)std::sin (phase) * (float) sweep_amp_linear;
  }

  // fade out end of sweep (useless ~=20KHz frequencies anyway)
  const int fade_samples = samplerate / 100;
  const size_t total = NPRE + NM + NGAP + N;
  for (int i = 0; i < fade_samples && i < (int)total; ++i) {
    float g = (float)i / (float)fade_samples;
    out [total - 1 - i] *= g;
  }
  
  buf_out.import_from (out);
  buf_out.set_samplerate (samplerate);
}

void generate_log_sweep (s_prefs &p, c_wavebuffer &out) {
  generate_log_sweep (p.sweep_seconds,
                      p.preroll_seconds,
                      p.marker_seconds,
                      p.marker_gap_seconds,
                      p.sweep_sr,
                      p.sweep_amp_db,
                      p.sweep_f1,
                      p.sweep_f2,
                      out);
}                        
                        
static void print_usage (const char *prog, bool full = false) {
  std::ostream &out = full ? std::cout : std::cerr;
  
  if (full) {
    out << "\nDIRT - Delt's Impulse Response Tool\n"
           "Version " << g_dirt_version << " build " << g_dirt_build_timestamp <<
#ifdef USE_JACK
    //"\nJACK support: enabled\n"
    "\n"
#else
    "\nJACK support: disabled\n"
#endif
    //"\nhttps://deimos.ca/dirt\n"
    //"Latest version available at: https://github.com/DeimosLabs/dirt\n"
    "\n";
  }
  
  out <<
    (full ? "Usage:\n" :
            "Usage: (see --help for full list of options)\n") <<
#ifdef GUI
    "  " << prog << " (runs GUI, or optionally add --gui to command line)\n"
#endif
    "  " << prog << " [options] dry_sweep.wav wet_sweep.wav out_ir.wav\n"
    "  " << prog << " [options] -d dry_sweep.wav -w wet_sweep.wav -o out_ir.wav\n"
    "  " << prog << " [options] --makesweep dry_sweep.wav\n"
#ifdef USE_JACK
    "  " << prog << " [options] --playsweep\n"
#endif
    "\n" <<
    (full ? "Deconvolver" : "Some deconvolver") << "/general options: [default]\n" <<
    "  -h, --help               Show " << (full ? "this" : "full")
                             << " help/usage text and version\n"
    "  -V, --version            Same as --help\n"
    "  -g, --gui                Use X11/wxWidgets UI [on if no options]\n"
    "  -d, --dry FILE           Dry sweep WAV file\n"
    "  -w, --wet FILE           Recorded (wet) sweep WAV file\n"
    "  -o, --out FILE           Output IR WAV file\n" <<
    (full ? 
    "  -n, --len N              IR length in samples [0 = auto]\n" : "") <<
    "  -A, --align METHOD       Choose alignment method" << (full ? ": [dry]\n"
    "                             none: assume already aligned sweeps\n"
    "                             marker: try to detect marker/gap in both sweeps\n"
    "                             dry: detect marker/gap in dry, reuse in wet\n"
    "                             silence: use first nonsilent sample, see -t\n"
                                : " (see --help)\n");
//    "                             silence, marker, dry, none (default none)\n"
  if (full) out <<
    "  -t, --thresh dB          Threshold in dB for sweep silence detection\n"
    "                             (negative) ["
                             << DEFAULT_SWEEP_SILENCE_THRESH_DB << "]\n"
    "  -T, --ir-thresh dB       Threshold in dB for IR silence detection\n"
    "                             (negative) ["
                             << DEFAULT_IR_SILENCE_THRESH_DB << "]\n"
    "      --hpf dB             High-pass filter frequency in Hz ["
                             << DEFAULT_HPF << "]\n"
    "      --lpf dB             Low-pass filter frequency in Hz ["
                             << DEFAULT_LPF << "]\n"
    "  -O, --offset N           Offset (delay) wet sweep by N samples [10]\n"
    "  -z, --zero-peak          Try to align peak to zero"
                             << (DEFAULT_ZEROPEAK ? " [default]\n" : "\n") <<
    "  -Z, --no-zero-peak       Don't try to align peak to zero"
                             << (!DEFAULT_ZEROPEAK ? " [default]\n" : "\n") << 
    "  -m, --mono               Force mono/single channel [no/autodetect]\n"
    "  -q, --quiet              Less verbose output\n"
    "  -v, --verbose            More verbose output\n"
    << (full ? 
    "  -D, --dump PREFIX        Dump exact wav files of deconvolver input\n"
    : "\n");
    out <<
    (full ? "\nSweep generator" : "\nSome sweep generator") << " options: [default]\n" <<
    "  -s, --makesweep          Generate sweep WAV file\n"
#ifdef USE_JACK
    "  -S, --playsweep          Generate sweep and play it via " 
    << AUDIO_BACKEND <<"\n"
#endif
    "  -R, --sweep-sr SR        Sweep samplerate [" << DEFAULT_SAMPLERATE << "]\n"
    "  -L, --sweep-seconds SEC  Sweep length in seconds [30]\n"
    "  -a, --sweep-amplitude dB Sweep amplitude in dB [-1]\n" // DONE
    "  -X, --sweep-f1 F         Sweep start frequency [" << DEFAULT_F1 << "]\n"
    "  -Y, --sweep-f2 F         Sweep end frequency [" << DEFAULT_F2 << "]\n";
  if (full) out <<
    "  -P, --preroll SEC        Prepend SEC leading silence, in seconds ["
                                << DEFAULT_PREROLL_SEC << "]\n"
    "  -M, --marker SEC         Prepend SEC alignment marker, in seconds ["
                                << DEFAULT_MARKER_SEC << "]\n"
    "  -G, --gap SEC            Add SEC gap after marker, in seconds ["
                                << DEFAULT_MARKGAP_SEC << "]\n"
    "  -W, --wait               Wait for input before playing sweep\n"
#ifdef USE_JACK
    //"  -j, --jack-port PORT     Connect to this JACK port\n"
    "  -J, --jack-name          JACK client name for playback/recording sweeps\n"
#endif
    "\n"; else out <<
    "\nFor more info, try: " << prog << " --help\n"
    ;
  if (full) out <<
    "Example usage:\n"
    "\n"
    "  # Generate a 30-second dry sweep with 1sec. preroll and 0.1sec. marker:\n"
    "  " << prog << " --makesweep -L 30 --preroll 1 --marker 0.1 -o drysweep.wav\n"
    "\n"
    "  # Deconvolve a wet/recorded sweep against a dry one, and output IR\n"
    "  " << prog << " -d drysweep.wav -w wetsweep.wav -o ir.wav\n"
#ifdef USE_JACK
    "\n"
    "  # Loopback (play & record) a sweep through Carla, extract resulting IR:\n"
    "  " << prog << " -d Carla:audio-in1 -w Carla:audio-out1 -o carla_ir.wav\n"
#endif
    //"\n"
    ;
}

static bool resolve_sources (s_prefs &opt, int paths_bf) {
  debug ("start");
  // lambda for trimming whitespace from beginning/end of a string
  auto trim = [] (std::string &s) {
    size_t a = s.find_first_not_of (" \t");
    size_t b = s.find_last_not_of (" \t");
    if (a == std::string::npos) { s.clear (); return; }
    s = s.substr (a, b - a + 1);
  };
  
  debug ("opt->portname_dry=%s", opt.portname_dry.c_str ());
  debug ("opt->portname_wetL=%s", opt.portname_wetL.c_str ());
  debug ("opt->portname_wetR=%s", opt.portname_wetR.c_str ());

  // --- dry source ---
  if ((paths_bf & 1) || opt.gui) {
#ifdef USE_JACK
    if (opt.dry_path == "jack") {
#else
    if (0) {
#endif
      opt.dry_source = sig_source::JACK;
      // if only dry was specified, assume playsweep to default
      if (!(paths_bf & 2) || opt.gui) {
        opt.jack_autoconnect_dry        = true;
        opt.jack_autoconnect_to_default = true;
      }
    } else if (looks_like_jack_port (opt.dry_path)) {
      opt.dry_source          = sig_source::JACK;
      opt.jack_autoconnect_dry = true;  // explicit jack port -> autoconnect
      opt.portname_dry = opt.dry_path;
    } else if (file_exists (opt.dry_path) || opt.mode != opmode::DECONVOLVE) {
      opt.dry_source = sig_source::FILE;
    } else {
      std::cerr << "Dry source \"" << opt.dry_path
#ifdef USE_JACK
                << "\" is not an existing file or a JACK port\n";
#else
                << "\" not found\n";
#endif
      return false;
    }
  }
  
// --- wet source ---
  if ((paths_bf & 2) || opt.gui) {
#ifdef USE_JACK
    if (opt.wet_path == "jack") {
#else
    if (0) {
#endif
      opt.wet_source = sig_source::JACK;
      // no default autoconnect here; user should patch or give explicit port
    } else {
      // check for stereo form "portL,portR"
      auto comma = opt.wet_path.find (',');
      if (comma != std::string::npos) {
        std::string left  = opt.wet_path.substr (0, comma);
        std::string right = opt.wet_path.substr (comma + 1);
        trim(left);
        trim(right);

        if (left.empty() || right.empty ()) {
          std::cerr << "Wet JACK stereo ports must be \"L,R\" (got: \""
                    << opt.wet_path << "\")\n";
          return false;
        }
        
        if (looks_like_jack_port (left) && looks_like_jack_port (right)) {
          opt.wet_source           = sig_source::JACK;
          opt.jack_autoconnect_wet = true;
          opt.portname_wetL        = left;
          opt.portname_wetR        = right;
        } else if (file_exists (opt.wet_path) || opt.mode != opmode::DECONVOLVE) {
          // treat whole thing as file name if not a JACK pair
          opt.wet_source = sig_source::FILE;
        } else {
          std::cerr << "Wet source \"" << opt.wet_path
#ifdef USE_JACK
                    << "\" is not an existing file or valid JACK ports\n";
#else
                << "\" not found\n";
#endif
          return false;
        }

      } else if (looks_like_jack_port (opt.wet_path)) {
        // mono JACK wet port
        opt.wet_source           = sig_source::JACK;
        opt.jack_autoconnect_wet = true;
        opt.portname_wetL        = opt.wet_path;
        opt.portname_wetR.clear ();

      } else if (file_exists (opt.wet_path) || opt.mode != opmode::DECONVOLVE) {
        opt.wet_source = sig_source::FILE;

      } else {
        std::cerr << "Wet source \"" << opt.wet_path
#ifdef USE_JACK
                  << "\" is not an existing file or a JACK port\n";
#else
                << "\" not found\n";
#endif
        return false;
      }
    }
  }
  
  // --- output file/port ---
  if ((paths_bf & 4) || opt.gui) {
    if (opt.out_path == "jack" || looks_like_jack_port (opt.out_path)) {
      if (opt.mode == opmode::DECONVOLVE) {
        std::cerr << "Can't write IR directly to a JACK port\n\n";
        return false;
      }
    }
  } else if (opt.mode == opmode::DECONVOLVE) {
    std::cerr << "No output file specified\n";
    return false;
  }
  
  //if (opt.dry_source == sig_source::JACK && opt.wet_source == sig_source::JACK) {
  //  opt.preroll_seconds = 0.1;
  //  opt.marker_seconds = 0;
  //  opt.marker_gap_seconds = 0;
  //}
  
  debug ("end");
  return true;
}

// returns bit-field of paths that were provided by user, or -1 on error
// bit 1: dry, bit 2: wet, bit 3: out
// doing_env is true when called from parse_env ()
int parse_args (int argc, char **argv, s_prefs &opt, bool doing_env = false) {
  debug ("start");
  const int ret_err = -1;
  int i;
  bool doubledash = false;
  
  // First pass: parse flags 
  std::vector<const char *> positionals;
  for (i = 1; i < argc; ++i) {
    std::string arg = argv [i];
    
    if (arg == "--") {
      doubledash = true;
    } else if (arg == "-h" || arg == "--help" || arg == "-V" || arg == "--version") {
      print_usage (argv [0], true);
      exit (0);
    } else if (arg == "-d" || arg == "--dry") {
      if (++i >= argc) { std::cerr << "Missing value for " << arg << "\n"; return ret_err; }
      opt.dry_path = argv [i];
    } else if (arg == "-g" || arg == "--gui") {
      opt.gui = true;
      opt.mode = opmode::GUI;
    } else if (arg == "-w" || arg == "--wet") {
      if (++i >= argc) { std::cerr << "Missing value for " << arg << "\n"; return ret_err; }
      opt.wet_path = argv [i];
    } else if (arg == "-o" || arg == "--out") {
      if (++i >= argc) { std::cerr << "Missing value for " << arg << "\n"; return ret_err; }
      opt.out_path = argv [i];
    } else if (arg == "-n" || arg == "--len") {
      if (++i >= argc) { std::cerr << "Missing value for " << arg << "\n"; return ret_err; }
      opt.ir_length_samples = std::strtol (argv [i], nullptr, 10);
      if (opt.ir_length_samples < 0) {
        std::cerr << "Invalid IR length: " << argv [i] << "\n";
        return ret_err;
      }
    } else if (arg == "-A" || arg == "--align") {
      if (++i >= argc) { std::cerr << "Missing value for " << arg << "\n"; return ret_err; }
      std::string method = argv [i];
      struct { align_method num; std::string name; } methods [] = {
        { align_method::MARKER,       "marker" },
        { align_method::MARKER_DRY,   "dry" },
        { align_method::SILENCE,      "silence" },
        { align_method::NONE,         "none" }
      };
      bool valid = false;
      for (int j = 0; j < (int) align_method::MAX; j++)
        if (argv [i] == methods [j].name) {
          valid = true;
          opt.align = methods [j].num;
          break;
        }
      if (!valid) {
        std::cerr << "Invalid alignment method \"" << argv [i] << "\"\n";
        return ret_err;
      }
    } else if (arg == "-t" || arg == "--thresh") {
      if (++i >= argc) {
        std::cerr << "Missing value for " << arg << "\n";
        return ret_err;
      }
      opt.sweep_silence_db = std::atof (argv [i]);
      if (opt.sweep_silence_db < -200.0 || opt.sweep_silence_db > 0.0) {
        std::cerr << "Invalid threshold dB: "
                  << argv [i] << " (must be between -200 and 0)\n";
        return ret_err;
      }
    } else if (arg == "-T" || arg == "--ir-thresh") {
      if (++i >= argc) {
        std::cerr << "Missing value for " << arg << "\n";
        return ret_err;
      }
      opt.ir_silence_db = std::atof (argv [i]);
      if (opt.ir_silence_db < -200.0 || opt.ir_silence_db > 0.0) {
        std::cerr << "Invalid threshold dB: "
                  << argv [i] << " (must be between -200 and 0)\n";
        return ret_err;
      }
    } else if (arg == "--hpf") {
      if (++i >= argc) {
        std::cerr << "Missing value for " << arg << "\n";
        return ret_err;
      }
      opt.hpf_mode = 1;
      opt.hpf = std::atoi (argv [i]);
      if (opt.hpf < 40 || opt.hpf > 24000) {
        std::cerr << "Invalid frequency (Hz): "
                  << argv [i] << " (must be between 20 and 24000)\n";
        return ret_err;
      }
    } else if (arg == "--lpf") {
      if (++i >= argc) {
        std::cerr << "Missing value for " << arg << "\n";
        return ret_err;
      }
      opt.lpf_mode = 1;
      opt.lpf = std::atoi (argv [i]);
      if (opt.hpf < 40 || opt.hpf > 24000) {
        std::cerr << "Invalid frequency (Hz): "
                  << argv [i] << " (must be between 20 and 24000)\n";
        return ret_err;
      }
    } else if (arg == "-m" || arg == "--mono") {
      opt.request_stereo = false;
    } else if (arg == "--stereo") { // just accept this for symmetry
      opt.request_stereo = true;
    } else if (arg == "-q" || arg == "--quiet") {
      opt.quiet = true;
    } else if (arg == "-v" || arg == "--verbose") {
      opt.verbose = true;
    } else if (arg == "-D" || arg == "--dump") {
      if (++i >= argc) { std::cerr << "Missing value for " << arg << "\n"; return ret_err; }
      opt.dump_debug = true;
      opt.dump_prefix = argv[i];
    } else if (arg == "-W" || arg == "--wait") {
      opt.sweepwait = true;
    } else if (arg == "-s" || arg == "--makesweep") {
      //if (++i >= argc) { std::cerr << "Missing value for " << arg << "\n"; return ret_err; }
      opt.mode = opmode::MAKESWEEP;
      //opt.sweep_seconds = std::atof (argv [i]);
    } else if (arg == "-S" || arg == "--playsweep") {
#ifdef USE_JACK
      //if (++i >= argc) { std::cerr << "Missing value for " << arg << "\n"; return ret_err; }
      opt.mode = opmode::PLAYSWEEP;
      //opt.sweep_seconds = std::atof (argv [i]);
#else
       std::cerr << "Error: this version of " << argv [0] << " was built without JACK support.";
#endif
    } else if (arg == "-L" || arg == "--sweep-seconds") {
      if (++i >= argc) { std::cerr << "Missing value for " << arg << "\n"; return ret_err; }
      opt.sweep_seconds = std::atoi (argv [i]);
    } else if (arg == "-R" || arg == "--sweep-sr") {
      if (++i >= argc) { std::cerr << "Missing value for " << arg << "\n"; return ret_err; }
      opt.sweep_sr = std::atoi (argv [i]);
      opt.sweep_sr_given = true;
    } else if (arg == "-X" || arg == "--sweep-f1") {
      if (++i >= argc) { std::cerr << "Missing value for " << arg << "\n"; return ret_err; }
      opt.sweep_f1 = std::atof (argv [i]);
    } else if (arg == "-Y" || arg == "--sweep-f2") {
      if (++i >= argc) { std::cerr << "Missing value for " << arg << "\n"; return ret_err; }
      opt.sweep_f2 = std::atof (argv [i]);
    } else if (arg == "-O" || arg == "--offset") {
      if (++i >= argc) { std::cerr << "Missing value for " << arg << "\n"; return ret_err; }
      opt.sweep_offset_smp = std::atof (argv [i]);
    } else if (arg == "-z" || arg == "--zero-peak") {
      opt.zeropeak = true;//!opt.zeropeak;
    } else if (arg == "-Z" || arg == "--no-zero-peak") {
      opt.zeropeak = false;//!opt.zeropeak;
    } else if (arg == "-a" || arg == "--sweep-amplitude") {
      if (++i >= argc) { std::cerr << "Missing value for " << arg << "\n"; return ret_err; }
      opt.sweep_amp_db = std::atof (argv [i]);
      if (opt.sweep_amp_db < -200 || opt.sweep_amp_db > 00) {
        std::cerr << "Amplitude needs to be from -200 to 0\n";
        return ret_err;
      }
    } else if (arg == "-M" || arg == "--marker") {
        if (++i >= argc) {
            std::cerr << "Missing value for " << arg << "\n";
            return ret_err;
        }
        opt.marker_seconds = std::atof (argv [i]);
        /*if (opt.preroll_seconds == 0.0)
          opt.preroll_seconds = 1.0;
        if (opt.marker_gap_seconds == 0.0)
          opt.marker_gap_seconds = 1.0;*/
        if (opt.marker_seconds < 0.0) {
            std::cerr << "Marker length cannot be negative.\n";
            return ret_err;
        }
    } else if (arg == "-P" || arg == "--preroll") {
        if (++i >= argc) {
            std::cerr << "Missing value for " << arg << "\n";
            return ret_err;
        }
        opt.preroll_seconds = std::atof (argv [i]);
        if (opt.preroll_seconds < 0.0) {
            std::cerr << "Preroll length cannot be negative.\n";
            return ret_err;
        }
    } else if (arg == "-G" || arg == "--gap") {
        if (++i >= argc) {
            std::cerr << "Missing value for " << arg << "\n";
            return ret_err;
        }
        opt.marker_gap_seconds = std::atof (argv [i]);
        if (opt.marker_gap_seconds < 0.0) {
            std::cerr << "Post-marker gap length cannot be negative.\n";
            return ret_err;
        }
    } 
#ifdef USE_JACK
    else if (arg == "-J" || arg == "--jack-name") {
      if (++i >= argc) { std::cerr << "Missing value for " << arg << "\n"; return ret_err; }
      opt.jack_name = argv[i];
    } /*else if (arg == "-j" || arg == "--jack-port") {
      if (++i >= argc) { std::cerr << "Missing value for " << arg << "\n"; return ret_err; }
      opt.jack_portname = argv[i];
    }*/
#endif
    else if (!arg.empty() && arg[0] == '-' && !doubledash) {
      std::cerr << "Unknown option \"" << argv[i] << "\"\n";
      return ret_err;
    } else {
      // positional
      positionals.push_back(argv[i]);
    }
  }
  
  if (doing_env) {
    debug ("doing_env=true, returning early");
    return 0;
  }
    
  // if not provided by flags, use positionals:
  //   dry wet out [len]
  // ...except if mode is makesweep and we have only 1 positional param
  // Map positionals depending on mode
  //   - makesweep: use first positional as out.wav (if -o not given)
  //   - others (deconvolve): dry wet out [len]
  if (opt.mode == opmode::MAKESWEEP) {
      // makesweep: we only need an output path
      // allow either:
      //   dirt -s -o sweep.wav
      //   dirt -s sweep.wav
      if (opt.out_path.empty () && !positionals.empty ()) {
          opt.out_path = positionals [0];
      }
      // don't interpret positionals as dry/wet in makesweep mode
  } else {
      // deconvolution / normal modes:
      //   dry wet out [len]
      if (opt.dry_path.empty() && positionals.size() >= 1) {
          opt.dry_path = positionals [0];
      }
      if (opt.wet_path.empty() && positionals.size() >= 2) {
          opt.wet_path = positionals [1];
      }
      if (opt.out_path.empty() && positionals.size() >= 3) {
          opt.out_path = positionals [2];
      }
      if (opt.ir_length_samples == 0 && positionals.size() >= 4) {
          opt.ir_length_samples = std::strtol (positionals [3], nullptr, 10);
      }
  }
  
  //if (opt.gui) return 0;
  int bf = 0;
  if (!opt.dry_path.empty()) bf |= 1;
  if (!opt.wet_path.empty()) bf |= 2;
  if (!opt.out_path.empty()) bf |= 4;

  if (opt.gui) return bf;
  
  // --- Non-deconvolution modes: handle simply and bail out early ---
  if (opt.mode == opmode::MAKESWEEP) {
      if (opt.out_path.empty()) {
          std::cerr << "No output file specified for makesweep mode\n";
          return ret_err;
      }
      // nothing else to validate here
      debug ("end");
      return bf;
  }

#ifdef USE_JACK
  if (opt.mode == opmode::PLAYSWEEP) {
      // playsweep: need a dry destination (JACK port or "jack")
      if (opt.dry_path.empty()) {
          // default to "jack" and autoconnect to default ports
          opt.dry_path = "jack";
          opt.dry_source = sig_source::JACK;
          opt.jack_autoconnect_dry = true;
          bf |= 1;
      }
      debug ("end");
      return bf;
  }
#else
  if (opt.mode == opmode::PLAYSWEEP) {
      std::cerr << "Error: playsweep mode requested but DIRT was built without JACK support.\n";
      return ret_err;
  }
#endif
  
  // --- Deconvolution mode: original bitfield logic ---
  if (opt.mode != opmode::DECONVOLVE) {
      // Shouldn't happen, but just in case
      debug ("end");
      return bf;
  }
  switch (bf) {
    case 0:
      // deconvolve mode but no paths at all -> show full usage
      if (!opt.gui) {
        print_usage (argv [0], false);
        exit (1);
      }
      break;

    case 1: // only dry path given
      if (looks_like_jack_port (opt.dry_path)) {
        opt.mode = opmode::PLAYSWEEP;
        opt.jack_autoconnect_dry = true;
      } else if (opt.dry_path == "jack") {
        opt.dry_path.clear();
        opt.mode = opmode::PLAYSWEEP;
        opt.jack_autoconnect_to_default = true;
        opt.jack_autoconnect_dry = true;
      } else {
        std::cerr << "Only dry file specified\n";
        return ret_err;
      }
      break;

    case 2: // wet
      std::cerr << "Only wet file specified\n";
      return ret_err;
      break;

    case 3: // wet, dry
      std::cerr << "No output file specified\n";
      return ret_err;
      break;

    case 4: // out
      std::cerr << "Only output file specified, no sweeps to deconvolve\n";
      return ret_err;
      break;

    case 5: // dry, out
      std::cerr << "No wet file specified\n\n";
      return ret_err;
      break;

    case 6: // wet, out
      opt.mode = opmode::DECONVOLVE;
      opt.dry_source = sig_source::GENERATE;
      break;

    case 7: // dry, wet, out
      opt.mode = opmode::DECONVOLVE;
      opt.dry_source = sig_source::FILE;
      break;

    default:
      return ret_err;
      break;
  }

  debug ("end");
  return bf;
}

// this function looks for environment variables matching command-line options,
// and gathers up an argv [] style table of those found.
// For each one that takes an extra arg (has_arg) the value of the
// env. variable is added as next argv entry.
// Then parse_args () is used on the resulting table.
int parse_env (int in_argc, char **in_argv, s_prefs &p) {
  int i, argc = 1;
  const char *argv [128];
  const char *env, *equal;
  
  struct s_arg {
    const char *envname;
    const char *argname;
    bool has_arg;
  } argtable [] = {
    // env.variable name    cmdline flag name         has extra arg?
    { "DIRT_DRY",           "--dry",                  true   },
    { "DIRT_WET",           "--wet",                  true   },
    { "DIRT_OUT",           "--out",                  true   },
    { "DIRT_THRESH",        "--thresh",               true   },
    { "DIRT_IR_THRESH",     "--ir-thresh",            true   },
    { "DIRT_ALIGN",         "--align",                true   },
    { "DIRT_DEBUGDUMP",     "--dump",                 true   },
    { "DIRT_SR",            "--samplerate",           true   },
    { "DIRT_SAMPLERATE",    "--samplerate",           true   },
    { "DIRT_SWEEP_F1",      "--sweep-f1",             true   },
    { "DIRT_SWEEP_F2",      "--sweep-f2",             true   },
    { "DIRT_HPF",           "--hpf",                  true   },
    { "DIRT_LPF",           "--lpf",                  true   },
    { "DIRT_SWEEP_LENGTH",  "--sweep-seconds",        true   },
    { "DIRT_SWEEP_SECONDS", "--sweep-seconds",        true   },
    { "DIRT_SWEEP_PREROLL", "--preroll",              true   },
    { "DIRT_SWEEP_MARKER",  "--marker",               true   },
    { "DIRT_SWEEP_GAP",     "--gap",                  true   },
    { "DIRT_PREROLL",       "--preroll",              true   },
    { "DIRT_MARKER",        "--marker",               true   },
    { "DIRT_GAP",           "--gap",                  true   },
    { "DIRT_JACKNAME",      "--",                     true   },
    { "DIRT_WAIT",          "--jack-name",            true   },
    { "DIRT_GUI",           "--gui",                  false  },
    { "DIRT_FORCEMONO",     "--mono",                 false  },
    { "DIRT_ZEROPEAK",      "--zero-peak",            false  },
    { "DIRT_NO_ZEROPEAK",   "--no-zero-peak",         false  },
    { "DIRT_QUIET",         "--quiet",                false  },
    { "DIRT_VERBOSE",       "--verbose",              false  },
    { NULL,                 NULL,                     false  }
  };
  
  argv [0] = in_argv [0];
  argc = 1;
  
  for (i = 1; argtable [i].envname; i++) {
    env = getenv (argtable [i].envname);
    //debug ("i=%d, env='%s'", i, env);
    
    if (env) {
      //debug ("i=%d ('envname=%s, argname='%s') FOUND: argv [%d] = '%s'",
      //       i, argtable [i].envname, argtable[i].argname, argc, env);
      argv [argc] = argtable [i].argname;
      argc++;
      if (argtable [i].has_arg) {
        argv [argc] = env;
        argc++;
      }
    }
  }
  
  debug ("after parsing environment vars, we have %d entries:", argc);
  
  for (i = 0; i < argc; i++) {
    debug ("  argv [%d] = '%s'", i, argv [i]);
  }
  
  return parse_args (argc, (char **) argv, p, true);
}

int main (int argc, char **argv) {
  debug ("start");
  //return old_main (argc, argv);
  char realjackname [256] = { 0 };
  int ret = 0;
  s_prefs p;
  //c_wavebuffer sweep;
  bool main_done = false;
  std::string str;
  
#ifdef USE_WXWIDGETS
  if (argc <= 1) {
    p.gui = true;
  }
#endif
  
  int paths_bf = parse_env (argc, argv, p);
  paths_bf |= parse_args (argc, argv, p);
  
  //if (!p.gui) {
  if (paths_bf == -1 || p.mode == opmode::ERROR) {
    if (!p.gui) {
      print_usage (argv [0]);
      debug ("return");
      return 1;
    }
  }
  
  debug ("paths_bf=%x", paths_bf);
  
  if (!resolve_sources (p, paths_bf)) {
    if (!p.gui) {
      print_usage (argv [0], false);
      debug ("return");
      return 1;
    }
  }
  
  //c_deconvolver *dec = NULL;
  c_deconvolver dec (&p);
  c_audioclient *audio = NULL;

  // start gui BEFORE init audio
  if (p.gui) {
    p.mode = opmode::GUI;
  }

  // using jack at all?
  // avoid upper freq. aliasing: now that we have sample rate from either user
  // or jack, make sure we don't sweep past 95% of nyquist frequency
  // only relevant when we are going to *generate* a sweep
  bool will_use_jack = (p.dry_source == sig_source::JACK ||
                        p.wet_source == sig_source::JACK ||
                        p.mode == opmode::GUI);
  
  
  if (will_use_jack) {
    if (p.dry_source == sig_source::JACK && p.wet_source == sig_source::JACK)
      p.mode = opmode::ROUNDTRIP;
#ifdef USE_JACK
    audio = new c_jackclient (&p);
    debug ("audio=%lx", (long int) audio);
    
    if (p.dry_source == sig_source::JACK || p.wet_source == sig_source::JACK || p.gui) {
      snprintf (realjackname, 255, p.jack_name.c_str (), argv [0]);
      
      if (!audio->init (realjackname, p.sweep_sr, p.request_stereo) || 
                            !audio->register_input (p.request_stereo) ||
                            !audio->register_output (false)/* ||
                            !audio->ready ()*/) {
        std::cout << "Error initializing audio\n";
        
        if (p.mode != opmode::GUI) {
          ret = 1;
          main_done = true;
        }
      } else {
        p.sweep_sr = audio->get_samplerate ();
        std::cout << "Audio backend reports a samplerate of " << p.sweep_sr << "\n";
      }
    }
#else
  std::cerr << argv [0] << " was built without JACK support\n";
  ret = 1;
#endif
  }
  
  if (main_done) {
    debug ("main () aborting");
    if (audio)
      delete audio;
    return ret;
  }
  
  bool play_ok = true;
  bool rec_ok = true;
  c_wavebuffer drysweep, wetsweep_l, wetsweep_r, dummy_r;
  std::vector<float> v_drysweep, v_drysweep_l, v_drysweep_r, v_dummy_r;
  int sr_dry, sr_wet;
  
  if (p.gui) p.mode = opmode::GUI;
  
  switch (p.mode) {
    case opmode::MAKESWEEP:
      {
        debug ("MAKESWEEP");
        generate_log_sweep (p, drysweep);
        std::vector<float> sweepv;
        drysweep.export_to (sweepv);
        if (!write_mono_wav (p.out_path.c_str (), sweepv, drysweep.get_samplerate ())) {
          ret = 1;
        }
        if (!p.quiet) {
            std::cerr << "Wrote sweep: " << p.out_path
                      << " (" << drysweep.size () << " samples @ "
                      << p.sweep_sr << " Hz)\n";
        }
        //if (dec) delete dec;
        ret = 1;
      }
    break;
    
    case opmode::PLAYSWEEP:
      debug ("PLAYSWEEP");
      // playsweep: only when JACK is enabled
      if (p.sweepwait) {
        std::cout << "Press enter to play sinewave sweep... \n";
        std::getline (std::cin, str);
      }
      generate_log_sweep (p, drysweep);
      play_ok = (audio->play (&drysweep) ? true : false);
      if (!play_ok) {
        std::cout << "Failed to initialize audio\n";
        ret = 1;
      } else {
        std::cout << "Waiting for audio device...\n";
        while (audio->state == audiostate::IDLE) {
          usleep (1000);
        }
        std::cout << "Playing...\n";
        while (audio->state == audiostate::PLAY) {
          usleep (1000);
        }
      }
      audio->unregister ();
      if (!play_ok) ret = 1;
      main_done = true;
    break;
    
    case opmode::DECONVOLVE:
      debug ("DECONVOLVE");
      
      // load dry sweep from file
      if (!read_wav (p.dry_path.c_str (), drysweep, dummy_r)) {
        std::cerr << "Failed to load wav file: " << p.dry_path << "\n";
        return false;
      }
      debug ("dry: read %ld samples", (long int) drysweep.size ());
      
      // wet from file
      if (!read_wav (p.wet_path.c_str (), wetsweep_l, wetsweep_r)) {
        std::cerr << "Failed to load wav file: " << p.wet_path << "\n";
        return false;
      }
      debug ("wet: read %ld, %ld samples", (long int) wetsweep_l.size (),
                                           (long int) wetsweep_r.size ());
    
    break;
    
    case opmode::ROUNDTRIP:
      debug ("ROUNDTRIP");
      audio->set_stereo (p.request_stereo);
      generate_log_sweep (p, drysweep);
      
      // play dry sweep while recording wet
      if (!(p.dry_source == sig_source::JACK) != !(p.wet_source == sig_source::JACK)) {
        std::cout << "Error: can't have only 1 of dry & wet as a JACK port\n";
        ret = 1;
      }
      
      audio->arm_record ();
      
      if (p.sweepwait) {
        //std::getline (std::cin, str);
        usleep (100000);
        while (!stdin_has_enter ()) {
          vu_wait (audio->vu_in, "Press ENTER to play+record sweep...");
        }
      }
      
      debug ("(before recording) sweep lengths: dry %ld, wet %ld, %ld",
             (long int) drysweep.size (), (long int) wetsweep_l.size (), wetsweep_r.size ());
      
      rec_ok = audio->playrec (&drysweep, &dummy_r, &wetsweep_l, &wetsweep_r);
      if (!rec_ok) {
        std::cerr << "audio->playrec returned error\n";
        ret = 1;
      }
      
      //std::cout << "Waiting for audio device...\n";
      while (audio->state == audiostate::IDLE) { CP
        usleep (1000);
      }
      
      usleep (100000);
      
      while (audio->state == audiostate::PLAYREC) {
        int sec_left = audio->get_play_remaining () / audio->get_samplerate ();
        char txtbuf [128];
        snprintf (txtbuf, 127, "Playing and recording (%d)...  ", sec_left);
        vu_wait (audio->vu_in, txtbuf);
      }
      audio->stop ();
      debug ("(after recording) sweep lengths: dry %ld, wet %ld, %ld",
             (long int) drysweep.size (), (long int) wetsweep_l.size (), wetsweep_r.size ());
      
    break;
    
    case opmode::GUI:
#ifdef USE_WXWIDGETS
      ret = wx_main (1, argv, audio);
#endif
      main_done = true;
    break;
    
    default:
      std::cerr << "Invalid operation mode: %d", (int) p.mode;
      ret = 1;
    break;
  }
  
  if (audio) audio->stop ();
  usleep (10000);
  
  //debug ("AFTER SWITCH - sweep sizes dry=%ld, wet=%ld,%ld", dec.sweep_dry.size (),
  //       dec.sweep_wet_l.size (), dec.sweep_wet_r.size ());
  
  debug ("AFTER SWITCH - sweep sizes dry=%ld, wet=%ld,%ld", drysweep.size (),
         wetsweep_l.size (), wetsweep_r.size ());
  
  // check if error/done
  if (ret || main_done) {
    if (audio)
      delete audio;
    return ret;
  }
  
  // not done / no error? continue with deconvolving process
  //dec = new c_deconvolver (&p);
  c_wavebuffer ir_l, ir_r;
  
  
  
  // now that we have both sweeps, process
  
  
  // now load dry/wet into deconvolver
  if (!dec.set_sweep_dry (drysweep)) {
    std::cerr << "Deconvolver refused to load dry sweep\n";
    ret - 1;
  }
  
  if (!dec.set_sweep_wet (wetsweep_l, wetsweep_r)) {
    std::cerr << "Deconvolver refused to load wet sweep\n";
    ret = 1;
  }
  
  if (!ret) {
    std::cout << "Detected sample rate: " << dec.get_samplerate () << std::endl;
    if (!dec.render_ir (ir_l, ir_r)) {
      std::cerr << "Error rendering IR file!\n";
      //if (dec) delete dec;
      return 1;
    }

    if (!dec.export_file (p.out_path.c_str (), ir_l, ir_r)) {
      std::cerr << "Error writing IR file!\n";
      //if (dec) delete dec;
      return 1;
    }
  }
  
  usleep (100000); // make sure all callbacks are done before destroying stuff
  //if (dec) delete dec;
  
  debug ("end");
  return ret;
}

