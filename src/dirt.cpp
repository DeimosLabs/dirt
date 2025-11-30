
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

#include "dirt.h"
#include "timestamp.h"
#include "deconvolv.h"
#include "jack.h"

#ifdef DEBUG
#define CMDLINE_IMPLEMENTATION
#define CMDLINE_DEBUG
#include "cmdline/cmdline.h"
#define debug(...) cmdline_debug(stderr,ANSI_RED,__FILE__,__LINE__,__FUNC__,__VA_ARGS__)
#else
#define debug(...)
#endif


c_audioclient::c_audioclient (c_deconvolver *dec) {
  debug ("start");
  dec_ = dec;
  prefs_ = dec->prefs_;
  debug ("end");
}

c_audioclient::~c_audioclient () { }

void c_audioclient::peak_acknowledge () {
  peak_plus_l = 0;
  peak_plus_r = 0;
  peak_minus_l = 0;
  peak_minus_r = 0;
  clip_l = false;
  clip_r = false;
  peak_new = false;
  audio_error = false;
}

static bool stdin_has_enter()   /* 100% copy-pasted from chatgpt */
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

// new: marker_gap_seconds, preroll_seconds
static void generate_log_sweep (double seconds,
                                double preroll_seconds,
                                double start_marker_seconds,
                                double marker_gap_seconds,
                                int samplerate,
                                float sweep_amp_db,
                                float f1, float f2,
                                std::vector<float> &out)
{
  const size_t N      = (size_t) (seconds * samplerate);          // sweep
  const size_t NPRE   = (size_t) (preroll_seconds * samplerate);  // NEW
  const size_t NM     = (size_t) (start_marker_seconds * samplerate);
  const size_t NGAP   = (size_t) (marker_gap_seconds * samplerate);
  size_t n = 0;
  
  if (samplerate <= 0) {
    std::cout << "can't have a sample rate of zero!\n";
    return;
  }

  out.resize (NPRE + NM + NGAP + N);

  std::cerr << "Generating sweep, "   << samplerate << " Hz sample rate:\n"
            << "  Length:           " << seconds << ((seconds == 1) ?
                                         " second\n" : " seconds\n")
            << "  Frequency range:  " << f1 << "Hz to " << f2 << " Hz\n"
            << "  Preroll:          " << NPRE << " samples\n"
            << "  Marker:           " << NM << " samples\n"
            << "  Gap after marker: " << NGAP << " samples\n\n";

  // 0) preroll silence
  for (n = 0; n < NPRE; ++n) {
    out[n] = 0.0f;
  }

  // 1) marker (square wave)
  float sweep_amp_linear = db_to_linear (sweep_amp_db);
  
  const int period = samplerate / MARKER_FREQ; // 1 kHz etc.
  for (; n < NPRE + NM; ++n) {
    size_t k = n - NPRE; // so marker indexing still starts at 0
    out[n] = ((k / (period / 2)) & 1) ? sweep_amp_linear : (-1 * sweep_amp_linear);
  }

  // 2) silence gap
  for (; n < NPRE + NM + NGAP; ++n) {
    out[n] = 0.0f;
  }

  // 3) sweep
  for (; n < NPRE + NM + NGAP + N; ++n) {
    double t     = (double)(n - (NPRE + NM + NGAP)) / samplerate;
    double T     = seconds;
    double w1    = 2.0 * M_PI * f1;
    double w2    = 2.0 * M_PI * f2;
    double L     = std::log (w2 / w1);
    double phase = w1 * T / L * (std::exp (t * L / T) - 1.0);
    out[n]       = (float)std::sin (phase) * (float) sweep_amp_linear;
  }

  // fade out end of sweep (useless ~=20KHz frequencies anyway)
  const int fade_samples = samplerate / 100;
  const size_t total = NPRE + NM + NGAP + N;
  for (int i = 0; i < fade_samples && i < (int)total; ++i) {
    float g = (float)i / (float)fade_samples;
    out[total - 1 - i] *= g;
  }
}

static void print_usage (const char *prog, bool full = false) {
  std::ostream &out = full ? std::cout : std::cerr;
  
  if (full) {
    out << "\nDIRT - Delt's Impulse Response Tool\n"
           "Version " << DIRT_VERSION << " build " << BUILD_TIMESTAMP <<
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
    "  -O, --offset N           Offset (delay) wet sweep by N samples [10]\n"
    "  -z, --zero-peak          Try to align peak to zero"
                             << (DEFAULT_ZEROPEAK ? " [default]\n" : "\n") <<
    "  -Z, --no-zero-peak       Don't try to align peak to zero"
                             << (!DEFAULT_ZEROPEAK ? " [default]\n" : "\n") << 
    "  -M, --mono               Force mono/single channel [no/autodetect]\n"
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
    "  -R, --sweep-sr SR        Sweep samplerate [48000]\n"
    "  -L, --sweep-seconds SEC  Sweep length in seconds [30]\n"
    "  -a, --sweep-amplitude dB Sweep amplitude in dB [-1]\n" // DONE
    "  -X, --sweep-f1 F         Sweep start frequency [" << DEFAULT_F1 << "]\n"
    "  -Y, --sweep-f2 F         Sweep end frequency [" << DEFAULT_F2 << "]\n";
  if (full) out <<
    "  -p, --preroll SEC        Prepend leading silence of SEC seconds\n"
    "  -m, --marker SEC         Prepend alignment marker of SEC seconds\n"
    "  -g, --gap SEC            Add gap of SEC seconds after marker\n"
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
    s = s.substr(a, b - a + 1);
  };

  // --- dry source ---
  if (paths_bf & 1) {
#ifdef USE_JACK
    if (opt.dry_path == "jack") {
#else
    if (0) {
#endif
      opt.dry_source = src_jack;
      // if only dry was specified, assume playsweep to default
      if (!(paths_bf & 2)) {
        opt.jack_autoconnect_dry        = true;
        opt.jack_autoconnect_to_default = true;
      }
    } else if (looks_like_jack_port (opt.dry_path)) {
      opt.dry_source          = src_jack;
      opt.jack_autoconnect_dry = true;  // explicit jack port -> autoconnect
      opt.portname_dry = opt.dry_path;
    } else if (file_exists (opt.dry_path) || opt.mode != mode_deconvolve) {
      opt.dry_source = src_file;
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
  if (paths_bf & 2) {
#ifdef USE_JACK
    if (opt.wet_path == "jack") {
#else
    if (0) {
#endif
      opt.wet_source = src_jack;
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

        if (looks_like_jack_port(left) && looks_like_jack_port (right)) {
          opt.wet_source           = src_jack;
          opt.jack_autoconnect_wet = true;
          opt.portname_wetL        = left;
          opt.portname_wetR        = right;
        } else if (file_exists (opt.wet_path) || opt.mode != mode_deconvolve) {
          // treat whole thing as file name if not a JACK pair
          opt.wet_source = src_file;
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
        opt.wet_source           = src_jack;
        opt.jack_autoconnect_wet = true;
        opt.portname_wetL        = opt.wet_path;
        opt.portname_wetR.clear ();

      } else if (file_exists (opt.wet_path) || opt.mode != mode_deconvolve) {
        opt.wet_source = src_file;

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
  if (paths_bf & 4) {
    if (opt.out_path == "jack" || looks_like_jack_port (opt.out_path)) {
      if (opt.mode == mode_deconvolve) {
        std::cerr << "Can't write IR directly to a JACK port\n\n";
        return false;
      }
    }
  } else if (opt.mode == mode_deconvolve) {
    std::cerr << "No output file specified\n";
    return false;
  }
  
  if (opt.dry_source == src_jack && opt.wet_source == src_jack) {
    opt.preroll_seconds = 0.1;
    opt.marker_seconds = 0;
    opt.marker_gap_seconds = 0;
  }
  
  debug ("end");
  return true;
}

// returns bit-field of paths that were provided by user, or -1 on error
// bit 1: dry, bit 2: wet, bit 3: out
int parse_args (int argc, char **argv, s_prefs &opt) {
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
        { align_marker,       "marker" },
        { align_marker_dry,   "dry" },
        { align_silence,      "silence" },
        { align_none,         "none" }
      };
      bool valid = false;
      for (int j = 0; j < align_method_max; j++)
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
    } else if (arg == "-M" || arg == "--mono") {
      opt.request_stereo = false;
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
      opt.mode = deconv_mode::mode_makesweep;
      //opt.sweep_seconds = std::atof (argv [i]);
    } else if (arg == "-S" || arg == "--playsweep") {
#ifdef USE_JACK
      //if (++i >= argc) { std::cerr << "Missing value for " << arg << "\n"; return ret_err; }
      opt.mode = deconv_mode::mode_playsweep;
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
    } else if (arg == "-m" || arg == "--marker") {
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
    } else if (arg == "-p" || arg == "--preroll") {
        if (++i >= argc) {
            std::cerr << "Missing value for " << arg << "\n";
            return ret_err;
        }
        opt.preroll_seconds = std::atof (argv [i]);
        if (opt.preroll_seconds < 0.0) {
            std::cerr << "Preroll length cannot be negative.\n";
            return ret_err;
        }
    } else if (arg == "-g" || arg == "--gap") {
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
  
  // if not provided by flags, use positionals:
  //   dry wet out [len]
  // ...except if mode is makesweep and we have only 1 positional param
  // Map positionals depending on mode
  //   - makesweep: use first positional as out.wav (if -o not given)
  //   - others (deconvolve): dry wet out [len]
  if (opt.mode == deconv_mode::mode_makesweep) {
      // makesweep: we only need an output path
      // allow either:
      //   dirt -s -o sweep.wav
      //   dirt -s sweep.wav
      if (opt.out_path.empty() && !positionals.empty()) {
          opt.out_path = positionals[0];
      }
      // don't interpret positionals as dry/wet in makesweep mode
  } else {
      // deconvolution / normal modes:
      //   dry wet out [len]
      if (opt.dry_path.empty() && positionals.size() >= 1) {
          opt.dry_path = positionals[0];
      }
      if (opt.wet_path.empty() && positionals.size() >= 2) {
          opt.wet_path = positionals[1];
      }
      if (opt.out_path.empty() && positionals.size() >= 3) {
          opt.out_path = positionals[2];
      }
      if (opt.ir_length_samples == 0 && positionals.size() >= 4) {
          opt.ir_length_samples = std::strtol(positionals[3], nullptr, 10);
      }
  }

  int bf = 0;
  if (!opt.dry_path.empty()) bf |= 1;
  if (!opt.wet_path.empty()) bf |= 2;
  if (!opt.out_path.empty()) bf |= 4;

  // --- Non-deconvolution modes: handle simply and bail out early ---
  if (opt.mode == deconv_mode::mode_makesweep) {
      if (opt.out_path.empty()) {
          std::cerr << "No output file specified for makesweep mode\n";
          return ret_err;
      }
      // nothing else to validate here
      debug ("end");
      return bf;
  }

#ifdef USE_JACK
  if (opt.mode == deconv_mode::mode_playsweep) {
      // playsweep: need a dry destination (JACK port or "jack")
      if (opt.dry_path.empty()) {
          // default to "jack" and autoconnect to default ports
          opt.dry_path = "jack";
          opt.dry_source = src_jack;
          opt.jack_autoconnect_dry = true;
          bf |= 1;
      }
      debug ("end");
      return bf;
  }
#else
  if (opt.mode == deconv_mode::mode_playsweep) {
      std::cerr << "Error: playsweep mode requested but DIRT was built without JACK support.\n";
      return ret_err;
  }
#endif

  // --- Deconvolution mode: original bitfield logic ---
  if (opt.mode != deconv_mode::mode_deconvolve) {
      // Shouldn't happen, but just in case
      debug ("end");
      return bf;
  }

  switch (bf) {
    case 0:
      // deconvolve mode but no paths at all -> show full usage
      print_usage(argv[0], false);
      exit(1);
      break;

    case 1: // only dry path given
      if (looks_like_jack_port (opt.dry_path)) {
        opt.mode = mode_playsweep;
        opt.jack_autoconnect_dry = true;
      } else if (opt.dry_path == "jack") {
        opt.dry_path.clear();
        opt.mode = mode_playsweep;
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
      opt.mode = deconv_mode::mode_deconvolve;
      opt.dry_source = src_generate;
      break;

    case 7: // dry, wet, out
      opt.mode = deconv_mode::mode_deconvolve;
      opt.dry_source = src_file;
      break;

    default:
      return ret_err;
      break;
  }

  debug ("end");
  return bf;
}

int main (int argc, char **argv) {
  debug ("start");
  s_prefs p;
  char realjackname [256] = { 0 };
  
  int paths_bf = parse_args (argc, argv, p);
  if (paths_bf == -1 || p.mode == deconv_mode::mode_error) {
    print_usage (argv [0]);
    debug ("return");
    return 1;
  }

  if (!resolve_sources (p, paths_bf)) {
    print_usage (argv [0], false);
    debug ("return");
    return 1;
  }
  
  // our main deconvolver object
  c_deconvolver dec (&p);
  
  // using jack at all?
#ifdef USE_JACK
  
  if (p.dry_source == src_jack || p.wet_source == src_jack) {
    snprintf (realjackname, 255, p.jack_name.c_str (), argv [0]);
    
    if (!dec.audio_init (realjackname, p.sweep_sr, p.request_stereo) || !dec.audio_ready ()) {
      std::cout << "Error initializing audio\n";
      return 1;
    }
  }
#endif
  
  // avoid upper freq. aliasing: now that we have sample rate from either user
  // or jack, make sure we don't sweep past 95% of nyquist frequency
  // only relevant when we are going to *generate* a sweep
  bool will_generate_sweep =
      (p.mode == deconv_mode::mode_makesweep)  ||
      (p.mode == deconv_mode::mode_playsweep)  ||
      (p.dry_source == src_generate)           ||
      (p.dry_source == src_jack)               ||  // live JACK playrec
      (p.wet_source == src_jack);
      
  if (will_generate_sweep) {
    float sweep_max = (p.sweep_sr / 2.0f) * 0.95f;
    if (p.sweep_f2 > sweep_max) {
      std::cout << "NOTE: requested upper frequency " << p.sweep_f2
                << " Hz is above 95% of nyquist frequency (" << sweep_max << " Hz)\n";
#if 0
      std::cout << "Capping to " << sweep_max << std::endl;
      p.sweep_f2 = sweep_max;
#endif
    }
  } else if (p.sweep_sr_given && p.sweep_sr != 0) {
    std::cerr << "Warning: ignoring supplied sample rate " << p.sweep_sr << std::endl;
  }
  
  std::vector<float> sweep;

  // --makesweep: available even if compiled without JACK
  if (p.mode == deconv_mode::mode_makesweep) {
    generate_log_sweep(p.sweep_seconds,
                       p.preroll_seconds,
                       p.marker_seconds,
                       p.marker_gap_seconds,
                       p.sweep_sr,
                       p.sweep_amp_db,
                       p.sweep_f1,
                       p.sweep_f2,
                       sweep);
    if (!write_mono_wav (p.out_path.c_str (), sweep, p.sweep_sr)) {
      return 1;
    }
    if (!p.quiet) {
        std::cerr << "Wrote sweep: " << p.out_path
                  << " (" << sweep.size() << " samples @ "
                  << p.sweep_sr << " Hz)\n";
    }
    debug ("return");
    return 0;
  }

#ifdef USE_JACK
  // playsweep: only when JACK is enabled
  if (p.mode == deconv_mode::mode_playsweep) {
    generate_log_sweep(p.sweep_seconds,
                       p.preroll_seconds,
                       p.marker_seconds,
                       p.marker_gap_seconds,
                       p.sweep_sr,
                       p.sweep_amp_db,
                       p.sweep_f1,
                       p.sweep_f2,
                       sweep);

    if (p.sweepwait) {
      std::cout << "Press enter to play sinewave sweep... ";
      std::string str;
      std::getline(std::cin, str);
    }
    
    int play_ok = dec.audio->play (sweep);
    dec.audio->shutdown();
    
    debug ("return");
    return play_ok ? 0 : 1;
  }

  // normal deconvolution path (with or without JACK)
  // Special "live" JACK->IR mode: dry=JACK, wet=JACK
  if (p.dry_source == src_jack && p.wet_source == src_jack) {
    generate_log_sweep(p.sweep_seconds,
                       p.preroll_seconds,
                       p.marker_seconds,
                       p.marker_gap_seconds,
                       p.sweep_sr,
                       p.sweep_amp_db,
                       p.sweep_f1,
                       p.sweep_f2,
                       sweep);
    
    std::vector<float> wet_l;
    std::vector<float> wet_r;
    
    if (p.sweepwait) {
      CP
      dec.audio_arm_record ();
      std::cout << "Press enter to play and record sinewave sweep... ";
      std::string str;
      std::getline(std::cin, str);
    }
    
    if (!dec.audio_playrec (sweep, std::vector<float> (), wet_l, wet_r)) {
      debug ("return");
      return 1;
    }
    
    if (!dec.set_dry_from_buffer (sweep, p.sweep_sr)) return 1;
    if (!dec.set_wet_from_buffer (wet_l, std::vector<float>(), p.sweep_sr)) return 1;
    if (!dec.output_ir (p.out_path.c_str(), p.ir_length_samples)) return 1;
    
    debug ("return");
    return 0;
  }
#endif

  // file / generated dry + file wet deconvolution

  if (p.dry_source == src_file) {
      if (!dec.load_sweep_dry (p.dry_path.c_str())) return 1;
  } else if (p.dry_source == src_generate) {
      std::vector<float> sweep_local;
      generate_log_sweep(p.sweep_seconds,
                         p.preroll_seconds,
                         p.marker_seconds,
                         p.marker_gap_seconds,
                         p.sweep_sr,
                         p.sweep_amp_db,
                         p.sweep_f1,
                         p.sweep_f2,
                         sweep_local);
      if (!dec.set_dry_from_buffer (sweep_local, p.sweep_sr)) return 1;
  } else {
#ifdef USE_JACK
    std::cerr << "Error: Dry JACK only supported when wet is JACK.\n";
#else
    std::cerr << "Error: Dry JACK not supported (built without JACK).\n";
#endif
    debug ("return");
    return 1;
  }

  if (p.wet_source == src_file) {
      if (!dec.load_sweep_wet (p.wet_path.c_str())) return 1;
  } else {
#ifdef USE_JACK
    std::cerr << "Error: Wet JACK only supported when dry is JACK.\n";
#else
    std::cerr << "Error: Wet JACK not supported (built without JACK).\n";
#endif
    debug ("return");
    return 1;
  }
  
  std::cout << "Detected sample rate: " << dec.samplerate () << std::endl;
  if (!dec.output_ir (p.out_path.c_str (), p.ir_length_samples)) {
    debug ("return");
    return 1;
  }

  debug ("end");
  return 0;
}

