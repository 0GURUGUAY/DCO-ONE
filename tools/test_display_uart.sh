#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

cat > "$work/test.cpp" <<'CPP'
#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <string>
namespace seed { constexpr int D13 = 13, D14 = 14; }
struct UartHandler {
    enum class Result { OK, ERR };
    struct Config {
        enum class Peripheral { USART_1 };
        enum class Mode { TX_RX };
        Peripheral periph;
        Mode mode;
        struct { int tx, rx; } pin_config;
        uint32_t baudrate;
    };
    static inline bool failInit = false;
    std::string output;
    int calls = 0;
    Result Init(const Config& config) {
        assert(config.periph == Config::Peripheral::USART_1);
        assert(config.mode == Config::Mode::TX_RX);
        assert(config.pin_config.tx == seed::D13);
        assert(config.pin_config.rx == seed::D14);
        assert(config.baudrate == 921600);
        return failInit ? Result::ERR : Result::OK;
    }
    Result BlockingTransmit(uint8_t* data, size_t size, uint32_t timeout) {
        assert(timeout == 5);
        assert(size <= 255);
        output.append(reinterpret_cast<char*>(data), size);
        ++calls;
        return Result::OK;
    }
};
CPP

sed -n '/^static UartHandler s_display_uart;/,/^static void DisplayPrintLine/ p' "$root/daisy/src/main.cpp" | sed '$d' >> "$work/test.cpp"
sed -n '/^static void DisplayPrintLine/,/^}/p' "$root/daisy/src/main.cpp" >> "$work/test.cpp"

cat >> "$work/test.cpp" <<'CPP'
int main() {
    DisplayPrintLine("NAV,P=0");
    assert(s_display_uart.calls == 0);
    UartHandler::failInit = true;
    InitDisplayUart();
    DisplayPrintLine("NAV,P=0");
    assert(s_display_uart.calls == 0);
    UartHandler::failInit = false;
    InitDisplayUart();
    DisplayPrintLine("NAV,P=%d.%d", 1, 2);
    DisplayPrintLine("EDIT,V=%d,MIN=-100,MAX=100,U=%%", -50);
    assert(s_display_uart.output == "NAV,P=1.2\nEDIT,V=-50,MIN=-100,MAX=100,U=%\n");
    s_display_uart.output.clear();
    const std::string longest(254, 'X');
    DisplayPrintLine("%s", longest.c_str());
    assert(s_display_uart.output == longest + "\n");
    s_display_uart.output.clear();
    const int calls = s_display_uart.calls;
    const std::string oversized(255, 'X');
    DisplayPrintLine("%s", oversized.c_str());
    assert(s_display_uart.calls == calls);
    DisplayPrintLine("NAV,P=0");
    assert(s_display_uart.output == "NAV,P=0\n");
    puts("UART: configuration, unavailable transport, framing and length boundaries OK");
}
CPP

clang++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined "$work/test.cpp" -o "$work/test"
"$work/test"
if grep -Eq 'hw\.PrintLine\("(STAT|VCF|LFO|MAT|EN2|LF2|OSC|FXS|PRST|NAV|EDIT|POLY|WAVE)' "$root/daisy/src/main.cpp"; then
    echo 'Display frame still routed over USB' >&2
    exit 1
fi
grep -q 'Serial1.begin(921600, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN)' "$root/esp32/src/main.cpp"
grep -q 'while (Serial1.available())' "$root/esp32/src/main.cpp"
grep -q 'char c = Serial1.read();' "$root/esp32/src/main.cpp"