#pragma once

#include "daisysp.h"
#include <cstdint>

namespace dco {

// Effect types available in each FX slot.
// Kept in sync with menu.json's fxN_type options.
enum class FxType : uint8_t {
    OFF = 0,
    REVERB,
    DELAY,
    CHORUS,
    FLANGER,
    PHASER,
    BITCRUSHER,
    COUNT
};

namespace detail {

// ---------------------------------------------------------------------------
// Simple delay effect (DaisySP only provides a raw DelayLine utility).
// ---------------------------------------------------------------------------
class SimpleDelay {
public:
    // 500 ms @ 48 kHz is the longest delay time exposed by the UI.
    static constexpr size_t kMaxSamples = 24000;

    void Init(float sampleRate) {
        sampleRate_ = sampleRate;
        del_.Init();
        SetTimeMs(250.0f);
        SetFeedback(0.3f);
    }

    float Process(float in) {
        const float delayed = del_.Read();
        const float write   = in + delayed * feedback_;
        del_.Write(write);
        return delayed;
    }

    void SetTimeMs(float ms) {
        timeMs_ = ms;
        const float samples = timeMs_ * 0.001f * sampleRate_;
        del_.SetDelay(samples);
    }

    void SetFeedback(float fb) {
        feedback_ = fb < 0.0f ? 0.0f : (fb > 0.95f ? 0.95f : fb);
    }

private:
    float sampleRate_ = 48000.0f;
    float timeMs_     = 250.0f;
    float feedback_   = 0.3f;
    daisysp::DelayLine<float, kMaxSamples> del_;
};

// ---------------------------------------------------------------------------
// Simple Schroeder-style reverb built from DaisySP DelayLines.
// Four parallel comb filters + two series allpass filters.
// ---------------------------------------------------------------------------
class SimpleReverb {
public:
    void Init(float sampleRate) {
        sampleRate_ = sampleRate;
        c1_.Init();
        c2_.Init();
        c3_.Init();
        c4_.Init();
        ap1_.Init();
        ap2_.Init();
        // DelayLine::Init() resets the internal delay to just 1 sample --
        // each comb's actual delay length must be set explicitly or the
        // "reverb" collapses into an inaudible 1-sample feedback loop. The
        // allpass stages don't need this: Allpass() takes its delay as an
        // explicit argument on every call instead of using this member.
        c1_.SetDelay(static_cast<float>(kComb1 - 1));
        c2_.SetDelay(static_cast<float>(kComb2 - 1));
        c3_.SetDelay(static_cast<float>(kComb3 - 1));
        c4_.SetDelay(static_cast<float>(kComb4 - 1));
        SetDecay(0.5f);
        SetDamping(0.5f);
    }

    float Process(float in) {
        // Comb filter 1: ~29.7 ms
        float d1 = c1_.Read();
        combFb_[0] = d1 * (1.0f - damping_) + combFb_[0] * damping_;
        c1_.Write(in + combFb_[0] * decay_);

        // Comb filter 2: ~33.1 ms
        float d2 = c2_.Read();
        combFb_[1] = d2 * (1.0f - damping_) + combFb_[1] * damping_;
        c2_.Write(in + combFb_[1] * decay_);

        // Comb filter 3: ~36.5 ms
        float d3 = c3_.Read();
        combFb_[2] = d3 * (1.0f - damping_) + combFb_[2] * damping_;
        c3_.Write(in + combFb_[2] * decay_);

        // Comb filter 4: ~41.1 ms
        float d4 = c4_.Read();
        combFb_[3] = d4 * (1.0f - damping_) + combFb_[3] * damping_;
        c4_.Write(in + combFb_[3] * decay_);

        float out = (d1 + d2 + d3 + d4) * 0.25f;
        out = ap1_.Allpass(out, kAp1, 0.7f);
        out = ap2_.Allpass(out, kAp2, 0.7f);
        return out;
    }

    void SetDecay(float decay) {
        decay_ = decay < 0.0f ? 0.0f : (decay > 0.98f ? 0.98f : decay);
    }

    void SetDamping(float damping) {
        damping_ = damping < 0.0f ? 0.0f : (damping > 0.95f ? 0.95f : damping);
    }

private:
    static constexpr size_t kComb1 = 1426;
    static constexpr size_t kComb2 = 1589;
    static constexpr size_t kComb3 = 1752;
    static constexpr size_t kComb4 = 1973;
    static constexpr size_t kAp1   = 144;
    static constexpr size_t kAp2   = 48;

    float sampleRate_ = 48000.0f;
    float decay_      = 0.5f;
    float damping_    = 0.5f;
    float combFb_[4]  = {0.0f, 0.0f, 0.0f, 0.0f};

    daisysp::DelayLine<float, kComb1> c1_;
    daisysp::DelayLine<float, kComb2> c2_;
    daisysp::DelayLine<float, kComb3> c3_;
    daisysp::DelayLine<float, kComb4> c4_;
    daisysp::DelayLine<float, kAp1>   ap1_;
    daisysp::DelayLine<float, kAp2>   ap2_;
};

// Beats-per-division for the DELAY effect's tempo-synced Time control (whole
// note = 4 beats, same convention as the sequencer/LFO tempo-sync tables in
// main.cpp's PolyDivisionBeats()/LfoSyncDivisionBeats()).
static constexpr float kDelayDivisionBeats[] = {
    4.0f,        // 1/1
    2.0f,        // 1/2
    1.0f,        // 1/4
    2.0f / 3.0f, // 1/4T
    0.5f,        // 1/8
    1.0f / 3.0f, // 1/8T
    0.25f,       // 1/16
    1.0f / 6.0f, // 1/16T
    0.125f,      // 1/32
};
static constexpr int kDelayDivisionCount =
    sizeof(kDelayDivisionBeats) / sizeof(kDelayDivisionBeats[0]);

} // namespace detail

// ---------------------------------------------------------------------------
// Per-slot processor storage.
// Only the active effect's instance is initialized; the union shares memory
// across the different effect types to keep the four-slot chain affordable.
// ---------------------------------------------------------------------------
class FxSlot {
public:
    FxSlot() = default;

    void Init(float sampleRate) {
        sampleRate_ = sampleRate;
        ConstructActive(FxType::OFF);
    }

    // Called from the main/menu thread. Only records the request -- the
    // actual (re)construction is deferred to ApplyPendingType() so the
    // shared `processors_` union is never rebuilt while the audio IRQ might
    // be concurrently reading/writing it inside Process().
    void RequestType(FxType type) {
        pending_type_ = static_cast<int32_t>(type);
    }

    // Audio-thread only: call once per block, before Process(). Performs the
    // deferred placement-new + Init() for a pending type change, if any.
    void ApplyPendingType() {
        int32_t requested = pending_type_;
        pending_type_ = -1;
        if (requested < 0 || static_cast<FxType>(requested) == type_)
            return;
        ConstructActive(static_cast<FxType>(requested));
    }

    void SetMix(float mix01) {
        mix_ = mix01 < 0.0f ? 0.0f : (mix01 > 1.0f ? 1.0f : mix01);
    }

    void SetParam1(float p1_01) {
        p1_ = p1_01 < 0.0f ? 0.0f : (p1_01 > 1.0f ? 1.0f : p1_01);
        ApplyParameters();
    }

    void SetParam2(float p2_01) {
        p2_ = p2_01 < 0.0f ? 0.0f : (p2_01 > 1.0f ? 1.0f : p2_01);
        ApplyParameters();
    }

    // Audio-thread only: call once per block (mirrors the VCF cutoff/LFO1
    // rate per-block recompute pattern in AudioCallback). Only DELAY reacts:
    // its Time control is tempo-synced, so its effective ms must track BPM
    // changes even when the Time knob itself isn't touched.
    void SetTempo(float bpm) {
        bpm_ = bpm;
        if (type_ == FxType::DELAY)
            ApplyParameters();
    }

    float Process(float in) {
        if (type_ == FxType::OFF)
            return in;

        float wet = 0.0f;
        switch (type_) {
            case FxType::REVERB:     wet = processors_.reverb.Process(in); break;
            case FxType::DELAY:      wet = processors_.delay.Process(in); break;
            case FxType::CHORUS:     wet = processors_.chorus.Process(in); break;
            case FxType::FLANGER:    wet = processors_.flanger.Process(in); break;
            case FxType::PHASER:     wet = processors_.phaser.Process(in); break;
            case FxType::BITCRUSHER: wet = processors_.decimator.Process(in); break;
            default:                 wet = in; break;
        }
        return in * (1.0f - mix_) + wet * mix_;
    }

private:
    void ApplyParameters() {
        switch (type_) {
            case FxType::REVERB:
                processors_.reverb.SetDecay(p1_ * 0.98f);
                processors_.reverb.SetDamping(p2_ * 0.95f);
                break;
            case FxType::DELAY: {
                // Time is a tempo-synced note division, not raw ms: the
                // knob's normalised 0..1 position picks a division from
                // detail::kDelayDivisionBeats, and the actual ms is derived
                // from the last tempo pushed via SetTempo().
                int divIndex = static_cast<int>(p1_ * (detail::kDelayDivisionCount - 1) + 0.5f);
                divIndex = divIndex < 0 ? 0 : (divIndex >= detail::kDelayDivisionCount
                                                   ? detail::kDelayDivisionCount - 1
                                                   : divIndex);
                float bpm = bpm_ < 1.0f ? 1.0f : bpm_;
                float ms  = detail::kDelayDivisionBeats[divIndex] * 60000.0f / bpm;
                processors_.delay.SetTimeMs(ms);
                processors_.delay.SetFeedback(p2_ * 0.95f);
                break;
            }
            case FxType::CHORUS:
                processors_.chorus.SetLfoFreq(0.1f + p1_ * 4.9f);  // 0.1 .. 5 Hz
                processors_.chorus.SetLfoDepth(p2_);
                processors_.chorus.SetDelay(0.5f);
                processors_.chorus.SetFeedback(0.0f);
                break;
            case FxType::FLANGER:
                processors_.flanger.SetLfoFreq(0.1f + p1_ * 4.9f); // 0.1 .. 5 Hz
                processors_.flanger.SetLfoDepth(0.8f);
                processors_.flanger.SetFeedback(p2_ * 0.95f);
                processors_.flanger.SetDelay(0.5f);
                break;
            case FxType::PHASER:
                processors_.phaser.SetLfoFreq(0.1f + p1_ * 4.9f);  // 0.1 .. 5 Hz
                processors_.phaser.SetLfoDepth(p2_);
                processors_.phaser.SetFeedback(0.3f);
                break;
            case FxType::BITCRUSHER:
                processors_.decimator.SetBitcrushFactor(p1_);
                processors_.decimator.SetDownsampleFactor(p2_);
                break;
            default:
                break;
        }
    }

    // Audio-thread only: (re)constructs whichever processor `type` needs,
    // in place, inside the shared union -- always, even if that type was
    // already used before. The union has no memory of which type last lived
    // there, so skipping construction on a "already seen once" type (the old
    // behaviour) left stale bytes from a *different*, differently-shaped
    // processor being reinterpreted as this one -- undefined behaviour that
    // could read garbage DelayLine indices/state.
    void ConstructActive(FxType type) {
        type_ = type;
        switch (type_) {
            case FxType::OFF:
                break;
            case FxType::REVERB:
                new (&processors_.reverb) detail::SimpleReverb();
                processors_.reverb.Init(sampleRate_);
                break;
            case FxType::DELAY:
                new (&processors_.delay) detail::SimpleDelay();
                processors_.delay.Init(sampleRate_);
                break;
            case FxType::CHORUS:
                new (&processors_.chorus) daisysp::Chorus();
                processors_.chorus.Init(sampleRate_);
                break;
            case FxType::FLANGER:
                new (&processors_.flanger) daisysp::Flanger();
                processors_.flanger.Init(sampleRate_);
                break;
            case FxType::PHASER:
                new (&processors_.phaser) daisysp::Phaser();
                processors_.phaser.Init(sampleRate_);
                break;
            case FxType::BITCRUSHER:
                new (&processors_.decimator) daisysp::Decimator();
                processors_.decimator.Init();
                break;
            default:
                type_ = FxType::OFF;
                break;
        }
        ApplyParameters();
    }

    float sampleRate_ = 48000.0f;
    FxType type_ = FxType::OFF;
    float mix_   = 0.5f;
    float p1_    = 0.5f;
    float p2_    = 0.5f;
    float bpm_   = 120.0f; // last tempo pushed via SetTempo(); DELAY only

    // Requested type, written by the main/menu thread; -1 = no pending
    // request. Single-writer(main)/single-reader(audio) lock-free field,
    // same pattern as EncoderReader's committed_steps_.
    volatile int32_t pending_type_ = -1;

    // One active processor at a time.
    union ProcessorUnion {
        detail::SimpleReverb reverb;
        detail::SimpleDelay delay;
        daisysp::Chorus chorus;
        daisysp::Flanger flanger;
        daisysp::Phaser phaser;
        daisysp::Decimator decimator;

        ProcessorUnion() {}
        ~ProcessorUnion() {}
    } processors_;
};

// ---------------------------------------------------------------------------
// Four-slot serial FX chain: FX1 -> FX2 -> FX3 -> FX4.
// Each slot has its own type, dry/wet mix and two parameters whose meaning
// depends on the active effect.  Only one processor is active per slot at a
// time; the others are left uninitialized to save RAM.
// ---------------------------------------------------------------------------
class FxChain {
public:
    static constexpr int kNumSlots = 4;

    void Init(float sampleRate);

    // Audio-thread only: applies any slot type change requested from the
    // main/menu thread since the last call. Call once per block, before
    // Process(), so a slot's processor is never (re)constructed while a
    // sample is concurrently being processed through it.
    void ApplyPendingTypeChanges();

    // Audio-thread only: refreshes the current tempo used by DELAY slots'
    // note-synced Time control. Call once per block.
    void SetTempo(float bpm);

    // Process one sample through the four slots in series.
    float Process(float in);

    // Thread-safe (callable from the main/menu thread): only queues the
    // change: see ApplyPendingTypeChanges().
    void SetSlotType(int slot, FxType type);
    void SetSlotMix(int slot, float mix01);
    void SetSlotParam1(int slot, float p1_01);
    void SetSlotParam2(int slot, float p2_01);

private:
    FxSlot slots_[kNumSlots];
};

} // namespace dco
