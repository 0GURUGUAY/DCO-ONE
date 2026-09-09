#include "input_manager.h"

namespace dco {

// Dead time after a committed detent during which further raw transitions
// are ignored, to swallow the trailing mechanical bounce of that same click.
static constexpr uint32_t kPostCommitDeadTimeUs = 3000;

EncoderReader::EncoderReader()
    : has_switch_(false),
      invert_direction_(false),
      prev_a_(0),
      prev_b_(0),
      sub_step_accum_(0),
      last_commit_us_(0),
      committed_steps_(0),
      last_consumed_steps_(0)
{
}

void EncoderReader::Init(daisy::Pin pin_a, daisy::Pin pin_b, daisy::Pin pin_sw, bool invert_direction)
{
    invert_direction_ = invert_direction;
    // Configure encoder phase A (input, pull-up)
    a_.Init(pin_a, daisy::GPIO::Mode::INPUT, daisy::GPIO::Pull::PULLUP);
    
    // Configure encoder phase B (input, pull-up)
    b_.Init(pin_b, daisy::GPIO::Mode::INPUT, daisy::GPIO::Pull::PULLUP);
    
    // Configure optional pushbutton, reusing ButtonReader's debounced logic
    // Default Pin() is invalid (port=PORTX, pin=255)
    if (pin_sw.port != daisy::PORTX)
    {
        switch_reader_.Init(pin_sw);
        has_switch_ = true;
    }
    else
    {
        has_switch_ = false;
    }
    
    // Read initial state
    prev_a_ = a_.Read() ? 1 : 0;
    prev_b_ = b_.Read() ? 1 : 0;
    sub_step_accum_ = 0;
    last_commit_us_ = 0;
    committed_steps_ = 0;
    last_consumed_steps_ = 0;
}

void EncoderReader::Poll()
{
    uint8_t curr_a = a_.Read() ? 1 : 0;
    uint8_t curr_b = b_.Read() ? 1 : 0;
    
    // Robust quadrature detection: observe ALL state transitions, not just A changes.
    // This catches fast B-only transitions that would be missed by A-only detection.
    // Quadrature is typically: CW = 00→01→11→10→00, CCW = 00→10→11→01→00
    
    // Check if either A or B has changed
    if (prev_a_ != curr_a || prev_b_ != curr_b)
    {
        uint8_t prev_state = (prev_a_ << 1) | prev_b_;
        uint8_t curr_state = (curr_a << 1) | curr_b;
        
        // Gray-code quadrature decoder: for each valid transition,
        // emit a quarter-step. Invalid transitions (bounces) yield DIRECTION_NONE.
        // Gray-code CW sequence: 00→01→11→10→00
        // Gray-code CCW sequence: 00→10→11→01→00
        Direction quarter_step = DIRECTION_NONE;
        switch (prev_state)
        {
            case 0b00:  // 00
                quarter_step = (curr_state == 0b01) ? DIRECTION_CW : 
                               (curr_state == 0b10) ? DIRECTION_CCW : DIRECTION_NONE;
                break;
            case 0b01:  // 01
                quarter_step = (curr_state == 0b11) ? DIRECTION_CW : 
                               (curr_state == 0b00) ? DIRECTION_CCW : DIRECTION_NONE;
                break;
            case 0b11:  // 11
                quarter_step = (curr_state == 0b10) ? DIRECTION_CW : 
                               (curr_state == 0b01) ? DIRECTION_CCW : DIRECTION_NONE;
                break;
            case 0b10:  // 10
                quarter_step = (curr_state == 0b00) ? DIRECTION_CW : 
                               (curr_state == 0b11) ? DIRECTION_CCW : DIRECTION_NONE;
                break;
        }
        
        if (quarter_step != DIRECTION_NONE)
        {
            // Swallow the trailing contact bounce of the click we just
            // committed instead of letting it register as the first edge
            // of a spurious extra step (this is what made a single detent
            // occasionally count as 2 steps).
            uint32_t now_us = daisy::System::GetUs();
            if (now_us - last_commit_us_ >= kPostCommitDeadTimeUs)
            {
                int32_t signed_step = static_cast<int32_t>(quarter_step);
                if (invert_direction_)
                    signed_step = -signed_step;

                // This hardware's detents produce 2 raw quadrature transitions
                // each (verified on real hardware); only commit once that full
                // cycle completes.
                sub_step_accum_ += signed_step;
                if (sub_step_accum_ >= 2)
                {
                    committed_steps_ += 1;
                    sub_step_accum_ = 0;
                    last_commit_us_ = now_us;
                }
                else if (sub_step_accum_ <= -2)
                {
                    committed_steps_ -= 1;
                    sub_step_accum_ = 0;
                    last_commit_us_ = now_us;
                }
            }
        }
    }
    
    prev_a_ = curr_a;
    prev_b_ = curr_b;
    
    // Update the pushbutton via the same debounced ButtonReader logic used for D25
    if (has_switch_)
    {
        switch_reader_.Update();
    }
}

int32_t EncoderReader::GetAndClearSteps()
{
    int32_t current = committed_steps_;
    int32_t delta    = current - last_consumed_steps_;
    last_consumed_steps_ = current;
    return delta;
}

int32_t EncoderReader::DebugGetRawA() { return a_.Read() ? 1 : 0; }
int32_t EncoderReader::DebugGetRawB() { return b_.Read() ? 1 : 0; }
int32_t EncoderReader::DebugGetRawSw() { return switch_reader_.DebugGetRaw(); }

bool EncoderReader::IsSwitchPressed() const
{
    return switch_reader_.IsPressed();
}

// ============================================================================
// ButtonReader Implementation
// ============================================================================

ButtonReader::ButtonReader()
    : is_pressed_(false),
      prev_state_(false),
      changed_(false),
      active_high_(false),
      debounce_state_(0x00),
      last_update_ms_(0)
{
}

void ButtonReader::Init(daisy::Pin pin, bool active_high)
{
    active_high_ = active_high;
    
    // Active-high switches connect to VCC when pressed -> use pull-down.
    // Active-low switches connect to GND when pressed -> use pull-up.
    daisy::GPIO::Pull pull = active_high_ ? daisy::GPIO::Pull::PULLDOWN
                                          : daisy::GPIO::Pull::PULLUP;
    gpio_.Init(pin, daisy::GPIO::Mode::INPUT, pull);
    
    // Debounce state starts at "released" (0x00), same as daisy::Switch::Init,
    // regardless of the pin's actual level at boot.
    prev_state_ = false;
    is_pressed_ = false;
    changed_ = false;
    debounce_state_ = 0x00;
    last_update_ms_ = daisy::System::GetNow();
}

bool ButtonReader::Update()
{
    // 8-sample shift-register debounce throttled to ~1kHz, same technique
    // as daisy::Switch::Debounce() (see DaisyExamples seed/Button/Button.cpp).
    changed_ = false;
    uint32_t now = daisy::System::GetNow();
    if (now - last_update_ms_ < 1)
    {
        return changed_;
    }
    last_update_ms_ = now;
    
    bool raw_pressed = active_high_ ? gpio_.Read() : !gpio_.Read();
    debounce_state_ = static_cast<uint8_t>((debounce_state_ << 1) | (raw_pressed ? 1 : 0));
    
    bool curr_state = prev_state_;
    if (debounce_state_ == 0xFF)
    {
        curr_state = true;
    }
    else if (debounce_state_ == 0x00)
    {
        curr_state = false;
    }
    
    changed_ = (curr_state != prev_state_);
    is_pressed_ = curr_state;
    prev_state_ = curr_state;
    return changed_;
}

int32_t ButtonReader::DebugGetRaw()
{
    return gpio_.Read() ? 1 : 0;
}

} // namespace dco
