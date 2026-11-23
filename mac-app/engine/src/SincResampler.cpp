#include "etb/SincResampler.h"
#include <cmath>
#include <algorithm>

namespace etb {

static double besselI0(double x) {
    double sum = 1.0, term = 1.0, q = x * x / 4.0;
    for (int k = 1; k < 64; ++k) { term *= q / (double(k) * k); sum += term; if (term < 1e-12 * sum) break; }
    return sum;
}

SincResampler::SincResampler(int taps, int oversample, double beta, double maxStretch)
    : taps_(std::max(8, taps & ~1)), os_(oversample), maxStretch_(maxStretch) {
    const int half = taps_ / 2;
    table_.resize(size_t(half * os_ + 2), 0.0f);
    const double i0b = besselI0(beta);
    for (int i = 0; i <= half * os_; ++i) {
        double x = double(i) / os_;
        double sinc = x == 0.0 ? 1.0 : std::sin(M_PI * x) / (M_PI * x);
        double r = x / half;
        double w = r >= 1.0 ? 0.0 : besselI0(beta * std::sqrt(1.0 - r * r)) / i0b;
        table_[size_t(i)] = float(sinc * w);
    }
}

inline float SincResampler::kernel(double x) const {
    x = std::fabs(x) * os_;
    size_t i = size_t(x); double f = x - double(i);
    if (i + 1 >= table_.size()) return 0.0f;
    return float(table_[i] + (table_[i + 1] - table_[i]) * f);
}

void SincResampler::readFrame(const float* src, std::size_t frames, int channels, double pos, double speed, float* out) const {
    // cutoff scale c in (0,1]: 1 at |speed|<=1, 1/|speed| above (anti-alias)
    double a = std::fabs(speed);
    double c = a <= 1.0 ? 1.0 : 1.0 / std::min(a, maxStretch_);
    const double half = taps_ / 2.0;
    const double reach = half / c;                         // source frames each side
    long i0 = long(std::floor(pos - reach)) + 1, i1 = long(std::floor(pos + reach));
    for (int ch = 0; ch < channels; ++ch) out[ch] = 0.0f;
    double norm = 0.0;
    for (long i = i0; i <= i1; ++i) {
        double w = c * kernel((double(i) - pos) * c);
        norm += w;
        if (i < 0 || size_t(i) >= frames) continue;       // silence outside the track (lead-in/out)
        const float* s = src + size_t(i) * size_t(channels);
        for (int ch = 0; ch < channels; ++ch) out[ch] += float(w) * s[ch];
    }
    // DC-normalise the kernel (removes tiny ripple from table interpolation; keeps gain exactly 1)
    if (norm > 1e-9) for (int ch = 0; ch < channels; ++ch) out[ch] = float(out[ch] / norm);
}

float SincResampler::read(const float* src, std::size_t frames, int channels, int ch, double pos, double speed) const {
    float tmp[8]; readFrame(src, frames, channels, pos, speed, tmp); return tmp[ch];
}

float readLinear(const float* src, std::size_t frames, int channels, int ch, double pos) {
    long i = long(std::floor(pos)); double f = pos - double(i);
    auto at = [&](long k) { return (k < 0 || size_t(k) >= frames) ? 0.0f : src[size_t(k) * size_t(channels) + size_t(ch)]; };
    return float(at(i) + (at(i + 1) - at(i)) * f);
}

} // namespace etb
