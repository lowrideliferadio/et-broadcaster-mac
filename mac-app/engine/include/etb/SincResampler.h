// ET Broadcaster — variable-speed band-limited resampler for scratching.
// Windowed-sinc (Kaiser) with continuously variable cutoff: when |speed| > 1 the kernel is
// stretched (cutoff = 1/|speed|) so content above the output Nyquist is filtered, not aliased.
// Fractional positions are read from a finely oversampled kernel table with linear interpolation,
// so position and speed may change every sample (incl. through zero) without clicks or zipper.
#pragma once
#include <vector>
#include <cstddef>

namespace etb {

class SincResampler {
public:
    // taps = zero-crossings span at speed <= 1 (>= 32 recommended). maxStretch caps the kernel
    // width at high speeds (CPU bound); above it the cutoff keeps falling via a cheaper path.
    explicit SincResampler(int taps = 64, int oversample = 1024, double kaiserBeta = 9.0, double maxStretch = 6.0);

    // Read one sample of channel `ch` from interleaved source `src` (frames, channels)
    // at fractional frame position `pos`, band-limited for playback speed `speed`.
    float read(const float* src, std::size_t frames, int channels, int ch, double pos, double speed) const;

    // Read all channels at once (faster; out has `channels` floats).
    void readFrame(const float* src, std::size_t frames, int channels, double pos, double speed, float* out) const;

    int taps() const { return taps_; }

private:
    float kernel(double x) const;   // x in zero-crossings, |x| < taps/2
    int taps_, os_; double maxStretch_;
    std::vector<float> table_;      // half kernel, index = |x| * os
};

// Baseline for comparison in tests (what most "simple" players do).
float readLinear(const float* src, std::size_t frames, int channels, int ch, double pos);

} // namespace etb
