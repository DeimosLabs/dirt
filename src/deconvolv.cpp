
/* DIRT - Delt's Impulse Response Tool
 * Version 0.1
 *
 * Licensed under the GPL. See dirt.h and dirt.cpp for more info.
 */

#include "dirt.h"
#include "deconvolv.h"
#include "jack.h"

#ifdef DEBUG
#define CMDLINE_DEBUG
#include "cmdline/cmdline.h"
#define debug(...) cmdline_debug(stderr,ANSI_CYAN,__FILE__,__LINE__,__FUNC__,__VA_ARGS__)
#else
#define debug(...)
#define CP
#define BP
#endif


size_t find_first_nonsilent (const std::vector<float> &buf,
                                   float silence_thresh_db) {
  float silence_thresh = db_to_linear (silence_thresh_db);
  debug ("silence_thresh db=%f lin=%f", silence_thresh_db, silence_thresh);
  
  for (size_t i = 0; i < buf.size (); i++) {
    if (std::fabs (buf [i]) > silence_thresh) return i;
  }
  return buf.size ();
}

size_t find_last_nonsilent (const std::vector<float> &buf,
                                   float silence_thresh_db) {
  float silence_thresh = db_to_linear (silence_thresh_db);
  debug ("silence_thresh db=%f lin=%f", silence_thresh_db, silence_thresh);
  
  //for (size_t i = 0; i < buf.size (); ++i) {
  for (ssize_t i = (ssize_t) buf.size () - 1; i >= 0; --i) {
    if (std::fabs (buf [i]) > silence_thresh) return (size_t) i;
  }
  return buf.size ();
}

/*static size_t find_peak (const std::vector<float> &buf) {
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
}*/

size_t find_peak (const std::vector<float> &buf) {
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
void dc_kill(std::vector<float>& irf, float samplerate)
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

size_t detect_sweep_start_with_marker (const std::vector<float> &buf,
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

size_t detect_sweep_start (const std::vector<float> &buf,
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
size_t detect_dry_sweep_start (const std::vector<float> &dry,
                                      int samplerate,
                                      s_prefs &prefs) {
    return detect_sweep_start (dry, samplerate, prefs, /*ignore_marker=*/false);
}

// WET: if align_marker_dry, reuse cached dry preroll;
// otherwise just use normal logic on wet
size_t detect_wet_sweep_start (const std::vector<float> &wet,
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

  // More than 2 channels is unsupported
  std::cerr << "Error: only mono or stereo WAV files supported: "
            << filename << "\n";
  return false;
}

bool write_mono_wav (const char *path,
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

bool write_stereo_wav (const char *path,
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

void shift_ir_left (std::vector<float> &buf, size_t shift) {
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

void shift_ir_right (std::vector<float> &buf, size_t shift) {
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

inline void shift_ir (std::vector<float> &buf, long int shift) {
  if (shift > 0)
    shift_ir_right (buf, shift);
  else if (shift < 0)
    shift_ir_left (buf, shift * -1);
}

// joint alignment: preserve L/R offset
void align_stereo_joint (std::vector<float> &L,
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

void align_stereo_joint_no_chop (std::vector<float> &L,
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

// c_deconvolver "passthrough" red tape for audio backend

bool c_deconvolver::audio_init (std::string clientname,
           int samplerate, bool stereo_out) {
#ifdef USE_JACK
  audio = new c_jackclient (this);
  if (audio && audio->init (clientname, samplerate, stereo_out))
    return true;
#endif
  return false;
}

bool c_deconvolver::audio_shutdown () {CP
  if (audio) delete audio;
  return true; // for now
}

bool c_deconvolver::audio_ready () {CP
  return audio && audio->ready ();
}

bool c_deconvolver::audio_playback_done () {
  //debug ("play_go=%d", audio->play_go);
  return !audio->play_go;
}

audiostate c_deconvolver::get_audio_state () {CP
  return audio->state;
}

bool c_deconvolver::audio_play (const std::vector<float> &out) {CP
  return audio && audio->play (out);
}

bool c_deconvolver::audio_play (const std::vector<float> &out_l,
           const std::vector<float> &out_r) {CP
  return audio && audio->play (out_l, out_r);
}

bool c_deconvolver::audio_arm_record () {CP
  return audio && audio->arm_record ();
}

bool c_deconvolver::audio_rec () {
  return audio && audio->rec ();
}

bool c_deconvolver::audio_playrec (const std::vector<float> &out_l,
                      const std::vector<float> &out_r) {CP
  return audio && audio->playrec (out_l, out_r);
}

bool c_deconvolver::audio_stop () {CP
  return audio->stop ();
}

bool c_deconvolver::audio_stop_playback () {CP
  return audio->stop_playback ();
}

bool c_deconvolver::audio_stop_record () {CP
  return audio->stop_record ();
}

// TODO: these should just print messages like "recording/playing",
// "done" etc to stdout

int c_deconvolver::on_play_loop (void *data)         { return 0; }
int c_deconvolver::on_arm_rec_start (void *data)     { return 0; }
int c_deconvolver::on_arm_rec_stop (void *data)      { return 0; }
int c_deconvolver::on_record_start (void *data)      { return 0; }
int c_deconvolver::on_record_loop (void *data)       { return 0; }

int c_deconvolver::on_record_stop (void *data) { CP
  audio_stop_playback ();
  return 0;
}

int c_deconvolver::on_play_start (void *data)        {
  std::cout << "Playing sweep via " << audio-> backend_name << "... " << std::flush;
  return 0;
}

int c_deconvolver::on_play_stop (void *data)         {
  std::cout << "done.\n" << std::flush;
  return 0;
}
int c_deconvolver::on_playrec_start (void *data)     {
  std::cout << "Playing + recording sweep via JACK... " << std::flush;
  return 0; 
}


//#define DONT_USE_ANSI

#ifdef DONT_USE_ANSI

#ifdef DEBUG
#define ANSI_DUMMY
#else
#define ANSI_DUMMY
#endif

static void print_vu_meter (float, bool)    { ANSI_DUMMY }
static void ansi_cursor_move_x (int n)      { ANSI_DUMMY }
static void ansi_cursor_move_to_x (int n)   { ANSI_DUMMY }
static void ansi_cursor_move_y (int n)      { ANSI_DUMMY }
static void ansi_cursor_hide ()             { ANSI_DUMMY }
static void ansi_cursor_show ()             { ANSI_DUMMY }
static void ansi_clear_screen ()            { ANSI_DUMMY }
static void ansi_clear_to_endl ()           { ANSI_DUMMY }

#else

static void print_vu_meter (float peak, bool xrun) {
  int i, size = ANSI_VU_METER_MIN_SIZE;
  char buf [size] = { ' ' };
  int n = (int) ((float) (peak) * (float) (size - 6));
  if (n > size) n = size;
  
  for (i = 1; i <n; i++)   buf [i] = '-';
  for (; i < size; i++)    buf [i] = ' ';

  buf [0] = '[';
  buf [size - 6] = ']';
  buf [size - 1] = 0;
  
    
  //printf ("%s", buf);
  std::cout << buf << " \n" << std::flush;
}

static void ansi_cursor_move_x (int n) {
  if (n == 0)
    printf ("\x1b[G"); // special case: start of line
  else if (n < 0)
    printf ("\x1b[%dD", -n); // left
  else
    printf ("\x1b[%dC", n); // right
}

static void ansi_cursor_move_to_x (int n) {
  printf ("\x1b[%dG", n);
}

static void ansi_cursor_move_y (int n) {
  if (n < 0)
    printf ("\x1b[%dA", -n);
  else if (n > 0)
    printf ("\x1b[%dB", n);
}

static void ansi_cursor_hide () {
  printf ("\x1b[?25l");
}

static void ansi_cursor_show () {
  printf ("\x1b[?25h");
}

static void ansi_clear_screen () {
  printf ("\x1b[2J");
}

static void ansi_clear_to_endl () {
  printf ("\x1b[K");
}
#endif

int c_deconvolver::on_playrec_loop (void *data) {
  float pl, pr;
  
  static float show_l = 0;
  static float show_r = 0;
  static float hold_l = 0;
  static float hold_r = 0;
  static uint32_t hold_l_timestamp = 0;
  static uint32_t hold_r_timestamp = 0;
  static uint32_t clip_l_timestamp = 0;
  static uint32_t clip_r_timestamp = 0;
  
  if (audio->bufsize == 0) return -1;
  
  const float sec_per_redraw = ANSI_VU_REDRAW_EVERY;
  int redraw_every = (int) (sec_per_redraw * audio->samplerate / audio->bufsize);
  if (redraw_every < 1)
    redraw_every = 1;
  
  uint32_t now = playrec_loop_passes / redraw_every;
  
  if (playrec_loop_passes % redraw_every == 0) {
    pl = std::max (std::fabs (audio->peak_plus_l), std::fabs (audio->peak_minus_l));
    pr = std::max (std::fabs (audio->peak_plus_r), std::fabs (audio->peak_minus_r));
    //debug ("pl/r=%f,%f", (float) pl, (float) pr);
    
    if (pl > 0.99)
      clip_l_timestamp = playrec_loop_passes / redraw_every;
    if (pr > 0.99)
      clip_r_timestamp = playrec_loop_passes / redraw_every;
    
    if (pl > show_l) { hold_l = show_l = pl; hold_l_timestamp = now; }
    if (pr > show_r) { hold_r = show_r = pl; hold_r_timestamp = now; }
    
    print_vu_meter (show_l, audio->xrun);
    
    if (audio->is_stereo) {  // 2 vu meters
      ansi_cursor_move_x (0);
      print_vu_meter (show_r, audio->xrun);
      ansi_cursor_move_x (0);
      ansi_cursor_move_y (-2);
    } else {       // just 1 vu meter
      ansi_cursor_move_y (-1);
    }
    
    show_l -= ANSI_VU_FALL_SPEED;
    if (show_l < 0) show_l = 0;
    show_r -= ANSI_VU_FALL_SPEED;
    if (show_r < 0) show_r = 0;
    
    audio->peak_acknowledge ();
    //std::cout << "peak L=" << pl << ", peak R=" << pr << "\n" << std::flush;
  }
  
  playrec_loop_passes++;
  return 0; 
}

int c_deconvolver::on_playrec_stop (void *data)      {
  std::cout << "done.\n" << std::flush;
  return 0;
}

void c_deconvolver::update_peak_data () {
}

int c_deconvolver::on_arm_rec_loop (void *data) {
  return on_playrec_loop (data);
}

// other c_deconvolver member functions
 
c_deconvolver::c_deconvolver (struct s_prefs *prefs, std::string name) {
  set_prefs (prefs);
  set_name (name);
}

c_deconvolver::~c_deconvolver () {
}

void c_deconvolver::set_name (std::string name) {
  audio_clientname = name;
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
  
  bool is_stereo = (prefs_->request_stereo && right.size () > 1);

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
    std::cerr << (prefs_->request_stereo ? "Wet buffers are empty.\n" :
                                           "Wet buffer is empty.\n");
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
  
  std::string align_names [] = {
    "marker detection",
    "marker detection on dry",
    "silence detection",
    "no alignment"
  };
  
  if (prefs_->align < 0 || prefs_->align >= align_method_max) {
    std::cerr << "Error: invalid alignment method id: " << prefs_->align;
    return false;
  } else {
    std::cerr << "Alignment method: " << align_names [prefs_->align] << " (id "
              << prefs_->align << ")\n";
  }
  
  std::cerr << "\nDry offset: " << dry_offset_
            << "\nWet offset: " << wet_offset_ << "\n\n";

  // --- deconvolve left ---
  if (!calc_ir_raw (wet_L_, dry_, irL, ir_length_samples)) {
    std::cerr << "Error: calc_ir_raw failed for "
              << (prefs_->request_stereo ? "left" : "mono")
              << " channel.\n";
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
  float lp_cut = std::min ((float) LOWPASS_F, cutoff);

  if (irL.size () > 0) {
#ifdef HIGHPASS_F
    highpass_ir (irL, prefs_->sweep_sr, HIGHPASS_F);
#endif
#ifdef LOWPASS_F
    lowpass_ir (irL, prefs_->sweep_sr, lp_cut);
#endif
  }
  if (irR.size () > 0) {
#ifdef HIGHPASS_F
    highpass_ir (irR, prefs_->sweep_sr, HIGHPASS_F);
#endif
#ifdef LOWPASS_F
    lowpass_ir (irR, prefs_->sweep_sr, lp_cut);
#endif
  }
  
  // TODO: figure out why this even works for L/R delay
  //align_stereo_independent (irL, irR, 0);
  
  /*if (!irL.empty () || !irR.empty ()) {
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
  
  debug ("have_right_channel=%d, have_right_energy=%d, irR.empty=%d",
         (int) have_right_channel, have_right_energy, irR.empty ());
         
  if (prefs_->verbose && have_right_channel && !have_right_energy) {
      std::cerr << "Note: Right channel silent, writing MONO IR\n";
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
  
  // nope, we're overwriting user's -n / --length parameter
  //ir_length_samples = std::max ((size_t) ir_length_samples,
  //                              (size_t) (MAX_IR_LEN * prefs_->sweep_sr));

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
    double hr = nr / (mag2 + eps);
    double hi = ni / (mag2 + eps);

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
    if (prefs_) prefs_->sweep_sr = sr;
    return true;
  }
  return (samplerate_ == sr);
}

int c_deconvolver::samplerate () {
  return samplerate_; //prefs_->sweep_sr;
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
  
  //float thr_start = db_to_linear (thr_start_db);
  //float thr_end = db_to_linear (thr_end_db);

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
  if (fade_end > 0)
    for (size_t i = fade_end; i < len; ++i) {
      float t = float (i - fade_end) / float (len - fade_end - 1);
      float g = 1.0f - t;
      //if (fade_pow2)
          g *= g;
      
      if (i < L.size ()) L [i] *= g;
      if (i < R.size ()) R [i] *= g;
    }
  
  
  // 3) Remove common leading silence
  /*size_t*/int64_t firstl, firstr, first_nonsilent;
  len = L.size();
  if (hasR) len = std::min(L.size(), R.size());

  if (!zeropeak) {
    // old behavior -> set first_nonsilent at ~= start
    if (thr_start_db > 0.0f) {
      debug("thr_start_db=%f, skipping start trim", thr_start_db);
      first_nonsilent = 0;
    } else {
      firstl = find_first_nonsilent(L, thr_start_db);
      firstr = hasR ? find_first_nonsilent(R, thr_start_db) : firstl;
      first_nonsilent = std::min(firstl, firstr);
    }
    first_nonsilent = std::max (first_nonsilent, prefs_->sweep_offset_smp);
  } else {
    // new behavior: place the peak at ~= sweep_offset_smp by trimming to
    // the first "non-silent" sample at orafter (peak_idx - sweep_offset_smp).
    // We pretend everything before (peak_idx - sweep_offset_smp) is silence.

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
    //size_t len = L.size (); -- we already have this above
    //if (hasR) len = std::min (L.size (), R.size ());

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
  
  first_nonsilent = std::max<int64_t> (first_nonsilent, 0);
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

