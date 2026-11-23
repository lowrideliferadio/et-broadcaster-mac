#include "etb/Deck.h"
#include <cmath>
#include <algorithm>

namespace etb {

Deck::Deck(const PlatterConfig& cfg, int taps) : cfg_(cfg), rs_(taps) {}

void Deck::setTrack(const float* d, std::size_t frames, int channels) { data_ = d; frames_ = frames; ch_ = channels; pos_ = ghost_ = 0.0; }

void Deck::touch(bool down) {
    if (down == touched_) return;
    touched_ = down;
    if (down) {                        // hand lands: follower starts where the record is, at its speed
        handTarget_ = handPos_ = 0.0; rampLeft_ = rampRate_ = 0.0; evInterval_ = 0.0; sinceEv_ = 0;
        handVel_ = recVel_ * cfg_.trackRate / framesPerRev();  // revs/s
    } else {
        if (cfg_.slip) {                // jump back to where the track would be — crossfaded, no click
            fadePos_ = pos_; fadeVel_ = recVel_;
            fadeLen_ = fadeLeft_ = std::max(1, int(cfg_.slipXfadeMs * 1e-3 * cfg_.sampleRate));
            pos_ = ghost_;
            recVel_ = platVel_;
        }
    }
}

void Deck::handMove(double dRev, int atSample) {
    if (pendingN_ < 64) { pendingDelta_[pendingN_] = dRev; pendingAt_[pendingN_] = atSample; ++pendingN_; }
}

double Deck::recordAngleDeg() const { return std::fmod(pos_ / framesPerRev() * 360.0, 360.0); }

void Deck::process(float* outL, float* outR, int n) {
    const double dt = 1.0 / cfg_.sampleRate;
    const double ratio = cfg_.trackRate / cfg_.sampleRate;        // track frames per output frame at 1.0
    const double fpr = framesPerRev();
    const double revPerSecNominal = cfg_.trackRate / fpr;          // = rpm/60
    // follower: critically damped, natural freq w
    const double w = 2.0 * M_PI * cfg_.handSmoothHz;
    const double acc = 1.0 / std::max(0.01, cfg_.startTime), dec = 1.0 / std::max(0.01, cfg_.brakeTime);
    const double slipK = 1.0 - std::exp(-dt / std::max(1e-4, cfg_.slipmatTau));
    int pe = 0;
    float fr[8], fo[8];
    for (int i = 0; i < n; ++i) {
        while (pe < pendingN_ && pendingAt_[pe] <= i) {
            // Mouse/trackpad (~60–120 Hz) and MIDI jogs (~250–1000 Hz) deliver position in steps. Spreading each
            // step over the measured event interval turns the staircase into a ramp → constant velocity, no zipper.
            double iv = std::clamp(double(sinceEv_), 0.001 * cfg_.sampleRate, 0.040 * cfg_.sampleRate);
            evInterval_ = evInterval_ <= 0 ? iv : evInterval_ * 0.6 + iv * 0.4;
            double pending = rampLeft_ + pendingDelta_[pe];
            rampRate_ = pending / evInterval_; rampLeft_ = pending; sinceEv_ = 0; ++pe;
        }
        ++sinceEv_;
        if (sinceEv_ > long(0.05 * cfg_.sampleRate)) evInterval_ = 0;       // input went quiet: reset estimate
        if (rampLeft_ != 0.0) {
            double stepv = (std::fabs(rampRate_) >= std::fabs(rampLeft_)) ? rampLeft_ : rampRate_;
            handTarget_ += stepv; rampLeft_ -= stepv;
        }
        // --- platter (motor torque / brake: constant angular acceleration) ---
        double target = motorOn_ ? pitch_ : 0.0;
        if (platVel_ < target) platVel_ = std::min(target, platVel_ + (motorOn_ ? acc : dec) * dt * std::max(1.0, pitch_));
        else if (platVel_ > target) platVel_ = std::max(target, platVel_ - dec * dt * std::max(1.0, pitch_));
        // --- record ---
        if (touched_) {
            // semi-implicit integration of x'' = w²(target - x) - 2w x'   (critically damped)
            handVel_ += (w * w * (handTarget_ - handPos_) - 2.0 * w * handVel_) * dt;
            handPos_ += handVel_ * dt;
            double v = handVel_ / revPerSecNominal;                 // in "nominal speed" units
            if (cfg_.mode == DeckMode::Vinyl) recVel_ = v;           // hand holds the record on the slipmat
            else recVel_ = v + 0.0;                                  // CDJ: jog = scratch, platter stopped under hand
        } else if (cfg_.mode == DeckMode::Vinyl) {
            recVel_ += (platVel_ - recVel_) * slipK;                 // slipmat drags record back to platter speed
        } else {
            recVel_ += (platVel_ - recVel_) * std::min(1.0, slipK * 4.0);  // CDJ: near-instant resume
        }
        double speed = recVel_ * ratio;
        if (data_) {
            rs_.readFrame(data_, frames_, ch_, pos_, speed, fr);
            float l = fr[0], r = ch_ > 1 ? fr[1] : fr[0];
            if (fadeLeft_ > 0) {                                     // slip-return crossfade (equal power)
                rs_.readFrame(data_, frames_, ch_, fadePos_, fadeVel_ * ratio, fo);
                double g = double(fadeLeft_) / fadeLen_;
                double gi = std::sin((1.0 - g) * M_PI / 2), go = std::sin(g * M_PI / 2);
                l = float(l * gi + fo[0] * go); r = float(r * gi + (ch_ > 1 ? fo[1] : fo[0]) * go);
                fadePos_ += fadeVel_ * ratio; --fadeLeft_;
            }
            outL[i] = l; outR[i] = r;
        } else { outL[i] = outR[i] = 0.0f; }
        pos_ += speed;
        ghost_ += platVel_ * ratio;
        if (!cfg_.slip || !touched_) ghost_ = touched_ ? ghost_ : pos_;
        if (pos_ < 0) pos_ = 0;   // can't scratch before the start of the record
    }
    // move unconsumed events to the next block
    int k = 0; for (int j = pe; j < pendingN_; ++j) { pendingDelta_[k] = pendingDelta_[j]; pendingAt_[k] = pendingAt_[j] - n; ++k; }
    pendingN_ = k;
}

void Crossfader::gains(double x, double& gA, double& gB) const {
    x = std::clamp(x, 0.0, 1.0); if (hamster) x = 1.0 - x;
    const double width = 1.0 + (0.010 - 1.0) * std::clamp(curve, 0.0, 1.0);   // ramp width
    auto f = [&](double d) {
        if (curve < 1e-6) return std::sin(M_PI / 2 * d);                     // pure constant power
        double u = std::clamp((d - cutLag) / std::max(1e-6, width - cutLag * (1 - curve)), 0.0, 1.0);
        return std::sin(M_PI / 2 * u);
    };
    gA = f(1.0 - x); gB = f(x);
}

void GainSmoother::prepare(double sr, double ms) { coef = 1.0 - std::exp(-1.0 / (ms * 1e-3 * sr)); }

double JogDecoder::feed7(int v) {
    int d = 0;
    switch (kind) {
        case RelTwosComplement: d = v < 64 ? v : v - 128; break;
        case RelBinaryOffset:   d = v - 64; break;
        case RelSignMagnitude:  d = (v & 64) ? -(v & 63) : (v & 63); break;
        default: break;
    }
    return double(d) / ticksPerRev;
}

double JogDecoder::feed14(int msb, int lsb) {
    int v = ((msb & 127) << 7) | (lsb & 127);
    if (last14 < 0) { last14 = v; return 0.0; }
    int d = v - last14; last14 = v;
    if (d > 8192) d -= 16384; else if (d < -8192) d += 16384;   // wrap-around
    return double(d) / ticksPerRev;
}

} // namespace etb
