# DCO-ONE — PROJECT ARCHITECTURE V2

Nous allons créer le projet DCO-ONE basé sur le projet download/Kimi_teensy.
La différence majeure est que nous avons maintenant un DAISY SEED3 (https://docs.daisy.audio/hardware/Seed3/) à la place du teensy et un display waveshare OMALED 1.75" (https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.75) à la place du simulateur.

## 0. Project definition

DCO-ONE is an embedded audio instrument based on a **Daisy Seed 3**, with a separate **ESP32-S3 + Waveshare 1.75" AMOLED display** for the graphical user interface.

The project is developed in **native C++**, using:
- libDaisy
- DaisySP

The architecture must keep the real-time audio system independent from the graphical display system.

---

# 1. HARDWARE ARCHITECTURE

## 1.1 Daisy Seed 3

The Daisy Seed 3 is the main processor of DCO-ONE.
https://docs.daisy.audio/hardware/Seed3/#technical-specs


### Physical controls

| Control | Pins |
|---|---|
| Main encoder A/B | D21 / D22 |
| Main encoder switch | D23 |
| Programming encoder A/B | D15 / D16 |
| Programming encoder switch | D24 |
| Menu encoder A/B | D17 / D18 |
| Menu encoder switch | D25 |
| Sub-menu encoder A/B | D19 / D20 |
| Sub-menu encoder switch | D28 |
| Home button | D12 |
| Mute button | D11 |
| Solo button | D10 |
| Trigger button | D9 |

### Audio

- Audio IN 1
- Audio IN 2
- Audio OUT 1
- Audio OUT 2

There is 2 microphones on the ESP32-S3 board that I want to use to get samples.

### CV

| Function | Pin |
|---|---|
| CV 1 | D3 |
| CV 2 | D2 |

### Input detection

| Function | Pin |
|---|---|
| Input 1 detection | D5 |
| Input 2 detection | D4 |

### USB

The Daisy USB connection must support:
- USB Audio
- USB MIDI input
- USB MIDI output

The exact USB implementation must be validated against the Daisy Seed 3 hardware/software capabilities before implementation.

---

# 2. ESP32-S3 DISPLAY SYSTEM

The ESP32-S3 is dedicated primarily to graphical display processing.
https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.75

Display:
- Waveshare ESP32-S3 Touch AMOLED 1.75"
- Resolution: **466 × 466 pixels**

The ESP32-S3 should exploit the display's graphical capabilities as much as reasonably possible.

il y a déjà un code fonctionnel disponible sur le répertoire /download/Waveshare_AMOLED_Clock
Je veux la même font et la même définition de l'affichage. 

### Responsibilities

The ESP32-S3 is responsible for:
- graphical rendering
- menus
- parameter visualization
- animations
- graphical meters
- waveform visualization where appropriate
- display-specific interaction
- There is 2 microphones on the ESP32-S3 board that I want to use to get samples inside DAISY

The ESP32-S3 must **not** perform the main audio DSP.

The Daisy Seed remains the functional master of DCO-ONE.

---

# 3. PROCESSOR RESPONSIBILITY

## Daisy Seed = REAL-TIME / APPLICATION MASTER

The Daisy is responsible for:
- audio input/output
- audio DSP
- synthesis
- parameter processing
- physical controls
- CV inputs
- MIDI
- USB Audio
- sequencer
- presets
- application state

## ESP32-S3 = DISPLAY PROCESSOR

The ESP32-S3 is responsible for:
- graphical rendering
- display refresh
- UI visualization
- display-side interaction
- There is 2 microphones on the ESP32-S3 board that I want to use to get samples.

The ESP32 must not be required for the Daisy's audio engine to continue operating.

If communication with the ESP32 fails, **audio processing must continue normally**.

---

# 4. SOFTWARE ARCHITECTURE

The software must be modular.

Recommended high-level structure:

    DCO-ONE
    |
    +-- Hardware
    |   +-- Encoders
    |   +-- Buttons
    |   +-- CV
    |   +-- Audio
    |   +-- USB
    |
    +-- AudioEngine
    |   +-- AudioInput
    |   +-- DSP
    |   +-- Mixer
    |   +-- AudioOutput
    |   +-- USB Audio
    |
    +-- MIDI
    |   +-- USB MIDI Input
    |   +-- USB MIDI Output
    |   +-- MIDI Mapping
    |
    +-- Control
    |   +-- EncoderManager
    |   +-- ButtonManager
    |   +-- CVManager
    |   +-- ParameterManager
    |
    +-- Sequencer
    |
    +-- Presets
    |
    +-- ApplicationState
    |
    +-- DisplayCommunication
    |
    +-- Diagnostics

The exact directory/file structure may evolve, but responsibilities must remain separated.

---

# 5. REAL-TIME AUDIO RULES

The audio callback is a strict real-time context.

## Forbidden inside the audio callback

Do not use:
- malloc()
- free()
- new
- delete
- dynamic memory allocation
- filesystem operations
- blocking operations
- long logging operations
- uncontrolled loops
- operations whose execution time is unpredictable

Audio buffers and DSP working memory must be preallocated.

The audio callback must have deterministic execution as far as reasonably possible.

Parameter changes must not cause dynamic allocation in the audio callback.

Communication between real-time and non-real-time code must use appropriate lock-free or otherwise real-time-safe mechanisms where necessary.

---

# 6. AUDIO ARCHITECTURE

The following values are intentionally **NOT DEFINED YET** and must not be invented by the AI:

- Sample rate: 48kHz/24-Bit Stereo Audio 
- Audio block size: TODO
- Audio format: WAV
- Number of voices: 8 maxi
- Maximum target CPU usage: TODO
- Maximum target RAM usage: TODO

If implementation depends on an undefined value, Copilot must identify it as TODO rather than silently choosing a value.

---

# 7. PARAMETER SYSTEM

All user-controllable parameters should be managed through a central ParameterManager.

Each parameter should have, where applicable:

- unique ID
- name
- type
- minimum value
- maximum value
- default value
- current value
- MIDI mapping
- CV mapping
- display information

Conceptual architecture:

    Physical Control
          |
          v
    ParameterManager
          |
          +------> Audio Engine
          |
          +------> Display
          |
          +------> MIDI
          |
          +------> Presets

The graphical UI must not directly manipulate internal DSP objects.

The ParameterManager is the preferred interface between UI/control logic and the audio engine.

---

# 8. MIDI

The Daisy must handle USB MIDI.

Required capabilities:
- MIDI input
- MIDI output
- parameter mapping
- future MIDI control of DCO-ONE
- future synchronization with external equipment

MIDI processing must not compromise audio real-time performance.

The MIDI architecture should remain independent from the DSP implementation.

---

# 9. USB AUDIO

The Daisy USB connection must provide USB Audio in addition to USB MIDI.

The exact USB Audio implementation must be validated before implementation.

USB Audio must not compromise:
- audio stability
- DSP timing
- physical I/O
- MIDI operation

Do not assume a USB architecture that has not been verified against the Daisy Seed 3 environment.

---

# 10. DAISY ↔ ESP32-S3 COMMUNICATION

## Phase 1 — Development

Initially, the ESP32-S3 display system will be developed using the ESP32-S3 USB connection.

This allows the display interface to be developed and tested independently.

## Phase 2 — Integrated system

The final Daisy ↔ ESP32 communication will use UART.

The Daisy is the master of application state.

The ESP32 receives information required to render the UI.

The communication layer must be designed so that:
- lost messages do not crash either processor
- corrupted messages can be detected
- communication failure does not stop audio
- display communication does not block the audio callback

---

# 11. DISPLAY PROTOCOL

A structured communication protocol must be used between Daisy and ESP32.

The protocol should support, at minimum, messages such as:

- parameter update
- menu change
- value update
- application status
- meter information
- waveform/display data
- display command
- error/status information

The exact binary protocol is **TODO** and must be designed before final implementation.

Potential message structure:

    HEADER
    COMMAND
    LENGTH
    PAYLOAD
    CHECKSUM

The protocol should include:
- message framing
- message type
- payload length
- error detection
- version information

---

# 12. USER INTERFACE ARCHITECTURE

The UI should be state-driven.

Suggested concepts:

- ApplicationState
- MenuState
- PageState
- ParameterState
- DisplayState

The UI should not contain audio DSP logic.

The display should receive a defined representation of application state rather than directly accessing DSP internals.

---

# 13. SEQUENCER

The project will eventually include a sequencer.

The sequencer must be designed independently from:
- graphical rendering
- low-level hardware drivers
- DSP implementation

Sequencer timing must be stable and must not depend on display refresh.

Detailed sequencer architecture: TODO.

---

# 14. PRESETS

The project will eventually include preset management.

Preset data should be represented independently from:
- display code
- hardware drivers
- DSP implementation

Preset format, storage medium and persistence strategy: TODO.

---

# 15. ERROR HANDLING AND DIAGNOSTICS

The project must include a diagnostic strategy suitable for embedded development.

Diagnostics should allow investigation of:
- audio processing overload
- communication errors
- MIDI errors
- invalid parameters
- hardware initialization failures
- unexpected application states

Diagnostics must not introduce blocking operations or destabilize the audio callback.

Heavy logging must never be performed from the real-time audio callback.

---

# 16. DEVELOPMENT RULES FOR COPILOT

These rules are mandatory.

1. Do not invent hardware connections.
2. Do not change pin assignments.
3. Do not invent undefined technical specifications.
4. Use native C++ with libDaisy and DaisySP.
5. Do not introduce another framework without explicit approval.
6. Do not modify the architecture without explaining the reason.
7. Keep hardware, audio, MIDI, UI and application logic separated.
8. Never sacrifice real-time audio safety for UI convenience.
9. Do not perform dynamic allocation in the audio callback.
10. If information is missing, mark it TODO rather than guessing.
11. Before making major architectural changes, explain the proposed change.
12. Keep the project compilable after each development step.
13. Prefer small, testable changes over large automatic rewrites.
14. Do not modify unrelated files when implementing a feature.
15. Preserve existing working functionality when adding new features.
16. When a hardware/library capability is uncertain, identify the uncertainty and request verification rather than assuming.
17. Do not generate the entire application in one step.
18. Before implementing a subsystem, explain its proposed architecture briefly.
19. Every significant subsystem must be testable independently.
20. Never hide compilation warnings or errors by disabling diagnostics.

---

# 17. DEVELOPMENT STRATEGY

Development must be incremental.

Each stage should:
1. compile
2. run on hardware
3. be tested
4. be validated
5. only then proceed

Recommended order:

### Phase 1
Daisy hardware initialization.
On the firt step, I want a minimum setup :
    - from Daisy : SIN wave 440 Hz on USB interface
    - from ESP32-S3 : oscilloscope type display showing this SIN wave.

### Phase 2
Audio input/output.

### Phase 3
Physical controls.

### Phase 4
CV inputs.

### Phase 5
USB MIDI.

### Phase 6
USB Audio.

### Phase 7
Basic DSP engine.

### Phase 8
Parameter Manager.

### Phase 9
Display communication.

### Phase 10
ESP32-S3 display interface.

### Phase 11
Sequencer.

### Phase 12
Presets.

### Phase 13
Optimization and profiling.

This order may be modified when justified, but major changes must be explained.

---

# 18. TESTING PRINCIPLES

Each major subsystem should have a clear validation method.

Examples:

- Audio I/O → signal pass-through test
- Encoder → value/count test
- Buttons → debounce/state test
- CV → measured input range test
- MIDI → send/receive test
- USB Audio → host playback/recording test
- Display communication → message integrity test
- ESP32 display → rendering test
- DSP → known input/output test

Tests should be repeatable whenever possible.

---

# 19. PROJECT STATUS / TODO

The following information remains intentionally undefined and must be resolved before dependent implementation:

- [ ] Audio sample rate
- [ ] Audio block size
- [ ] Audio architecture
- [ ] Number of voices
- [ ] DSP signal flow
- [ ] USB Audio architecture
- [ ] USB MIDI architecture
- [ ] Daisy ↔ ESP32 UART protocol
- [ ] Display protocol
- [ ] Parameter list
- [ ] MIDI CC mapping
- [ ] CV scaling and calibration
- [ ] Sequencer architecture
- [ ] Preset storage
- [ ] Error/diagnostic strategy
- [ ] Final project directory structure

---

# 20. CORE ARCHITECTURAL PRINCIPLE

The fundamental architecture of DCO-ONE is:

    DAISY SEED 3
    = AUDIO + CONTROL + APPLICATION MASTER

              |
              | UART
              v

    ESP32-S3
    = GRAPHICAL DISPLAY PROCESSOR

The ESP32-S3 is not part of the real-time audio path, except for the microphones.

The audio engine must remain operational if the display processor is disconnected or unavailable.

The display must never compromise the stability of the audio engine.

This principle takes priority over implementation convenience.


