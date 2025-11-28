
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

#ifdef DEBUG
#define CMDLINE_IMPLEMENTATION // This should only be in ONE implementation file!!
#define CMDLINE_DEBUG
#include "cmdline/cmdline.h"
#define debug(...) cmdline_debug(stderr,ANSI_RED,__FILE__,__LINE__,__FUNC__,__VA_ARGS__)
#else
#define debug(...)
#endif

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
  
  // RNG math from:
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

static bool file_exists (const std::string &path) {
  struct stat st{};
  return (::stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode));
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

static size_t find_first_nonsilent (const std::vector<float> &buf,
                                   float silence_thresh_db) {
  float silence_thresh = db_to_linear (silence_thresh_db);
  debug ("silence_thresh db=%f lin=%f", silence_thresh_db, silence_thresh);
  
  for (size_t i = 0; i < buf.size (); i++) {
    if (std::fabs (buf [i]) > silence_thresh) return i;
  }
  return buf.size ();
}

static size_t find_last_nonsilent (const std::vector<float> &buf,
                                   float silence_thresh_db) {
  float silence_thresh = db_to_linear (silence_thresh_db);
  debug ("silence_thresh db=%f lin=%f", silence_thresh_db, silence_thresh);
  
  //for (size_t i = 0; i < buf.size (); ++i) {
  for (ssize_t i = (ssize_t) buf.size () - 1; i >= 0; --i) {
    if (std::fabs (buf [i]) > silence_thresh) return (size_t) i;
  }
  return buf.size ();
}

void lowpass_ir (std::vector<float> &irf, double samplerate, double cutoff_hz) {
    if (irf.empty()) return;

    const size_t N = irf.size();
    const double bin_hz = samplerate / double(N);

    // FFT bin corresponding to cutoff frequency
    int bin_cutoff = int(cutoff_hz / bin_hz);

    // r2c gives N/2 + 1 complex bins (0 .. N/2)
    const int max_bin = int (N / 2);

    // Clamp cutoff to valid range
    if (bin_cutoff < 0)        bin_cutoff = 0;
    if (bin_cutoff > max_bin)  bin_cutoff = max_bin;

    // Allocate FFTW buffers
    std::vector<double> temp (N);
    for (size_t i = 0; i < N; ++i)
        temp[i] = irf[i];

    fftw_complex *freq = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * (max_bin + 1));
    double       *time = (double*)       fftw_malloc(sizeof(double) * N);

    memcpy(time, temp.data(), N * sizeof(double));

    fftw_plan fwd = fftw_plan_dft_r2c_1d(int(N), time, freq, FFTW_ESTIMATE);
    fftw_plan inv = fftw_plan_dft_c2r_1d(int(N), freq, time, FFTW_ESTIMATE);

    // Forward FFT
    fftw_execute(fwd);

    //
    // Smooth spectral taper: fade out above cutoff_hz,
    // reducing ringing compared to brick-wall zeroing
    //

    // Fade width ~15% of cutoff frequency (in bins)
    int fade_width = int((0.15 * cutoff_hz) / bin_hz);
    if (fade_width < 1) fade_width = 1;

    // Fade band starts below cutoff
    int bin_fade = bin_cutoff - fade_width;
    if (bin_fade < 0) {
        bin_fade = 0;
        fade_width = bin_cutoff; // compress fade if cutoff small
    }

    for (int i = 0; i <= max_bin; ++i) {
        if (i <= bin_fade) {
            // Pass through unchanged
            continue;
        }
        else if (i < bin_cutoff) {
            // Hann-style taper: 1 → 0 over the fade region
            double t = double(i - bin_fade) / double(bin_cutoff - bin_fade);
            double w = 0.5 * (1.0 + std::cos(M_PI * t));
            freq[i][0] *= w;
            freq[i][1] *= w;
        }
        else {
            // Kill above cutoff
            freq[i][0] = 0.0;
            freq[i][1] = 0.0;
        }
    }

    // Inverse FFT
    fftw_execute(inv);

    // Normalize (FFTW inverse is unnormalized)
    const double invN = 1.0 / double(N);
    for (size_t i = 0; i < N; ++i)
        irf[i] = float(time[i] * invN);

    fftw_destroy_plan(fwd);
    fftw_destroy_plan(inv);
    fftw_free(freq);
    fftw_free(time);
}

void highpass_ir (std::vector<float> &irf, double samplerate, double cutoff)
{
    if (irf.empty ()) return;

    size_t N = irf.size ();
    double bin_hz = samplerate / double (N);

    // map cutoff frequency to bin index
    int bin_cutoff = int (cutoff / bin_hz);

    // FFTW r2c: N/2 + 1 complex bins: 0 .. N/2
    int max_bin = int (N / 2);

    // clamp cutoff to valid range
    if (bin_cutoff < 0)        bin_cutoff = 0;
    if (bin_cutoff > max_bin)  bin_cutoff = max_bin;

    // allocate double buffer for FFTW
    std::vector<double> ir (N);
    for (size_t i = 0; i < N; ++i)
      ir [i] = irf [i];

    fftw_complex *freq = (fftw_complex*) fftw_malloc (sizeof (fftw_complex) * (max_bin + 1));
    double *time       = (double*)       fftw_malloc (sizeof (double) * N);

    memcpy (time, ir.data (), N * sizeof (double));

    fftw_plan fwd = fftw_plan_dft_r2c_1d (int (N), time, freq, FFTW_ESTIMATE);
    fftw_plan inv = fftw_plan_dft_c2r_1d (int (N), freq, time, FFTW_ESTIMATE);

    // Forward FFT
    fftw_execute (fwd);

#if 0
    // brick-wall HP: kill DC..cutoff
    for (int i = 0; i <= bin_cutoff && i <= max_bin; ++i) {
        freq [i] [0] = 0.0;
        freq [i] [1] = 0.0;
    }
#else
    // hann-ish fade band around the cutoff, TODO: fix this

    // fade_width is in *bins*, computed from ~15% of cutoff in Hz
    int fade_width = int ( (0.15 * cutoff) / bin_hz );   // ~15% of cutoff
    if (fade_width < 1) fade_width = 1;

    int bin_fade = bin_cutoff - fade_width;             // where fade starts
    if (bin_fade < 0) {
        bin_fade   = 0;
        fade_width = bin_cutoff - bin_fade;
        if (fade_width < 1) fade_width = 1;
    }

    for (int i = 0; i <= max_bin; ++i) {
        if (i <= bin_fade) {
            // keep as-is
            continue;
        } else if (i < bin_cutoff) {
            // smooth fade from 1 -> 0 over [bin_fade, bin_cutoff]
            double t = double (i - bin_fade) / double (bin_cutoff - bin_fade);
            double w = 0.5 * (1.0 + std::cos (M_PI * t));  // t=0 => 1, t=1 => 0
            freq [i] [0] *= w;
            freq [i] [1] *= w;
        } else {
            // fully killed below cutoff
            freq [i] [0] = 0.0;
            freq [i] [1] = 0.0;
        }
    }
#endif

    // inverse fft
    fftw_execute (inv);
        
    // normalize fftw inverse (not normalized by default)
    for (size_t i = 0; i < N; ++i)
        irf[i] = float (time [i] / double (N));

    fftw_destroy_plan (fwd);
    fftw_destroy_plan (inv);
    fftw_free (freq);
    fftw_free (time);
}

// simple butterworth highpass at ~30 Hz zero-phase (forward+reverse)
// unused (for now), keeping this for reference
static void dc_kill(std::vector<float>& irf, float samplerate)
{
    const float fc = 30.0f;
    const float w0 = 2.0f * M_PI * fc / samplerate;
    const float alpha = (1.0f - std::sin(w0)) / std::cos(w0);

    float prev = irf[0];
    for (size_t i = 1; i < irf.size(); ++i) {
        float v = irf[i];
        irf[i] = irf[i] - prev + alpha * irf[i];
        prev = v;
    }

    // reverse pass to make it zero-phase
    prev = irf.back();
    for (size_t i = irf.size() - 2; i > 0; --i) {
        float v = irf[i];
        irf[i] = irf[i] - prev + alpha * irf[i];
        prev = v;
    }
}

static size_t detect_sweep_start_with_marker (const std::vector<float> &buf,
                                              int samplerate,
                                              float silence_thresh_db,
                                              float sweep_amp_db,
                                              double marker_freq,          // MARKER_FREQ
                                              double marker_seconds_hint,  // 0 = unknown/auto
                                              double gap_seconds_hint,     // 0 = unknown/auto
                                              bool verbose,
                                              size_t *out_marker_len = NULL,
                                              size_t *out_gap_len = NULL) {
  debug ("start");
  
  float sweep_amp_linear = db_to_linear (sweep_amp_db);
  float silence_thresh = db_to_linear (silence_thresh_db);
  debug ("silence_thresh db=%f lin=%f", silence_thresh_db, silence_thresh);
  
  // default outputs
  if (out_marker_len) *out_marker_len = 0;
  if (out_gap_len)    *out_gap_len    = 0;

  if (buf.empty ()) return 0;
  
  // 1) first non-silent sample in the whole file
  size_t i0 = find_first_nonsilent (buf, silence_thresh_db);
  if (i0 >= buf.size ()) return 0; // all silent, fall back

  if (marker_freq <= 0.0) {
    // No marker mode, just use the first non-silent sample like before
    if (verbose) {
      std::cerr << "No marker_freq configured, using first non-silent at "
                << i0 << "\n";
    }
    debug ("return");
    return i0;
  }

  // 2) analyze a short window after i0 to see if it's our marker square wave
  const double max_marker_window_sec = (marker_seconds_hint > 0.0)
                                       ? marker_seconds_hint
                                       : 5;
  const size_t max_window_samples =
      std::min (buf.size () - i0,
               (size_t) (max_marker_window_sec * samplerate));

  if (max_window_samples < 8) {
    // Not enough samples to decide, just fall back
    if (verbose) {
      std::cerr << "Marker window too short, using first non-silent at "
                << i0 << "\n";
    }
    debug ("return");
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
    debug ("return");
    return i0;
  }

  double avg_samples_per_flip =
      (double) (last_flip_idx - first_flip_idx) / flip_count;

  // For a square wave, 2 flips per full period
  double est_period = 2.0 * avg_samples_per_flip;
  double est_freq   = (est_period > 0.0)
                      ? (double)samplerate / est_period
                      : 0.0;

  double rel_err = std::fabs (est_freq - marker_freq) / marker_freq;

  if (verbose) {
    std::cerr << "Marker detect: est_freq=" << est_freq
              << " Hz (target " << marker_freq << " Hz), rel_err="
              << rel_err << ", flips=" << flip_count << "\n";
  }

  if (est_freq <= 0.0 || rel_err > 0.2) {
    // >20% off -> probably not our marker
    if (verbose) {
        std::cerr << "Marker frequency mismatch, using first non-silent at "
                  << i0 << "\n";
    }
    debug ("return");
    return i0;
  }

  // 3) marker looks valid -> decide where it ends
  size_t marker_end = i0;
  if (marker_seconds_hint > 0.0) {
    marker_end = i0 + (size_t)(marker_seconds_hint * samplerate);
    if (marker_end > buf.size ()) marker_end = buf.size ();
  } else {
    // Auto: extend until signal no longer looks like strong marker
    const size_t max_marker_samples =
        std::min(buf.size () - i0,
                 (size_t)(0.5 * samplerate)); // up to 0.5s
    marker_end = i0;
    for (size_t n = 0; n < max_marker_samples; ++n) {
        float v = buf [i0 + n];
        if (std::fabs (v) < amp_thresh * 0.3f) { // decayed significantly
            marker_end = i0 + n;
            break;
        }
    }
    if (marker_end <= i0) marker_end = i0 + max_marker_samples;
    if (marker_end > buf.size ())      marker_end = buf.size ();
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
      ? (size_t) (gap_seconds_hint * samplerate)
      : (size_t) (0.05 * samplerate); // 50 ms minimum

  // find first non-silent after marker
  // but ensure at least min_gap_samples of silence if possible
  size_t silent_run = 0;
  bool in_silence = false;
  for (size_t i = marker_end; i < buf.size (); ++i) {
    if (std::fabs (buf [i]) < silence_thresh) {
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
  if (gap_end > gap_start && gap_end < buf.size ()) {
    sweep_start = gap_end;
  } else {
    // No clear long silence run; just take first non-silent after marker_end
    std::vector<float> tail (buf.begin() + marker_end, buf.end ());
    size_t rel = find_first_nonsilent (tail, silence_thresh_db);
    sweep_start = rel + marker_end;
  }

  if (sweep_start >= buf.size ()) sweep_start = i0; // fallback

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

  debug ("end");
  return sweep_start;
}

static size_t detect_sweep_start (const std::vector<float> &buf,
                                  int samplerate,
                                  s_prefs &prefs,
                                  bool ignore_marker = false) {
    debug("start");
  
    size_t marker_len = 0;
    size_t gap_len    = 0;
    size_t start      = 0;
  
    align_method align =
        (ignore_marker && prefs.align == align_marker_dry)
            ? align_marker   // for wet: treat it like plain marker align
            : prefs.align;
  
    switch (align) {
      case align_marker:
        start = detect_sweep_start_with_marker (
                    buf,
                    samplerate,
                    prefs.sweep_silence_db,
                    prefs.sweep_amp_db,
                    MARKER_FREQ,
                    0,                 // no assumed durations
                    0,
                    prefs.verbose,
                    &marker_len,
                    &gap_len);
        break;

      case align_marker_dry:
        // only called for DRY, because for wet we’ll handle this separately
        start = detect_sweep_start_with_marker (
                    buf,
                    samplerate,
                    prefs.sweep_silence_db,
                    prefs.sweep_amp_db,
                    MARKER_FREQ,
                    prefs.marker_seconds,
                    prefs.marker_gap_seconds,
                    prefs.verbose,
                    &marker_len,
                    &gap_len);

        prefs.cache_dry_marker_len = marker_len;
        prefs.cache_dry_gap_len    = gap_len;
        break;
  
      case align_silence:
        start = find_first_nonsilent (buf, prefs.sweep_silence_db);
        break;
  
      case align_none:
        start = 0;
        break;
    }

    if (prefs.verbose) {
      std::cerr << "start="      << start
                << " marker_len=" << marker_len
                << " gap_len="    << gap_len << "\n";
    }
  
    debug("end");
    return start;
}

// DRY: use full logic (marker, silence, caching…)
static size_t detect_dry_sweep_start (const std::vector<float> &dry,
                                      int samplerate,
                                      s_prefs &prefs) {
    return detect_sweep_start (dry, samplerate, prefs, /*ignore_marker=*/false);
}

// WET: if align_marker_dry, reuse cached dry preroll;
// otherwise just use normal logic on wet
static size_t detect_wet_sweep_start (const std::vector<float> &wet,
                                      int samplerate,
                                      s_prefs &prefs) {
    debug("detect_wet_sweep_start: start");

    size_t start = 0;

    if (prefs.align == align_marker_dry) {
        // 1) where does wet actually start?
        size_t wet_first = find_first_nonsilent (wet, prefs.sweep_silence_db);

        // 2) skip the same marker+gap duration we found in dry
        start = wet_first
              + prefs.cache_dry_marker_len
              + prefs.cache_dry_gap_len;

        if (prefs.verbose) {
            std::cerr << "wet: first_nonsilent=" << wet_first
                      << " using cached marker_len=" << prefs.cache_dry_marker_len
                      << " gap_len=" << prefs.cache_dry_gap_len
                      << " -> start=" << start << "\n";
        }
    } else {
        // plain marker / silence / none logic on wet buffer
        start = detect_sweep_start (wet, samplerate, prefs, /*ignore_marker=*/false);
    }

    debug("detect_wet_sweep_start: end");
    return start;
}

bool read_wav (const char *filename,
                std::vector<float> &left,
                std::vector<float> &right,
                int &sr) {
  debug ("start");
  
  left.clear ();
  right.clear ();

  SF_INFO info {};
  SNDFILE *f = sf_open (filename, SFM_READ, &info);
  if (!f) {
    std::cerr << "Error: failed to open WAV file: " << filename << "\n";
    return false;
  }

  sr = info.samplerate;
  int chans = info.channels;
  sf_count_t frames = info.frames;

  if (frames <= 0 || chans <= 0) {
    std::cerr << "Error: invalid WAV format in " << filename << "\n";
    sf_close (f);
    return false;
  }
  
  std::vector<float> buf (frames * chans);
  sf_count_t readFrames = sf_readf_float (f, buf.data (), frames);
  sf_close (f);

  if (readFrames <= 0) {
    std::cerr << "Error: no samples read from " << filename << "\n";
    return false;
  }

  if (chans == 1) {
    // mono: store in left channel only
    left.assign (buf.begin (), buf.begin () + readFrames);
    // right stays empty
    debug ("return (mono)");
    return true;
  }

  if (chans == 2) {
    // stereo: split into left/right
    left.resize (readFrames);
    right.resize (readFrames);
    
    for (sf_count_t i = 0; i < readFrames; ++i) {
      left[i]  = buf [2 * i + 0];
      right[i] = buf [2 * i + 1];
    }

    debug ("return (stereo)");
    return true;
  }

  // More than 2 channels is unsupported (consistent with your earlier code)
  std::cerr << "Error: only mono or stereo WAV files supported: "
            << filename << "\n";
  return false;
}

static bool write_mono_wav (const char *path,
                            const std::vector<float> &data,
                            int samplerate) {
  debug ("start");
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
  debug ("end");
  return true;
}

static bool write_stereo_wav (const char *path,
                              const std::vector<float> &L,
                              const std::vector<float> &R,
                              int samplerate) {
  debug ("start");
  
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
  debug ("end");
  return true;
}

bool dump_double_wav(const std::string& path,
                     const std::vector<double>& buf,
                     int samplerate,
                     size_t length) {
    SF_INFO info;
    memset(&info, 0, sizeof(info));
    info.channels = 1;
    info.samplerate = samplerate;
    info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_24;

    SNDFILE* snd = sf_open(path.c_str(), SFM_WRITE, &info);
    if (!snd) {
        fprintf(stderr, "Error writing %s: %s\n", path.c_str(), sf_strerror(NULL));
        return false;
    }

    sf_count_t written = sf_writef_double(snd, buf.data(), length);
    sf_close(snd);

    return (written == (sf_count_t)length);
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

static void align_stereo_joint_no_chop (std::vector<float> &L,
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
}

bool c_deconvolver::load_sweep_dry (const char *in_filename) {
  debug ("start");
  int sr = 0;
  std::vector<float> tmp, tmp_r;

  if (!read_wav (in_filename, tmp, tmp_r, sr)) {
    std::cerr << "Failed to load dry sweep: " << in_filename << "\n";
    return false;
  }
  if (!set_samplerate_if_needed (sr)) {
    std::cerr << "Samplerate mismatch in dry sweep: " << in_filename << "\n";
    return false;
  }

  dry_ = std::move (tmp);
  have_dry_ = true;

  if (prefs_->align != align_none) {
    // NEW: detect sweep start using marker+gap, also caches marker/gap in prefs_
    dry_offset_ = detect_dry_sweep_start (dry_, samplerate_, *prefs_);
    if (prefs_->verbose) {
      std::cerr << "Dry sweep start offset: " << dry_offset_ << " samples\n";
    }
  }
  
  debug ("end");
  return true;
}

bool c_deconvolver::load_sweep_wet (const char *in_filename) {
  debug ("start");

  int sr = 0;
  std::vector<float> left;
  std::vector<float> right;

  if (!read_wav (in_filename, left, right, sr)) {
    std::cerr << "Failed to load wet sweep: " << in_filename << "\n";
    return false;
  }

  bool is_stereo = (right.size () > 1);

  if (!set_samplerate_if_needed (sr)) {
    std::cerr << "Samplerate mismatch in wet sweep: " << in_filename << "\n";
    return false;
  }

  wet_L_.clear ();
  wet_R_.clear ();

  if (!is_stereo) {
    // --- MONO WET: only L channel is used ---
    wet_L_ = std::move (left);

    if (prefs_->align != align_none) {
      wet_offset_ = detect_wet_sweep_start (wet_L_, samplerate_, *prefs_);
      if (prefs_->verbose) {
        std::cerr << "Wet sweep (mono) start offset: "
                  << wet_offset_ << " samples\n";
      }
    }

  } else {
    // --- STEREO WET: L/R in separate vectors ---
    wet_L_ = std::move (left);
    wet_R_ = std::move (right);

    if (prefs_->align != align_none) {
      // for detection, use a mono mix so we get a single start index
      // kind of hackish, but meh
      const sf_count_t frames = std::min (wet_L_.size (), wet_R_.size ());
      std::vector<float> mix (frames);
      for (sf_count_t i = 0; i < frames; ++i) {
        mix [i] = 0.5f * (wet_L_ [i] + wet_R_ [i]);
      }
      
      wet_offset_ = detect_wet_sweep_start (mix, samplerate_, *prefs_);
      if (prefs_->verbose) {
        std::cerr << "Wet sweep (stereo) start offset: "
                  << wet_offset_ << " samples\n";
      }
    }
  }

  have_wet_ = true;

  debug ("end");
  return true;
}

bool c_deconvolver::set_dry_from_buffer (const std::vector<float>& buf, int sr) {
  debug ("start");
  if (buf.empty()) {
    std::cerr << "Dry buffer is empty.\n";
    return false;
  }
  if (!set_samplerate_if_needed (sr)) {
    std::cerr << "Samplerate mismatch for dry buffer.\n";
    return false;
  }

  dry_ = buf;
  have_dry_ = true;

  if (prefs_->align != align_none) {
    dry_offset_ = detect_dry_sweep_start (dry_, samplerate_, *prefs_);
    if (prefs_->verbose) {
      std::cerr << "Dry sweep start offset (buffer): "
                << dry_offset_ << " samples\n";
    }
  }

  debug ("end");
  return true;
}

bool c_deconvolver::set_wet_from_buffer (const std::vector<float>& bufL,
                                         const std::vector<float>& bufR,
                                         int sr) {
  debug ("start");
  if (bufL.empty () && bufR.empty ()) {
    std::cerr << "Wet buffers are empty.\n";
    return false;
  }
  if (!set_samplerate_if_needed (sr)) {
    std::cerr << "Samplerate mismatch for wet buffer.\n";
    return false;
  }

  wet_L_ = bufL;
  wet_R_ = bufR;

  if (prefs_->align != align_none) {
    if (!wet_R_.empty ()) {
      const size_t n = std::min(wet_L_.size (), wet_R_.size ());
      std::vector<float> mix (n);
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
  }

  have_wet_ = true;
  debug ("end");
  return true;
}

bool c_deconvolver::output_ir (const char *out_filename, long ir_length_samples) {
  debug ("start, ir_length_samples=%ld", ir_length_samples);
  std::vector<float> irL;
  std::vector<float> irR;
  
  if (prefs_->align == align_none) {
    dry_offset_ = 0;
    wet_offset_ = 0;
  }
  
  std::cerr << "\nDry offset: " << dry_offset_
            << "\nWet offset: " << wet_offset_ << "\n\n";

  // --- deconvolve left ---
  if (!calc_ir_raw (wet_L_, dry_, irL, ir_length_samples)) {
    std::cerr << "Error: calc_ir_raw failed for left channel.\n";
    return false;
  }

  // --- deconvolve right if we have input data ---
  const bool have_right_channel = !wet_R_.empty ();
  if (have_right_channel) {
    if (!calc_ir_raw (wet_R_, dry_, irR, ir_length_samples)) {
      std::cerr << "Error: calc_ir_raw failed for right channel.\n";
      return false;
    }
  }
  
  // get rid of junk above hearing threshold, say 90% of nyquist freq
  float cutoff = (prefs_->sweep_sr / 2) * 0.9;
  // ...or just a fixed frequency
  //float cutoff = 18000;
  if (irL.size () > 0) {
#ifdef HIGHPASS_F
    highpass_ir (irL, prefs_->sweep_sr, HIGHPASS_F);
#endif
#ifdef LOWPASS_F
    lowpass_ir (irL, prefs_->sweep_sr, std::min ((float) LOWPASS_F, cutoff));
#endif
  }
  if (irR.size () > 0) {
#ifdef HIGHPASS_F
    highpass_ir (irR, prefs_->sweep_sr, std::min ((float) LOWPASS_F, cutoff));
#endif
#ifdef LOWPASS_F
    lowpass_ir (irR, prefs_->sweep_sr, cutoff);
#endif
  }
  
  // TODO: figure out why this even works for L/R delay
  //align_stereo_independent (irL, irR, 0);
  
  /*if (!irL.empty() || !irR.empty()) {
    align_stereo_joint_no_chop (irL, irR, 0); // move reference peak to index 0
  }*/
  //normalize_and_trim_stereo (irL, irR, prefs_->zeropeak);
  normalize_and_trim_stereo (irL,
                             irR,
                             prefs_->zeropeak,
                             prefs_->ir_start_silence_db, // start trim
                             prefs_->ir_silence_db,       // tail trim (from -T)
                             0.05f);                      // 5% fade
                           
  // Decide mono vs stereo based on whether right has any energy left
  bool have_right_energy = false;
  const float thr = db_to_linear (-90);
  for (float v : irR) {
    if (std::fabs (v) > thr) {
      have_right_energy = true;
      break;
    }
  }
  
  if (!have_right_channel || !have_right_energy || irR.empty ()) {
    // MONO IR
    if (!write_mono_wav (out_filename, irL, samplerate_)) {
      std::cerr << "Error: failed to write mono IR to " << out_filename << "\n";
      return false;
    }
    std::cout << "Wrote MONO IR: " << out_filename
              << " (" << irL.size () << " samples @ " << samplerate_ << " Hz)\n";
    debug ("return");
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
  debug ("end");
  return true;
}

// ok THIS WORKS DON'T TOUCH IT!!!
bool c_deconvolver::calc_ir_raw (const std::vector<float> &wet,
                                 const std::vector<float> &dry,
                                 std::vector<float>       &ir_raw,
                                 long                     ir_length_samples) {
  debug ("start, ir_length_samples=%ld", ir_length_samples);
  if (wet.empty () || dry.empty ()) {
    std::cerr << "Error: wet or dry buffer is empty.\n";
    return false;
  }

  // Use class member offsets.
  // For stereo, wet_offset_ is the "common" silence we decided to skip.
  // Use initial offsets detected from sweeps
  size_t wet_offset = wet_offset_;
  size_t dry_offset = dry_offset_;
  size_t out_offset = 0;

  // Apply user offset (positive delays wet, negative delays dry)
  int offs = prefs_->sweep_offset_smp;
  if (offs > 0) {
      size_t pos = (size_t) offs;
      if (wet_offset > pos)
          wet_offset -= pos;
      else
          wet_offset = 0;
  } else if (offs < 0) {
      size_t pos = (size_t) (-offs);
      if (dry_offset > pos)
          dry_offset -= pos;
      else
          dry_offset = 0;
  }

  if (wet_offset >= wet.size()) wet_offset = 0;
  if (dry_offset >= dry.size()) dry_offset = 0;
  
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

  debug ("final offsets: dry=%ld, wet=%ld, offs=%ld",
         (long int) dry_offset, (long int) wet_offset, (long int) offs);

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
  
  ir_length_samples = std::max ((size_t) ir_length_samples,
                                (size_t) (MAX_IR_LEN * prefs_->sweep_sr));
  

  if (prefs_->dump_debug) {
    std::string prefix = prefs_->dump_prefix;

    std::string dryname = prefix + "-dry.wav";
    std::string wetname = prefix + "-wet.wav";

    debug ("Dumping debug buffers: %s, %s", dryname.c_str (), wetname.c_str ());

    dump_double_wav (dryname, xTime, prefs_->sweep_sr, usable_dry);
    dump_double_wav (wetname, yTime, prefs_->sweep_sr, usable_wet);
  }

  // Frequency-domain buffers
  fftw_complex *X = (fftw_complex *) fftw_malloc (sizeof (fftw_complex) * nFreq);
  fftw_complex *Y = (fftw_complex *) fftw_malloc (sizeof (fftw_complex) * nFreq);
  fftw_complex *H = (fftw_complex *) fftw_malloc (sizeof (fftw_complex) * nFreq);
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
  double eps = (maxMag2 > 0.0) ? maxMag2 * 1e-10 : 1e-14;
  //double reg = (maxMag2 > 0.0) ? maxMag2 * 1e-4 : 1e-8;
  
  // TEST
  //for (size_t k = 0; k < nFreq; ++k) {
  //  H[k][0] = 1.0;
  //  H[k][1] = 0.0;
  //} if (0) // for (size_t k = 0; ....)
  
  double tiny = (maxMag2 > 0.0) ? maxMag2 * 1e-16 : 1e-30;
  
  // IMPROVED: maximum allowed |H[k]| gain (linear). 10 == +20 dB, 32 ~= +30 dB.
  // this gives us flawless 1:1 identity IR when deconvolving a sweep with itself,
  // AND no noise floor added in the >= upper sweep frequency
  const double max_H_gain = 32.0;
  //const double max_H_gain = 1e6;

  for (size_t k = 0; k < nFreq; ++k) {
      double xr = X [k] [0];
      double xi = X [k] [1];
      double yr = Y [k] [0];
      double yi = Y [k] [1];

      double mag2 = xr * xr + xi * xi;

      // effectively zero energy here -> don't try to invert it.
      if (mag2 < tiny) {
        H [k] [0] = 0.0;
        H [k] [1] = 0.0;
        continue;
      }

      // conj(X)
      double cr = xr;
      double ci = -xi;

      // Y * conj(X)
      double nr = yr * cr - yi * ci;
      double ni = yr * ci + yi * cr;

      // Raw inverse
      double hr = nr / mag2 + eps;
      double hi = ni / mag2 + eps;

      // Optional safety clamp on |H|
      double magH = std::sqrt (hr * hr + hi * hi);
      if (magH > max_H_gain) {
          double scale = max_H_gain / magH;
          hr *= scale;
          hi *= scale;
      }

      H [k] [0] = hr;
      H [k] [1] = hi;
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

  // Hard cap based on MAX_IR_LEN (seconds), unless user asked for shorter
  size_t max_ir_samples = (size_t) std::llround (MAX_IR_LEN * prefs_->sweep_sr);
  if (max_ir_samples > 0 && irLen > max_ir_samples) {
      irLen = max_ir_samples;
  }

  if (ir_length_samples > 0 && (size_t) ir_length_samples < irLen) {
      irLen = (size_t) ir_length_samples;   // user length wins if smaller
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
    ir_raw [i] = 0.0f;
  }

  // copy IR starting at out_offset
  for (size_t i = 0; i < irLen; ++i) {
    ir_raw [i + out_offset] = (float) hTime[i];
  }

  // Cleanup
  fftw_destroy_plan (planX);
  fftw_destroy_plan (planY);
  fftw_destroy_plan (planInv);
  fftw_free (X);
  fftw_free (Y);
  fftw_free (H);

  debug ("end");
  return true;
}

// returns index of |largest| sample
bool c_deconvolver::set_samplerate_if_needed (int sr) {
  if (samplerate_ == 0) {
    samplerate_ = sr;
    return true;
  }
  return (samplerate_ == sr);
}

int c_deconvolver::samplerate () {
  return samplerate_; //prefs_->sweep_sr;
}

static size_t find_peak (std::vector<float> &buf) {
  int i = 0, len = buf.size (), peakpos = 0;
  float peak = 0;
  for (i = 0; i < len; i++) {
    float abs = fabs (buf [i]);
    if (abs > peak) {
      peakpos = i;
      peak = abs;
    }
  }
  
  return peakpos;
}

void c_deconvolver::normalize_and_trim_stereo (std::vector<float> &L,
                                               std::vector<float> &R,
                                               bool zeropeak,
                                               float thr_start_db, // > 0 -> don't trim start
                                               float thr_end_db,   // > 0 -> don't trim end
                                               float fade_end_percent) {
  debug ("start, zeropeak=%s, thr_start=%f, thr_end=%f, fade_end_percent=%f",
        zeropeak ? "true" : "false", thr_start_db, thr_end_db, fade_end_percent);
  const size_t tail_pad       = 32;
  //const float  silence_thresh = 1e-5f;  // relative to peak (~ -100 dB)
  const bool hasR = !R.empty ();
  
  float thr_start = db_to_linear (thr_start_db);
  float thr_end = db_to_linear (thr_end_db);

  // avoid edge case where we could wipe the whole buffer
  if (L.empty () && !hasR) return;

  
  // 1) Find global peak across both channels
  float peak = 0.0f;
  for (float v : L) peak = std::max (peak, std::fabs (v));
  if (hasR) {
    for (float v : R) peak = std::max (peak, std::fabs (v));
  }

  if (peak < 1e-12f) {
    // all silence (or numerical dust) -> nothing to normalize
    L.assign (1, 0.0f);
    if (hasR) R.assign (1, 0.0f);
    debug ("return (all silence)");
    return;
  }
  
  const float norm = prefs_->normalize_amp / peak;
  
  
  // 2) Apply same gain to both channels
  for (float &v : L) v *= norm;
  if (hasR) {
    for (float &v : R) v *= norm;
  }
  
  size_t len = std::max (L.size (), R.size ());
  size_t fade_end = (size_t) (len * fade_end_percent);
  
  // fade end
  //bool fade_pow2 = true;
  for (size_t i = fade_end; i < len; ++i) {
    float t = float (i - fade_end) / float (len - fade_end - 1);
    float g = 1.0f - t;
    //if (fade_pow2)
        g *= g;
    
    if (i < L.size ()) L [i] *= g;
    if (i < R.size ()) R [i] *= g;
  }
  
  
  // 3) Remove common leading silence
  size_t firstl, firstr, first_nonsilent;
  len = L.size();
  if (hasR) len = std::min(L.size(), R.size());

  if (!zeropeak) {
    // Old/current behavior
    if (thr_start_db > 0.0f) {
      debug("thr_start_db=%f, skipping start trim", thr_start_db);
      first_nonsilent = 0;
    } else {
      firstl  = find_first_nonsilent(L, thr_start_db);
      firstr = hasR ? find_first_nonsilent(R, thr_start_db) : firstl;
      first_nonsilent = std::min(firstl, firstr);
    }
    first_nonsilent = std::max (first_nonsilent, prefs_->sweep_offset_smp);
  } else {
    // New behavior: place the peak at sweep_offset_smp
    // by trimming to the first "non-silent" sample at or
    // after (peak_idx - sweep_offset_smp).

    // global peak index (after normalization)
    size_t peakL   = find_peak (L);
    size_t peakR   = hasR ? find_peak (R) : peakL;
    size_t peak_idx = std::min (peakL, peakR);

    // absolute threshold relative to (already normalized) peak
    float rel_lin = db_to_linear (thr_start_db);  // thr_start_db < 0
    float thresh  = peak * rel_lin;

    // where we *want* the peak to end up, after shifting
    size_t desired_peak_pos =
        (prefs_ && prefs_->sweep_offset_smp > 0)
        ? (size_t) prefs_->sweep_offset_smp
        : 0;

    // start scanning for "first non-silent" sample
    // at or after (peak_idx - desired_peak_pos)
    size_t len = L.size ();
    if (hasR) len = std::min (L.size (), R.size ());

    size_t search_begin = 0;
    if (peak_idx > desired_peak_pos)
      search_begin = peak_idx - desired_peak_pos;
    if (search_begin > len) search_begin = len;

    size_t cut   = 0;
    bool   found = false;

    // 1) preferred path: find first sample >= thresh
    //    in [search_begin .. peak_idx]
    for (size_t i = search_begin; i <= peak_idx && i < len; ++i) {
      float aL = (i < L.size ())               ? std::fabs (L[i]) : 0.0f;
      float aR = (hasR && i < R.size ()) ? std::fabs (R[i]) : 0.0f;
      float a  = std::max (aL, aR);

      if (a >= thresh) {
        cut   = i;     // we'll drop [0 .. i-1]
        found = true;
        break;
      }
    }

    // 2) fallback: if *everything* before peak_idx is below threshold
    //    in that window (or we somehow didn't find anything), behave
    //    like the old "trim leading silence up to the last below-thresh".
    if (!found) {
      size_t last_below = 0;
      for (size_t i = 0; i < peak_idx && i < len; ++i) {
        float aL = (i < L.size ())               ? std::fabs (L[i]) : 0.0f;
        float aR = (hasR && i < R.size ()) ? std::fabs (R[i]) : 0.0f;
        float a  = std::max (aL, aR);

        if (a < thresh) {
          last_below = i + 1;
        } else {
          break;
        }
      }
      cut = last_below;
    }

    first_nonsilent = cut;

    debug ("zero peak: peak_idx=%zu, cut=%zu, desired_peak_pos=%zu, thr_start_db=%f",
           peak_idx, cut, desired_peak_pos, thr_start_db);
  }

  if (first_nonsilent == len) {
    // everything below threshold even after norm -> just keep 1 sample of silence
    L.assign(1, 0.0f);
    if (hasR) R.assign(1, 0.0f);
    debug ("return (all silence after norm");
    return;
  }
  
  if (first_nonsilent > 0) {
    shift_ir_left(L, first_nonsilent);
    if (hasR) shift_ir_left(R, first_nonsilent);
  }
  
  
  // 4) Compute trim position at the tail based on max (|L|, |R|)
  if (thr_end_db > 0.0) {
    debug ("thr_end_db=%f, skipping", thr_end_db);
  } else {

    size_t lastl = find_last_nonsilent (L, thr_end_db);
    size_t lastr = hasR ? find_last_nonsilent (R, thr_end_db) : lastl;
    size_t last_nonsilent = std::max (lastl, lastr);

    size_t newLen;
    if (last_nonsilent < 0) {
      newLen = 1; // all silent after normalization/leading trim
    } else {
      newLen = (size_t) last_nonsilent + 1 + tail_pad;
    }

    if (newLen > L.size ()) newLen = L.size ();
    if (hasR && newLen > R.size ()) newLen = R.size ();

    if (newLen == 0) newLen = 1; // paranoia

    L.resize (newLen);
    if (hasR) R.resize (newLen);
  }
}

#ifdef USE_JACK
static int jack_process_cb (jack_nframes_t nframes, void *arg) {
  s_jackclient *j = (s_jackclient *) arg;
  
  // playback
  if (j->play_go) {
    jack_default_audio_sample_t *port_outL =
      (jack_default_audio_sample_t *) jack_port_get_buffer (j->port_outL, nframes);
    /*jack_default_audio_sample_t *port_outR =
      (jack_default_audio_sample_t *) jack_port_get_buffer (j->port_outR, nframes);*/

    for (jack_nframes_t i = 0; i < nframes; ++i) {
      float v = 0.0f;
      if (j->index < j->sig_out.size ()) {
        v = j->sig_out [j->index++];
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

    for (jack_nframes_t i = 0; i < nframes && j->rec_index < j->rec_total; ++i) {
      float vL = port_inL[i];
      float vR = port_inR ? port_inR[i] : vL;
      float m  = 0.5f * (vL + vR); // simple mono mix
      j->sig_in_L[j->rec_index++] = m;
    }

    if (j->rec_index >= j->rec_total) {
      j->rec_done = true;
      j->rec_go   = false;
    }
  }

  return 0;
}

static int jack_get_default_playback (jack_client_t *c,
                                       int howmany,
                                       std::vector<std::string> &v) {
  debug ("start");
  //v.clear ();
  if (!c || howmany <= 0) return 0;

  const char **ports = jack_get_ports (
    c,
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

static int jack_get_default_capture (jack_client_t *c,
                                      int howmany,
                                      std::vector<std::string> &v) {
  debug ("start");
  //v.clear ();
  if (!c || howmany <= 0) return 0;

  const char **ports = jack_get_ports (
    c,
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

bool c_deconvolver::jack_init (std::string clientname,
                               int samplerate,
                               //const char *jack_out_port,
                               //const char *jack_in_port,
                               bool stereo_out) {
  debug ("start");
  if (jack_inited && jackclient.client) {
    // already have a client; nothing to do
    return true;
  }
  
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

  jackclient.client = jack_client_open (name, JackNullOption, &status);
  if (!jackclient.client) {
    std::cerr << "Error: cannot connect to JACK (client name \""
              << name << "\")\n";
    jack_inited = false;
    return false;
  }

  // JACK is alive, get its sample rate
  jackclient.samplerate = (int) jack_get_sample_rate (jackclient.client);

  if (prefs_) {
    if (!prefs_->quiet && prefs_->sweep_sr != jackclient.samplerate) {
        std::cerr << "Note: overriding sample rate (" << prefs_->sweep_sr
                  << " Hz) with JACK sample rate " << jackclient.samplerate << " Hz.\n";
    }
    prefs_->sweep_sr = jackclient.samplerate;
  }
  
  // Reset runtime state
  jackclient.play_go = false;
  jackclient.rec_go  = false;
  jackclient.index   = 0;
  jackclient.sig_in_L.clear ();
  jackclient.sig_out.clear ();
  jackclient.port_inL = jackclient.port_inR = nullptr;
  //jackclient.port_outL = jackclient.port_outR = nullptr;
  
  // register jack ports
  jackclient.port_outL = jack_port_register (jackclient.client, "out",
                            JACK_DEFAULT_AUDIO_TYPE,
                            JackPortIsOutput, 0);
  /*jackclient.port_outR = jack_port_register (jackclient.client, "out_R",
                            JACK_DEFAULT_AUDIO_TYPE,
                            JackPortIsOutput, 0);*/
  jackclient.port_inL  = jack_port_register (jackclient.client, "in_L",
                            JACK_DEFAULT_AUDIO_TYPE,
                            JackPortIsInput, 0);
  jackclient.port_inR  = jack_port_register (jackclient.client, "in_R",
                            JACK_DEFAULT_AUDIO_TYPE,
                            JackPortIsInput, 0);

  if (!jackclient.port_outL || !jackclient.port_inL || !jackclient.port_inR) {
    std::cerr << "Error: cannot register JACK ports.\n";
    //jack_deactivate (jackclient.client);
    jack_client_close (jackclient.client);
    jackclient.client  = nullptr;
    jack_inited        = false;
    return false;
  }
  
  jack_set_process_callback (jackclient.client, jack_process_cb, &jackclient);

  // 5) Activate client
  if (jack_activate (jackclient.client) != 0) {
    std::cerr << "Error: cannot activate JACK client.\n";
    jack_client_close (jackclient.client);
    jackclient.client = nullptr;
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
    int err = jack_connect (jackclient.client,
                            jack_port_name (jackclient.port_outL),
                            prefs_->portname_dry.c_str ());
    if (err) std::cerr << "warning: failed to connect to JACK input port " + 
                          prefs_->portname_dry << std::endl;
                            
  } else {
    if (prefs_->mode == deconv_mode::mode_playsweep && !prefs_->sweepwait) {
      // connect to system default
      std::vector<std::string> strv;
      int n = jack_get_default_playback (jackclient.client, 1, strv);
      if (n == 1) {
        int err = jack_connect (jackclient.client,
                                jack_port_name (jackclient.port_outL),
                                strv [0].c_str ());
         if (err) std::cerr << "warning: failed to connect to JACK output port" +
                               strv [0] << std::endl;
      }
    }
  }
  
  // autoconnect input where it makes sense:
  // any port given: auto connect to it
  // no port given: no autoconnect
  std::cout << "output port L: " + prefs_->portname_wetL << std::endl;
  std::cout << "output port R: " + prefs_->portname_wetR << std::endl;
  if (prefs_->portname_wetL.length () > 0) {
    debug ("got portname_wetL");
    // connect to portname_wetL
    int err = jack_connect (jackclient.client,
                            prefs_->portname_wetL.c_str (),
                            jack_port_name (jackclient.port_inL));
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

bool c_deconvolver::jack_shutdown () {
  if (!jack_inited)
    return false;
  jack_deactivate (jackclient.client);
  jack_client_close (jackclient.client);
  jackclient.client = nullptr;
  jack_inited       = false;
  
  return true;
}

bool c_deconvolver::jack_playrec (const std::vector<float> &sweep,
                                        std::vector<float> &captured_l,
                                        std::vector<float> &captured_r) {
  if (sweep.empty ()) {
    std::cerr << "Sweep buffer is empty.\n";
    return false;
  }

  // 1) Ensure JACK client
  if (!jack_inited || !jackclient.client) {
    //if (!jack_init(std::string (), chn_stereo, chn_stereo)) {
      return false;
    //}
  }

  // 2) Prepare state
  jackclient.sig_out  = sweep;
  jackclient.index    = 0;
  jackclient.play_go  = false;

  // capture same length + a bit of extra tail
  const size_t extra_tail = (size_t) (0.5 * jackclient.samplerate); // 0.5s
  jackclient.rec_total = sweep.size () + extra_tail;
  jackclient.sig_in_L.assign (jackclient.rec_total, 0.0f);
  jackclient.rec_index = 0;
  jackclient.rec_done  = false;
  jackclient.rec_go    = false;

  std::cout << "Playing + recording sweep via JACK... " << std::flush;
  jackclient.play_go = true;
  jackclient.rec_go  = true;

  // 7) Block until done
  while ((jackclient.index < jackclient.sig_out.size ()) ||
         (!jackclient.rec_done)) {
    usleep (10 * 1000); // 10 ms
  }

  std::cout << "done.\n";

  // Copy captured mono buffer out (you could trim it here if desired)
  captured_l = jackclient.sig_in_L;

  return true;
}

bool c_deconvolver::jack_play (std::vector<float> &sig) {
  if (!jack_inited || !jackclient.client) {
    return false;
  }

  jackclient.sig_out = sig;
  jackclient.index   = 0;
  jackclient.play_go = false;

  std::cout << "Playing sweep via JACK... " << std::flush;
  jackclient.play_go = true;

  while (jackclient.index < jackclient.sig_out.size ()) {
    usleep (10 * 1000); // 10 ms
  }

  std::cout << "done.\n";

  // let caller decide when to deactivate/close
  return true;
}
#endif

static void print_usage (const char *prog, bool full = false) {
  std::ostream &out = full ? std::cout : std::cerr;
  
  if (full) {
    out << "\nDIRT - Delt's Impulse Response Tool, version " << DIRT_VERSION 
        << " build " << BUILD_TIMESTAMP <<
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
    "  -A, --align METHOD       Choose alignment method" << (full ? ": [none]\n"
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
    "  -z, --zero-peak          Try to align peak to zero"
                             << (DEFAULT_ZEROPEAK ? " [default]\n" : "\n") <<
    "  -Z, --no-zero-peak       Don't try to align peak to zero"
                             << (!DEFAULT_ZEROPEAK ? " [default]\n" : "\n") <<
    "\n";
    out <<
    "  -q, --quiet              Less verbose output\n"
    "  -v, --verbose            More verbose output\n"
    << (full ? 
    "  -D, --dump PREFIX        Dump wav files of sweeps used by deconvolver\n"
    : "") <<
    "\n" <<
    (full ? "Sweep generator" : "Some sweep generator") << " options: [default]\n" <<
    "  -s, --makesweep          Generate sweep WAV instead of deconvolving\n"
#ifdef USE_JACK
    "  -S, --playsweep          Play sweep via JACK instead of deconvolving\n"
#endif
    "  -R, --sweep-sr SR        Sweep samplerate [48000]\n"
    "  -L, --sweep-seconds SEC  Sweep length in seconds [30]\n"
    "  -a, --sweep-amplitude dB Sweep amplitude in dB [-1]\n" // DONE
    "  -X, --sweep-f1 F         Sweep start frequency [" << DEFAULT_F1 << "]\n"
    "  -Y, --sweep-f2 F         Sweep end frequency [" << DEFAULT_F2 << "]\n";
  if (full) out <<
    "  -O, --offset N           Offset (delay) wet sweep by N samples [10]\n"
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
    "  # Generate a 30-second dry sweep with 1 second of silence at start\n"
    "  # and an alignment marker of 0.1 second:\n"
    "  " << prog << " --makesweep -L 30 --preroll 1 --marker 0.1 -o drysweep.wav\n"
    "\n"
    "  # Deconvolve a wet/recorded sweep against a dry one, and output an\n"
    "  # impulse response: (tries to detect preroll and marker from dry file)\n"
    "  " << prog << " -d drysweep.wav -w wetsweep.wav -o ir.wav\n"
#ifdef USE_JACK
    "\n"
    "  # Play a sweep through Carla and record the result at the same time,\n"
    "  # then extract IR directly to \"carla_ir.wav\":\n"
    "  " << prog << " -d Carla:audio-in1 -w Carla:audio-out1 -o carla_ir.wav\n"
#endif
    //"\n"
    ;
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
        if (opt.preroll_seconds < 0.0) {
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
      debug("end");
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
      debug("end");
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
      debug("end");
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

  debug("end");
  return bf;
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
    opt.preroll_seconds = 0.01;
    opt.marker_seconds = 0;
    opt.marker_gap_seconds = 0;
  }
  
  debug ("end");
  return true;
}

int main (int argc, char **argv) {
  debug ("start");
  s_prefs p;
  
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
  c_deconvolver dec(&p);
  
  // using jack at all?
#ifdef USE_JACK
  if (p.dry_source == src_jack || p.wet_source == src_jack) {
    char realjackname [256] = { 0 };
    snprintf (realjackname, 255, p.jack_name.c_str (), argv [0]);
    dec.jack_init (std::string (realjackname),
               (p.wet_source == src_jack ? chn_mono : chn_none),
               (p.dry_source == src_jack || p.mode == deconv_mode::mode_playsweep ?
                chn_mono : chn_none));
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
    if (!write_mono_wav(p.out_path.c_str(), sweep, p.sweep_sr)) {
      return 1;
    }
    if (!p.quiet) {
        std::cerr << "Wrote sweep: " << p.out_path
                  << " (" << sweep.size() << " samples @ "
                  << p.sweep_sr << " Hz)\n";
    }
    debug("return");
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

    int retval = dec.jack_play(sweep);
    dec.jack_shutdown();
    
    debug("return");
    return retval;
  }

  // normal deconvolution path (with or without JACK)
  // Special "live" JACK→IR mode: dry=JACK, wet=JACK
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
        std::cout << "Press enter to play and record sinewave sweep... ";
        std::string str;
        std::getline(std::cin, str);
      }

      if (!dec.jack_playrec(sweep, wet_l, wet_r)) {
        debug("return");
        return 1;
      }

      if (!dec.set_dry_from_buffer(sweep, p.sweep_sr)) return 1;
      if (!dec.set_wet_from_buffer(wet_l, std::vector<float>(), p.sweep_sr)) return 1;

      if (!dec.output_ir(p.out_path.c_str(), p.ir_length_samples)) return 1;
      debug("return");
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
    debug("return");
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
    debug("return");
    return 1;
  }
  
  std::cout << "Detected sample rate: " << dec.samplerate () << std::endl;
  if (!dec.output_ir (p.out_path.c_str (), p.ir_length_samples)) {
    debug("return");
    return 1;
  }

  debug ("end");
  return 0;
}

