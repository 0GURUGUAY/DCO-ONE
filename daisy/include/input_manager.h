#pragma once

#include "daisy_seed.h"

namespace dco {

/**
 * @class ButtonReader
 * @brief Simple push-button reader (non-encoder button, active-low pull-up)
 */
class ButtonReader
{
public:
    ButtonReader();
    
    /**
     * Initialize button on specified GPIO pin
     * @param pin GPIO pin for the button
     * @param active_high true if the switch connects to VCC when pressed
     *                    (requires internal pull-down); false for the usual
     *                    active-low switch to GND with internal pull-up.
     */
    void Init(daisy::Pin pin, bool active_high = false);
    
    /**
     * Call this regularly to update button state
     * @return true if button is currently pressed
     */
    bool Update();
    
    /**
     * @return true if button is currently pressed
     */
    bool IsPressed() const { return is_pressed_; }
    
    /**
     * @return true if button state changed in the last Update() call
     */
    bool HasChanged() const { return changed_; }
    
    // Debug: read raw GPIO value
    int32_t DebugGetRaw();

private:
    daisy::GPIO gpio_;
    bool is_pressed_;
    bool prev_state_;
    bool changed_;
    bool active_high_;
    
    // Debounce (same shift-register technique as daisy::Switch)
    uint8_t debounce_state_;
    uint32_t last_update_ms_;
};

/**
 * @class EncoderReader
 * @brief Simple quadrature encoder reader (edge-based detection) with a
 * debounced pushbutton, reusing ButtonReader for the switch.
 *
 * Sampling quadrature signals from the slow/variable-latency main loop
 * misses transitions (a physical detent's 4 raw edges can pass faster than
 * the loop gets back around), which makes "1 notch = 1 step" unreliable.
 * Poll() must instead be called from a fast, fixed-rate context (e.g. the
 * audio callback); GetAndClearSteps() can then be called once per main-loop
 * tick to consume however many full detents were committed meanwhile.
 */
class EncoderReader
{
public:
    enum Direction
    {
        DIRECTION_NONE = 0,
        DIRECTION_CW   = 1,   // Clockwise (increment)
        DIRECTION_CCW  = -1   // Counter-clockwise (decrement)
    };

    EncoderReader();
    
    /**
     * Initialize encoder on specified GPIO pins
     * @param pin_a GPIO pin for encoder A
     * @param pin_b GPIO pin for encoder B
     * @param pin_sw GPIO pin for pushbutton (optional)
     * @param invert_direction true to invert CW/CCW direction (for physically reversed encoders)
     */
    void Init(daisy::Pin pin_a, daisy::Pin pin_b, daisy::Pin pin_sw = daisy::Pin(), bool invert_direction = false);
    
    /**
     * Samples the raw quadrature signal and commits one detent's worth of
     * rotation whenever a full 4-transition cycle completes. Call this from
     * a fast fixed-rate context (e.g. once per audio block, ~1.3ms @ 48kHz/
     * 64 samples) -- NOT from the main loop.
     */
    void Poll();

    /**
     * Consumes and returns the net number of full detents turned since the
     * last call (positive = CW, negative = CCW). Safe to call once per
     * main-loop tick regardless of how many Poll() calls happened between.
     */
    int32_t GetAndClearSteps();
    
    /**
     * @return true if pushbutton is currently pressed
     */
    bool IsSwitchPressed() const;
    
    /**
     * @return absolute encoder position count (increments/decrements with each detent)
     */
    int32_t GetPositionCount() const { return committed_steps_; }
    
    /**
     * Reset position counter to zero
     */
    void ResetPositionCount() { committed_steps_ = 0; last_consumed_steps_ = 0; }
    
    // Debug: read raw GPIO values without state change tracking
    int32_t DebugGetRawA();
    int32_t DebugGetRawB();
    int32_t DebugGetRawSw();

private:
    daisy::GPIO a_;
    daisy::GPIO b_;
    ButtonReader switch_reader_;  // Same debounced logic as the D25 menu button
    
    bool has_switch_;
    bool invert_direction_;       // Invert CW/CCW for reversed encoders
    
    // Previous state for edge detection
    uint8_t prev_a_;
    uint8_t prev_b_;

    // A mechanical detent produces 4 raw quadrature transitions; accumulated
    // here (written only by Poll(), i.e. the audio IRQ) until a full detent
    // completes.
    int32_t sub_step_accum_;

    // Timestamp (us) of the last committed detent, used to swallow the
    // trailing contact bounce of that same click for a short dead time
    // instead of letting it register as the start of a spurious extra step.
    uint32_t last_commit_us_;

    // Monotonic committed detent count, written only by Poll() (audio IRQ);
    // GetAndClearSteps() (main loop) only ever reads/diffs it -- single-
    // writer/single-reader, same lock-free pattern as s_write_idx in main.cpp.
    volatile int32_t committed_steps_;
    int32_t last_consumed_steps_;
};

} // namespace dco
