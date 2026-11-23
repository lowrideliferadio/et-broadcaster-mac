// ET Broadcaster — turntable deck: platter physics + scratch input + band-limited playback.
#pragma once
#include "etb/SincResampler.h"
#include <cstddef>
#include <atomic>

namespace etb {

enum class DeckMode { Vinyl, CDJ };

struct PlatterConfig {
    double sampleRate   = 44100.0;  // output rate
    double trackRate    = 44100.0;  // decoded track rate
    double rpm          = 100.0 / 3.0;
    double startTime    = 0.25;     // s from stop to full speed (motor torque); 0.1 = "high torque"
    double brakeTime    = 0.40;     // s from full speed to stop when STOP pressed
    double slipmatTau   = 0.030;    // s, record catching the platter after a release (slipmat friction)
    double handSmoothHz = 140.0;    // bandwidth of the hand/jog follower (removes event-rate zipper)
    double slipXfadeMs  = 6.0;      // crossfade when slip mode jumps back to the ghost position
    DeckMode mode       = DeckMode::Vinyl;
    bool slip           = false;    // slip mode: track keeps "playing underneath" while you scratch
};

class Deck {
public:
    explicit Deck(const PlatterConfig& cfg = {}, int taps = 64);
    void setTrack(const float* interleaved, std::size_t frames, int channels);
    PlatterConfig& config() { return cfg_; }

    // ---- controls (block-synchronous; the app feeds them from a lock-free FIFO) ----
    void setMotor(bool on)            { motorOn_ = on; }
    void setPitch(double ratio)       { pitch_ = ratio; }          // 1.0 = 0 %, 1.08 = +8 %
    void touch(bool down);                                         // hand on record / jog touch
    // Relative rotation of the record by the hand, in revolutions (+ = forward), arriving `atSample`
    // samples into the next process() block. Mouse drag, trackpad rotate and MIDI jog all end up here.
    void handMove(double deltaRevolutions, int atSample = 0);
    void cue(double frame)            { pos_ = ghost_ = frame; }

    // ---- audio ----
    void process(float* outL, float* outR, int n);

    // ---- state for UI / tests ----
    double position() const      { return pos_; }                 // track frame
    double ghostPosition() const { return ghost_; }
    double recordSpeed() const   { return recVel_; }              // 1.0 = nominal forward
    double platterSpeed() const  { return platVel_; }
    double recordAngleDeg() const;                                 // for the spinning-label animation
    bool   touched() const       { return touched_; }
    bool   keylockActive() const { return keylock_ && !touched_; } // key-lock is bypassed while scratching
    void   setKeylock(bool k)    { keylock_ = k; }
    double framesPerRev() const  { return 60.0 / cfg_.rpm * cfg_.trackRate; }

private:
    PlatterConfig cfg_;
    SincResampler rs_;
    const float* data_ = nullptr; std::size_t frames_ = 0; int ch_ = 2;
    bool motorOn_ = true, touched_ = false, keylock_ = false;
    double pitch_ = 1.0, platVel_ = 0.0, recVel_ = 0.0;
    double pos_ = 0.0, ghost_ = 0.0;
    // hand follower: target (revs) → critically-damped 2nd-order follower
    double handTarget_ = 0.0, handPos_ = 0.0, handVel_ = 0.0;
    // event de-staircasing: each input event is spread over the measured event interval
    double rampLeft_ = 0.0, rampRate_ = 0.0, evInterval_ = 0.0; long sinceEv_ = 0;
    double pendingDelta_[64]; int pendingAt_[64]; int pendingN_ = 0;
    // CDJ release ramp, slip crossfade
    double fadePos_ = 0.0, fadeVel_ = 0.0; int fadeLeft_ = 0, fadeLen_ = 0;
};

// Crossfader with adjustable curve, cut-in lag and hamster (reverse) switch.
struct Crossfader {
    double curve = 0.0;      // 0 = smooth (constant power, mixing) … 1 = sharp scratch cut
    double cutLag = 0.004;   // dead zone at each end, fraction of travel
    bool hamster = false;
    void gains(double x, double& gA, double& gB) const;   // x: 0 = full A, 1 = full B
};

// Per-sample smoothed gains (≈0.25 ms) — fast cuts without clicks.
struct GainSmoother {
    double a = 0.0, b = 0.0, coef = 0.0;
    void prepare(double sr, double ms = 0.25);
    void step(double ta, double tb) { a += (ta - a) * coef; b += (tb - b) * coef; }
};

// MIDI jog decoding: 14-bit absolute (MSB/LSB pair) and the three common relative encodings.
struct JogDecoder {
    enum Kind { Absolute14, RelTwosComplement, RelBinaryOffset, RelSignMagnitude } kind = RelTwosComplement;
    int ticksPerRev = 2048;                     // set by MIDI Learn calibration (one full turn)
    double feed7(int value);                    // relative encoders: one CC value → delta revolutions
    double feed14(int msb, int lsb);            // absolute 14-bit: wraps around
    int last14 = -1;
};

} // namespace etb
