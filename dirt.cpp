
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

// our random number generator
class c_randomgen {
public:
  inline c_randomgen () { new_seed (); }
  inline void new_seed () {
    // default random seed to microseconds returned by gettimeofday ()
    struct timeval tv;
    //struct timezone tz = { 0, 0 }; /* thanks chatgpt */
    gettimeofday (&tv, NULL/*&tz*/);
    set_seed (tv.tv_sec * 60 + tv.tv_usec);
    next ();
  }
  inline c_randomgen (unsigned int origseed) { set_seed (origseed); }
  
  inline void set_seed (unsigned int newseed) { rnd_seed = newseed; }
  inline uint32_t get_seed () { return rnd_seed; }
  
  // math from:
  // https://www.christianpinder.com/articles/pseudo-random-number-generation/
  inline unsigned int next () {
    int k1;
    int ix = rnd_seed;

    k1 = ix / 127773;
    ix = 16807 * (ix - k1 * 127773) - k1 * 2836;
    if (ix < 0)
      ix += 2147483647;
    rnd_seed = ix;
    
    return rnd_seed;
  }
	inline uint32_t next32 () { return (uint32_t) next (); }
  inline uint64_t next64 () { return (((uint64_t) next ()) << 32) | next (); }
  inline unsigned long int nextlong () { return (unsigned long int) next64 (); }
	/*inline double nextf (double max) {
	  long int intvalue = 0;
		intvalue = next64 () % (long int) (max * 1000000000.0);
		return ((float) intvalue) / 1000000000.0;
		
	}* /
	inline double nextf (double max) {
	  if (max == 0)
		  return 0;
		
    return (next64 () % static_cast <uint64_t> (max)) / static_cast <double> (max);
  }*/
	inline double nextf (double max) {
	  return ((next64 () >> 32) / 4294967296.0f) * max;
	}
  
private:
  // cycle of about 4.2 billion random values... good enough for now
  /*THREAD_SAFE*/ uint32_t rnd_seed;
};

// some static helper functions

static bool file_exists(const std::string &path) {
  struct stat st{};
  return (::stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode));
}

static bool looks_like_jack_port(const std::string &s) {
  return (s.find(':') != std::string::npos);
}

static inline size_t next_pow2 (size_t n) {
  size_t p = 1;
  while (p < n) p <<= 1;
  return p;
}

static inline double db_to_linear (double db) {
  // clamp so we don't get crazy values
  if (db > 200.0f) db = 200.0f;
  if (db < -200.0f) db = -200.0f;
  return std::pow (10.0f, db / 20.0f);
}

static size_t find_first_nonsilent(const std::vector<float> &buf,
                                   float silence_thresh)
{
    for (size_t i = 0; i < buf.size(); ++i) {
        if (std::fabs(buf[i]) > silence_thresh) return i;
    }
    return buf.size();
}

static size_t detect_sweep_start_with_marker(const std::vector<float> &buf,
                                             int samplerate,
                                             float silence_thresh,
                                             float sweep_amp_db,
                                             double marker_freq,          // MARKER_FREQ
                                             double marker_seconds_hint,  // 0 = unknown / autodetect
                                             double gap_seconds_hint,     // 0 = unknown / autodetect
                                             bool verbose,
                                             size_t *out_marker_len = NULL,
                                             size_t *out_gap_len = NULL) {
    // default outputs
    if (out_marker_len) *out_marker_len = 0;
    if (out_gap_len)    *out_gap_len    = 0;

    if (buf.empty()) return 0;
    
    float sweep_amp_linear = db_to_linear (sweep_amp_db);

    // 1) First non-silent sample in the whole file
    size_t i0 = find_first_nonsilent(buf, silence_thresh);
    if (i0 >= buf.size()) return 0; // all silent, fall back

    if (marker_freq <= 0.0) {
        // No marker mode, just use the first non-silent sample like before
        if (verbose) {
            std::cerr << "No marker_freq configured, using first non-silent at "
                      << i0 << "\n";
        }
        return i0;
    }

    // 2) Analyze a short window after i0 to see if it's our marker square wave
    const double max_marker_window_sec = (marker_seconds_hint > 0.0)
                                         ? marker_seconds_hint
                                         : 0.2; // 200 ms default window
    const size_t max_window_samples =
        std::min(buf.size() - i0,
                 (size_t) (max_marker_window_sec * samplerate));

    if (max_window_samples < 8) {
        // Not enough samples to decide, just fall back
        if (verbose) {
            std::cerr << "Marker window too short, using first non-silent at "
                      << i0 << "\n";
        }
        return i0;
    }

    // Count sign flips above some amplitude
    const float amp_thresh = 0.5f * sweep_amp_linear; // strong signal only
    int last_sign = 0;
    size_t last_flip_idx = 0;
    int flip_count = 0;
    size_t first_flip_idx = 0;

    for (size_t n = 0; n < max_window_samples; ++n) {
        float v = buf[i0 + n];
        if (std::fabs(v) < amp_thresh) continue;

        int s = (v > 0.0f) ? 1 : -1;
        if (last_sign == 0) {
            last_sign = s;
            first_flip_idx = n;
            last_flip_idx = n;
        } else if (s != last_sign) {
            last_sign = s;
            flip_count++;
            last_flip_idx = n;
        }
    }

    if (flip_count < 4) {
        // Not enough flips for a clean square wave, treat as no marker
        if (verbose) {
            std::cerr << "Not enough marker sign flips ("
                      << flip_count << "), using first non-silent at "
                      << i0 << "\n";
        }
        return i0;
    }

    double avg_samples_per_flip =
        (double)(last_flip_idx - first_flip_idx) / flip_count;

    // For a square wave, 2 flips per full period
    double est_period = 2.0 * avg_samples_per_flip;
    double est_freq   = (est_period > 0.0)
                        ? (double)samplerate / est_period
                        : 0.0;

    double rel_err = std::fabs(est_freq - marker_freq) / marker_freq;

    if (verbose) {
        std::cerr << "Marker detect: est_freq=" << est_freq
                  << " Hz (target " << marker_freq << " Hz), rel_err="
                  << rel_err << ", flips=" << flip_count << "\n";
    }

    if (est_freq <= 0.0 || rel_err > 0.2) {
        // >20% off → probably not our marker
        if (verbose) {
            std::cerr << "Marker frequency mismatch, using first non-silent at "
                      << i0 << "\n";
        }
        return i0;
    }

    // 3) Marker looks valid → decide where it ends
    size_t marker_end = i0;
    if (marker_seconds_hint > 0.0) {
        marker_end = i0 + (size_t)(marker_seconds_hint * samplerate);
        if (marker_end > buf.size()) marker_end = buf.size();
    } else {
        // Auto: extend until signal no longer looks like strong marker
        const size_t max_marker_samples =
            std::min(buf.size() - i0,
                     (size_t)(0.5 * samplerate)); // up to 0.5s
        marker_end = i0;
        for (size_t n = 0; n < max_marker_samples; ++n) {
            float v = buf[i0 + n];
            if (std::fabs(v) < amp_thresh * 0.3f) { // decayed significantly
                marker_end = i0 + n;
                break;
            }
        }
        if (marker_end <= i0) marker_end = i0 + max_marker_samples;
        if (marker_end > buf.size())      marker_end = buf.size();
    }

    if (verbose) {
        std::cerr << "Marker detected from " << i0
                  << " to " << marker_end << " samples\n";
    }

    // 4) Skip gap (silence) after marker
    size_t gap_start = marker_end;
    size_t gap_end   = gap_start;

    const size_t min_gap_samples =
        (gap_seconds_hint > 0.0)
        ? (size_t)(gap_seconds_hint * samplerate)
        : (size_t)(0.05 * samplerate); // 50 ms minimum

    // find first non-silent after marker
    // but ensure at least min_gap_samples of silence if possible
    size_t silent_run = 0;
    bool in_silence = false;
    for (size_t i = marker_end; i < buf.size(); ++i) {
        if (std::fabs(buf[i]) < silence_thresh) {
            if (!in_silence) {
                in_silence = true;
                silent_run = 1;
                gap_start  = i;
            } else {
                silent_run++;
            }
        } else {
            if (in_silence && silent_run >= min_gap_samples) {
                gap_end = i;
                break;
            }
            // reset and keep looking for a clean run
            in_silence = false;
            silent_run = 0;
        }
    }

    size_t sweep_start = 0;
    if (gap_end > gap_start && gap_end < buf.size()) {
        sweep_start = gap_end;
    } else {
        // No clear long silence run; just take first non-silent after marker_end
        std::vector<float> tail(buf.begin() + marker_end, buf.end());
        size_t rel = find_first_nonsilent(tail, silence_thresh);
        sweep_start = rel + marker_end;
    }

    if (sweep_start >= buf.size()) sweep_start = i0; // fallback

    // Compute marker_len and gap_len in samples,
    // defined relative to first non-silent (i0)
    if (sweep_start != i0 && marker_end > i0) {
        size_t marker_len = marker_end - i0;
        size_t gap_len    = sweep_start - marker_end;  // silence between marker and sweep

        if (out_marker_len) *out_marker_len = marker_len;
        if (out_gap_len)    *out_gap_len    = gap_len;

        if (verbose) {
            std::cerr << "marker_len=" << marker_len
                      << " gap_len="   << gap_len << "\n";
        }
    }

    if (verbose) {
        std::cerr << "Sweep start index = " << sweep_start << "\n";
    }

    return sweep_start;
}

size_t detect_dry_sweep_start(const std::vector<float> &dry,
                              int samplerate,
                              s_prefs &prefs)
{
    size_t marker_len = 0;
    size_t gap_len    = 0;

    size_t start = detect_sweep_start_with_marker(
        dry,
        samplerate,
        prefs.silence_thresh,  // FIXED: these were swapped
        prefs.sweep_amp_db,    // 
        MARKER_FREQ,
        prefs.marker_seconds,
        prefs.marker_gap_seconds,
        prefs.verbose,
        &marker_len,
        &gap_len);

    prefs.cache_dry_marker_len = marker_len;
    prefs.cache_dry_gap_len    = gap_len;

    if (prefs.verbose) {
        std::cerr << "dry: start=" << start
                  << " marker_len=" << marker_len
                  << " gap_len="    << gap_len << "\n";
    }

    return start;
}

size_t detect_wet_sweep_start(const std::vector<float> &wet,
                              int samplerate,
                              const s_prefs &prefs)
{
    size_t first = find_first_nonsilent(wet, prefs.silence_thresh);

    if (prefs.cache_dry_marker_len || prefs.cache_dry_gap_len) {
        size_t start = first
                     + prefs.cache_dry_marker_len
                     + prefs.cache_dry_gap_len;

        if (prefs.verbose) {
            std::cerr << "wet: first=" << first
                      << " +marker=" << prefs.cache_dry_marker_len
                      << " +gap="    << prefs.cache_dry_gap_len
                      << " -> start=" << start << "\n";
        }

        return start;
    }

    // fallback: no cache (e.g. no marker detected in dry)
    size_t start = detect_sweep_start_with_marker(
        wet,
        samplerate,
        prefs.silence_thresh,  // FIXED: these were swapped
        prefs.sweep_amp_db,    // 
        MARKER_FREQ,
        prefs.marker_seconds,
        prefs.marker_gap_seconds,
        prefs.verbose);

    if (prefs.verbose) {
        std::cerr << "wet: no cached marker, start=" << start << "\n";
    }

    return start;
}

static bool read_mono_wav (const char *path,
                           std::vector<float> &out,
                           int &samplerate) {
  SF_INFO info { };
  SNDFILE *f = sf_open (path, SFM_READ, &info);
  if (!f) {
    std::cerr << "Error: cannot open file " << path << ": "
              << sf_strerror (nullptr) << "\n";
    return false;
  }

  samplerate = info.samplerate;
  const sf_count_t frames = info.frames;
  const int chans = info.channels;

  if (frames <= 0 || chans <= 0) {
    std::cerr << "Error: invalid format in " << path << "\n";
    sf_close (f);
    return false;
  }

  std::vector<float> buf (frames * chans);
  sf_count_t read = sf_readf_float (f, buf.data (), frames);
  sf_close (f);

  if (read <= 0) {
    std::cerr << "Error: no samples read from " << path << "\n";
    return false;
  }

  out.resize (read);
  for (sf_count_t i = 0; i < read; ++i) {
    float sum = 0.0f;
    for (int ch = 0; ch < chans; ++ch) {
      sum += buf [i * chans + ch];
    }
    out [i] = sum / chans;
  }

  return true;
}

static bool write_mono_wav (const char *path,
                            const std::vector<float> &data,
                            int samplerate) {
  SF_INFO info { };
  info.channels   = 1;
  info.samplerate = samplerate;
  info.format     = SF_FORMAT_WAV | SF_FORMAT_FLOAT;

  SNDFILE *out = sf_open (path, SFM_WRITE, &info);
  if (!out) {
    std::cerr << "Error: cannot open output file " << path
              << " for writing: " << sf_strerror (nullptr) << "\n";
    return false;
  }

  sf_count_t frames  = (sf_count_t) data.size ();
  sf_count_t written = sf_writef_float (out, data.data (), frames);
  sf_close (out);

  if (written != frames) {
    std::cerr << "Warning: wrote only " << written
              << " of " << frames << " frames to " << path << "\n";
  }
  return true;
}

static bool write_stereo_wav (const char *path,
                              const std::vector<float> &L,
                              const std::vector<float> &R,
                              int samplerate) {
  if (L.size () != R.size ()) {
    std::cerr << "Internal error: L/R size mismatch in write_stereo_wav\n";
    return false;
  }

  SF_INFO info { };
  info.channels   = 2;
  info.samplerate = samplerate;
  info.format     = SF_FORMAT_WAV | SF_FORMAT_FLOAT;

  SNDFILE *out = sf_open (path, SFM_WRITE, &info);
  if (!out) {
    std::cerr << "Error: cannot open output file " << path
              << " for writing: " << sf_strerror (nullptr) << "\n";
    return false;
  }

  sf_count_t frames = (sf_count_t) L.size ();
  std::vector<float> interleaved (frames * 2);
  for (sf_count_t i = 0; i < frames; ++i) {
    interleaved [2 * i + 0] = L [i];
    interleaved [2 * i + 1] = R [i];
  }

  sf_count_t written = sf_writef_float (out, interleaved.data (), frames);
  sf_close (out);

  if (written != frames) {
    std::cerr << "Warning: wrote only " << written
              << " of " << frames << " frames to " << path << "\n";
  }
  return true;
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
  const size_t N      = (size_t)(seconds * samplerate);          // sweep
  const size_t NPRE   = (size_t)(preroll_seconds * samplerate);  // NEW
  const size_t NM     = (size_t)(start_marker_seconds * samplerate);
  const size_t NGAP   = (size_t)(marker_gap_seconds * samplerate);
  size_t n = 0;

  out.resize (NPRE + NM + NGAP + N);

  std::cerr << "Generating sweep:   " << seconds << ((seconds == 1) ?
                                         " second\n" : " seconds\n") <<
               "  Preroll:          " << NPRE << 
             "\n  Marker:           " << NM << 
             "\n  Gap after marker: " << NGAP << "\n\n";

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

  // fade out sweep tail
  const int fade_samples = samplerate / 100;
  const size_t total = NPRE + NM + NGAP + N;
  for (int i = 0; i < fade_samples && i < (int)total; ++i) {
    float g = (float)i / (float)fade_samples;
    out[total - 1 - i] *= g;
  }
}

static size_t find_peak (const std::vector<float> &buf) {
  size_t idx = 0;
  float maxv = 0.0f;
  for (size_t i = 0; i < buf.size (); ++i) {
    float a = std::fabs (buf [i]);
    if (a > maxv) {
      maxv = a;
      idx = i;
    }
  }
  return idx;
}

static void shift_ir_left (std::vector<float> &buf, size_t shift) {
  if (buf.empty () || shift == 0) return;
  if (shift >= buf.size ()) {
    std::fill (buf.begin (), buf.end (), 0.0f);
    return;
  }
  size_t n = buf.size () - shift;
  for (size_t i = 0; i < n; ++i)
    buf [i] = buf [i + shift];
  for (size_t i = n; i < buf.size (); ++i)
    buf [i] = 0.0f;
}

static void shift_ir_right (std::vector<float> &buf, size_t shift) {
  if (buf.empty () || shift == 0) return;
  size_t n = buf.size ();
  if (shift >= n) {
    std::fill (buf.begin (), buf.end (), 0.0f);
    return;
  }
  for (size_t i = n; i-- > shift; )
    buf [i] = buf [i - shift];
  for (size_t i = 0; i < shift; ++i)
    buf [i] = 0.0f;
}

static inline void shift_ir (std::vector<float> &buf, long int shift) {
  if (shift > 0)
    shift_ir_right (buf, shift);
  else if (shift < 0)
    shift_ir_left (buf, shift * -1);
}

// joint alignment: preserve L/R offset
static void align_stereo_joint (std::vector<float> &L,
                                std::vector<float> &R,
                                size_t target_index) {
  if (L.empty () && R.empty ()) return;

  // choose left as reference; if L is empty, fall back to right
  size_t ref_peak = !L.empty () ? find_peak (L) : find_peak (R);

  if (ref_peak > target_index) {
    size_t shift = ref_peak - target_index;
    shift_ir_left (L, shift);
    shift_ir_left (R, shift);
  } else {
    size_t shift = target_index - ref_peak;
    shift_ir_right (L, shift);
    shift_ir_right (R, shift);
  }
}

static void align_stereo_joint_no_chop(std::vector<float> &L,
                                       std::vector<float> &R,
                                       size_t target_index)
{
    if (L.empty() && R.empty()) return;

    size_t ref_peak = !L.empty() ? find_peak(L) : find_peak(R);

    if (ref_peak >= target_index) {
        // don't chop, just leave as-is
        return;
    }

    size_t shift = target_index - ref_peak;
    shift_ir_right(L, shift);
    shift_ir_right(R, shift);
}

// c_deconvolver member functions

c_deconvolver::c_deconvolver (struct s_prefs *prefs) {
  set_prefs (prefs);
}

void c_deconvolver::set_prefs (struct s_prefs *prefs) {
  if (prefs)
    prefs_ = prefs;
  else
    prefs_ = &default_prefs_;

  // if user didn't override it, use default dB value
  if (prefs_->silence_thresh < 0.0f) {
    prefs_->silence_thresh = (float) db_to_linear (DEFAULT_SILENCE_THRESH_DB);
  }
}

bool c_deconvolver::load_sweep_dry (const char *in_filename) {
  int sr = 0;
  std::vector<float> tmp;

  if (!read_mono_wav (in_filename, tmp, sr)) {
    std::cerr << "Failed to load dry sweep: " << in_filename << "\n";
    return false;
  }
  if (!set_samplerate_if_needed (sr)) {
    std::cerr << "Samplerate mismatch in dry sweep: " << in_filename << "\n";
    return false;
  }

  dry_ = std::move (tmp);
  have_dry_ = true;

#ifndef DISABLE_LEADING_SILENCE_DETECTION
  // NEW: detect sweep start using marker+gap, also caches marker/gap in prefs_
  dry_offset_ = detect_dry_sweep_start(dry_, samplerate_, *prefs_);
  if (prefs_->verbose) {
    std::cerr << "Dry sweep start offset: " << dry_offset_ << " samples\n";
  }
#else
  dry_offset_ = 0;
#endif

  return true;
}

bool c_deconvolver::load_sweep_wet (const char *in_filename) {
  SF_INFO info { };
  SNDFILE *f = sf_open (in_filename, SFM_READ, &info);
  if (!f) {
    std::cerr << "Failed to open wet sweep: " << in_filename << "\n";
    return false;
  }

  int sr        = info.samplerate;
  int chans     = info.channels;
  sf_count_t frames = info.frames;

  if (!set_samplerate_if_needed (sr)) {
    std::cerr << "Samplerate mismatch in wet sweep: " << in_filename << "\n";
    sf_close (f);
    return false;
  }

  if (frames <= 0 || chans <= 0) {
    std::cerr << "Error: invalid format in " << in_filename << "\n";
    sf_close (f);
    return false;
  }

  std::vector<float> buf (frames * chans);
  sf_count_t readFrames = sf_readf_float (f, buf.data (), frames);
  sf_close (f);

  if (readFrames <= 0) {
    std::cerr << "Error: no samples read from " << in_filename << "\n";
    return false;
  }

  wet_L_.clear ();
  wet_R_.clear ();

  if (chans == 1) {
    // mono wet → only L
    wet_L_.assign (buf.begin (), buf.begin () + readFrames);
#ifndef DISABLE_LEADING_SILENCE_DETECTION
    wet_offset_ = detect_wet_sweep_start(wet_L_, samplerate_, *prefs_);
    if (prefs_->verbose) {
      std::cerr << "Wet sweep (mono) start offset: "
                << wet_offset_ << " samples\n";
    }
#else
    wet_offset_ = 0;
#endif
  } else if (chans == 2) {
    wet_L_.resize (readFrames);
    wet_R_.resize (readFrames);
    for (sf_count_t i = 0; i < readFrames; ++i) {
      wet_L_[i] = buf[2 * i + 0];
      wet_R_[i] = buf[2 * i + 1];
    }

#ifndef DISABLE_LEADING_SILENCE_DETECTION
    // For detection, use a mono mix so we get a single start index
    std::vector<float> mix(readFrames);
    for (sf_count_t i = 0; i < readFrames; ++i) {
      mix[i] = 0.5f * (wet_L_[i] + wet_R_[i]);
    }

    wet_offset_ = detect_wet_sweep_start(mix, samplerate_, *prefs_);
    if (prefs_->verbose) {
      std::cerr << "Wet sweep (stereo) start offset: "
                << wet_offset_ << " samples\n";
    }
#else
    wet_offset_ = 0;
#endif
  } else {
    std::cerr << "Only mono or stereo wet sweeps supported.\n";
    return false;
  }

  have_wet_ = true;
  return true;
}

bool c_deconvolver::set_dry_from_buffer(const std::vector<float>& buf, int sr) {
  if (buf.empty()) {
    std::cerr << "Dry buffer is empty.\n";
    return false;
  }
  if (!set_samplerate_if_needed(sr)) {
    std::cerr << "Samplerate mismatch for dry buffer.\n";
    return false;
  }

  dry_ = buf;
  have_dry_ = true;

#ifndef DISABLE_LEADING_SILENCE_DETECTION
  dry_offset_ = detect_dry_sweep_start(dry_, samplerate_, *prefs_);
  if (prefs_->verbose) {
    std::cerr << "Dry sweep start offset (buffer): "
              << dry_offset_ << " samples\n";
  }
#else
  dry_offset_ = 0;
#endif

  return true;
}

bool c_deconvolver::set_wet_from_buffer(const std::vector<float>& bufL,
                                        const std::vector<float>& bufR,
                                        int sr) {
  if (bufL.empty() && bufR.empty()) {
    std::cerr << "Wet buffers are empty.\n";
    return false;
  }
  if (!set_samplerate_if_needed(sr)) {
    std::cerr << "Samplerate mismatch for wet buffer.\n";
    return false;
  }

  wet_L_ = bufL;
  wet_R_ = bufR;

#ifndef DISABLE_LEADING_SILENCE_DETECTION
  if (!wet_R_.empty()) {
    const size_t n = std::min(wet_L_.size(), wet_R_.size());
    std::vector<float> mix(n);
    for (size_t i = 0; i < n; ++i) {
      mix[i] = 0.5f * (wet_L_[i] + wet_R_[i]);
    }
    wet_offset_ = detect_wet_sweep_start(mix, samplerate_, *prefs_);
  } else {
    wet_offset_ = detect_wet_sweep_start(wet_L_, samplerate_, *prefs_);
  }

  if (prefs_->verbose) {
    std::cerr << "Wet sweep (buffer) start offset: "
              << wet_offset_ << " samples\n";
  }
#else
  wet_offset_ = 0;
#endif

  have_wet_ = true;
  return true;
}

bool c_deconvolver::output_ir (const char *out_filename, long ir_length_samples) {
  std::vector<float> irL;
  std::vector<float> irR;

  // --- deconvolve left ---
  if (!calc_ir_raw (wet_L_, dry_, irL, ir_length_samples)) {
    std::cerr << "Error: calc_ir_raw failed for left channel.\n";
    return false;
  }

  // --- deconvolve right if we have input data ---
  const bool haveRightInput = !wet_R_.empty ();
  if (haveRightInput) {
    if (!calc_ir_raw (wet_R_, dry_, irR, ir_length_samples)) {
      std::cerr << "Error: calc_ir_raw failed for right channel.\n";
      return false;
    }
  }
  
  // TODO: figure out why this even works for L/R delay
  //align_stereo_independent (irL, irR, 0);
  
  /*if (!irL.empty() || !irR.empty()) {
    align_stereo_joint_no_chop (irL, irR, 0); // move reference peak to index 0
  }*/
  // --- normalize and trim (stereo-aware) ---
  normalize_and_trim_stereo (irL, irR);

  // Decide mono vs stereo based on whether right has any energy left
  bool rightHasEnergy = false;
  const float energyThresh = 1e-5f;
  for (float v : irR) {
    if (std::fabs (v) > energyThresh) {
      rightHasEnergy = true;
      break;
    }
  }
  
  if (!haveRightInput || !rightHasEnergy || irR.empty ()) {
    // MONO IR
    if (!write_mono_wav (out_filename, irL, samplerate_)) {
      std::cerr << "Error: failed to write mono IR to " << out_filename << "\n";
      return false;
    }
    std::cout << "Wrote MONO IR: " << out_filename
              << " (" << irL.size () << " samples @ " << samplerate_ << " Hz)\n";
    return true;
  }

  // STEREO IR
  size_t len = std::max (irL.size (), irR.size ());
  irL.resize (len, 0.0f);
  irR.resize (len, 0.0f);

  if (!write_stereo_wav (out_filename, irL, irR, samplerate_)) {
    std::cerr << "Error: failed to write stereo IR to " << out_filename << "\n";
    return false;
  }

  std::cout << "Wrote STEREO IR: " << out_filename
            << " (" << len << " samples @ " << samplerate_ << " Hz)\n";
  return true;
}

bool c_deconvolver::calc_ir_raw (const std::vector<float> &wet,
                                 const std::vector<float> &dry,
                                 std::vector<float>       &ir_raw,
                                 long                      ir_length_samples) {
  if (wet.empty () || dry.empty ()) {
    std::cerr << "Error: wet or dry buffer is empty.\n";
    return false;
  }

  // Use class member offsets.
  // For stereo, wet_offset_ is the "common" silence we decided to skip.
  size_t wet_offset = wet_offset_;
  size_t dry_offset = dry_offset_;
  size_t out_offset = 0;

  if (wet_offset >= wet.size ()) {
    std::cerr << "Warning: wet_offset (" << wet_offset
              << ") >= wet.size() (" << wet.size ()
              << "), clamping to 0.\n";
    wet_offset = 0;
  }
  if (dry_offset >= dry.size ()) {
    std::cerr << "Warning: dry_offset (" << dry_offset
              << ") >= dry.size() (" << dry.size ()
              << "), clamping to 0.\n";
    dry_offset = 0;
  }

  const size_t usable_wet = wet.size () - wet_offset;
  const size_t usable_dry = dry.size () - dry_offset;

  if (usable_wet == 0 || usable_dry == 0) {
    std::cerr << "Error: usable_wet or usable_dry is zero after offsets.\n";
    return false;
  }

  const size_t maxLen = std::max (usable_wet, usable_dry);
  const size_t N      = next_pow2 (maxLen);
  const size_t nFreq  = N / 2 + 1;

  std::cerr << "Deconvolution: FFT size " << N
            << " samples (usable wet=" << usable_wet
            << ", usable dry=" << usable_dry << ")\n";

  // Time-domain buffers (double for FFTW)
  std::vector<double> xTime (N, 0.0);
  std::vector<double> yTime (N, 0.0);

  // Copy from dry_/wet_ starting at their logical offsets
  for (size_t i = 0; i < usable_dry; ++i) {
    if (i >= N) break;
    xTime [i] = dry [dry_offset + i];
  }
  for (size_t i = 0; i < usable_wet; ++i) {
    if (i >= N) break;
    yTime [i] = wet [wet_offset + i];
  }

  // Frequency-domain buffers
  fftw_complex* X = (fftw_complex*) fftw_malloc (sizeof (fftw_complex) * nFreq);
  fftw_complex* Y = (fftw_complex*) fftw_malloc (sizeof (fftw_complex) * nFreq);
  fftw_complex* H = (fftw_complex*) fftw_malloc (sizeof (fftw_complex) * nFreq);
  if (!X || !Y || !H) {
    std::cerr << "Error: FFTW malloc failed.\n";
    if (X) fftw_free (X);
    if (Y) fftw_free (Y);
    if (H) fftw_free (H);
    return false;
  }

  std::vector<double> hTime (N, 0.0);

  // Plans
  fftw_plan planX   = fftw_plan_dft_r2c_1d ((int) N, xTime.data (), X, FFTW_ESTIMATE);
  fftw_plan planY   = fftw_plan_dft_r2c_1d ((int) N, yTime.data (), Y, FFTW_ESTIMATE);
  fftw_plan planInv = fftw_plan_dft_c2r_1d ((int) N, H, hTime.data (), FFTW_ESTIMATE);

  if (!planX || !planY || !planInv) {
    std::cerr << "Error: FFTW plan creation failed.\n";
    if (planX) fftw_destroy_plan (planX);
    if (planY) fftw_destroy_plan (planY);
    if (planInv) fftw_destroy_plan (planInv);
    fftw_free (X);
    fftw_free (Y);
    fftw_free (H);
    return false;
  }

  // Forward FFTs
  fftw_execute (planX);
  fftw_execute (planY);

  // H = Y * conj(X) / (|X|^2 + eps)
  double maxMag2 = 0.0;
  for (size_t k = 0; k < nFreq; ++k) {
    double xr = X [k] [0];
    double xi = X [k] [1];
    double mag2 = xr * xr + xi * xi;
    if (mag2 > maxMag2) maxMag2 = mag2;
  }
  //double eps = (maxMag2 > 0.0) ? maxMag2 * 1e-8 : 1e-12;
  double reg = (maxMag2 > 0.0) ? maxMag2 * 1e-4 : 1e-8;

  for (size_t k = 0; k < nFreq; ++k) {
    double xr = X [k] [0];
    double xi = X [k] [1];
    double yr = Y [k] [0];
    double yi = Y [k] [1];

    //double mag2 = xr * xr + xi * xi + eps;
    double mag2 = xr * xr + xi * xi + reg;

    // conj(X)
    double cr = xr;
    double ci = -xi;

    // Y * conj(X)
    double nr = yr * cr - yi * ci;
    double ni = yr * ci + yi * cr;

    H [k] [0] = nr / mag2;
    H [k] [1] = ni / mag2;
  }

  // Back to time domain
  fftw_execute (planInv);

  // Normalize FFTW inverse by N
  for (size_t i = 0; i < N; ++i) {
    hTime [i] /= (double) N;
  }

  // Initial IR window length (pre-normalization, pre-trim)
  // Limit by FFT size AND usable sweep lengths (after offsets)
  // Initial IR window length (pre-headroom)
  size_t irLen = N;

  if (usable_dry < irLen) irLen = usable_dry;
  if (usable_wet < irLen) irLen = usable_wet;

  if (ir_length_samples > 0 && (size_t) ir_length_samples < irLen) {
    irLen = (size_t) ir_length_samples;
  }

  if (irLen == 0) {
    std::cerr << "Error: computed IR length is zero.\n";
    fftw_destroy_plan (planX);
    fftw_destroy_plan (planY);
    fftw_destroy_plan (planInv);
    fftw_free (X);
    fftw_free (Y);
    fftw_free (H);
    return false;
  }

  // Convert headroom_seconds to samples
  if (prefs_ && prefs_->headroom_seconds > 0.0f) {
    out_offset = (size_t) std::llround (
        (double) prefs_->headroom_seconds * (double) samplerate_);
  }

  ir_raw.resize (irLen + out_offset);

  // headroom: leading zeros
  for (size_t i = 0; i < out_offset; ++i) {
    ir_raw[i] = 0.0f;
  }

  // copy IR starting at out_offset
  for (size_t i = 0; i < irLen; ++i) {
    ir_raw[i + out_offset] = (float) hTime[i];
  }

  // Cleanup
  fftw_destroy_plan (planX);
  fftw_destroy_plan (planY);
  fftw_destroy_plan (planInv);
  fftw_free (X);
  fftw_free (Y);
  fftw_free (H);

  return true;
}

bool c_deconvolver::set_samplerate_if_needed (int sr) {
  if (samplerate_ == 0) {
    samplerate_ = sr;
    return true;
  }
  return (samplerate_ == sr);
}

void c_deconvolver::normalize_and_trim_stereo (std::vector<float> &L,
                                               std::vector<float> &R,
                                               bool trim_end,
                                               bool trim_start) {
  const size_t tail_pad       = 32;
  //const float  silence_thresh = 1e-5f;  // relative to peak (~ -100 dB)
  const bool hasR = !R.empty ();

  // avoid edge case where we could wipe the whole buffer
  if (L.empty() && !hasR) return;
  
  // 1) Find global peak across both channels
  float peak = 0.0f;
  for (float v : L) peak = std::max (peak, std::fabs (v));
  if (hasR) {
    for (float v : R) peak = std::max (peak, std::fabs (v));
  }

  if (peak < 1e-12f) {
    // all silence (or numerical dust) → nothing to normalize
    L.assign (1, 0.0f);
    if (hasR) R.assign (1, 0.0f);
    return;
  }

  const float norm = 0.99f / peak;

  // 2) Apply same gain to both channels
  for (float &v : L) v *= norm;
  if (hasR) {
    for (float &v : R) v *= norm;
  }
  
  size_t len = 0;
  
  // 3) Remove common leading silence: trim by *earliest* non-silent sample
  if (trim_start) {
    len = L.size ();
    if (hasR) len = std::min (L.size (), R.size ());

    size_t firstNonSilent = len;  // assume all silent
    for (size_t i = 0; i < len; ++i) {
      float aL = std::fabs (L [i]);
      float aR = hasR ? std::fabs (R [i]) : 0.0f;
      if (std::max (aL, aR) > prefs_->silence_thresh) {
        firstNonSilent = i;
        break;
      }
    }

    if (firstNonSilent == len) {
      // everything below threshold even after norm → just keep 1 sample of silence
      L.assign (1, 0.0f);
      if (hasR) R.assign (1, 0.0f);
      return;
    }

    if (firstNonSilent > 0) {
      shift_ir_left (L, firstNonSilent);
      if (hasR) shift_ir_left (R, firstNonSilent);
    }
  }

  // 4) Compute trim position at the tail based on max (|L|, |R|)
  if (trim_end) {
    len = L.size ();
    if (hasR) len = std::min (L.size (), R.size ());

    ssize_t lastNonSilent = -1;
    for (ssize_t i = (ssize_t) len - 1; i >= 0; --i) {
      float aL = std::fabs (L [(size_t) i]);
      float aR = hasR ? std::fabs (R [(size_t) i]) : 0.0f;
      if (std::max (aL, aR) > prefs_->silence_thresh) {
        lastNonSilent = i;
        break;
      }
    }

    size_t newLen;
    if (lastNonSilent < 0) {
      newLen = 1; // all silent after normalization/leading trim
    } else {
      newLen = (size_t) lastNonSilent + 1 + tail_pad;
    }

    if (newLen > L.size ()) newLen = L.size ();
    if (hasR && newLen > R.size ()) newLen = R.size ();

    if (newLen == 0) newLen = 1; // paranoia

    L.resize (newLen);
    if (hasR) R.resize (newLen);
  }
}

static int jack_process_cb (jack_nframes_t nframes, void *arg) {
  s_jackclient *j = (s_jackclient *) arg;

  // Playback
  if (j->play_go) {
    jack_default_audio_sample_t *outL =
      (jack_default_audio_sample_t *) jack_port_get_buffer (j->outL, nframes);
    jack_default_audio_sample_t *outR =
      (jack_default_audio_sample_t *) jack_port_get_buffer (j->outR, nframes);

    for (jack_nframes_t i = 0; i < nframes; ++i) {
      float v = 0.0f;
      if (j->index < j->sig_out.size ()) {
        v = j->sig_out [j->index++];
      }
      outL [i] = v;
      outR [i] = v; // same sweep on both channels
    }
  }

  // Recording
  if (j->rec_go && j->inL && j->rec_index < j->rec_total) {
    jack_default_audio_sample_t *inL =
      (jack_default_audio_sample_t *) jack_port_get_buffer (j->inL, nframes);
    jack_default_audio_sample_t *inR = nullptr;
    if (j->inR) {
      inR = (jack_default_audio_sample_t *) jack_port_get_buffer (j->inR, nframes);
    }

    for (jack_nframes_t i = 0; i < nframes && j->rec_index < j->rec_total; ++i) {
      float vL = inL[i];
      float vR = inR ? inR[i] : vL;
      float m  = 0.5f * (vL + vR); // simple mono mix
      j->sig_in[j->rec_index++] = m;
    }

    if (j->rec_index >= j->rec_total) {
      j->rec_done = true;
      j->rec_go   = false;
    }
  }

  return 0;
}

bool c_deconvolver::init_jack (std::string clientname,
                               sig_channels chan_in,
                               sig_channels chan_out)
{
  if (jack_inited && jackclient.client) {
    // already have a client; nothing to do
    return true;
  }

  jack_status_t status = JackFailure;

  // Decide on a client name:
  const char *name = nullptr;
  if (!clientname.empty()) {
    // explicit from caller (e.g. formatted with argv[0])
    name = clientname.c_str();
  } else if (prefs_ && !prefs_->jack_name.empty()) {
    // from prefs (may literally be "%s_ir_sweep" if not formatted)
    name = prefs_->jack_name.c_str();
  } else {
    // last-chance default
    name = "deconvolver";
  }

  jackclient.client = jack_client_open(name, JackNullOption, &status);
  if (!jackclient.client) {
    std::cerr << "Error: cannot connect to JACK (client name \""
              << name << "\")\n";
    jack_inited = false;
    return false;
  }

  // JACK is alive, get its sample rate
  int jack_sr = (int) jack_get_sample_rate(jackclient.client);

  if (prefs_) {
    if (!prefs_->quiet && prefs_->sweep_sr != jack_sr) {
        std::cerr << "Note: overriding sample rate (" << prefs_->sweep_sr
                  << " Hz) with JACK sample rate " << jack_sr << " Hz.\n";
    }
    prefs_->sweep_sr = jack_sr;
  }

  // Reset runtime state
  jackclient.play_go = false;
  jackclient.rec_go  = false;
  jackclient.index   = 0;
  jackclient.sig_in.clear();
  jackclient.sig_out.clear();
  jackclient.inL = jackclient.inR = nullptr;
  jackclient.outL = jackclient.outR = nullptr;

  jack_inited = true;

  // For now we ignore chan_in / chan_out; when you add recording
  // you can use those to register in/out ports here instead of in jack_play/jack_rec.
  (void)chan_in;
  (void)chan_out;

  return true;
}

bool c_deconvolver::jack_playrec_sweep (const std::vector<float> &sweep,
                                        int samplerate,
                                        const char *jack_out_port,
                                        const char *jack_in_port,
                                        std::vector<float> &captured)
{
  if (sweep.empty()) {
    std::cerr << "Sweep buffer is empty.\n";
    return false;
  }

  // 1) Ensure JACK client
  if (!jack_inited || !jackclient.client) {
    if (!init_jack(std::string(), chn_stereo, chn_stereo)) {
      return false;
    }
  }

  // 2) Prepare state
  jackclient.sig_out  = sweep;
  jackclient.index    = 0;
  jackclient.play_go  = false;

  // capture same length + a bit of extra tail
  const size_t extra_tail = (size_t) (0.5 * samplerate); // 0.5s
  jackclient.rec_total = sweep.size() + extra_tail;
  jackclient.sig_in.assign(jackclient.rec_total, 0.0f);
  jackclient.rec_index = 0;
  jackclient.rec_done  = false;
  jackclient.rec_go    = false;

  // 3) Register ports
  jackclient.outL = jack_port_register (jackclient.client, "out_L",
                            JACK_DEFAULT_AUDIO_TYPE,
                            JackPortIsOutput, 0);
  jackclient.outR = jack_port_register (jackclient.client, "out_R",
                            JACK_DEFAULT_AUDIO_TYPE,
                            JackPortIsOutput, 0);
  jackclient.inL  = jack_port_register (jackclient.client, "in_L",
                            JACK_DEFAULT_AUDIO_TYPE,
                            JackPortIsInput, 0);
  jackclient.inR  = jack_port_register (jackclient.client, "in_R",
                            JACK_DEFAULT_AUDIO_TYPE,
                            JackPortIsInput, 0);

  if (!jackclient.outL || !jackclient.outR || !jackclient.inL) {
    std::cerr << "Error: cannot register JACK ports.\n";
    jack_deactivate (jackclient.client);
    jack_client_close (jackclient.client);
    jackclient.client  = nullptr;
    jack_inited        = false;
    return false;
  }

  // 4) Set callback
  jack_set_process_callback (jackclient.client, jack_process_cb, &jackclient);

  /*jack_nframes_t jack_sr = jack_get_sample_rate (jackclient.client);
  if (jack_sr != (jack_nframes_t)samplerate) {
    std::cerr << "Warning: JACK sample rate (" << jack_sr
              << ") != sweep sample rate (" << samplerate
              << ")\nUsing JACK sample rate for playback/record.\n";
  }
  prefs_->sweep_sr = (int) jack_sr;*/

  // 5) Activate client
  if (jack_activate (jackclient.client) != 0) {
    std::cerr << "Error: cannot activate JACK client.\n";
    jack_client_close (jackclient.client);
    jackclient.client = nullptr;
    jack_inited       = false;
    return false;
  }

  // 6) Connect ports

  // Output connection: use jack_out_port if provided,
  // otherwise auto-connect to physical playback
  if (jack_out_port && jack_out_port[0] != '\0') {
    int errL = jack_connect (jackclient.client,
                             jack_port_name (jackclient.outL),
                             jack_out_port);
    int errR = jack_connect (jackclient.client,
                             jack_port_name (jackclient.outR),
                             jack_out_port);
    if (errL != 0 || errR != 0) {
      std::cerr << "Warning: failed to connect sweep to " << jack_out_port
                << " (errL=" << errL << ", errR=" << errR << ")\n";
    }
  } else {
    const char **ports = jack_get_ports (jackclient.client, nullptr, nullptr,
                                         JackPortIsPhysical | JackPortIsInput);
    if (ports) {
      if (ports[0])
        jack_connect (jackclient.client,
                      jack_port_name (jackclient.outL), ports[0]);
      if (ports[1])
        jack_connect (jackclient.client,
                      jack_port_name (jackclient.outR), ports[1]);
      jack_free (ports);
    }
  }

  // Input connection: connect in_L from jack_in_port
  if (jack_in_port && jack_in_port[0] != '\0') {
    int err = jack_connect (jackclient.client,
                            jack_in_port,
                            jack_port_name (jackclient.inL));
    if (err != 0) {
      std::cerr << "Warning: failed to connect input from "
                << jack_in_port << " (err=" << err << ")\n";
    }
  } else {
    // optional: auto-connect from first physical capture
    const char **ports = jack_get_ports (jackclient.client, nullptr, nullptr,
                                         JackPortIsPhysical | JackPortIsOutput);
    if (ports && ports[0]) {
      jack_connect (jackclient.client,
                    ports[0],
                    jack_port_name (jackclient.inL));
                    
    }
    if (ports) jack_free (ports);
  }

  std::cout << "Playing + recording sweep via JACK... " << std::flush;
  jackclient.play_go = true;
  jackclient.rec_go  = true;

  // 7) Block until done
  while ((jackclient.index < jackclient.sig_out.size ()) ||
         (!jackclient.rec_done)) {
    usleep (10 * 1000); // 10 ms
  }

  std::cout << "done.\n";

  jack_deactivate (jackclient.client);
  jack_client_close (jackclient.client);
  jackclient.client = nullptr;
  jack_inited       = false;

  // Copy captured mono buffer out (you could trim it here if desired)
  captured = jackclient.sig_in;

  return true;
}


bool c_deconvolver::jack_rec (std::vector<float> &sig,
                               int samplerate,
                               const char *jack_port) {
  return false;
}

bool c_deconvolver::jack_play (std::vector<float> &sig,
                               int samplerate,
                               const char *jack_port)
{
  // Ensure we have a JACK client
  if (!jack_inited || !jackclient.client) {
    if (!init_jack(std::string(), chn_none, chn_stereo)) {
      return false;
    }
  }

  // Prepare playback state
  jackclient.sig_out = sig;
  jackclient.index   = 0;
  jackclient.play_go = false;  // will flip to true just before we start

  // Register output ports (stereo sweep)
  jackclient.outL = jack_port_register (jackclient.client, "out_L",
                                        JACK_DEFAULT_AUDIO_TYPE,
                                        JackPortIsOutput, 0);
  jackclient.outR = jack_port_register (jackclient.client, "out_R",
                                        JACK_DEFAULT_AUDIO_TYPE,
                                        JackPortIsOutput, 0);

  if (!jackclient.outL || !jackclient.outR) {
    std::cerr << "Error: cannot register JACK output ports.\n";
    jack_client_close (jackclient.client);
    jackclient.client = nullptr;
    jack_inited = false;
    return false;
  }

  // Process callback (currently playback-only; later you can add rec handling)
  jack_set_process_callback (jackclient.client, jack_process_cb, &jackclient);

  /*jack_nframes_t jack_sr = jack_get_sample_rate (jackclient.client);
  if (jack_sr != (jack_nframes_t)samplerate) {
    std::cerr << "Warning: JACK sample rate (" << jack_sr
              << ") != sweep sample rate (" << samplerate
              << ")\nUsing JACK sample rate for playback.\n";
  }*/

  if (jack_activate (jackclient.client) != 0) {
    std::cerr << "Error: cannot activate JACK client.\n";
    jack_client_close (jackclient.client);
    jackclient.client = nullptr;
    jack_inited = false;
    return false;
  }

  // Connect outputs either to the requested JACK port or to
  // physical playback ports if none given.
  if (jack_port && jack_port[0] != '\0') {
    int errL = jack_connect (jackclient.client,
                             jack_port_name (jackclient.outL),
                             jack_port);
    // Only one port if requested, keep R free for now
    int errR = 0; // or connect elsewhere later if you want

    if (errL != 0) {
      std::cerr << "Warning: failed to connect L to " << jack_port
                << " (err=" << errL << ")\n";
    }
    if (errR != 0) {
      std::cerr << "Warning: failed to connect R to " << jack_port
                << " (err=" << errR << ")\n";
    }
  } else {
    // Auto-connect to physical playback
    const char **ports = jack_get_ports (jackclient.client, nullptr, nullptr,
                                         JackPortIsPhysical | JackPortIsInput);
    if (ports) {
      if (ports[0]) {
        jack_connect (jackclient.client,
                      jack_port_name (jackclient.outL), ports[0]);
      }
      if (ports[1]) {
        jack_connect (jackclient.client,
                      jack_port_name (jackclient.outR), ports[1]);
      }
      jack_free (ports);
    }
  }

  std::cout << "Playing sweep via JACK... " << std::flush;
  jackclient.play_go = true;

  // Crude blocking wait until the sweep has been fully sent
  while (jackclient.index < jackclient.sig_out.size ()) {
    usleep (10 * 1000); // 10 ms
  }

  std::cout << "done.\n";

  jack_deactivate (jackclient.client);
  jack_client_close (jackclient.client);
  jackclient.client = nullptr;
  jack_inited = false;

  return true;
}

static void print_usage (const char *prog) {
  std::cerr <<
    "DIRT - Delt's Impulse Response Tool, version " << DIRT_VERSION << "\n\n"
    "Usage:\n"
    "  " << prog << " [options] dry.wav wet.wav out.wav\n"
    "  " << prog << " [options] -d dry.wav -w wet.wav -o out.wav\n"
    "\n"
    "Deconvolver options:\n"
    "  -h, --help               Show this help/usage text and version\n"
    "  -V, --version            Same as --help\n"
    "  -d, --dry FILE           Dry sweep WAV file\n"
    "  -w, --wet FILE           Recorded (wet) sweep WAV file\n"
    "  -o, --out FILE           Output IR WAV file\n"
    "  -n, --len N              IR length in samples (default 0 = auto)\n"
    "  -q, --quiet              Less verbose output\n"
    "  -v, --verbose            More verbose output\n"
    "  -t, --thresh dB          Threshold in dB for silence detection\n"
    "                           (negative, default " << DEFAULT_SILENCE_THRESH_DB << " dB)\n"
    "Sweep generator options:\n"
    "  -s, --makesweep SEC      Generate sweep WAV instead of deconvolving\n"
    "  -S, --playsweep SEC      Play sweep via JACK instead of deconvolving\n"
    "  -R, --sweep-sr SR        Sweep samplerate (default 48000)\n"
    "  -a, --sweep-amplitude dB Sweep amplitude (default -1dB)\n" // DONE
    "  -X, --sweep-f1 F         Sweep start frequency (default 20)\n"
    "  -Y, --sweep-f2 F         Sweep end frequency (default 20000)\n"
    "  -p, --preroll SEC        Prepend leading silence of SEC seconds\n"
    "  -m, --marker SEC         Prepend alignment marker of SEC seconds\n"
    "  -W, --wait               Wait for input before playing sweep\n"
    "  -j, --jack-port PORT     Connect to this JACK port\n"
    "  -J, --jack-name          Connect to JACK with this client name\n"
    "\n"
    "Example usage:\n"
    "\n"
    "  # Generate a 30-second dry sweep with 1 second of silence at start\n"
    "  # and an alignment marker of 0.1 second:\n"
    "  " << prog << " --makesweep 30 --preroll 1 --marker 0.1 -o drysweep.wav\n"
    "\n"
    "  # Deconvolve a wet/recorded sweep against a dry one, and output an\n"
    "  # impulse response: (tries to detect preroll and marker from dry file)\n"
    "  " << prog << " -d drysweep.wav -w wetsweep.wav -o ir.wav\n"
    "\n"
    "  # Play a sweep through an instance of Carla and record it at the same\n"
    "  # time, then extract IR directly to \"ir.wav\":\n"
    "  " << prog << " -d Carla:audio-in1 -w Carla:audio-out1 -o ir.wav"
    "\n\n"
    ;
}

// returns bit-field of paths that were provided by user, or -1 on error
// bit 1: dry, bit 2: wet, bit 3: out
int parse_args (int argc, char **argv, s_prefs &opt) {
  const int ret_err = -1;

  if (argc < 2) {
    //print_usage (argv [0]);
    return ret_err;
  }
  
  // First pass: parse flags 
  std::vector<const char *> positionals;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv [i];

    if (arg == "-h" || arg == "--help" || arg == "-V" || arg == "--version") {
      print_usage (argv [0]);
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
    } else if (arg == "-t" || arg == "--thresh") {
      if (++i >= argc) {
        std::cerr << "Missing value for " << arg << "\n";
        return ret_err;
      }
      double db = std::atof (argv[i]);
      // Allow -200 to 0 dB ...should we tolerate positive by flipping sign? (see db_to_linear)
      if (db < -200.0 || db > 0.0) {
        std::cerr << "Invalid threshold dB: "
                  << argv[i] << " (must be between -200 and 0)\n";
        return ret_err;
      }
      opt.silence_thresh = (float) db_to_linear (db);
    } else if (arg == "-q" || arg == "--quiet") {
      opt.quiet = true;
    } else if (arg == "-v" || arg == "--verbose") {
      opt.verbose = true;
    } else if (arg == "-W" || arg == "--wait") {
      opt.sweepwait = true;
    } else if (arg == "-s" || arg == "--makesweep") {
      if (++i >= argc) { std::cerr << "Missing value for " << arg << "\n"; return ret_err; }
      opt.mode = deconv_mode::mode_makesweep;
      opt.sweep_seconds = std::atof (argv [i]);
    } else if (arg == "-S" || arg == "--playsweep") {
      if (++i >= argc) { std::cerr << "Missing value for " << arg << "\n"; return ret_err; }
      opt.mode = deconv_mode::mode_playsweep;
      opt.sweep_seconds = std::atof (argv [i]);
    } else if (arg == "-R" || arg == "--sweep-sr") {
      if (++i >= argc) { std::cerr << "Missing value for " << arg << "\n"; return ret_err; }
      opt.sweep_sr = std::atoi (argv [i]);
    } else if (arg == "-X" || arg == "--sweep-f1") {
      if (++i >= argc) { std::cerr << "Missing value for " << arg << "\n"; return ret_err; }
      opt.sweep_f1 = std::atof (argv [i]);
    } else if (arg == "-y" || arg == "--sweep-f2") {
      if (++i >= argc) { std::cerr << "Missing value for " << arg << "\n"; return ret_err; }
      opt.sweep_f2 = std::atof (argv [i]);
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
        if (opt.preroll_seconds == 0.0)
          opt.preroll_seconds = 1.0;
        if (opt.marker_gap_seconds == 0.0)
          opt.marker_gap_seconds = 1.0;
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
            std::cerr << "Marker length cannot be negative.\n";
            return ret_err;
        }
    } else if (arg == "-J" || arg == "--jack-name") {
      if (++i >= argc) { std::cerr << "Missing value for " << arg << "\n"; return ret_err; }
      opt.jack_name = argv [i];
    } else if (arg == "-j" || arg == "--jack-port") {
      if (++i >= argc) { std::cerr << "Missing value for " << arg << "\n"; return ret_err; }
      opt.jack_portname = argv [i];
    } else if (!arg.empty () && arg [0] == '-') {
      std::cerr << "Unknown option: " << arg << "\n";
      return ret_err;
    } else {
      // positional
      positionals.push_back (argv [i]);
    }
  }

  // If not provided by flags, use positionals:
  //   dry wet out [len]
  if (opt.dry_path.empty () && positionals.size () >= 1) {
    opt.dry_path = positionals [0];
  }
  if (opt.wet_path.empty () && positionals.size () >= 2) {
    opt.wet_path = positionals [1];
  }
  if (opt.out_path.empty () && positionals.size () >= 3) {
    opt.out_path = positionals [2];
  }
  if (opt.ir_length_samples == 0 && positionals.size () >= 4) {
    opt.ir_length_samples = std::strtol (positionals [3], nullptr, 10);
  }
  
  int bf = 0;
  if (!opt.dry_path.empty ())           bf |= 1;
  if (!opt.wet_path.empty ())           bf |= 2;
  if (!opt.out_path.empty ())           bf |= 4;
  
  // If user explicitly chose makesweep or playsweep, don't override mode.
  if (opt.mode == deconv_mode::mode_makesweep ||
      opt.mode == deconv_mode::mode_playsweep) {
    // You can still return bf as “what paths were supplied”,
    // but don't touch opt.mode here.
    return bf;
  }

  switch (bf) {
    /*case 1: // dry only - should we treat this as valid? probably not
      opt.mode = deconv_mode::makesweep;
    break;*/
    
    case 6: // wet, out
    case 7: // dry, wet, out
      opt.mode = deconv_mode::mode_deconvolve;
    break;
    
    default:
      opt.mode = deconv_mode::mode_error;
      return ret_err;
    break;
  }

  return bf;
}

static bool resolve_sources (s_prefs &opt) {
  opt.early_jack_init = false;

  // playsweep: we *do* care about JACK early init (for SR),
  // but not about dry/wet.
  if (opt.mode == deconv_mode::mode_playsweep) {
    opt.early_jack_init = true;   // we know we need JACK out
    return true;
  }

  // makesweep: pure file, no JACK needed
  if (opt.mode == deconv_mode::mode_makesweep) {
    return true;
  }

  // --- deconvolution below ---
  // DRY
  if (opt.dry_path.empty()) {
    opt.dry_source = src_generate;
  } else if (file_exists(opt.dry_path)) {
    opt.dry_source = src_file;
  } else if (looks_like_jack_port(opt.dry_path)) {
    opt.dry_source     = src_jack;
    opt.early_jack_init = true;
  } else {
    std::cerr << "Dry source '" << opt.dry_path
              << "' is neither an existing file nor a JACK port.\n";
    return false;
  }

  // WET
  if (opt.wet_path.empty()) {
    std::cerr << "Wet source (-w) is required in deconvolution mode.\n";
    return false;
  } else if (file_exists(opt.wet_path)) {
    opt.wet_source = src_file;
  } else if (looks_like_jack_port(opt.wet_path)) {
    opt.wet_source     = src_jack;
    opt.early_jack_init = true;
  } else {
    std::cerr << "Wet source '" << opt.wet_path
              << "' is neither an existing file nor a JACK port.\n";
    return false;
  }

  return true;
}

int main (int argc, char **argv) {
  s_prefs p;

  int source_bf = parse_args(argc, argv, p);
  if (source_bf == -1 || p.mode == deconv_mode::mode_error) {
    print_usage(argv[0]);
    return 1;
  }

  if (!resolve_sources(p)) {
    return 1;
  }

  c_deconvolver dec(&p);
  if (p.early_jack_init) {
    char realjackname [256] = { 0 };
    snprintf (realjackname, 255, p.jack_name.c_str (), argv [0]);
    dec.init_jack (std::string (realjackname),
               (p.wet_source == src_jack ? chn_mono : chn_none),
               (p.dry_source == src_jack || p.mode == deconv_mode::mode_playsweep ?
                chn_mono : chn_none));

  }

  // --- MODE: makesweep ---
  if (p.mode == deconv_mode::mode_makesweep) {
    std::vector<float> sweep;
    generate_log_sweep(p.sweep_seconds,
                       p.preroll_seconds,
                       p.marker_seconds,
                       p.marker_gap_seconds,
                       p.sweep_sr,
                       p.sweep_amp_db,
                       p.sweep_f1,
                       p.sweep_f2,
                       sweep);
    if (!write_mono_wav(p.out_path.c_str(), sweep, p.sweep_sr)) {
      return 1;
    }
    if (!p.quiet) {
      std::cerr << "Wrote sweep: " << p.out_path
                << " (" << sweep.size() << " samples @ "
                << p.sweep_sr << " Hz)\n";
    }
    return 0;
  }

  // --- MODE: playsweep ---
  if (p.mode == deconv_mode::mode_playsweep) {
    std::vector<float> sweep;
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

    return dec.jack_play(sweep, p.sweep_sr, p.jack_portname.c_str());
  }
  
  // "live" / full-duplex mode: generate sweep, play it and record the 
  // result at the same time
  if (p.dry_source == src_jack && p.wet_source == src_jack) {

      std::vector<float> sweep;
      generate_log_sweep(p.sweep_seconds,
                         p.preroll_seconds,
                         p.marker_seconds,
                         p.marker_gap_seconds,
                         p.sweep_sr,
                         p.sweep_amp_db,
                         p.sweep_f1,
                         p.sweep_f2,
                         sweep);

      std::vector<float> wet;
      if (!dec.jack_playrec_sweep(sweep,
                                  p.sweep_sr,
                                  p.dry_path.c_str(),
                                  p.wet_path.c_str(),
                                  wet)) {
          return 1;
      }
    
    // detect & compensate for offset in recorded sweep
    if (!dec.set_dry_from_buffer(sweep, p.sweep_sr)) return 1;
    if (!dec.set_wet_from_buffer(wet, std::vector<float>(), p.sweep_sr)) return 1;

    if (!dec.output_ir(p.out_path.c_str(), p.ir_length_samples)) return 1;
    return 0;
  }
  
  if (p.dry_source == src_file) {
      if (!dec.load_sweep_dry(p.dry_path.c_str())) return 1;
  } else if (p.dry_source == src_generate) {
      std::vector<float> sweep;
      generate_log_sweep(p.sweep_seconds,
                         p.preroll_seconds,
                         p.marker_seconds,
                         p.marker_gap_seconds,
                         p.sweep_sr,
                         p.sweep_amp_db,
                         p.sweep_f1,
                         p.sweep_f2,
                         sweep);
      if (!dec.set_dry_from_buffer(sweep, p.sweep_sr)) return 1;
  } else {
      std::cerr << "Error: Dry JACK only supported when wet is JACK.\n";
      return 1;
  }

  if (p.wet_source == src_file) {
      if (!dec.load_sweep_wet(p.wet_path.c_str())) return 1;
  } else {
      std::cerr << "Error: Wet JACK only supported when dry is JACK.\n";
      return 1;
  }

  if (!dec.output_ir(p.out_path.c_str(), p.ir_length_samples))
    return 1;
  
  return 0;
}
