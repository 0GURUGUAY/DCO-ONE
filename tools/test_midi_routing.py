import contextlib
import io
import sys
import unittest
from pathlib import Path
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import combined_bridge as bridge


class MidiRoutingTests(unittest.TestCase):
    def open_input(self, ports, requested=None):
        midi_input = Mock()
        midi_input.get_ports.return_value = ports
        with patch.object(bridge.rtmidi, "MidiIn", return_value=midi_input):
            with contextlib.redirect_stdout(io.StringIO()):
                result = bridge.open_midi_input(requested, output_port_name="IAC Driver Bus 1")
        return result, midi_input

    def test_same_bus_is_disabled_when_it_is_the_only_input(self):
        result, midi_input = self.open_input(["IAC Driver Bus 1"])
        self.assertIsNone(result)
        midi_input.open_port.assert_not_called()

    def test_auto_input_skips_output_bus(self):
        result, midi_input = self.open_input(["IAC Driver Bus 1", "Keyboard"])
        self.assertIs(result, midi_input)
        midi_input.open_port.assert_called_once_with(1)

    def test_explicit_output_bus_is_rejected_by_name_or_index(self):
        for requested in ("IAC Driver Bus 1", "0"):
            with self.subTest(requested=requested):
                result, midi_input = self.open_input(["IAC Driver Bus 1"], requested)
                self.assertIsNone(result)
                midi_input.open_port.assert_not_called()

    def test_distinct_input_is_preserved(self):
        for requested in ("IAC Driver Bus 2", "1"):
            with self.subTest(requested=requested):
                result, midi_input = self.open_input(
                    ["IAC Driver Bus 1", "IAC Driver Bus 2"], requested
                )
                self.assertIs(result, midi_input)
                midi_input.open_port.assert_called_once_with(1)

    def test_input_can_be_disabled(self):
        result, midi_input = self.open_input(["Keyboard"], "-")
        self.assertIsNone(result)
        midi_input.open_port.assert_not_called()

    def test_invalid_indices_do_not_open_a_port(self):
        for requested in ("-1", "2"):
            with self.subTest(requested=requested):
                result, midi_input = self.open_input(["Keyboard"], requested)
                self.assertIsNone(result)
                midi_input.open_port.assert_not_called()


if __name__ == "__main__":
    unittest.main()