#!/usr/bin/env python3
"""
generate_menu.py

Single source of truth for the DCO-ONE menu tree.

Reads menu.json from the repository root and generates two C++ include
fragments:
  - daisy/src/menu_generated.inc   (Daisy Seed audio engine)
  - esp32/src/menu_generated.inc   (ESP32-S3 AMOLED display)

Both generated files define the same tree shape, labels and defaults so
that NAV,P=... frames from the Daisy resolve identically on the ESP32.

Numeric parameter ranges/steps/units are also generated from the JSON.
"""

import json
import os
import re
import sys
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parent.parent
JSON_PATH = ROOT / "menu.json"
DAISY_OUT = ROOT / "daisy" / "src" / "menu_generated.inc"
ESP32_OUT = ROOT / "esp32" / "src" / "menu_generated.inc"

# Map JSON main_menus order to Daisy/ESP32 color ids.
CATEGORY_ORDER = [
    "PLAY",
    "OSC",
    "VCF",
    "ENV1",
    "ENV2",
    "LFO",
    "MATRIX",
    "FX",
    "PRESETS",
    "MIDI",
    "SYSTEM",
]

COLOR_ENUM = {
    "PLAY": "COLOR_PLAY",
    "OSC": "COLOR_OSC",
    "VCF": "COLOR_VCF",
    "ENV1": "COLOR_ENV1",
    "ENV2": "COLOR_ENV2",
    "LFO": "COLOR_LFO",
    "MATRIX": "COLOR_MATRIX",
    "FX": "COLOR_FX",
    "PRESETS": "COLOR_PRESETS",
    "MIDI": "COLOR_MIDI",
    "SYSTEM": "COLOR_SYSTEM",
}

ESP32_PALETTE = {
    "PLAY": "C_PALE_PLAY",
    "OSC": "C_PALE_OSC",
    "VCF": "C_PALE_VCF",
    "ENV1": "C_PALE_ENV1",
    "ENV2": "C_PALE_ENV2",
    "LFO": "C_PALE_LFO",
    "MATRIX": "C_PALE_MATRIX",
    "FX": "C_PALE_FX",
    "PRESETS": "C_PALE_PRESETS",
    "MIDI": "C_PALE_MIDI",
    "SYSTEM": "C_PALE_SYSTEM",
}


def load_json() -> dict:
    if not JSON_PATH.exists():
        print(f"ERROR: {JSON_PATH} not found", file=sys.stderr)
        sys.exit(1)
    with open(JSON_PATH, "r", encoding="utf-8") as f:
        return json.load(f)


def escape_c_str(s: str) -> str:
    return s.replace('"', '\\"')


def to_c_name(s: str) -> str:
    """Convert a label to a valid C identifier token."""
    s = s.strip().replace("/", "_").replace("&", "_").replace("-", "_")
    s = re.sub(r"[^0-9a-zA-Z_]", "_", s)
    s = re.sub(r"_+", "_", s).strip("_")
    return s or "item"


def param_name(item_id: str) -> str:
    """Map known IDs to the historical NumericParam variable names."""
    # Keep existing variable names to avoid touching code that references them.
    mapping = {
        "play_bpm": "kBpmParam",
        "play_auto": "kPlayAutoParam",
        "play_octaves": "kPlayOctParam",
        "play_poly_steps": "kPlayStepParam",
        "osc_coarse": "kOscCoarseParam",
        "osc_fine": "kOscFineParam",
        "osc_pw": "kOscPulseParam",
        "osc_sub": "kOscSubParam",
        "osc_sync": "kOscHardParam",
        "osc_fm_amt": "kOscFmAmtParam",
        "osc_fm_ratio": "kOscFmRatioParam",
        "osc_fm_ratio_fine": "kOscFmRatioFineParam",
        "vcf_cutoff": "kVcfCutoffParam",
        "vcf_res": "kVcfResonanceParam",
        "vcf_keytrack": "kVcfKeyParam",
        "vcf_drive": "kVcfDriveParam",
        "vcf_env_amt": "kVcfEnvParam",
        "env1_attack": "kEnv1AttackParam",
        "env1_decay": "kEnv1DecayParam",
        "env1_sustain": "kEnv1SustainParam",
        "env1_release": "kEnv1ReleaseParam",
        "env1_vel_sens": "kEnv1VelocityParam",
        "env2_attack": "kEnv2AttackParam",
        "env2_decay": "kEnv2DecayParam",
        "env2_sustain": "kEnv2SustainParam",
        "env2_release": "kEnv2ReleaseParam",
        "lfo1_rate": "kLfo1RateParam",
        "lfo1_amp": "kLfo1AmpParam",
        "lfo1_phase": "kLfo1PhaseParam",
        "lfo2_rate": "kLfo2RateParam",
        "lfo2_amp": "kLfo2AmpParam",
        "lfo2_phase": "kLfo2PhaseParam",
        "mod1_amt": "kMatSlot1AmtParam",
        "mod2_amt": "kMatSlot2AmtParam",
        "fx_mix": "kFxDryWetParam",
        "fx_p1": "kFxTimeParam",
        "fx_p2": "kFxFbParam",
        "midi_chan": "kMidiChannelParam",
        "pitch_bend": "kMidiBendParam",
        "sys_bright": "kSysLuminositeParam",
        "sys_volume": "kSysVolumeParam",
        "patch_load": "kPatchLoadParam",
        "patch_save": "kPatchSaveParam",
    }
    return mapping.get(item_id)


def apply_callback_name(item_id: str) -> str:
    """Return the onSelect callback name used by the Daisy firmware."""
    mapping = {
        "osc_wave": "ApplyWaveform",
        "play_auto": "ApplyPlayAuto",
        "osc_coarse": "ApplyOscCoarse",
        "osc_fine": "ApplyOscFine",
        "osc_pw": "ApplyOscPulse",
        "osc_sub": "ApplyOscSub",
        "osc_sync": "ApplyOscHard",
        "osc_fm_amt": "ApplyOscFmAmt",
        "osc_fm_ratio": "ApplyOscFmRatio",
        "osc_fm_ratio_fine": "ApplyOscFmRatioFine",
        "osc_fm_modwave": "ApplyOscFmModWave",
        "vcf_type": "ApplyVcfType",
        "vcf_cutoff": "ApplyVcfCutoff",
        "vcf_res": "ApplyVcfResonance",
        "vcf_keytrack": "ApplyVcfKey",
        "vcf_drive": "ApplyVcfDrive",
        "vcf_env_amt": "ApplyVcfEnv",
        "env1_attack": "ApplyEnv1Attack",
        "env1_decay": "ApplyEnv1Decay",
        "env1_sustain": "ApplyEnv1Sustain",
        "env1_release": "ApplyEnv1Release",
        "env2_attack": "ApplyEnv2Attack",
        "env2_decay": "ApplyEnv2Decay",
        "env2_sustain": "ApplyEnv2Sustain",
        "env2_release": "ApplyEnv2Release",
        "lfo1_shape": "ApplyLfo1Shape",
        "lfo1_rate": "ApplyLfo1Rate",
        "lfo1_sync": "ApplyLfo1Sync",
        "lfo1_amp": "ApplyLfo1Amp",
        "lfo1_phase": "ApplyLfo1Phase",
        "lfo2_shape": "ApplyLfo2Shape",
        "lfo2_rate": "ApplyLfo2Rate",
        "lfo2_sync": "ApplyLfo2Sync",
        "lfo2_amp": "ApplyLfo2Amp",
        "lfo2_phase": "ApplyLfo2Phase",
        "mod1_amt": "ApplyMatSlot1Amt",
        "mod2_amt": "ApplyMatSlot2Amt",
        "fx_mix": "ApplyFxDryWet",
        "fx_p1": "ApplyFxTime",
        "fx_p2": "ApplyFxFb",
        "midi_chan": "ApplyMidiChannel",
        "pitch_bend": "ApplyMidiBend",
        "sys_bright": "ApplySysLuminosite",
        "sys_volume": "ApplySysVolume",
        "sys_440hz": "ApplySys440Hz",
        "patch_init": "ApplyInitPatch",
    }
    return mapping.get(item_id)


def confirm_callback_name(item_id: str) -> str:
    """Return the callback executed when a numeric parameter edit is confirmed."""
    mapping = {
        "patch_save": "ApplyPatchSave",
        "patch_load": "ApplyPatchLoad",
    }
    return mapping.get(item_id)


def generate_daisy(data: dict) -> str:
    lines: list[str] = [
        "// Auto-generated from menu.json by tools/generate_menu.py",
        "// Do not edit manually: regenerate instead.",
        "",
        "// =============================================================================",
        "// Menu content mirrored from menu.json (repo root).",
        "// =============================================================================",
    ]

    def emit_options(name: str, options: list[str], color: str = "COLOR_DEFAULT") -> str:
        array_name = f"k{name}Options"
        count_name = f"k{name}OptionCount"
        lines.append(f"\nstatic MenuNode {array_name}[] = {{")
        for opt in options:
            lines.append(f'    {{ "{escape_c_str(opt)}", {color}, nullptr, 0, nullptr, 0 }},')
        lines.append("};")
        lines.append(f"static constexpr uint8_t {count_name} = sizeof({array_name}) / sizeof({array_name}[0]);\n")
        return array_name, count_name

    def nested_array_name(parent_array_name: str, label: str) -> str:
        base = parent_array_name
        if base.endswith("Submenu"):
            base = base[: -len("Submenu")]
        suffix = to_c_name(label).replace("_", "").capitalize()
        return f"{base}{suffix}Submenu"

    def emit_item_array(array_name: str, cat_key: str, items: list[dict]) -> tuple[str, str]:
        """Recursively emit a MenuNode array for `items` (supports nested
        "SUBMENU" items, e.g. OSC > FM). Returns (array_name, count_name)."""
        children_refs: list[tuple[str, str, str, str, str, int, str]] = []  # label, arr, cnt, on_select, param_ptr, default_sel, on_confirm
        local_numeric_params: list[tuple[str, dict]] = []

        for item in items:
            item_id = item.get("id", "")
            label = item.get("label", "")
            itype = item.get("type", "")
            sanitized = to_c_name(f"{cat_key}_{item_id}")
            confirm = confirm_callback_name(item_id) or "nullptr"

            if itype == "SUBMENU":
                nested_name = nested_array_name(array_name, label)
                nested_arr, nested_cnt = emit_item_array(nested_name, cat_key, item["submenus"])
                on_select = apply_callback_name(item_id) or "nullptr"
                children_refs.append((label, nested_arr, nested_cnt, on_select, "nullptr", 0, "nullptr"))
            elif itype == "ENUM":
                arr, cnt = emit_options(sanitized, item["options"])
                on_select = apply_callback_name(item_id) or "nullptr"
                default_sel = int(item.get("default", 0))
                children_refs.append((label, arr, cnt, on_select, "nullptr", default_sel, "nullptr"))
            elif itype in ("INT", "FLOAT", "TOGGLE"):
                varname = param_name(item_id)
                if varname:
                    local_numeric_params.append((varname, item))
                    param_ptr = f"&{varname}"
                else:
                    param_ptr = "nullptr"
                # Preset actions run only on confirmation, not while turning the encoder.
                if confirm != "nullptr":
                    on_select = "nullptr"
                else:
                    on_select = apply_callback_name(item_id) or "nullptr"
                children_refs.append((label, "nullptr", "0", on_select, param_ptr, 0, confirm))
            else:  # ACTION / unimplemented
                on_select = apply_callback_name(item_id) or "nullptr"
                children_refs.append((label, "nullptr", "0", on_select, "nullptr", 0, "nullptr"))

        # Numeric params belonging to this level, declared before the array
        # below so it can take their address.
        emitted_ids = set()
        for varname, item in local_numeric_params:
            if varname in emitted_ids:
                continue
            emitted_ids.add(varname)
            min_v = int(item.get("min", 0))
            max_v = int(item.get("max", 0))
            step_v = int(item.get("step", 1))
            default_v = int(item.get("default", min_v))
            unit = item.get("unit", "")
            lines.append(
                f'static NumericParam {varname} = {{ {min_v}, {max_v}, {step_v}, {default_v}, "{escape_c_str(unit)}" }};'
            )

        count_name = f"{array_name}Count"
        lines.append(f"\nstatic MenuNode {array_name}[] = {{")
        for label, arr, cnt, on_select, param_ptr, default_sel, on_confirm in children_refs:
            lines.append(
                f'    {{ "{escape_c_str(label)}", COLOR_DEFAULT, {arr}, {cnt}, {on_select}, {default_sel}, {param_ptr}, {on_confirm} }},'
            )
        lines.append("};")
        lines.append(f"static constexpr uint8_t {count_name} = sizeof({array_name}) / sizeof({array_name}[0]);\n")
        return array_name, count_name

    for cat in data["main_menus"]:
        cat_key = cat["key"]
        color = COLOR_ENUM[cat_key]
        lines.append(f"\n// ---- {cat_key}: {cat.get('title', '')} ----")

        sub_name = f"k{cat_key.title()}Submenu" if cat_key != "LFO" else "kLfoSubmenu"
        if cat_key == "MATRIX":
            sub_name = "kMatrixSubmenu"
        if cat_key == "FX":
            sub_name = "kFxSubmenu"
        if cat_key == "PRESETS":
            sub_name = "kPresetsSubmenu"
        if cat_key == "MIDI":
            sub_name = "kMidiSubmenu"
        if cat_key == "SYSTEM":
            sub_name = "kSystemSubmenu"
        if cat_key == "OSC":
            sub_name = "kOscSubmenu"
        if cat_key == "VCF":
            sub_name = "kVcfSubmenu"
        if cat_key == "ENV1":
            sub_name = "kEnv1Submenu"
        if cat_key == "ENV2":
            sub_name = "kEnv2Submenu"
        if cat_key == "PLAY":
            sub_name = "kPlaySubmenu"

        emit_item_array(sub_name, cat_key, cat["submenus"])

    # Root menu
    lines.append("\n// Root menu (main wheel), same order as menu.json's main_menus")
    lines.append("static MenuNode kRootMenu[] = {")
    for cat in data["main_menus"]:
        cat_key = cat["key"]
        sub_name = f"k{cat_key.title()}Submenu"
        # Fix exceptions to match current source names
        name_fixes = {
            "LFO": "kLfoSubmenu",
            "MATRIX": "kMatrixSubmenu",
            "FX": "kFxSubmenu",
            "PRESETS": "kPresetsSubmenu",
            "MIDI": "kMidiSubmenu",
            "SYSTEM": "kSystemSubmenu",
            "OSC": "kOscSubmenu",
            "VCF": "kVcfSubmenu",
            "ENV1": "kEnv1Submenu",
            "ENV2": "kEnv2Submenu",
            "PLAY": "kPlaySubmenu",
        }
        sub_name = name_fixes.get(cat_key, sub_name)
        lines.append(
            f'    {{ "{cat_key}", {COLOR_ENUM[cat_key]}, {sub_name}, {sub_name}Count, nullptr, 0, nullptr, nullptr }},'
        )
    lines.append("};")
    lines.append("static constexpr uint8_t kRootMenuCount = sizeof(kRootMenu) / sizeof(kRootMenu[0]);\n")
    lines.append("static MenuNode kRootNode = { \"MENU\", COLOR_DEFAULT, kRootMenu, kRootMenuCount, nullptr, 0, nullptr, nullptr };\n")

    return "\n".join(lines)


def generate_esp32(data: dict) -> str:
    lines: list[str] = [
        "// Auto-generated from menu.json by tools/generate_menu.py",
        "// Do not edit manually: regenerate instead.",
        "",
        "// =============================================================================",
        "// Menu content mirrored from menu.json (repo root).",
        "// =============================================================================",
    ]

    def emit_options(name: str, options: list[str], color: str = "0") -> str:
        array_name = f"k{name}Options"
        count_name = f"k{name}OptionCount"
        lines.append(f"\nstatic const MenuNode {array_name}[] = {{")
        for opt in options:
            lines.append(f'    {{ "{escape_c_str(opt)}", {color}, nullptr, 0 }},')
        lines.append("};")
        lines.append(f"static const int {count_name} = sizeof({array_name}) / sizeof({array_name}[0]);\n")
        return array_name, count_name

    def nested_array_name(parent_array_name: str, label: str) -> str:
        base = parent_array_name
        if base.endswith("Submenu"):
            base = base[: -len("Submenu")]
        suffix = to_c_name(label).replace("_", "").capitalize()
        return f"{base}{suffix}Submenu"

    def emit_item_array(array_name: str, cat_key: str, items: list[dict]) -> tuple[str, str]:
        """Mirrors generate_daisy's recursive SUBMENU support (purely visual
        here: no onSelect/numeric fields on the ESP32 side)."""
        children_refs: list[tuple[str, str, str]] = []

        for item in items:
            item_id = item.get("id", "")
            label = item.get("label", "")
            itype = item.get("type", "")
            sanitized = to_c_name(f"{cat_key}_{item_id}")

            if itype == "SUBMENU":
                nested_name = nested_array_name(array_name, label)
                nested_arr, nested_cnt = emit_item_array(nested_name, cat_key, item["submenus"])
                children_refs.append((label, nested_arr, nested_cnt))
            elif itype == "ENUM":
                arr, cnt = emit_options(sanitized, item["options"])
                children_refs.append((label, arr, cnt))
            else:
                children_refs.append((label, "nullptr", "0"))

        count_name = f"{array_name}Count"
        lines.append(f"\nstatic const MenuNode {array_name}[] = {{")
        for label, arr, cnt in children_refs:
            lines.append(f'    {{ "{escape_c_str(label)}", 0, {arr}, {cnt} }},')
        lines.append("};")
        lines.append(f"static const int {count_name} = sizeof({array_name}) / sizeof({array_name}[0]);\n")
        return array_name, count_name

    for cat in data["main_menus"]:
        cat_key = cat["key"]
        lines.append(f"\n// ---- {cat_key}: {cat.get('title', '')} ----")

        sub_name = f"k{cat_key.title()}Submenu"
        name_fixes = {
            "LFO": "kLfoSubmenu",
            "MATRIX": "kMatrixSubmenu",
            "FX": "kFxSubmenu",
            "PRESETS": "kPresetsSubmenu",
            "MIDI": "kMidiSubmenu",
            "SYSTEM": "kSystemSubmenu",
            "OSC": "kOscSubmenu",
            "VCF": "kVcfSubmenu",
            "ENV1": "kEnv1Submenu",
            "ENV2": "kEnv2Submenu",
            "PLAY": "kPlaySubmenu",
        }
        sub_name = name_fixes.get(cat_key, sub_name)
        emit_item_array(sub_name, cat_key, cat["submenus"])

    # Root menu
    lines.append("\n// Root menu (main wheel), same order as menu.json's main_menus")
    lines.append("static const MenuNode kRootMenu[] = {")
    for cat in data["main_menus"]:
        cat_key = cat["key"]
        sub_name = name_fixes.get(cat_key, f"k{cat_key.title()}Submenu")
        lines.append(
            f'    {{ "{cat_key}", {ESP32_PALETTE[cat_key]}, {sub_name}, {sub_name}Count }},'
        )
    lines.append("};")
    lines.append("static const int kRootMenuCount = sizeof(kRootMenu) / sizeof(kRootMenu[0]);\n")
    lines.append("static const MenuNode kRootNode = { \"MENU\", 0, kRootMenu, kRootMenuCount };\n")

    return "\n".join(lines)


def write_file(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        f.write(content)
    print(f"Wrote {path}")


def main() -> int:
    data = load_json()

    # Validate ordering
    json_keys = [cat["key"] for cat in data["main_menus"]]
    if json_keys != CATEGORY_ORDER:
        print(
            "WARNING: menu.json main_menus order differs from expected; "
            "using JSON order.",
            file=sys.stderr,
        )

    write_file(DAISY_OUT, generate_daisy(data))
    write_file(ESP32_OUT, generate_esp32(data))
    return 0


if __name__ == "__main__":
    sys.exit(main())
