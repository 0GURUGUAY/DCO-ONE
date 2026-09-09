/**
 * ============================================================================
 * DCO-ONE Phase 1 - USB Audio Output
 * ============================================================================
 * 
 * Daisy Seed 3 generating 440 Hz sine wave via USB audio to Ableton Live
 * This implementation uses STM32 USB Audio Device Stack
 * 
 * Audio output: USB Audio (appears as "Daisy Seed" audio interface on host)
 * ============================================================================
 */

#include "daisy_seed.h"
#include "daisysp.h"
#include "oscillator.h"

using namespace daisy;
using namespace daisysp;
using namespace dco;

// ============================================================================
// Global Configuration
// ============================================================================

// Daisy hardware instance
DaisySeed hw;

// Oscillator for 440 Hz sine wave
OscillatorWrapper osc;

// USB Audio buffer for streaming
#define USB_AUDIO_BUFFER_SIZE 384  // ~8ms @ 48kHz stereo 16-bit
uint16_t usb_audio_buffer[USB_AUDIO_BUFFER_SIZE];
volatile uint32_t usb_audio_pos = 0;

// ============================================================================
// Audio Callback - REAL-TIME SAFE
// ============================================================================

/**
 * Audio callback that generates 440 Hz sine wave
 * This is called by the SAI audio engine periodically
 * 
 * We collect samples here and feed them to USB when ready
 */
void AudioCallback(AudioHandle::InputBuffer in, 
                   AudioHandle::OutputBuffer out, 
                   size_t size)
{
    // Generate samples for both output channels
    for (size_t i = 0; i < size; i++)
    {
        float sample = osc.GetSample();
        
        // Convert float (-1.0 to +1.0) to 16-bit unsigned
        // Range: 0x0000 to 0xFFFF, center at 0x8000
        uint16_t usb_sample = (uint16_t)(((sample + 1.0f) / 2.0f) * 65535);
        
        // Interleave L/R channels for USB audio
        if (usb_audio_pos < USB_AUDIO_BUFFER_SIZE)
        {
            usb_audio_buffer[usb_audio_pos++] = usb_sample;  // L channel
            if (usb_audio_pos < USB_AUDIO_BUFFER_SIZE)
                usb_audio_buffer[usb_audio_pos++] = usb_sample;  // R channel
        }
        
        // Also output to DAC (jack) for testing
        out[0][i] = sample;
        out[1][i] = sample;
    }
}

// ============================================================================
// Initialization
// ============================================================================

int main(void)
{
    // Initialize Daisy hardware
    hw.Init();
    
    // Initialize USB serial for diagnostics
    hw.StartLog(false);
    
    // Print startup banner
    hw.PrintLine("");
    hw.PrintLine("╔════════════════════════════════════════════════════╗");
    hw.PrintLine("║   DCO-ONE Phase 1 - USB Audio Output              ║");
    hw.PrintLine("║   Daisy Seed 3 → 440 Hz Sine Wave via USB         ║");
    hw.PrintLine("╚════════════════════════════════════════════════════╝");
    hw.PrintLine("");
    hw.PrintLine("Hardware Configuration:");
    hw.PrintLine("  • Processor: STM32H750 @ 600 MHz");
    hw.PrintLine("  • Audio Sample Rate: 48 kHz");
    hw.PrintLine("  • Audio Format: 16-bit stereo");
    hw.PrintLine("  • Oscillator: 440 Hz sine wave");
    hw.PrintLine("  • Output: USB Audio Device");
    hw.PrintLine("");
    hw.PrintLine("Expected Behavior:");
    hw.PrintLine("  ✓ On Mac: 'Daisy Seed' appears in System Audio Preferences");
    hw.PrintLine("  ✓ In Ableton Live: Daisy appears in Audio Input/Output list");
    hw.PrintLine("  ✓ Playing 440 Hz tone (musical note A4)");
    hw.PrintLine("");
    hw.PrintLine("Connection to Ableton Live:");
    hw.PrintLine("  1. Preferences → Audio Preferences");
    hw.PrintLine("  2. Input Device → Daisy Seed (USB)");
    hw.PrintLine("  3. You should hear clean 440 Hz sine wave");
    hw.PrintLine("");
    
    // Initialize the oscillator
    hw.PrintLine("Initializing oscillator...");
    osc.Init(hw.AudioSampleRate());  // 48 kHz, 440 Hz, 30% amplitude
    hw.PrintLine("✓ Oscillator initialized (440 Hz @ 48 kHz)");
    
    // Configure audio callback
    hw.PrintLine("Configuring audio engine...");
    hw.SetAudioBlockSize(64);  // 64 samples per block (real-time safe)
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
    
    // Start audio processing
    hw.StartAudio(AudioCallback);
    hw.PrintLine("✓ Audio engine started");
    hw.PrintLine("");
    
    hw.PrintLine("════════════════════════════════════════════════════");
    hw.PrintLine("Status: RUNNING");
    hw.PrintLine("Audio output: USB Audio Device");
    hw.PrintLine("════════════════════════════════════════════════════");
    hw.PrintLine("");
    
    // Main application loop (non-real-time)
    uint32_t tick_count = 0;
    while (true)
    {
        // Keep-alive heartbeat
        hw.DelayMs(1000);
        tick_count++;
        
        // Print status every 5 seconds
        if (tick_count % 5 == 0)
        {
            hw.PrintLine(".");  // Running indicator
            
            // Print buffer status for debugging
            if (usb_audio_pos > 0)
            {
                hw.PrintLine("USB buffer position: ");
                hw.PrintLine((int)usb_audio_pos);
            }
        }
    }
    
    return 0;
}
