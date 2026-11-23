// Offline render tests for the scratch engine (run on Linux or macOS, no audio device needed).
// Writes WAV renders to tests/renders/ and prints PASS/FAIL with measured numbers.
#include "etb/Deck.h"
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstdint>
#include <vector>
#include <string>
#include <chrono>
#include <algorithm>
#include <functional>
using namespace etb;
static const double SR = 44100.0;
static int passed = 0, failed = 0;
static void report(bool ok, const char* name, const std::string& detail) {
    (ok ? passed : failed)++; std::printf("[%s] %s  %s\n", ok ? "PASS" : "FAIL", name, detail.c_str()); }
static std::string fmt(const char* f, double a, double b = 0, double c = 0) { char buf[256]; std::snprintf(buf, sizeof buf, f, a, b, c); return buf; }
static double db(double x) { return 20.0 * std::log10(std::max(x, 1e-12)); }

static void writeWav(const std::string& path, const std::vector<float>& l, const std::vector<float>& r) {
    FILE* f = std::fopen(path.c_str(), "wb"); if (!f) return;
    uint32_t n = uint32_t(l.size()), data = n * 4, rate = 44100, br = rate * 4; uint16_t fmt1 = 1, ch = 2, ba = 4, bits = 16; uint32_t sz = 36 + data, sixteen = 16;
    std::fwrite("RIFF", 1, 4, f); std::fwrite(&sz, 4, 1, f); std::fwrite("WAVEfmt ", 1, 8, f); std::fwrite(&sixteen, 4, 1, f);
    std::fwrite(&fmt1, 2, 1, f); std::fwrite(&ch, 2, 1, f); std::fwrite(&rate, 4, 1, f); std::fwrite(&br, 4, 1, f); std::fwrite(&ba, 2, 1, f); std::fwrite(&bits, 2, 1, f);
    std::fwrite("data", 1, 4, f); std::fwrite(&data, 4, 1, f);
    for (uint32_t i = 0; i < n; ++i) { int16_t a = int16_t(std::clamp(l[i], -1.f, 1.f) * 32767), b = int16_t(std::clamp(r[i], -1.f, 1.f) * 32767); std::fwrite(&a, 2, 1, f); std::fwrite(&b, 2, 1, f); }
    std::fclose(f);
}
// radix-2 FFT magnitude spectrum (Blackman-Harris window), returns |X| normalised so a full-scale sine = 1
static std::vector<double> spectrum(const std::vector<float>& x, size_t off, size_t N) {
    std::vector<std::complex<double>> a(N); double wsum = 0;
    for (size_t i = 0; i < N; ++i) { double t = 2 * M_PI * i / (N - 1);
        double w = 0.35875 - 0.48829 * std::cos(t) + 0.14128 * std::cos(2 * t) - 0.01168 * std::cos(3 * t); wsum += w; a[i] = x[off + i] * w; }
    for (size_t i = 1, j = 0; i < N; ++i) { size_t bit = N >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap(a[i], a[j]); }
    for (size_t len = 2; len <= N; len <<= 1) { auto wl = std::polar(1.0, -2 * M_PI / len);
        for (size_t i = 0; i < N; i += len) { std::complex<double> w(1); for (size_t k = 0; k < len / 2; ++k) { auto u = a[i + k], v = a[i + k + len / 2] * w; a[i + k] = u + v; a[i + k + len / 2] = u - v; w *= wl; } } }
    std::vector<double> m(N / 2); for (size_t i = 0; i < N / 2; ++i) m[i] = 2.0 * std::abs(a[i]) / wsum; return m;
}
static double bandPeak(const std::vector<double>& m, double f0, double f1, size_t N) {
    double p = 0; for (size_t i = size_t(f0 * N / SR); i <= size_t(f1 * N / SR) && i < m.size(); ++i) p = std::max(p, m[i]); return p; }

// sine "record": sum of partials, analytic so we can compute the exact ideal output at any position
struct Tone { std::vector<double> f, a; double at(double pos) const { double s = 0; for (size_t i = 0; i < f.size(); ++i) s += a[i] * std::sin(2 * M_PI * f[i] * pos / SR); return s; } };
static std::vector<float> renderTone(const Tone& t, size_t frames) { std::vector<float> v(frames * 2); for (size_t i = 0; i < frames; ++i) v[2 * i] = v[2 * i + 1] = float(t.at(double(i))); return v; }

int main() {
    std::string out = "tests/renders/";
    // ---------------------------------------------------------------- 1. direction changes, through zero
    {
        Tone t{{220, 660, 1320}, {0.4, 0.2, 0.1}};
        size_t F = size_t(SR * 20); auto rec = renderTone(t, F);
        SincResampler rs(64);
        double pos = SR * 8, maxErrS = 0, maxErrL = 0, errS2 = 0, errL2 = 0, sig2 = 0; size_t N = size_t(SR * 4);
        std::vector<float> l(N), r(N);
        for (size_t i = 0; i < N; ++i) {
            double tt = i / SR, speed = 1.6 * std::sin(2 * M_PI * 2.0 * tt) + 0.3 * std::sin(2 * M_PI * 7.0 * tt); // fast back/forth, 8+ zero crossings/s
            double ys = rs.read(rec.data(), F, 2, 0, pos, speed), yl = readLinear(rec.data(), F, 2, 0, pos);
            // ideal: the analytic record at the same position, low-passed the same way only matters >Nyquist/|speed|; partials ≤1320 Hz stay in band
            double ideal = t.at(pos); l[i] = r[i] = float(ys);
            maxErrS = std::max(maxErrS, std::fabs(ys - ideal)); maxErrL = std::max(maxErrL, std::fabs(yl - ideal));
            errS2 += (ys - ideal) * (ys - ideal); errL2 += (yl - ideal) * (yl - ideal); sig2 += ideal * ideal;
            pos += speed;
        }
        writeWav(out + "01-direction-changes.wav", l, r);
        double snrS = 10 * std::log10(sig2 / errS2), snrL = 10 * std::log10(sig2 / errL2);
        report(snrS > 90 && db(maxErrS) < -85, "Back-and-forth scratch through zero: output matches ideal record (no clicks)",
               fmt("sinc SNR %.1f dB, worst sample error %.1f dBFS  (linear-interp baseline: SNR %.1f dB)", snrS, db(maxErrS), snrL));
    }
    // ---------------------------------------------------------------- 2. aliasing at high speed
    {
        Tone t{{15000}, {0.5}}; size_t F = size_t(SR * 4); auto rec = renderTone(t, F);
        SincResampler rs(64); size_t N = 1 << 15; std::vector<float> ys(N), yl(N); double pos = 1000, sp = 2.0;
        for (size_t i = 0; i < N; ++i) { ys[i] = rs.read(rec.data(), F, 2, 0, pos, sp); yl[i] = readLinear(rec.data(), F, 2, 0, pos); pos += sp; }
        auto ms = spectrum(ys, 0, N), ml = spectrum(yl, 0, N);
        double alias = SR - 30000;     // 30 kHz folds to 14.1 kHz
        double aS = bandPeak(ms, alias - 200, alias + 200, N), aL = bandPeak(ml, alias - 200, alias + 200, N);
        double floorS = 0; for (size_t i = size_t(500.0 * N / SR); i < ms.size(); ++i) floorS = std::max(floorS, ms[i]);
        writeWav(out + "02-aliasing-15k-at-2x.wav", ys, ys);
        report(db(aS / 0.5) < -80 && db(floorS / 0.5) < -80, "Anti-aliasing: 15 kHz tone played at 2x (would fold to 14.1 kHz)",
               fmt("alias %.1f dB re tone, worst spur %.1f dB  (linear-interp baseline alias: %.1f dB)", db(aS / 0.5), db(floorS / 0.5), db(aL / 0.5)));
    }
    // ---------------------------------------------------------------- 3. wide-band aliasing: noise at 3x, energy check
    {
        // record contains ONLY content between 8.5 and 20 kHz; at 3x all of it lies above the source-side cutoff
        // (22.05/3 = 7.35 kHz) — an ideal player outputs silence, a naive one folds it all back into the audio band.
        size_t F = size_t(SR * 6); std::vector<float> rec(F * 2, 0.0f); uint32_t s = 12345; std::vector<double> fr, ph;
        for (int k = 0; k < 40; ++k) { s = s * 1664525u + 1013904223u; fr.push_back(8500 + (s >> 8) % 11500); ph.push_back((s & 255) / 40.0); }
        for (size_t i = 0; i < F; ++i) { double v = 0; for (int k = 0; k < 40; ++k) v += std::sin(2 * M_PI * fr[k] * i / SR + ph[k]); rec[2 * i] = rec[2 * i + 1] = float(v * 0.02); }
        SincResampler rs(64); size_t N = 1 << 15; std::vector<float> y(N), yl(N); double pos = 2000, sp = 3.0, ein = 0, eo = 0, el = 0;
        for (size_t i = 0; i < N; ++i) { y[i] = rs.read(rec.data(), F, 2, 0, pos, sp); yl[i] = readLinear(rec.data(), F, 2, 0, pos); pos += sp; }
        for (size_t i = 0; i < N; ++i) { ein += rec[2 * (i + 2000)] * rec[2 * (i + 2000)]; eo += y[i] * y[i]; el += yl[i] * yl[i]; }
        writeWav(out + "03-hf-noise-at-3x.wav", y, y);
        report(10 * std::log10(eo / ein) < -60, "Spectral aliasing check: 40 tones 8.5–20 kHz at 3x (all above the new cutoff)",
               fmt("leaked/aliased energy %.1f dB re input  (linear-interp baseline: %.1f dB)", 10 * std::log10(eo / ein), 10 * std::log10(el / ein)));
    }
    // ---------------------------------------------------------------- 4. zipper: hand events at 60 Hz (mouse) → no sidebands
    {
        Tone t{{1000}, {0.5}}; size_t F = size_t(SR * 30); auto rec = renderTone(t, F);
        PlatterConfig c; c.mode = DeckMode::Vinyl; Deck d(c); d.setTrack(rec.data(), F, 2); d.cue(SR * 5);
        d.setMotor(true); std::vector<float> L(SR * 4), R(SR * 4); size_t done = 0; const int B = 128;
        // run up to speed, then hold the record and drag it at a steady 0.5x with 60 Hz mouse events
        while (done < SR * 1) { d.process(&L[done], &R[done], B); done += B; }
        d.touch(true); double evEvery = SR / 60.0, nextEv = done, rps = (100.0 / 3.0) / 60.0;
        while (done + B <= L.size()) {
            for (int k = 0; k < B; ++k) if (done + k >= nextEv) { d.handMove(0.5 * rps / 60.0, k); nextEv += evEvery; }
            d.process(&L[done], &R[done], B); done += B;
        }
        size_t N = 1 << 16; auto m = spectrum(L, size_t(SR * 1.8), N);
        double main = bandPeak(m, 450, 550, N), spur = std::max(bandPeak(m, 100, 420, N), bandPeak(m, 580, 3000, N));
        writeWav(out + "04-mouse-drag-60hz-events.wav", L, R);
        report(db(spur / main) < -70, "No zipper noise: steady drag from 60 Hz mouse events (500 Hz expected)",
               fmt("strongest sideband %.1f dB below the tone; main tone at %.1f dBFS", -db(spur / main), db(main)));
    }
    // ---------------------------------------------------------------- 5. platter physics
    {
        std::vector<float> rec(size_t(SR * 60) * 2, 0.0f); PlatterConfig c; c.startTime = 0.25; c.brakeTime = 0.4;
        Deck d(c); d.setTrack(rec.data(), rec.size() / 2, 2); d.setMotor(false); std::vector<float> L(128), R(128);
        long ran = 0; auto run = [&](double sec) { for (int i = 0; i < int(sec * SR / 128); ++i) { d.process(L.data(), R.data(), 128); ran += 128; } };
        d.setMotor(true); double tUp = 0; while (d.platterSpeed() < 0.999 && tUp < 3) { run(128 / SR); tUp += 128 / SR; }
        d.setMotor(false); double tDown = 0; while (d.platterSpeed() > 0.001 && tDown < 3) { run(128 / SR); tDown += 128 / SR; }
        report(std::fabs(tUp - 0.25) < 0.01 && std::fabs(tDown - 0.4) < 0.01, "Motor torque + brake times",
               fmt("start %.3f s (set 0.25), stop %.3f s (set 0.40)", tUp, tDown));
        // grab: hand stops the record, platter keeps spinning under the slipmat; release → catch-up
        d.setMotor(true); run(1.0); d.touch(true); run(0.3);
        double recHeld = d.recordSpeed(), platHeld = d.platterSpeed();
        d.touch(false); double tc = 0; while (d.recordSpeed() < 0.95 && tc < 1) { run(128 / SR); tc += 128 / SR; }
        report(std::fabs(recHeld) < 0.01 && platHeld > 0.99 && tc < 0.15, "Slipmat: record held still while platter spins, catches up on release",
               fmt("held record speed %.3f, platter %.3f, back to 95%% in %.0f ms", recHeld, platHeld, tc * 1000));
        // slip mode: scratch for 1 s, release → playback lands where it would have been
        d.config().slip = true; run(0.5); double startPos = d.position(); long r0 = ran; d.touch(true);
        for (int k = 0; k < 20; ++k) { d.handMove((k % 2 ? -0.25 : 0.25), 0); run(0.05); }
        d.touch(false); run(128 / SR * 2); double expect = startPos + double(ran - r0), got = d.position();
        report(std::fabs(got - expect) < SR * 0.01, "Slip mode: after a 1 s scratch the track resumes where it would have been",
               fmt("position error %.1f ms", std::fabs(got - expect) / SR * 1000));
        d.config().slip = false;
        // CDJ mode resumes fast
        d.config().mode = DeckMode::CDJ; d.touch(true); run(0.2); d.touch(false); double tcd = 0;
        while (d.recordSpeed() < 0.95 && tcd < 1) { run(128 / SR); tcd += 128 / SR; }
        report(tcd < 0.04, "CDJ mode: jog release resumes almost instantly", fmt("back to 95%% in %.1f ms", tcd * 1000));
        report(!([&] { d.setKeylock(true); d.touch(true); bool k = d.keylockActive(); d.touch(false); return k; })() && d.keylockActive(),
               "Key-lock is bypassed while the record is touched", "keylock on → inactive while touched, active after release");
    }
    // ---------------------------------------------------------------- 6. crossfader: curve, cut lag, hamster, click-free fast cut
    {
        Crossfader x; x.curve = 1.0; double a, b;
        x.gains(0.0, a, b); bool e0 = a == 1.0 && b == 0.0; x.gains(0.003, a, b); bool lag = b == 0.0; x.gains(0.02, a, b); bool cutIn = b > 0.99 && a > 0.99;
        x.hamster = true; x.gains(0.0, a, b); bool ham = b == 1.0 && a == 0.0; x.hamster = false;
        Crossfader sm; sm.gains(0.5, a, b); bool cp = std::fabs(a * a + b * b - 1.0) < 1e-9;
        report(e0 && lag && cutIn && ham && cp, "Crossfader: sharp cut-in within 2% travel, cut lag, hamster reverse, constant-power smooth curve",
               "edges / lag / cut-in / hamster / constant-power all as specified");
        // fast cut on a full-scale 1 kHz tone: measure click energy above 8 kHz while chopping at 12 Hz
        GainSmoother g; g.prepare(SR, 0.25); size_t N = 1 << 16; std::vector<float> y(N);
        for (size_t i = 0; i < N; ++i) { double fx = (int(i / (SR / 24)) % 2) ? 0.02 : 0.0; x.gains(fx, a, b); g.step(a, b); y[i] = float(g.b * 0.5 * std::sin(2 * M_PI * 1000 * i / SR)); }
        auto m = spectrum(y, 0, N); double hf = 0, tot = 0; for (size_t i = 1; i < m.size(); ++i) { tot += m[i] * m[i]; if (i * SR / N > 8000) hf += m[i] * m[i]; }
        writeWav(out + "06-crossfader-chop.wav", y, y);
        report(10 * std::log10(hf / tot) < -50, "Fast crossfader cuts are click-free (0.25 ms gain ramp)",
               fmt("energy above 8 kHz while chopping 12x/s: %.1f dB re total", 10 * std::log10(hf / tot)));
    }
    // ---------------------------------------------------------------- 7. MIDI jog decoding
    {
        JogDecoder j; j.kind = JogDecoder::RelTwosComplement; j.ticksPerRev = 2048; double r1 = j.feed7(5) + j.feed7(123);
        JogDecoder a; a.kind = JogDecoder::Absolute14; a.ticksPerRev = 16384; a.feed14(127, 120); double wrap = a.feed14(0, 8);  // 16376 → 8 = +16 ticks
        JogDecoder so; so.kind = JogDecoder::RelSignMagnitude; double sm = so.feed7(64 + 3) * 2048;
        report(std::fabs(r1 - 0.0) < 1e-12 && std::fabs(wrap - 16.0 / 16384) < 1e-12 && std::fabs(sm + 3) < 1e-9, "MIDI jog: relative (two's complement / sign-magnitude) and 14-bit absolute with wrap",
               fmt("rel +5-5 = %.4f rev, 14-bit wrap = %+.0f ticks, sign-mag = %+.0f ticks", r1, wrap * 16384, sm));
    }
    // ---------------------------------------------------------------- 8. real-time budget
    {
        size_t F = size_t(SR * 120); std::vector<float> rec(F * 2); for (size_t i = 0; i < rec.size(); ++i) rec[i] = float(std::sin(i * 0.01) * 0.3);
        Deck d; d.setTrack(rec.data(), F, 2); d.setMotor(true); std::vector<float> L(128), R(128); int blocks = int(SR * 30 / 128);
        auto t0 = std::chrono::steady_clock::now();
        for (int b = 0; b < blocks; ++b) { if (b % 40 == 0) d.touch(!d.touched()); if (d.touched()) d.handMove(b % 2 ? 0.02 : -0.03, 0); d.process(L.data(), R.data(), 128); }
        double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count() / blocks;
        report(us < 2902 * 0.25, "Real-time: one deck at 128-sample buffer (2.9 ms budget), scratching continuously",
               fmt("%.0f µs per block = %.1f%% of budget on this box (Apple M1 is faster per core)", us, us / 2902 * 100));
    }
    std::printf("\n%d/%d passed\n", passed, passed + failed);
    return failed ? 1 : 0;
}
