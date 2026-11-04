# ESP32-S3 DevKitC-1 N16R8 prototype wiring

**Verify your specific PCB, pin labels and supply before wiring.** The custom `boards/shap-esp32-s3-n16r8.json` selects 16 MB QIO flash and 8 MB OPI PSRAM; firmware and LittleFS images compile for this configuration, but actual electrical behavior remains unvalidated. GPIO mapping depends on the chosen DevKitC-1 variant; the graphic in the UI is a functional conceptual pin map, not a measured physical PCB drawing.

| Function | GPIO | Connection / restriction |
|---|---:|---|
| DS3231 RTC SDA | 8 | 3.3 V I²C, SDA with appropriate pull-up |
| DS3231 RTC SCL | 9 | 3.3 V I²C, SCL with appropriate pull-up |
| microSD CS | 10 | 3.3 V SPI module; not 5 V tolerant |
| microSD MOSI | 11 | SPI MOSI |
| microSD SCK | 12 | SPI CLK |
| microSD MISO | 13 | SPI MISO |
| Example isolated active-low relay IN | 4 | GPIO4 → compatible relay driver input; separate protected supply, common *low-voltage* reference only if required by module design |
| Example protected dry contact | 14 | GPIO14 → isolated/protected input; configure pull-up if appropriate |
| Example analog sensor | 1 | 0–3.3 V conditioned ADC1 input, never directly connect higher voltage |

Firmware allowlist: GPIO 1–7, 14–18, 21, 38–42, 47 (digital/relay); ADC1 on 1–7 only. Reserved: 0 (BOOT), 8–9 (RTC), 10–13 (SD), 19–20 (USB), 26–37 (flash/PSRAM and board constraints), 43–46 (console/strapping), other unlisted pins. GPIO48 is reserved because it may be shared with an onboard LED. **These are conservative software defaults, not a universal electrical safety certificate.**

```text
3.3 V sensor/RTC/SD supply ───── controller 3V3 (within regulator budget)
DS3231 SDA/SCL ──────────────── GPIO8 / GPIO9
microSD CS/MOSI/SCK/MISO ────── GPIO10 / 11 / 12 / 13
Controller GPIO4 ────────────── isolated relay module logic input
Controller GND ──────────────── low-voltage GND (only as specified by module)
MAINS ─────────── [fuse + certified enclosure + rated isolated relay contacts] ── load
```

Do not connect AC mains or motor currents to GPIO. Use appropriate fuses, earthing, creepage/clearance, enclosures, flyback protection and rated relay modules. Do not energize any load while adding/testing a device until wiring is professionally checked. All relay outputs are software-initialized OFF; **external bias/pull resistors must keep relays OFF during reset/boot**, before firmware owns the pin. Software interlocks and timers are not fail-safe protection.
