/*
 * DCO-ONE Phase 1: ESP32-S3 AMOLED Display - Oscilloscope
 * 
 * Hardware: Waveshare ESP32-S3-Touch-AMOLED-1.75"
 * Driver: CO5300 (QSPI 4-bit)
 * Resolution: 466 x 466 pixels
 */

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include "Arduino_GFX_Library.h"
#include "XPowersLib.h"
#include "TouchDrvCSTXXX.hpp"
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeSans24pt7b.h>

#define LCD_WIDTH       466
#define LCD_HEIGHT      466

// Waveshare ESP32-S3-Touch-AMOLED 1.75" official pinout
#define PIN_LCD_CS      12
#define PIN_LCD_SCLK    38
#define PIN_LCD_D0      4
#define PIN_LCD_D1      5
#define PIN_LCD_D2      6
#define PIN_LCD_D3      7
#define PIN_LCD_RST     39

#define PIN_I2C_SDA     15
#define PIN_I2C_SCL     14
#define PIN_TP_RST      40
#define PIN_TP_INT      11

// UART pins for Daisy audio reception
#define UART_RX_PIN     44
#define UART_TX_PIN     43
#define UART_NUM        UART_NUM_1

// Display and bus initialization
Arduino_DataBus *bus = new Arduino_ESP32QSPI(
  PIN_LCD_CS, PIN_LCD_SCLK, PIN_LCD_D0, PIN_LCD_D1, PIN_LCD_D2, PIN_LCD_D3);

Arduino_CO5300 *gfx = new Arduino_CO5300(
  bus, PIN_LCD_RST, 0 /* rotation */, LCD_WIDTH, LCD_HEIGHT, 6, 0, 0, 0);

Arduino_Canvas_Indexed *canvas;

XPowersAXP2101 axp;

// CST9217 capacitive touch controller (I2C, shares the SDA/SCL bus with AXP2101)
TouchDrvCSTXXX touch;
static bool s_touch_ok = false;

// Color definitions
#define C_BLACK   RGB565_BLACK
#define C_RED     RGB565_RED
#define C_GREEN   RGB565_GREEN
#define C_BLUE    RGB565_BLUE
#define C_CYAN    RGB565_CYAN
#define C_YELLOW  RGB565_YELLOW
#define C_WHITE   RGB565_WHITE
#define C_ORANGE  0xFDA0
#define C_DKGRAY  0x4208

// Pale/pastel accent palette (softer than the raw neon colors from
// menu.json) used for the root menu wheel, one per category, in the same
// order as menu.json's main_menus (PLAY, OSC, VCF, ENV1, ENV2, LFO, MATRIX,
// FX, PRESETS, MIDI, SYSTEM). Also used as the generic default accent
// (replacing the old bright C_CYAN) for hub borders / decorative icons.
#define C_PALE_ACCENT  0xB75F // pale cyan, generic default accent
#define C_PALE_PLAY    0xFD97 // pale pink
#define C_PALE_OSC     0xFEB6 // pale peach
#define C_PALE_VCF     0xB75F // pale cyan
#define C_PALE_ENV1    0xBFF9 // pale mint
#define C_PALE_ENV2    0xDFF6 // pale lime
#define C_PALE_LFO     0xFD9B // pale rose
#define C_PALE_MATRIX  0xD59F // pale lavender
#define C_PALE_FX      0xB7FA // pale teal-green
#define C_PALE_PRESETS 0xFD9E // pale magenta
#define C_PALE_MIDI    0xFF16 // pale amber
#define C_PALE_SYSTEM  0xCE59 // pale grey

// Main menu state: a navigable tree (main wheel / submenus / value lists)
// mirrored from the Daisy side (see daisy/src/main.cpp). Every level is
// rendered with the same wheel format. To add an item at any depth, add an
// entry to the relevant array below and update its count -- keep both
// firmwares' trees in sync (same order, same child counts).
//
// Navigation is driven by "NAV,P=<path>" frames from the Daisy over USB
// (relayed by usb_bridge.py): <path> is a dot-separated list of child
// indices from the root, e.g. "0" = OSC highlighted at root, "0.0" = ONDE
// highlighted inside the OSC submenu, "0.0.1" = SQUARE highlighted inside
// ONDE's waveform list.
struct MenuNode {
    const char*     label;
    uint16_t        color;      // 0 = no override, use default palette
    const MenuNode* children;
    uint8_t         childCount;
};

// Menu content mirrored from menu.json, generated into menu_generated.inc.
// kRootNode, kRootMenu and kRootMenuCount are defined in menu_generated.inc.
#include "menu_generated.inc"

// Max navigation depth (root=0), matches the Daisy side's kMaxMenuDepth
static const int kMaxMenuDepth = 6;

// Current navigation state, resolved from the last "NAV,P=<path>" frame
static const MenuNode* s_menu_stack[kMaxMenuDepth] = { &kRootNode };
static int              s_menu_selected[kMaxMenuDepth] = { 0 }; // highlighted index at each depth
static int              s_menu_depth = 0;                       // current depth (0 = root wheel)
// Current value of the highlighted item, when it is itself a live-value list
// (e.g. ONDE's waveform) -- shown in the center hub instead of the parent's
// own label. -1 = none, show the parent's label as usual. Set from the
// optional ",V=<n>" suffix on "NAV,P=<path>[,V=<n>]" frames.
static int s_menu_preview_value = -1;
static String s_serial_buffer;

// Numeric parameter edit mode (e.g. BPM), driven by "EDIT,V=..,MIN=..,MAX=..,U=.."
// frames from the Daisy (see NumericParam in daisy/src/main.cpp). Replaces the
// wheel view with a RingGauge (menu.json's "parameter_edit" mode) until the
// next "NAV,P=<path>" frame (sent on confirm/cancel) switches back.
static bool    s_editing = false;
static String  s_edit_label;
static int32_t s_edit_value = 0;
static int32_t s_edit_min   = 0;
static int32_t s_edit_max   = 0;
static String  s_edit_unit;

// Root-wheel status snapshot, driven by "STAT,BPM=..,ROOT=..,SCALE=..,PLAY=..,NOTE=.."
// frames from the Daisy (see SendPlayStatus in daisy/src/main.cpp). Shown in
// the center hub only while the root wheel itself is on screen. Falls back
// to the plain waveform icon until the first frame arrives.
static bool    s_status_valid   = false;
static int32_t s_status_bpm     = 120;
static int32_t s_status_root    = 9;
static int32_t s_status_scale   = 0;
static bool    s_status_playing = false;
static int     s_status_note    = -1; // MIDI note currently sounding (-1 = none)
static bool    s_status_t440    = false;

// ENV1 envelope parameters, driven by STAT frames from the Daisy
static int32_t s_env1_attack  = 50;
static int32_t s_env1_decay   = 200;
static int32_t s_env1_sustain = 80;
static int32_t s_env1_release = 300;

// VCF parameters, driven by STAT frames from the Daisy
static int32_t s_vcf_type      = 0;
static int32_t s_vcf_cutoff    = 5000;
static int32_t s_vcf_resonance = 0;
static int32_t s_vcf_key       = 50;
static int32_t s_vcf_drive     = 0;
static int32_t s_vcf_env       = 0;

// Last values actually rendered, so repeated identical STAT heartbeats
// (sent alongside every NAV frame) don't force a redundant full redraw.
static int32_t s_status_bpm_drawn     = -1;
static int32_t s_status_root_drawn    = -1;
static int32_t s_status_scale_drawn   = -1;
static bool    s_status_playing_drawn = false;
static int     s_status_note_drawn    = -1;
static bool    s_status_t440_drawn    = false;
static int32_t s_env1_attack_drawn    = -1;
static int32_t s_env1_decay_drawn     = -1;
static int32_t s_env1_sustain_drawn   = -1;
static int32_t s_env1_release_drawn   = -1;
static int32_t s_vcf_type_drawn      = -1;
static int32_t s_vcf_cutoff_drawn    = -1;
static int32_t s_vcf_resonance_drawn = -1;

// Polymetric step wheel overlay, driven by "POLY,N=..,C=..,ST=..,DEG=..,PLAY=..,NOTE=.."
// frames from the Daisy (see SendPolyState in daisy/src/main.cpp). Replaces
// the normal wheel view until the next "NAV,P=" frame (sent when the Daisy
// exits the overlay on HOME) switches back.
static constexpr int kMaxPolySteps = 32; // == Daisy's kMaxPolySteps
static bool    s_poly_active  = false;
static int     s_poly_count   = 16;
static int     s_poly_cursor  = 0;
static char    s_poly_states[kMaxPolySteps + 1] = {};
static int32_t s_poly_degree  = 0;
static int     s_poly_play    = -1;
static int     s_poly_note    = -1; // MIDI note currently sounding (-1 = none)

// Audio waveform buffer for display
static constexpr int kAudioSampleCount = 12;
static int8_t s_audio_samples[kAudioSampleCount] = { 0 };
static bool s_audio_valid = false;

static const int kWheelGapDeg       = 3;   // black gap between wedges, in degrees
static const int kWheelOuterBulge   = 12;  // selected wedge extends further out
static const int kWheelInnerRadius  = 155; // thin wedge ring, most of the screen goes to the center hub
static const int kWheelCenterRadius = 140; // decorative center hub, kept as visible as possible

// Reads the integer following `key` in `s` (e.g. key="MIN=" in "...,MIN=1,...");
// relies on String::toInt() stopping at the first non-digit character, so the
// trailing comma/next field doesn't need to be located.
static long parseLongField(const String& s, const char* key)
{
    int p = s.indexOf(key);
    return (p < 0) ? 0 : s.substring(p + strlen(key)).toInt();
}

// Reads the string following `key` in `s`, up to the next comma or the end.
static String parseStringField(const String& s, const char* key)
{
    int p = s.indexOf(key);
    if (p < 0)
        return String();
    int start = p + strlen(key);
    int comma = s.indexOf(',', start);
    return comma >= 0 ? s.substring(start, comma) : s.substring(start);
}

// Parses "NAV,P=<path>" (a dot-separated list of child indices from the
// root) and walks the mirrored menu tree to resolve which node is currently
// being browsed and which of its children is highlighted, then redraws.
static void applyNavPath(const String& path)
{
    int indices[kMaxMenuDepth];
    int depth = 0;
    int start = 0;
    while (depth < kMaxMenuDepth)
    {
        int dot = path.indexOf('.', start);
        String tok = (dot >= 0) ? path.substring(start, dot) : path.substring(start);
        indices[depth] = tok.toInt();
        depth++;
        if (dot < 0)
            break;
        start = dot + 1;
    }

    s_menu_stack[0] = &kRootNode;
    for (int d = 1; d < depth; d++)
    {
        const MenuNode* parent = s_menu_stack[d - 1];
        int idx = indices[d - 1];
        if (idx < 0 || idx >= parent->childCount)
            idx = 0;
        s_menu_stack[d] = &parent->children[idx];
    }
    for (int d = 0; d < depth; d++)
        s_menu_selected[d] = indices[d];

    s_menu_depth = depth - 1;
}

// Center angle (in degrees) of wedge `index` out of `count` (0=index0 at 12
// o'clock, clockwise), matching Arduino_GFX's fillArc()/drawArc() convention
// (0deg=3 o'clock, clockwise, since both use the same cos/sin(angle)
// placement on a y-down screen).
static float wedgeCenterAngleDeg(int index, int count)
{
    return -90.0f + index * (360.0f / count);
}

// Point at `radius` px from screen center, at `angleDeg` (see wedgeCenterAngleDeg).
static void computeAngleRadiusPosition(float angleDeg, int radius, int& x, int& y)
{
    float angle = radians(angleDeg);
    x = LCD_WIDTH / 2 + (int)(radius * cosf(angle));
    y = LCD_HEIGHT / 2 + (int)(radius * sinf(angle));
}

// Converts a MIDI note number (0..127) to a name such as "C#4" or "--".
static const char* midiNoteToName(int note)
{
    static char buf[8];
    if (note < 0 || note > 127)
    {
        buf[0] = '-';
        buf[1] = '-';
        buf[2] = '\0';
        return buf;
    }
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    int octave = (note / 12) - 1;
    snprintf(buf, sizeof(buf), "%s%d", names[note % 12], octave);
    return buf;
}

// Draws `text` (already measured via getTextBounds at cursor origin (0,0),
// giving bx1/by1/bw/bh) centered on (cx,cy), rotated so its baseline runs
// tangential to the wheel's circle at `wedgeAngleDeg` (same convention as
// wedgeCenterAngleDeg/computeAngleRadiusPosition) instead of always
// horizontal. Tangential is the mathematically safest orientation: a label
// of length L centered at radius r never reaches farther than
// sqrt(r^2 + (L/2)^2) from screen center whatever its angular position,
// versus up to r + L/2 (i.e. straight off the round screen) for a long
// label drawn horizontally near the left/right side. The tangent line has
// two opposite directions; whichever keeps the text within +/-90 deg of
// upright is picked so labels are never drawn upside down.
static void drawRotatedLabel(int cx, int cy, float wedgeAngleDeg, const char* text,
                              const GFXfont* font, int16_t bx1, int16_t by1,
                              uint16_t bw, uint16_t bh, uint16_t color)
{
    float tangentDeg = wedgeAngleDeg + 90.0f;
    while (tangentDeg > 90.0f)   tangentDeg -= 180.0f;
    while (tangentDeg <= -90.0f) tangentDeg += 180.0f;

    if (fabsf(tangentDeg) < 0.5f)
    {
        // Practically horizontal already: use the cheap built-in text draw.
        canvas->setCursor(cx - (int)bw / 2 - bx1, cy - (int)bh / 2 - by1);
        canvas->print(text);
        return;
    }

    float rad = radians(tangentDeg);
    float cs  = cosf(rad), sn = sinf(rad);
    float localCx = bx1 + bw / 2.0f;
    float localCy = by1 + bh / 2.0f;

    int16_t penX = 0, penY = 0;
    for (const char* p = text; *p; p++)
    {
        uint8_t ch = (uint8_t)*p;
        if (ch < font->first || ch > font->last)
            continue;
        const GFXglyph& g = font->glyph[ch - font->first];
        uint16_t bo   = g.bitmapOffset;
        uint8_t  bits = 0, bit = 0;
        for (uint8_t yy = 0; yy < g.height; yy++)
        {
            for (uint8_t xx = 0; xx < g.width; xx++, bits <<= 1)
            {
                if (!(bit++ & 7))
                    bits = font->bitmap[bo++];
                if (bits & 0x80)
                {
                    float lx = (penX + g.xOffset + xx) - localCx;
                    float ly = (penY + g.yOffset + yy) - localCy;
                    int   rx = cx + (int)roundf(lx * cs - ly * sn);
                    int   ry = cy + (int)roundf(lx * sn + ly * cs);
                    canvas->drawPixel(rx, ry, color);
                }
            }
        }
        penX += g.xAdvance;
    }
}

// Picks black or white text for readability against an RGB565 background
// color, based on its perceived luminance (so a dark color like blue keeps
// white text, while a bright one like cyan or green gets black text).
static uint16_t contrastingTextColor(uint16_t bgColor565)
{
    uint8_t r8 = ((bgColor565 >> 11) & 0x1F) * 255 / 31;
    uint8_t g8 = ((bgColor565 >> 5) & 0x3F) * 255 / 63;
    uint8_t b8 = (bgColor565 & 0x1F) * 255 / 31;
    int luminance = (r8 * 299 + g8 * 587 + b8 * 114) / 1000;
    return luminance > 140 ? C_BLACK : C_WHITE;
}

// Draws a full waveform from audio samples, scaled to fit in a circular area.
// The waveform is drawn as connected line segments from sample to sample.
static void drawWaveformDisplay(int cx, int cy, int radius, uint16_t color)
{
    if (!s_audio_valid)
        return;

    // Map samples to screen coordinates
    // Center horizontally, scale vertically to fit within radius
    int prevX = 0, prevY = 0;
    for (int i = 0; i < kAudioSampleCount; i++)
    {
        // X: spread samples across the width
        int x = cx - radius + (i * (radius * 2) / (kAudioSampleCount - 1));
        
        // Y: scale sample value [-127, 127] to screen Y [-radius, radius]
        int sample_y = (s_audio_samples[i] * radius) / 127;
        int y = cy - sample_y;
        
        // Clamp to stay within circular area
        y = max(cy - radius, min(y, cy + radius));
        
        if (i > 0)
        {
            canvas->drawLine(prevX, prevY, x, y, color);
        }
        prevX = x;
        prevY = y;
    }
}

// Small decorative "audio bars" icon drawn in the wheel's center hub.
static void drawWaveformIcon(int cx, int cy, int halfSpan, uint16_t color)
{
    static const int kBarHeights[] = { 8, 18, 26, 18, 8 };
    const int barCount = sizeof(kBarHeights) / sizeof(kBarHeights[0]);
    const int spacing  = (halfSpan * 2) / (barCount + 1);
    int x = cx - halfSpan + spacing;
    for (int i = 0; i < barCount; i++, x += spacing)
    {
        int h = kBarHeights[i];
        canvas->drawFastVLine(x, cy - h, h * 2, color);
    }
}

// Draws an ADSR envelope shape inside a centered box of half-width/half-height
// determined by `radius`. Time is scaled with sqrt() so short segments remain
// visible while long ones still fill the available space.
static void drawEnvelopeShape(int cx, int cy, int radius, int32_t a_ms, int32_t d_ms, int32_t s_pct, int32_t r_ms, uint16_t color)
{
    int halfW = radius;
    int halfH = (int)(radius * 0.8f);
    int x0 = cx - halfW;
    int y0 = cy - halfH;
    int x1 = cx + halfW;
    int y1 = cy + halfH;

    float wa = sqrtf((float)a_ms);
    float wd = sqrtf((float)d_ms);
    float wr = sqrtf((float)r_ms);
    float ws = sqrtf(200.0f); // fixed visual sustain length
    float total = wa + wd + ws + wr;
    if (total < 1.0f)
        total = 1.0f;

    int w = x1 - x0;
    int attack_w  = (int)(wa  / total * w);
    int decay_w   = (int)(wd  / total * w);
    int sustain_w = (int)(ws  / total * w);
    int release_w = w - attack_w - decay_w - sustain_w;
    if (attack_w < 1)  attack_w = 1;
    if (decay_w < 1)   decay_w = 1;
    if (sustain_w < 1) sustain_w = 1;
    if (release_w < 1) release_w = 1;

    int y_base = y1;
    int y_top  = y0;
    int y_sus  = y_base - ((s_pct * (y_base - y_top)) / 100);

    int xa   = x0 + attack_w;
    int xad  = xa + decay_w;
    int xadr = xad + sustain_w;

    // Envelope outline
    canvas->drawLine(x0, y_base, xa, y_top, color);     // Attack
    canvas->drawLine(xa, y_top, xad, y_sus, color);     // Decay
    canvas->drawLine(xad, y_sus, xadr, y_sus, color);   // Sustain
    canvas->drawLine(xadr, y_sus, x1, y_base, color);   // Release

    // Joint markers
    canvas->fillCircle(x0, y_base, 3, color);
    canvas->fillCircle(xa, y_top, 3, color);
    canvas->fillCircle(xad, y_sus, 3, color);
    canvas->fillCircle(xadr, y_sus, 3, color);
    canvas->fillCircle(x1, y_base, 3, color);
}

// Small transport icon centered on (x,y): a right-pointing triangle for PLAY,
// a square for STOP.
static void drawPlayStopIcon(int x, int y, int size, bool playing, uint16_t color)
{
    if (playing)
        canvas->fillTriangle(x - size, y - size, x - size, y + size, x + size, y, color);
    else
        canvas->fillRect(x - size, y - size, size * 2, size * 2, color);
}

// Draws the shared center hub used by both the root menu wheel and the
// polymetric wheel: BPM, currently-sounding MIDI note, root+scale, live
// waveform and PLAY/STOP transport. When `showStepCount` is true (poly
// wheel) the active step count is added at the very top of the hub.
static void drawStatusHub(int cx, int cy, bool showStepCount)
{
    int16_t  tx1, ty1;
    uint16_t tw, th;
    char     buf[24];

    int yOffset = showStepCount ? -18 : 0;

    // Optional step count label, only on the poly wheel
    if (showStepCount)
    {
        snprintf(buf, sizeof(buf), "%d PAS", s_poly_count);
        canvas->setFont(&FreeSans12pt7b);
        canvas->setTextColor(C_PALE_ACCENT);
        canvas->getTextBounds(buf, 0, 0, &tx1, &ty1, &tw, &th);
        canvas->setCursor(cx - (int)tw / 2 - tx1, cy - 138 - ty1);
        canvas->print(buf);
    }

    // BPM at top
    snprintf(buf, sizeof(buf), "%ld BPM", (long)s_status_bpm);
    canvas->setFont(&FreeSans18pt7b);
    canvas->setTextColor(C_WHITE);
    canvas->getTextBounds(buf, 0, 0, &tx1, &ty1, &tw, &th);
    canvas->setCursor(cx - (int)tw / 2 - tx1, cy - 95 + yOffset - ty1);
    canvas->print(buf);

    // 440 Hz test-tone badge (top-right of BPM when active)
    if (s_status_t440)
    {
        const int badgeR = 18;
        const int badgeX = cx + (int)tw / 2 + badgeR + 12;
        const int badgeY = cy - 95 + yOffset - (int)th / 2;
        canvas->fillCircle(badgeX, badgeY, badgeR, C_YELLOW);
        canvas->setFont(&FreeSans9pt7b);
        canvas->setTextColor(C_BLACK);
        canvas->getTextBounds("440", 0, 0, &tx1, &ty1, &tw, &th);
        canvas->setCursor(badgeX - (int)tw / 2 - tx1, badgeY - (int)th / 2 - ty1);
        canvas->print("440");
    }

    // Root & Scale just below BPM (small)
    int rootIdx = (s_status_root >= 0 && s_status_root < kPLAY_play_rootOptionCount) ? s_status_root : 0;
    const char* rootLabel = kPLAY_play_rootOptions[rootIdx].label;
    int scaleIdx = (s_status_scale >= 0 && s_status_scale < kPLAY_play_scaleOptionCount) ? s_status_scale : 0;
    const char* scaleLabel = kPLAY_play_scaleOptions[scaleIdx].label;
    snprintf(buf, sizeof(buf), "%s %s", rootLabel, scaleLabel);
    canvas->setFont(&FreeSans9pt7b);
    canvas->setTextColor(C_PALE_ACCENT);
    canvas->getTextBounds(buf, 0, 0, &tx1, &ty1, &tw, &th);
    canvas->setCursor(cx - (int)tw / 2 - tx1, cy - 62 + yOffset - ty1);
    canvas->print(buf);

    // Currently-sounding MIDI note (large, central)
    const char* noteLabel = midiNoteToName(s_poly_active ? s_poly_note : s_status_note);
    canvas->setFont(&FreeSans24pt7b);
    canvas->setTextColor(C_WHITE);
    canvas->getTextBounds(noteLabel, 0, 0, &tx1, &ty1, &tw, &th);
    canvas->setCursor(cx - (int)tw / 2 - tx1, cy - 16 + yOffset - ty1);
    canvas->print(noteLabel);

    // Waveform under the note
    drawWaveformDisplay(cx, cy + 50 + yOffset, 50, C_PALE_ACCENT);

    // Play/Stop at bottom
    const char* stateLabel = s_status_playing ? "PLAY" : "STOP";
    uint16_t    stateColor = s_status_playing ? C_GREEN : C_RED;
    canvas->setFont(&FreeSans12pt7b);
    canvas->getTextBounds(stateLabel, 0, 0, &tx1, &ty1, &tw, &th);
    const int iconSize = 10;
    const int gap      = 8;
    const int rowY     = cy + 110 + yOffset;
    int blockW  = iconSize * 2 + gap + (int)tw;
    int startX  = cx - blockW / 2;
    drawPlayStopIcon(startX + iconSize, rowY, iconSize, s_status_playing, stateColor);
    canvas->setTextColor(stateColor);
    canvas->setCursor(startX + iconSize * 2 + gap - tx1, rowY - (int)th / 2 - ty1);
    canvas->print(stateLabel);
}

// Center hub variant used inside the ENV1 menu: a large ADSR shape plus the
// current parameter label and the focused parameter's value in large type.
static void drawEnvelopeHub(int cx, int cy, const char* label)
{
    int16_t  tx1, ty1;
    uint16_t tw, th;

    // Category / parameter label at the top
    canvas->setFont(&FreeSans12pt7b);
    canvas->setTextColor(C_PALE_ENV1);
    canvas->getTextBounds(label, 0, 0, &tx1, &ty1, &tw, &th);
    canvas->setCursor(cx - (int)tw / 2 - tx1, cy - 110 - ty1);
    canvas->print(label);

    // Large ADSR shape in the center (slightly smaller to leave room for value)
    drawEnvelopeShape(cx, cy - 18, 84, s_env1_attack, s_env1_decay, s_env1_sustain, s_env1_release, C_PALE_ENV1);

    // Focused parameter value (big) and unit below the envelope
    int32_t value = 0;
    const char* unit = "";
    if (strcmp(label, "Attack") == 0)       { value = s_env1_attack;  unit = "ms"; }
    else if (strcmp(label, "Decay") == 0)   { value = s_env1_decay;   unit = "ms"; }
    else if (strcmp(label, "Sustain") == 0) { value = s_env1_sustain; unit = "%"; }
    else if (strcmp(label, "Release") == 0) { value = s_env1_release; unit = "ms"; }

    char buf[24];
    snprintf(buf, sizeof(buf), "%ld", (long)value);
    canvas->setFont(&FreeSans24pt7b);
    canvas->setTextColor(C_WHITE);
    canvas->getTextBounds(buf, 0, 0, &tx1, &ty1, &tw, &th);
    canvas->setCursor(cx - (int)tw / 2 - tx1, cy + 102 - ty1);
    canvas->print(buf);

    if (unit[0] != '\0')
    {
        canvas->setFont(&FreeSans12pt7b);
        canvas->setTextColor(C_PALE_ENV1);
        canvas->getTextBounds(unit, 0, 0, &tx1, &ty1, &tw, &th);
        canvas->setCursor(cx - (int)tw / 2 - tx1, cy + 138 - ty1);
        canvas->print(unit);
    }
}

// Full-screen edit view used for ENV1 numeric parameters: large envelope
// preview plus the edited value, instead of the generic ring gauge.
static void drawEnvelopeEditScreen(const char* label, int32_t value, int32_t minV, int32_t maxV, const char* unit)
{
    (void)minV;
    (void)maxV;

    canvas->fillScreen(C_BLACK);

    const int cx = LCD_WIDTH / 2;
    const int cy = LCD_HEIGHT / 2;

    int16_t  tx1, ty1;
    uint16_t tw, th;

    // Parameter label at top
    canvas->setFont(&FreeSans12pt7b);
    canvas->setTextColor(C_PALE_ENV1);
    canvas->getTextBounds(label, 0, 0, &tx1, &ty1, &tw, &th);
    canvas->setCursor(cx - (int)tw / 2 - tx1, cy - 175 - ty1);
    canvas->print(label);

    // Large envelope preview
    drawEnvelopeShape(cx, cy - 10, 165, s_env1_attack, s_env1_decay, s_env1_sustain, s_env1_release, C_PALE_ENV1);

    // Edited value (big) and unit
    char valBuf[16];
    snprintf(valBuf, sizeof(valBuf), "%ld", (long)value);
    canvas->setFont(&FreeSans24pt7b);
    canvas->setTextColor(C_WHITE);
    canvas->getTextBounds(valBuf, 0, 0, &tx1, &ty1, &tw, &th);
    canvas->setCursor(cx - (int)tw / 2 - tx1, cy + 155 - ty1);
    canvas->print(valBuf);

    if (unit != nullptr && unit[0] != '\0')
    {
        canvas->setFont(&FreeSans12pt7b);
        canvas->setTextColor(C_PALE_ENV1);
        canvas->getTextBounds(unit, 0, 0, &tx1, &ty1, &tw, &th);
        canvas->setCursor(cx - (int)tw / 2 - tx1, cy + 200 - ty1);
        canvas->print(unit);
    }

    // Bottom hint
    canvas->setFont(&FreeSans9pt7b);
    canvas->setTextColor(C_DKGRAY);
    const char* hint = "PRESS TO VALIDATE";
    canvas->getTextBounds(hint, 0, 0, &tx1, &ty1, &tw, &th);
    canvas->setCursor(cx - (int)tw / 2 - tx1, cy + 210 - ty1);
    canvas->print(hint);

    canvas->flush();
}

// Center hub variant used inside the VCF menu: a stylized filter response
// curve plus the current filter type, cutoff and resonance values.
static void drawVcfHub(int cx, int cy, const char* label)
{
    (void)label;
    int16_t  tx1, ty1;
    uint16_t tw, th;

    // Filter type label at the top (big)
    int typeIdx = (s_vcf_type >= 0 && s_vcf_type < kVCF_vcf_typeOptionCount) ? s_vcf_type : 0;
    const char* typeLabel = kVCF_vcf_typeOptions[typeIdx].label;

    canvas->setFont(&FreeSans24pt7b);
    canvas->setTextColor(C_PALE_VCF);
    canvas->getTextBounds(typeLabel, 0, 0, &tx1, &ty1, &tw, &th);
    canvas->setCursor(cx - (int)tw / 2 - tx1, cy - 110 - ty1);
    canvas->print(typeLabel);

    // Draw a stylized filter response curve in the center hub
    const int halfW = 110;
    const int halfH = 70;
    int x0 = cx - halfW;
    int y0 = cy + halfH;   // baseline (low gain)
    int x1 = cx + halfW;
    int y1 = cy - halfH;   // top (high gain)

    // Cutoff position mapped logarithmically: 20 Hz -> x0, 20000 Hz -> x1
    float cutoffHz = (float)s_vcf_cutoff;
    if (cutoffHz < 20.0f) cutoffHz = 20.0f;
    if (cutoffHz > 20000.0f) cutoffHz = 20000.0f;
    float logCutoff = logf(cutoffHz / 20.0f) / logf(1000.0f); // 0..1
    int xCut = x0 + (int)(logCutoff * (x1 - x0));
    xCut = constrain(xCut, x0 + 4, x1 - 4);

    // Resonance peak height: 0% -> no peak, 100% -> tall peak above the box
    float resPct = (float)s_vcf_resonance * 0.01f;
    int peakH = (int)(resPct * (halfH * 0.9f));
    int yPeak = y0 - halfH - peakH;
    int yPlateau = y0 - (int)(halfH * 0.55f);

    uint16_t curveColor = C_PALE_VCF;

    // Box outline for the response graph
    canvas->drawRect(x0 - 2, y1 - 2, x1 - x0 + 4, y0 - y1 + 4, 0x8410);

    switch (typeIdx)
    {
        case 0: // LP24
        case 1: // LP12
        {
            int slopeW = (typeIdx == 0) ? (x1 - xCut) / 3 : (x1 - xCut) * 2 / 3;
            slopeW = max(slopeW, 4);
            int xSlopeEnd = (xCut + slopeW > x1) ? x1 : xCut + slopeW;

            // Plateau -> resonance peak -> slope down -> floor
            canvas->drawLine(x0, yPlateau, xCut, yPlateau, curveColor);
            canvas->drawLine(xCut, yPlateau, xCut, yPeak, curveColor);
            canvas->drawLine(xCut, yPeak, xSlopeEnd, y0, curveColor);
            canvas->drawLine(xSlopeEnd, y0, x1, y0, curveColor);

            // Lightweight fill: solid rectangle for the plateau, then a single
            // thick line for the downward slope. Avoids the previous per-pixel
            // drawFastVLine loop (220 calls per redraw) that caused menu lag.
            if (yPlateau < y0)
                canvas->fillRect(x0, yPlateau, xCut - x0, y0 - yPlateau, 0x2D13);
            if (xSlopeEnd > xCut && yPeak < y0)
            {
                // Approximate the slope fill with a filled triangle.
                canvas->fillTriangle(xCut, yPeak, xSlopeEnd, y0, xCut, y0, 0x2D13);
            }
            break;
        }
        case 2: // BP12
        {
            int bw = (x1 - x0) / 5;
            int xLow  = (xCut - bw < x0) ? x0 : xCut - bw;
            int xHigh = (xCut + bw > x1) ? x1 : xCut + bw;

            canvas->drawLine(x0, y0, xLow, yPlateau, curveColor);
            canvas->drawLine(xLow, yPlateau, xCut, yPeak, curveColor);
            canvas->drawLine(xCut, yPeak, xHigh, yPlateau, curveColor);
            canvas->drawLine(xHigh, yPlateau, x1, y0, curveColor);
            break;
        }
        case 3: // HP12
        {
            int slopeW = (x1 - x0) / 3;
            int xSlopeStart = (xCut - slopeW < x0) ? x0 : xCut - slopeW;

            canvas->drawLine(x0, y0, xSlopeStart, y0, curveColor);
            canvas->drawLine(xSlopeStart, y0, xCut, yPeak, curveColor);
            canvas->drawLine(xCut, yPeak, xCut, yPlateau, curveColor);
            canvas->drawLine(xCut, yPlateau, x1, yPlateau, curveColor);
            break;
        }
        case 4: // NOTCH
        {
            int nw = (x1 - x0) / 8;
            int xLow  = (xCut - nw < x0) ? x0 : xCut - nw;
            int xHigh = (xCut + nw > x1) ? x1 : xCut + nw;

            canvas->drawLine(x0, yPlateau, xLow, yPlateau, curveColor);
            canvas->drawLine(xLow, yPlateau, xCut, y0 - peakH, curveColor);
            canvas->drawLine(xCut, y0 - peakH, xHigh, yPlateau, curveColor);
            canvas->drawLine(xHigh, yPlateau, x1, yPlateau, curveColor);
            break;
        }
    }

    // Cutoff value (left-bottom of curve box)
    char buf[24];
    snprintf(buf, sizeof(buf), "%ld Hz", (long)s_vcf_cutoff);
    canvas->setFont(&FreeSans12pt7b);
    canvas->setTextColor(C_WHITE);
    canvas->getTextBounds(buf, 0, 0, &tx1, &ty1, &tw, &th);
    canvas->setCursor(x0 - tx1, y0 + 26 - ty1);
    canvas->print(buf);

    // Resonance value (right-bottom of curve box)
    snprintf(buf, sizeof(buf), "RES %ld%%", (long)s_vcf_resonance);
    canvas->setTextColor(C_PALE_VCF);
    canvas->getTextBounds(buf, 0, 0, &tx1, &ty1, &tw, &th);
    canvas->setCursor(x1 - (int)tw - tx1, y0 + 26 - ty1);
    canvas->print(buf);
}

// Generic renderer used at every navigation depth: draws `parent`'s children
// as a pie-wedge wheel (same format as the main menu), the `selectedIndex`
// wedge highlighted, and `parent`'s own label in the decorative center hub
// (so the hub reads "MENU" at the root, "OSC" inside the OSC submenu, etc).
// Items with a non-default color (see MenuColor on the Daisy side) keep a
// colored outline even when idle, e.g. blue for OSC, green for SETUP.
static void drawMenuWheel(const MenuNode* parent, int selectedIndex)
{
    canvas->fillScreen(C_BLACK);

    const int   cx          = LCD_WIDTH / 2;
    const int   cy          = LCD_HEIGHT / 2;
    const int   outerRadius = min(LCD_WIDTH, LCD_HEIGHT) / 2 - 4;
    const int   count       = parent->childCount;
    if (count <= 0)
    {
        canvas->flush();
        return;
    }
    const float step        = 360.0f / count;
    const float halfGap     = kWheelGapDeg / 2.0f;

    canvas->setFont(&FreeSans9pt7b);
    canvas->setTextSize(1);

    for (int i = 0; i < count; i++)
    {
        const MenuNode& item = parent->children[i];
        bool  selected   = (i == selectedIndex);
        float center     = wedgeCenterAngleDeg(i, count);
        float startA     = center - step / 2.0f + halfGap;
        float endA       = center + step / 2.0f - halfGap;
        int   outerR     = outerRadius + (selected ? kWheelOuterBulge : 0);
        uint16_t itemColor = item.color != 0 ? item.color : C_PALE_ACCENT;

        canvas->fillArc(cx, cy, outerR, kWheelInnerRadius, startA, endA,
                         selected ? itemColor : C_BLACK);
        if (selected)
            canvas->drawArc(cx, cy, outerR + 2, outerR + 2, startA, endA, C_WHITE);
        else if (item.color != 0)
            canvas->fillArc(cx, cy, outerR, outerR - 4, startA, endA, itemColor);

        int lx, ly;
        computeAngleRadiusPosition(center, (kWheelInnerRadius + outerRadius) / 2, lx, ly);

        uint16_t textColor = contrastingTextColor(selected ? itemColor : C_BLACK);
        canvas->setTextColor(textColor);
        int16_t  tx1, ty1;
        uint16_t tw, th;
        canvas->getTextBounds(item.label, 0, 0, &tx1, &ty1, &tw, &th);
        drawRotatedLabel(lx, ly, center, item.label, &FreeSans9pt7b, tx1, ty1, tw, th, textColor);
    }

    // Decorative center hub. Normally labeled with the currently browsed
    // node's own name, but if the highlighted item is itself a live-value
    // list (e.g. ONDE), show its current value instead (e.g. "SIN").
    const char* hubLabel = parent->label;
    if (s_menu_preview_value >= 0 && selectedIndex >= 0 && selectedIndex < count)
    {
        const MenuNode& highlighted = parent->children[selectedIndex];
        if (highlighted.children != nullptr && s_menu_preview_value < highlighted.childCount)
            hubLabel = highlighted.children[s_menu_preview_value].label;
    }

    canvas->fillCircle(cx, cy, kWheelCenterRadius, C_BLACK);
    // Removed: canvas->drawCircle(cx, cy, kWheelCenterRadius, C_PALE_ACCENT);
    bool in_env1 = (s_menu_depth >= 1 && s_menu_stack[1]->children == kEnv1Submenu);
    bool in_vcf  = (s_menu_depth >= 1 && s_menu_stack[1]->children == kVcfSubmenu);

    // Inside ENV1, the hub shows the focused ADSR parameter's integer value,
    // so the hub label must be the selected child's label (Attack/Decay/...
    // instead of the parent "ENV1").
    if (in_env1 && selectedIndex >= 0 && selectedIndex < count)
        hubLabel = parent->children[selectedIndex].label;
    if (parent == &kRootNode)
    {
        drawStatusHub(cx, cy, false);
    }
    else if (in_env1)
    {
        drawEnvelopeHub(cx, cy, hubLabel);
    }
    else if (in_vcf)
    {
        drawVcfHub(cx, cy, hubLabel);
    }
    else
    {
        // Use the biggest font that still fits within the hub, for max visibility.
        const int   hubMaxWidth = (kWheelCenterRadius - 12) * 2;
        const GFXfont* hubFont  = &FreeSans24pt7b;
        canvas->setFont(hubFont);
        canvas->setTextColor(C_PALE_ACCENT);
        int16_t  tx1, ty1;
        uint16_t tw, th;
        canvas->getTextBounds(hubLabel, 0, 0, &tx1, &ty1, &tw, &th);
        if ((int)tw > hubMaxWidth)
        {
            hubFont = &FreeSans18pt7b;
            canvas->setFont(hubFont);
            canvas->getTextBounds(hubLabel, 0, 0, &tx1, &ty1, &tw, &th);
        }
        if ((int)tw > hubMaxWidth)
        {
            hubFont = &FreeSans12pt7b;
            canvas->setFont(hubFont);
            canvas->getTextBounds(hubLabel, 0, 0, &tx1, &ty1, &tw, &th);
        }
        canvas->setCursor(cx - (int)tw / 2 - tx1, cy - (int)th / 2 - ty1);
        canvas->print(hubLabel);
    }

    canvas->flush();
}

// Redraws whichever level is currently active, per the last resolved NAV path.
static void drawCurrentLevel()
{
    drawMenuWheel(s_menu_stack[s_menu_depth], s_menu_selected[s_menu_depth]);
}

// RingGauge parameter-edit screen (menu.json's "parameter_edit" ui mode): a
// 270-degree arc gauge with a gap at the bottom, filled proportionally to
// (value-min)/(max-min), a bright cursor dot at the current position, the
// parameter label above and the big value + unit centered inside the ring.
static void drawParamEditRing(const char* label, int32_t value, int32_t minV, int32_t maxV, const char* unit)
{
    canvas->fillScreen(C_BLACK);

    const int cx = LCD_WIDTH / 2;
    const int cy = LCD_HEIGHT / 2;
    const int outerR = 208;
    const int innerR = 178; // ~30px thick arc
    const float startDeg = 135.0f; // bottom-left; gap centered at the bottom (south)
    const float sweepDeg = 270.0f;

    float frac = (maxV > minV) ? (float)(value - minV) / (float)(maxV - minV) : 0.0f;
    frac = frac < 0.0f ? 0.0f : (frac > 1.0f ? 1.0f : frac);

    // Dim background track, then the filled portion on top in the accent color
    canvas->fillArc(cx, cy, outerR, innerR, startDeg, startDeg + sweepDeg, C_DKGRAY);
    if (frac > 0.001f)
        canvas->fillArc(cx, cy, outerR, innerR, startDeg, startDeg + sweepDeg * frac, C_PALE_ACCENT);

    // Bright cursor dot at the current value's position on the arc
    int cursorX, cursorY;
    computeAngleRadiusPosition(startDeg + sweepDeg * frac, (outerR + innerR) / 2, cursorX, cursorY);
    canvas->fillCircle(cursorX, cursorY, (outerR - innerR) / 2 + 4, C_WHITE);

    // Min/max labels at the arc's two ends
    canvas->setFont(&FreeSans9pt7b);
    canvas->setTextColor(C_DKGRAY);
    char endBuf[12];
    int16_t etx1, ety1;
    uint16_t etw, eth;
    int ex, ey;

    snprintf(endBuf, sizeof(endBuf), "%ld", (long)minV);
    canvas->getTextBounds(endBuf, 0, 0, &etx1, &ety1, &etw, &eth);
    computeAngleRadiusPosition(startDeg, innerR - 24, ex, ey);
    canvas->setCursor(ex - (int)etw / 2 - etx1, ey - (int)eth / 2 - ety1);
    canvas->print(endBuf);

    snprintf(endBuf, sizeof(endBuf), "%ld", (long)maxV);
    canvas->getTextBounds(endBuf, 0, 0, &etx1, &ety1, &etw, &eth);
    computeAngleRadiusPosition(startDeg + sweepDeg, innerR - 24, ex, ey);
    canvas->setCursor(ex - (int)etw / 2 - etx1, ey - (int)eth / 2 - ety1);
    canvas->print(endBuf);

    // Parameter label, above the value
    canvas->setFont(&FreeSans12pt7b);
    canvas->setTextColor(C_PALE_ACCENT);
    int16_t tx1, ty1;
    uint16_t tw, th;
    canvas->getTextBounds(label, 0, 0, &tx1, &ty1, &tw, &th);
    canvas->setCursor(cx - (int)tw / 2 - tx1, cy - 78 - ty1);
    canvas->print(label);

    // Big centered value
    char valBuf[16];
    snprintf(valBuf, sizeof(valBuf), "%ld", (long)value);
    canvas->setFont(&FreeSans24pt7b);
    canvas->setTextColor(C_WHITE);
    canvas->getTextBounds(valBuf, 0, 0, &tx1, &ty1, &tw, &th);
    canvas->setCursor(cx - (int)tw / 2 - tx1, cy - ty1 - (int)th / 2);
    canvas->print(valBuf);

    // Unit, just below the value
    if (unit != nullptr && unit[0] != '\0')
    {
        canvas->setFont(&FreeSans12pt7b);
        canvas->setTextColor(C_PALE_ACCENT);
        canvas->getTextBounds(unit, 0, 0, &tx1, &ty1, &tw, &th);
        canvas->setCursor(cx - (int)tw / 2 - tx1, cy + 46 - ty1);
        canvas->print(unit);
    }

    // Bottom hint, in the gap of the arc
    canvas->setFont(&FreeSans9pt7b);
    canvas->setTextColor(C_DKGRAY);
    const char* hint = "PRESS TO VALIDATE";
    canvas->getTextBounds(hint, 0, 0, &tx1, &ty1, &tw, &th);
    canvas->setCursor(cx - (int)tw / 2 - tx1, cy + outerR - 18 - ty1);
    canvas->print(hint);

    canvas->flush();
}

// Polymetric step wheel: one dot per step around the ring (gray=off,
// yellow=note, green=arpeggio, blue=chord), the edit cursor (encoder_main)
// highlighted with a thick red ring, the playhead (running while PLAY)
// highlighted with a cyan ring. Center hub shows the step count, the
// currently-sounding MIDI note and the shared PLAY/STOP transport indicator.
static void drawPolyWheel()
{
    canvas->fillScreen(C_BLACK);

    const int cx         = LCD_WIDTH / 2;
    const int cy         = LCD_HEIGHT / 2;
    const int ringRadius = 195;
    const int dotRadius  = 11;
    const int dotRadiusSel = 15;

    int count = s_poly_count > 0 ? s_poly_count : 1;
    for (int i = 0; i < count; i++)
    {
        float angle = wedgeCenterAngleDeg(i, count);
        int   x, y;
        computeAngleRadiusPosition(angle, ringRadius, x, y);

        char state = (i < kMaxPolySteps) ? s_poly_states[i] : '0';
        uint16_t color = C_DKGRAY;
        if (state == '1') color = C_YELLOW;
        else if (state == '2') color = C_GREEN;
        else if (state == '3') color = C_BLUE;

        bool isCursor   = (i == s_poly_cursor);
        bool isPlayhead = (i == s_poly_play);
        int  r = isCursor ? dotRadiusSel : dotRadius;

        canvas->fillCircle(x, y, r, color);
        if (isPlayhead)
            canvas->drawCircle(x, y, r + (isCursor ? 7 : 4), C_CYAN);
        if (isCursor)
        {
            canvas->drawCircle(x, y, r + 4, C_RED);
            canvas->drawCircle(x, y, r + 6, C_RED);
        }
    }

    // Decorative center hub: shared status info + poly step count
    canvas->fillCircle(cx, cy, kWheelCenterRadius, C_BLACK);
    drawStatusHub(cx, cy, true);

    canvas->flush();
}

void initDisplay()
{
    Serial.println("[DISPLAY] Initializing AMOLED...");
    
    // canvas->begin() also initializes the underlying gfx bus; do not call gfx->begin() separately
    canvas = new Arduino_Canvas_Indexed(LCD_WIDTH, LCD_HEIGHT, gfx, 0, 0, 0, 0);
    if (!canvas->begin())
    {
        Serial.println("[DISPLAY] ERROR: canvas->begin() failed!");
        while(1);
    }
    
    Serial.println("[DISPLAY] AMOLED initialized successfully");
    
    gfx->setBrightness(160); // dimmed vs. the previous 240 (pale palette should not glare)
    
    // Fill black background
    canvas->fillScreen(C_BLACK);
    canvas->flush();
    
    // Display startup message
    canvas->setFont(&FreeSans24pt7b);
    canvas->setTextSize(1);
    canvas->setTextColor(C_GREEN);
    
    int16_t x1, y1;
    uint16_t w, h;
    const char *msg = "DCO-ONE";
    canvas->getTextBounds(msg, 0, 0, &x1, &y1, &w, &h);
    canvas->setCursor((LCD_WIDTH - w) / 2, 100 - y1);
    canvas->println(msg);
    
    canvas->setFont(&FreeSans12pt7b);
    const char *subtitle = "Phase 3 - MIDI";
    canvas->getTextBounds(subtitle, 0, 0, &x1, &y1, &w, &h);
    canvas->setCursor((LCD_WIDTH - w) / 2, 150 - y1);
    canvas->setTextColor(C_CYAN);
    canvas->println(subtitle);
    
    const char *status = "Waiting for MENU encoder...";
    canvas->getTextBounds(status, 0, 0, &x1, &y1, &w, &h);
    canvas->setCursor((LCD_WIDTH - w) / 2, 200 - y1);
    canvas->setTextColor(C_WHITE);
    canvas->println(status);
    
    canvas->setFont(&FreeSans9pt7b);
    const char *copyr1 = "by Max Patissier";
    canvas->getTextBounds(copyr1, 0, 0, &x1, &y1, &w, &h);
    canvas->setCursor((LCD_WIDTH - w) / 2, 250 - y1);
    canvas->setTextColor(C_RED);
    canvas->println(copyr1);
    
    canvas->flush();
    delay(2000);
}

// Touch navigation is not wired up yet for the generic tree (it used to be
// ring-position hit-testing specific to the old submenu/edit views). The
// physical MENU encoder + button remain the primary/only input for now;
// revisit touch once the encoder-driven structure is validated.
static void handleTouchTap(int16_t tx, int16_t ty)
{
    (void)tx;
    (void)ty;
}


void initPower()
{
    Serial.println("[POWER] Initializing power management...");
    
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    
    // AXP2101 ALDO1 rail powers the AMOLED panel; display stays blank without it
    if (!axp.begin(Wire, AXP2101_SLAVE_ADDRESS, PIN_I2C_SDA, PIN_I2C_SCL))
    {
        Serial.println("[POWER] ERROR: AXP2101 not detected!");
    }
    else
    {
        Serial.println("[POWER] AXP2101 detected");
        axp.disableALDO1();
        axp.enableALDO1();
        axp.setALDO1Voltage(3300);
    }
}

void initUART()
{
    Serial.println("[UART] Initializing UART for Daisy communication...");
    
    Serial1.begin(115200, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
    
    Serial.println("[UART] UART configured");
}

void initTouch()
{
    Serial.println("[TOUCH] Initializing CST9217 touch controller...");

    touch.setPins(PIN_TP_RST, PIN_TP_INT);
    touch.setTouchDrvModel(TouchDrv_CST92XX);
    // sda/scl left at -1 (unused): Wire is already initialized by initPower()
    s_touch_ok = touch.begin(Wire, CST92XX_SLAVE_ADDRESS, -1, -1);

    if (!s_touch_ok)
    {
        Serial.println("[TOUCH] ERROR: CST9217 not detected!");
    }
    else
    {
        Serial.print("[TOUCH] Detected: ");
        Serial.println(touch.getModelName());
    }
}

void setup()
{
    // Start main serial for debugging
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n\n=== DCO-ONE Phase 1: ESP32-S3 Display ===\n");
    
    // Initialize power management
    initPower();
    
    // Initialize display
    initDisplay();
    
    // Initialize UART (not physically wired yet; menu link uses USB for now)
    initUART();
    
    // Initialize the capacitive touch controller (reserved for future use)
    initTouch();
    
    // Draw the initial menu state (root wheel, first item highlighted)
    drawCurrentLevel();
    
    Serial.println("[SETUP] Initialization complete\n");
}

void loop()
{
    // Read USB serial frames relayed from the Daisy (via usb_bridge.py):
    // - "NAV,P=<path>[,V=<n>]" : full navigation state (dot-separated indices
    //   from root) plus an optional preview of the highlighted item's current
    //   value (e.g. ONDE's waveform) when it is itself a live-value list.
    //
    // Redraws are deferred until the serial buffer is fully drained: if the
    // encoder is spun quickly, several NAV frames can queue up faster than a
    // full wheel redraw takes, and drawing once per line would replay every
    // stale intermediate state (visible as multi-second "catch-up" lag).
    // Rendering only the final resolved state once per loop() keeps the
    // display's latency bounded to a single redraw regardless of backlog.
    bool needsWheelRedraw = false;
    bool needsEditRedraw  = false;
    bool needsPolyRedraw  = false;

    while (Serial.available())
    {
        char c = Serial.read();

        if (c == '\n')
        {
            if (s_serial_buffer.startsWith("NAV,P="))
            {
                s_editing = false;
                s_poly_active = false;
                String rest = s_serial_buffer.substring(6);
                int vPos = rest.indexOf(",V=");
                String path = (vPos >= 0) ? rest.substring(0, vPos) : rest;
                s_menu_preview_value = (vPos >= 0) ? rest.substring(vPos + 3).toInt() : -1;
                applyNavPath(path);
                Serial.printf("[NAV] path=%s depth=%d preview=%d\n", path.c_str(), s_menu_depth, s_menu_preview_value);
                // NAV renders the hub with the current status snapshot too,
                // so the STAT handler below shouldn't redraw again for free.
                s_status_bpm_drawn     = s_status_bpm;
                s_status_root_drawn    = s_status_root;
                s_status_scale_drawn   = s_status_scale;
                s_status_playing_drawn = s_status_playing;
                s_status_note_drawn    = s_status_note;
                s_status_t440_drawn    = s_status_t440;
                s_vcf_type_drawn       = s_vcf_type;
                s_vcf_cutoff_drawn     = s_vcf_cutoff;
                s_vcf_resonance_drawn  = s_vcf_resonance;
                needsWheelRedraw = true;
                needsEditRedraw  = false;
            }
            else if (s_serial_buffer.startsWith("STAT,"))
            {
                // NOTE: VCF fields (VFT/VC/VR/VK/VD/VE) used to be appended
                // here too, but that pushed a single STAT PrintLine() past
                // libDaisy's hard 128-byte LOGGER_BUFFER limit (see the
                // hw.PrintLine() truncation note in daisy/src/main.cpp),
                // silently corrupting the frame and fusing it with whatever
                // came right after -- the actual cause of the long menu/HOME
                // lag. They now travel in their own short "VCF," frame below.
                String rest    = s_serial_buffer.substring(5);
                s_status_bpm     = parseLongField(rest, "BPM=");
                s_status_root    = parseLongField(rest, "ROOT=");
                s_status_scale   = parseLongField(rest, "SCALE=");
                s_status_playing = parseLongField(rest, "PLAY=") != 0;
                s_status_note    = (int)parseLongField(rest, "NOTE=");
                s_status_t440    = parseLongField(rest, "T440=") != 0;
                s_env1_attack    = parseLongField(rest, "EA=");
                s_env1_decay     = parseLongField(rest, "ED=");
                s_env1_sustain   = parseLongField(rest, "ES=");
                s_env1_release   = parseLongField(rest, "ER=");
                bool statusChanged = !s_status_valid
                                     || s_status_bpm != s_status_bpm_drawn
                                     || s_status_root != s_status_root_drawn
                                     || s_status_scale != s_status_scale_drawn
                                     || s_status_playing != s_status_playing_drawn
                                     || s_status_note != s_status_note_drawn
                                     || s_status_t440 != s_status_t440_drawn
                                     || s_env1_attack != s_env1_attack_drawn
                                     || s_env1_decay  != s_env1_decay_drawn
                                     || s_env1_sustain != s_env1_sustain_drawn
                                     || s_env1_release != s_env1_release_drawn;
                s_status_valid   = true;
                bool currentIsEnv1 = (s_menu_depth >= 1 && s_menu_stack[1]->children == kEnv1Submenu);
                bool needsHubRedraw = (s_menu_stack[s_menu_depth] == &kRootNode && statusChanged)
                                   || (currentIsEnv1 && statusChanged);
                if (needsHubRedraw && !s_editing && !s_poly_active)
                {
                    s_status_bpm_drawn     = s_status_bpm;
                    s_status_root_drawn    = s_status_root;
                    s_status_scale_drawn   = s_status_scale;
                    s_status_playing_drawn = s_status_playing;
                    s_status_note_drawn    = s_status_note;
                    s_status_t440_drawn    = s_status_t440;
                    s_env1_attack_drawn    = s_env1_attack;
                    s_env1_decay_drawn     = s_env1_decay;
                    s_env1_sustain_drawn   = s_env1_sustain;
                    s_env1_release_drawn   = s_env1_release;
                    needsWheelRedraw = true;
                }
            }
            else if (s_serial_buffer.startsWith("VCF,"))
            {
                String rest     = s_serial_buffer.substring(4);
                s_vcf_type      = parseLongField(rest, "VFT=");
                s_vcf_cutoff    = parseLongField(rest, "VC=");
                s_vcf_resonance = parseLongField(rest, "VR=");
                s_vcf_key       = parseLongField(rest, "VK=");
                s_vcf_drive     = parseLongField(rest, "VD=");
                s_vcf_env       = parseLongField(rest, "VE=");
                bool vcfHubChanged = s_vcf_type != s_vcf_type_drawn
                                   || s_vcf_cutoff != s_vcf_cutoff_drawn
                                   || s_vcf_resonance != s_vcf_resonance_drawn;
                bool currentIsVcf = (s_menu_depth >= 1 && s_menu_stack[1]->children == kVcfSubmenu);
                if (currentIsVcf && vcfHubChanged && !s_editing && !s_poly_active)
                {
                    s_vcf_type_drawn      = s_vcf_type;
                    s_vcf_cutoff_drawn    = s_vcf_cutoff;
                    s_vcf_resonance_drawn = s_vcf_resonance;
                    needsWheelRedraw = true;
                }
            }
            else if (s_serial_buffer.startsWith("EDIT,"))
            {
                if (!s_editing)
                {
                    // Entering edit mode: capture the label of the item highlighted
                    // in the currently displayed level (Daisy doesn't resend it).
                    const MenuNode* cur = s_menu_stack[s_menu_depth];
                    int sel = s_menu_selected[s_menu_depth];
                    s_edit_label = (sel >= 0 && sel < cur->childCount) ? cur->children[sel].label : "";
                    s_editing = true;
                }
                String rest  = s_serial_buffer.substring(5);
                s_edit_value = parseLongField(rest, "V=");
                s_edit_min   = parseLongField(rest, "MIN=");
                s_edit_max   = parseLongField(rest, "MAX=");
                s_edit_unit  = parseStringField(rest, "U=");
                Serial.printf("[EDIT] label=%s value=%ld min=%ld max=%ld unit=%s\n",
                              s_edit_label.c_str(), (long)s_edit_value, (long)s_edit_min,
                              (long)s_edit_max, s_edit_unit.c_str());
                needsEditRedraw  = true;
                needsWheelRedraw = false;
            }
            else if (s_serial_buffer.startsWith("POLY,"))
            {
                String rest   = s_serial_buffer.substring(5);
                s_poly_active = true;
                s_poly_count  = (int)parseLongField(rest, "N=");
                s_poly_cursor = (int)parseLongField(rest, "C=");
                String states = parseStringField(rest, "ST=");
                int copyLen = min((int)states.length(), kMaxPolySteps);
                for (int i = 0; i < copyLen; i++)
                    s_poly_states[i] = states[i];
                for (int i = copyLen; i < kMaxPolySteps; i++)
                    s_poly_states[i] = '0';
                s_poly_degree = parseLongField(rest, "DEG=");
                s_poly_play   = (int)parseLongField(rest, "PLAY=");
                s_poly_note   = (int)parseLongField(rest, "NOTE=");
                needsPolyRedraw  = true;
                needsWheelRedraw = false;
                needsEditRedraw  = false;
            }
            else if (s_serial_buffer.startsWith("WAVE,S="))
            {
                // Parse audio samples: "WAVE,S=<s0>,<s1>,...,<sN>"
                String rest = s_serial_buffer.substring(7);  // Skip "WAVE,S="
                for (int i = 0; i < kAudioSampleCount; i++)
                {
                    int commaPos = rest.indexOf(',');
                    String sampleStr;
                    if (commaPos >= 0)
                    {
                        sampleStr = rest.substring(0, commaPos);
                        rest = rest.substring(commaPos + 1);
                    }
                    else
                    {
                        sampleStr = rest;
                        rest = "";
                    }
                    if (sampleStr.length() > 0)
                        s_audio_samples[i] = (int8_t)sampleStr.toInt();
                    if (rest.length() == 0 && i < kAudioSampleCount - 1)
                        break;
                }
                s_audio_valid = true;
                if (!s_editing && !s_poly_active && s_menu_stack[s_menu_depth] == &kRootNode)
                    needsWheelRedraw = true;
            }

            s_serial_buffer = "";
        }
        else if (s_serial_buffer.length() < 256)
        {
            s_serial_buffer += c;
        }
    }

    // Render once with the final resolved state, regardless of how many
    // frames were drained above (see comment at the top of loop()).
    if (needsPolyRedraw)
        drawPolyWheel();
    else if (needsEditRedraw)
    {
        bool editing_env1 = (s_menu_depth >= 1 && s_menu_stack[s_menu_depth]->children == kEnv1Submenu);
        if (editing_env1)
            drawEnvelopeEditScreen(s_edit_label.c_str(), s_edit_value, s_edit_min, s_edit_max, s_edit_unit.c_str());
        else
            drawParamEditRing(s_edit_label.c_str(), s_edit_value, s_edit_min, s_edit_max, s_edit_unit.c_str());
    }
    else if (needsWheelRedraw)
        drawCurrentLevel();

    // Touch handling: react once on the press edge (not on every poll)
    static bool s_touch_was_pressed = false;
    bool pressed = s_touch_ok && touch.isPressed();
    if (pressed && !s_touch_was_pressed)
    {
        int16_t tx[1], ty[1];
        uint8_t n = touch.getPoint(tx, ty, 1);
        if (n > 0)
        {
            handleTouchTap(tx[0], ty[0]);
        }
    }
    s_touch_was_pressed = pressed;

    delay(20);
}

