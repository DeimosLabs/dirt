
/* DIRT - Delt's Impulse Response Tool
 * Version 0.1
 *
 * Licensed under the GPL. See dirt.h and dirt.cpp for more info.
 */

#include "dirt.h"
#include "deconvolv.h"
#ifdef USE_JACK
#include "jack.h"
#endif

#ifdef DEBUG
#define CMDLINE_DEBUG
#include "cmdline/cmdline.h"
#define debug(...) cmdline_debug(stderr,ANSI_CYAN,__FILE__,__LINE__,__FUNC__,__VA_ARGS__)
#else
#define debug(...)
#define CP
#define BP
#endif

////////////////////////////////////////////////////////////////////////////////
// GLOBAL / STATIC FUNCTIONS

size_t find_first_nonsilent_from (const std::vector<float> &buf,
                                   float silence_thresh_db,
                                   size_t from) { CP
  float silence_thresh = db_to_linear (silence_thresh_db);
  for (size_t i = 0; i < buf.size (); i++) {
    if (std::fabs (buf [i]) > silence_thresh) return i;
  }
  return buf.size ();
}

size_t find_last_nonsilent_from (const std::vector<float> &buf,
                                   float silence_thresh_db,
                                   size_t from) { CP
  float silence_thresh = db_to_linear (silence_thresh_db);
  //for (size_t i = 0; i < buf.size (); ++i) {
  size_t start_from = std::min (from, (size_t) buf.size () - 1);
  for (ssize_t i = start_from; i >= 0; --i) {
    if (std::fabs (buf [i]) > silence_thresh) return (size_t) i;
  }
  return buf.size ();
}

inline size_t find_first_nonsilent (const std::vector<float> &buf,float thr) { CP
  return find_first_nonsilent_from (buf, thr, 0);
}

inline size_t find_last_nonsilent (const std::vector<float> &buf, float thr) { CP
  return find_last_nonsilent_from (buf, thr, buf.size () - 1);
}

size_t find_peak (const std::vector<float> &buf) {
  int i = 0;
  int len = buf.size ();
  int peakpos = 0;
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
  debug ("start, sr=%f, cut=%f", (float) samplerate, (float) cutoff_hz);
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

void highpass_ir (std::vector<float> &irf, double samplerate, double cutoff) {
  debug ("start, sr=%f, cut=%f", (float) samplerate, (float) cutoff);
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
void dc_kill (std::vector<float>& irf, float samplerate) {
  const float fc = 30.0f;
  const float w0 = 2.0f * M_PI * fc / samplerate;
  const float alpha = (1.0f - std::sin (w0)) / std::cos (w0);

  float prev = irf [0];
  for (size_t i = 1; i < irf.size (); ++i) {
    float v = irf [i];
    irf[i] = irf [i] - prev + alpha * irf [i];
    prev = v;
  }

  // reverse pass to (supposedly) make it zero phase
  prev = irf.back ();
  for (size_t i = irf.size () - 2; i > 0; --i) {
    float v = irf [i];
    irf[i] = irf [i] - prev + alpha * irf [i];
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
  
  // first non-silent sample in the whole file
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

  // analyze a short window after i0 to see if it's our marker square wave
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

  // count sign flips above some amplitude
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
    // not enough flips for a clean square wave, treat as no marker
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

  // for a square wave, 2 flips per full period
  double est_period = 2.0 * avg_samples_per_flip;
  double est_freq   = (est_period > 0.0)
                      ? (double) samplerate / est_period
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
  
  // marker looks valid -> decide where it ends
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

  // skip gap (silence) after marker
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
  
  sweep_start--;
  if (sweep_start < 0) return 0;
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
        (ignore_marker && prefs.align == align_method::MARKER_DRY)
            ? align_method::MARKER   // for wet: same as plain marker align
            : prefs.align;
  
    switch (align) {
      case align_method::MARKER:
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

      case align_method::MARKER_DRY:
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
  
      case align_method::SILENCE:
        start = find_first_nonsilent (buf, prefs.sweep_silence_db);
        break;
  
      case align_method::NONE:
        start = 0;
        break;
    }

    if (prefs.verbose) {
      std::cerr << "start="      << start
                << " marker_len=" << marker_len
                << " gap_len="    << gap_len << "\n";
    }
  
    debug("end");
    
    if (buf [0] <= 0.00001 && start <= 1) return 0; // avoid offsets of 1 silent sample
    return start;
}

// DRY: use full logic (marker, silence, caching…)
size_t detect_dry_sweep_start (const std::vector<float> &dry,
                                      int samplerate,
                                      s_prefs &prefs) {
    return detect_sweep_start (dry, samplerate, prefs, /*ignore_marker=*/false);
}

// WET: if align_method::MARKER_DRY, reuse cached dry preroll;
// otherwise just use normal logic on wet
size_t detect_wet_sweep_start (const std::vector<float> &wet_l,
                               const std::vector<float> &wet_r,
                                      int samplerate,
                                      s_prefs &prefs) {
  debug("detect_wet_sweep_start: start");
  
  std::vector<float> mix;
  
  if (!wet_r.empty ()) {
    const size_t n = std::min (wet_l.size (), wet_r.size ());
    mix.resize (n);
    for (size_t i = 0; i < n; ++i) {
      mix [i] = 0.5f * (wet_l [i] + wet_r [i]);
    }
    return detect_sweep_start (mix, samplerate, prefs);
  } else {
    return detect_sweep_start (wet_l, samplerate, prefs);
  }
    

  size_t start = 0;

  if (prefs.align == align_method::MARKER_DRY) {
    // 1) where does wet actually start?
    size_t wet_first = find_first_nonsilent (mix, prefs.sweep_silence_db);

    // 2) skip the same marker+gap duration we found in dry
    start = wet_first
          + prefs.cache_dry_marker_len
          + prefs.cache_dry_gap_len;

    if (prefs.verbose) {
        std::cerr << "mix: first_nonsilent=" << wet_first
                  << " using cached marker_len=" << prefs.cache_dry_marker_len
                  << " gap_len=" << prefs.cache_dry_gap_len
                  << " -> start=" << start << "\n";
    }
  } else {
    // plain marker / silence / none logic on wet buffer
    start = detect_sweep_start (mix, samplerate, prefs, /*ignore_marker=*/false);
  }

  debug("detect_wet_sweep_start: end");
  return start;
}

// wrapper functions to convert our data to/from c_wavebuffer<->vectors of floats
bool read_wav (const char *filename,
               c_wavebuffer &left,
               c_wavebuffer &right) {
  debug ("start, filename=%s", filename);

  std::vector<float> vl;
  std::vector<float> vr;
  int sr;
  
  int ret = read_wav (filename, vl, vr, sr);
  left.import_from (vl);
  right.import_from (vr);  
  
  left.set_samplerate (sr);
  right.set_samplerate (sr);
  
  return ret;
  debug ("end");
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
    debug ("return (mono), readFrames=%d, got %ld",
           readFrames, (long int) left.size ());
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
    
    debug ("return (stereo), readFrames=%d, got %d, %d", readFrames,
           (long int) left.size (), (long int) right.size ());
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
  debug ("start, samplerate=%d", samplerate);
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

bool write_mono_wav (const char *path,
                     c_wavebuffer &data) { CP
  std::vector<float> datav;
  data.export_to (datav);
  return write_mono_wav (path, datav, data.get_samplerate ());
}
                            
bool write_stereo_wav (const char *path,
                       c_wavebuffer &l,
                       c_wavebuffer &r) { CP
  std::vector<float> lv;
  std::vector<float> rv;
  l.export_to (lv);
  r.export_to (rv);
  return write_stereo_wav (path, lv, rv, l.get_samplerate ());
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



////////////////////////////////////////////////////////////////////////////////
// c_deconvolver

bool c_deconvolver::set_sweep_dry (c_wavebuffer &buf) {
  debug ("start, size=%ld", buf.size ());
  if (buf.empty()) {
    std::cerr << "Got empty dry buffer\n";
    return false;
  }
  int sr = buf.get_samplerate ();
  if (!set_samplerate (sr)) {
    std::cerr << "Samplerate mismatch for dry buffer: already got wet " << sr
              << " Hz, new (dry) " << buf.get_samplerate () << " Hz\n";
    return false;
  }

  //sweep_dry = buf;
  buf.export_to (sweep_dry);
  //have_dry_ = true;

  /*if (prefs->align != align_method::NONE) {
    dry_offset = detect_dry_sweep_start (sweep_dry, samplerate_, *prefs);
    if (prefs->verbose) {
      std::cerr << "Dry sweep start offset (buffer): "
                << dry_offset << " samples\n";
    }
  }*/

  debug ("end");
  return true;
}

bool c_deconvolver::set_sweep_wet (c_wavebuffer &buf_l,
                                   c_wavebuffer &buf_r) {
  debug ("start, sizes=%ld,%ld", buf_l.size (), buf_r.size ());
  if (buf_l.empty () && buf_r.empty ()) {
    std::cerr << (prefs->request_stereo ? "Wet buffers are empty.\n" :
                                          "Wet buffer is empty.\n");
    return false;
  }
  if (buf_r.size () > 0 && buf_l.get_samplerate () != buf_l.get_samplerate ()) {
    std::cerr << "Samplerate mismatch between wet buffer channels.\n";
    return false;
  }
  int sr = buf_l.get_samplerate ();
  if (!set_samplerate (sr)) {
    std::cerr << "Samplerate mismatch: audio backend refused " << sr
              << " Hz for wet)\n";
    return false;
  }
  
  //sweep_wet_l = buf_l;
  //sweep_wet_r = buf_r;
  buf_l.export_to (sweep_wet_l);
  buf_r.export_to (sweep_wet_r);
  
  /*if (prefs->align != align_method::NONE) {
    if (!sweep_wet_r.empty ()) {
      const size_t n = std::min (sweep_wet_l.size (), sweep_wet_r.size ());
      std::vector<float> mix (n);
      for (size_t i = 0; i < n; ++i) {
        mix[i] = 0.5f * (sweep_wet_l[i] + sweep_wet_r[i]);
      }
      wet_offset = detect_wet_sweep_start (mix, samplerate_, *prefs);
    } else {
      wet_offset = detect_wet_sweep_start (sweep_wet_l, samplerate_, *prefs);
    }

    if (prefs->verbose) {
      std::cerr << "Wet sweep (buffer) start offset: "
                << wet_offset << " samples\n";
    }
  }*/

  debug ("end");
  return true;
}

bool c_deconvolver::has_wet_l () {
  return (sweep_wet_l.size () > 0);
}

bool c_deconvolver::has_wet_r () {
  return (sweep_wet_r.size () > 0);
}

// passthrough functions for c_audioclient & derived
// these just call hooks and corresponding c_audioclient functions 



////////////////////////////////////////////////////////////////////////////////
// c_deconvolver


c_deconvolver::c_deconvolver (struct s_prefs *prefs) { CP
  set_prefs (prefs);
}

c_deconvolver::~c_deconvolver () { CP
}

bool c_deconvolver::has_dry () {
  bool ret = /*have_dry_ && */(sweep_dry.size () > 0);
  //debug ("ret=%d", (int) ret);
  return ret;
}

void c_deconvolver::set_prefs (struct s_prefs *_prefs) {
  if (_prefs)
    prefs = _prefs;
  else
    prefs = &default_prefs;
}

bool c_deconvolver::render_ir (c_wavebuffer &out_l,
                               c_wavebuffer &out_r,
                               size_t max_length) {
  debug ("start, max_length=%ld, got dry=%ld, wet=%ld,%ld", max_length,
         sweep_dry.size (), sweep_wet_l. size (), sweep_wet_r.size ());
  if (!prefs) {
    std::cerr << "No prefs structure, aborting\n";
    return false;
  }
  std::vector<float> ir_l, ir_r;
  
  if (prefs->align == align_method::NONE) {
    dry_offset = 0;
    wet_offset = 0;
  }
  
  std::string align_names [] = {
    "marker detection",
    "marker detection on dry",
    "silence detection",
    "no alignment"
  };
  
  //align_sweeps ();
  debug ("Detecting sweep offsets:");
  dry_offset = detect_dry_sweep_start (sweep_dry, samplerate_, *prefs);
  wet_offset = detect_wet_sweep_start (sweep_wet_l, sweep_wet_r, samplerate_, *prefs);
  
  if ((int) prefs->align < 0 || prefs->align >= align_method::MAX) {
    std::cerr << "Error: invalid alignment method id: " << (int) prefs->align;
    return false;
  } else {
    std::cerr << "Alignment method: " << align_names [(int) prefs->align] << " (id "
              << (int) prefs->align << ")\n";
  }
  
  float dry_sec = (float) dry_offset / (float) samplerate_;
  float wet_sec = (float) wet_offset / (float) samplerate_;
  std::cerr << "\nDry offset: " << dry_offset << " (" << dry_sec << " sec)"
            << "\nWet offset: " << wet_offset << " (" << wet_sec << " sec) \n\n";

  // --- deconvolve left ---
  if (!calc_ir_raw (sweep_wet_l, sweep_dry, ir_l, max_length)) { CP
    std::cerr << "Error: calc_ir_raw failed for "
              << (prefs->request_stereo ? "left" : "mono")
              << " channel.\n";
    return false;
  }
  
  // --- deconvolve right if we have input data ---
  const bool have_right_channel = !sweep_wet_r.empty ();
  if (have_right_channel) {
    if (!calc_ir_raw (sweep_wet_r, sweep_dry, ir_r, max_length)) {  CP
      std::cerr << "Error: calc_ir_raw failed for right channel.\n";
      return false;
    }
  }
  
  // get rid of junk above hearing threshold, say 90% of nyquist freq
  /*float cutoff = (prefs->sweep_sr / 2) * 0.9;
  // ...or just a fixed frequency
  //float cutoff = 18000;
  float lp_cut = std::min ((float) LOWPASS_F, cutoff);*/
  
  if (ir_l.size () > 0) {
    if (prefs->hpf_mode) highpass_ir (ir_l, prefs->sweep_sr, prefs->hpf);
    if (prefs->lpf_mode) lowpass_ir (ir_l, prefs->sweep_sr, prefs->lpf);
  }
  if (ir_r.size () > 0) {
    if (prefs->hpf_mode) highpass_ir (ir_r, prefs->sweep_sr, prefs->hpf);
    if (prefs->lpf_mode) lowpass_ir (ir_r, prefs->sweep_sr, prefs->lpf);
  }
  
  // TODO: figure out why this even works for L/R delay
  //align_stereo_independent (irL, irR, 0);
  
  /*if (!irL.empty () || !irR.empty ()) {
    align_stereo_joint_no_chop (irL, irR, 0); // move reference peak to index 0
  }*/
  //normalize_and_trim_stereo (irL, irR, prefs->zeropeak);
  normalize_and_trim_stereo (ir_l,
                             ir_r,
                             prefs->zeropeak,
                             prefs->ir_start_silence_db,  // start trim
                             prefs->ir_silence_db,        // tail trim (from -T)
                             0.05f);             // 5% length fade at end
  
  out_l.import_from (ir_l);
  out_r.import_from (ir_r);
  out_l.set_samplerate (prefs->sweep_sr);
  out_r.set_samplerate (prefs->sweep_sr);
  
  debug ("resulting output l=%d, r=%d", ir_l.size (), ir_r.size ());
  
  debug ("end");
  return true;
}

bool c_deconvolver::import_file (const char *in_filename,
                                      c_wavebuffer &in_l,
                                      c_wavebuffer &in_r) {
  
  debug ("start in_filename=%s", in_filename);

  int sr = 0;
  
  std::vector<float> vl, vr;
  if (!read_wav (in_filename, vl, vr, sr)) {
    std::cerr << "Failed to load wav file: " << in_filename << "\n";
    return false;
  }
  debug ("read %ld, %ld", (long int) vl.size (), (long int) vr.size ());
  bool is_stereo = (prefs->request_stereo && in_r.size () > 1);

  if (!set_samplerate (sr)) {
    std::cerr << "Samplerate mismatch in sweep file: " << in_filename << "\n";
    return false;
  }

  debug ("end");
  return true;
}

bool c_deconvolver::export_file (const char *out_filename,
                                      c_wavebuffer &out_l,
                                      c_wavebuffer &out_r) {
  debug ("start, got sizes %ld, %ld", (long int) out_l.size (), (long int) out_r.size ());
  std::vector<float> vl, vr;
  out_l.export_to (vl);
  out_r.export_to (vr);
  int samplerate = out_l.get_samplerate ();
  
  // Decide mono vs stereo based on whether right has any energy left
  bool have_right_channel = !sweep_wet_r.empty ();
  bool have_right_energy = false;
  const float thr = db_to_linear (-90);
  for (float v : vr) {
    if (std::fabs (v) > thr) {
      have_right_energy = true;
      break;
    }
  }
  debug ("buffer sizes: %d, %d", vl.size (), vr.size ());
  
  debug ("have_right_channel=%d, have_right_energy=%d, vr.empty=%d",
         (int) have_right_channel, (int) have_right_energy, (int) vr.empty ());
         
  if (prefs->verbose && have_right_channel && !have_right_energy) {
      std::cerr << "Note: Right channel silent, writing MONO wav file\n";
  }
  
  
  if (!have_right_channel || !have_right_energy || vr.empty ()) {
    // MONO IR
    if (!write_mono_wav (out_filename, vl, samplerate)) {
      std::cerr << "Error: failed to write mono wav file to " << out_filename << "\n";
      return false;
    }
    std::cout << "Wrote MONO wav file: " << out_filename
              << " (" << vl.size () << " samples @ " << samplerate << " Hz)\n";
    debug ("return");
    return true;
  }
  
  // STEREO IR
  size_t len = std::max (vl.size (), vr.size ());
  vl.resize (len, 0.0f);
  vr.resize (len, 0.0f);

  if (!write_stereo_wav (out_filename, vl, vr, samplerate)) {
    std::cerr << "Error: failed to write stereo wav file to " << out_filename << "\n";
    return false;
  }
  
  std::cout << "Wrote STEREO wav file: " << out_filename
            << " (" << len << " samples @ " << samplerate << " Hz)\n";  

  debug ("end");
  return true;
}

// ok THIS WORKS DON'T TOUCH IT!!!
bool c_deconvolver::calc_ir_raw (const std::vector<float> &wet,
                                 const std::vector<float> &dry,
                                 std::vector<float>       &ir_raw,
                                 long                     ir_length_samples) {
  debug ("start, wet: %d, dry:%d, ir_length_samples=%ld",
         wet.size (), dry.size (), ir_length_samples);
  if (dry.empty ()) {
    std::cerr << "Error: dry buffer is empty.\n";
    return false;
  }
  if (wet.empty ()) {
    std::cerr << "Error: wet buffer is empty.\n";
    return false;
  }
  
  // Use class member offsets.
  // For stereo, wet_offset is the "common" silence we decided to skip.
  // Use initial offsets detected from sweeps
  size_t wet_offset = wet_offset;
  size_t dry_offset = dry_offset;
  size_t out_offset = 0;
  
  // Apply user offset (positive delays wet, negative delays dry)
  int offs = prefs->sweep_offset_smp;
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
  //                              (size_t) (MAX_IR_LEN * prefs->sweep_sr));
  
  if (prefs->dump_debug) {
    std::string prefix = prefs->dump_prefix;

    std::string dryname = prefix + "-dry.wav";
    std::string wetname = prefix + "-wet.wav";

    debug ("Dumping debug buffers: %s, %s", dryname.c_str (), wetname.c_str ());

    dump_double_wav (dryname, xTime, prefs->sweep_sr, usable_dry);
    dump_double_wav (wetname, yTime, prefs->sweep_sr, usable_wet);
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
  size_t max_ir_samples = (size_t) std::llround (MAX_IR_LEN * prefs->sweep_sr);
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
  if (prefs && prefs->headroom_seconds > 0.0f) {
    out_offset = (size_t) std::llround (
        (double) prefs->headroom_seconds * (double) samplerate_);
  }

  ir_raw.resize (irLen + out_offset);

  // headroom: leading zeros
  for (size_t i = 0; i < out_offset; ++i) {
    ir_raw [i] = 0.0f;
  }

  // copy IR starting at out_offset
  for (size_t i = 0; i < irLen; ++i) {
    ir_raw [i + out_offset] = (float) hTime [i];
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
bool c_deconvolver::set_samplerate (int sr) {
  if (samplerate_ == 0) {
    samplerate_ = sr;
    if (prefs) prefs->sweep_sr = sr;
    return true;
  }
  return (samplerate_ == sr);
}

int c_deconvolver::get_samplerate () {
  return samplerate_; //prefs->sweep_sr;
}

void c_deconvolver::clear_dry () {
  sweep_dry.clear ();
  if (!has_wet_l () && !has_wet_r ())
    samplerate_ = 0;
}

void c_deconvolver::clear_wet () {
  sweep_wet_r.clear ();
  if (!has_dry ())
    samplerate_ = 0;
}

void c_deconvolver::clear (bool dry, bool wet) {
  if (dry) clear_wet ();
  if (wet) clear_wet ();
}

void c_deconvolver::normalize_and_trim_stereo (std::vector<float> &L,
                                               std::vector<float> &R,
                                               bool zeropeak,
                                               float thr_start_db, // > 0 -> don't trim start
                                               float thr_end_db,   // > 0 -> don't trim end
                                               float fade_end_percent) {
  debug ("start, zeropeak=%s, thr_start=%f, thr_end=%f, fade_end_percent=%f",
        zeropeak ? "true" : "false", thr_start_db, thr_end_db, fade_end_percent);
  const size_t tail_pad       = 32; // some IR loaders choke without this
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
  
  const float norm = prefs->normalize_amp / peak;
  
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
    first_nonsilent = std::max (first_nonsilent, prefs->sweep_offset_smp);
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
        (prefs && prefs->sweep_offset_smp > 0)
        ? (size_t) prefs->sweep_offset_smp
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
    debug ("return (all silence after norm)");
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

