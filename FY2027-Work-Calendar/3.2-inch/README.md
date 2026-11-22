# FY2027 Work Calendar — original 3.2-inch CYD hologram projector board

An offline, Costco-inspired red-and-blue 13-period fiscal work calendar, adapted for **E32R32P / QD3223 3.2″ 240×320 ST7789 with XPT2046 resistive touch**, rotated to **320×240 landscape**. This is a separate firmware image; it REPLACES the hologram player on any board you flash. **No official Costco logo or proprietary font is included**; built-in bold bitmap fonts are used.

## Confirmed original-projector display pins

The existing user project `erikpunch/ESP32-WiFi-Hologram-Player/firmware-source/src/main.cpp` uses an ST7789 with `TFT_SCK=14`, `MOSI=13`, `MISO=12`, `BL=27`, `DC=2`, `CS=15`, and touch IRQ `36`. E32R32P documentation indicates XPT2046 touch CS `33` on the same SPI bus. This project matches these pins and starts at 20 MHz display SPI. Depending on screen batch, touch orientation and screen RGB order may still need calibration after hardware testing.

## Controls and fiscal dates

- Calendar **starts on Monday**. Four **complete** Monday-to-Sunday weeks per fiscal period.
- **FY2027 Period 4 Week 1 = Monday November 23, 2026**. That makes FY2027 P1W1 = August 31, 2026.
- **13 periods × 4 weeks = 364 days**, with 52-week-year continuation. A 53-week exception is NOT modeled; adjust future fiscal anchors if the employer's actual fiscal schedule changes.
- **Tap date**: opens note editor. Uppercase on-screen keyboard, up to 96 characters. **BACK** discards; **SAVE** writes notes.
- **Long press date (650 ms)**: choose **BLUE**, **RED**, or **CLEAR**. Color and note are independent.
- **< and > buttons**: previous/next period, wrap across fiscal years.
- A small dot indicates a saved note. **No automatic real-time date**: this board doesn't have a guaranteed accurate clock without a network or RTC; the device boots to FY2027 P4.
- Each note/color change saves to ESP32 internal flash via **LittleFS**, 500 nonempty dates maximum. Works entirely offline, no SD or Wi-Fi needed.

## What is in the zip

- `src/main.cpp`: complete calendar logic, touch handling, keyboard, flash persistence.
- `include/CYD32_Display.h`: ST7789 display and XPT2046 touch wiring for original 3.2″ projector board.
- `platformio.ini`: one `cyd32` build target; no Arduino IDE.
- `web/preview.html`: interactive simulation that runs locally in Chrome/Edge. Preview notes are only stored in browser localStorage, NOT the CYD.
- `web/install.html` and `web/manifest.json`: browser-install website source, needs an actual compiled firmware image and HTTPS hosting.
- `.github/workflows/build-firmware.yml`: compiles and assembles a flashable merged `firmware/cyd32-merged.bin` in GitHub Actions.

## Compile and flash without Arduino IDE

1. These files are uploaded under `FY2027-Work-Calendar/3.2-inch` in your existing repository; the active build workflow lives at repository root `.github/workflows/fy2027-cyd-calendars.yml`.
2. Select **Actions → Build FY2027 CYD Calendars → Run workflow**. Download artifact **FY2027-CYD-Calendars-Browser-Flasher**, then open its `3.2-inch` folder.
3. For a quick flash, use an ESP32 Web Serial flasher and select the generated `cyd32-merged.bin` at flash offset **0x0000**. Alternatively, host the artifact as an **HTTPS website** and open its `index.html` in desktop Chrome/Edge, which provides the one-click installer.
4. Use a USB *data* cable. On some boards hold BOOT during connection. Test on a spare board first: flashing replaces hologram software.

The website cannot flash anything when opened from the ZIP or from `file://`, and `web/manifest.json` intentionally references a **firmware artifact to be generated**, not a .bin already included here.

## Troubleshooting

- **Black/white screen:** verify actual PCB is original `QD3223 / E32R32P`, not a different 3.2″ 320×480 board. Check ST7789 inversion and panel driver.
- **Colors swapped:** change `cfg.rgb_order` in `include/CYD32_Display.h`.
- **Touch reversed/offset:** tune `cfg.x_min`, `cfg.x_max`, `cfg.y_min`, `cfg.y_max`, or `cfg.offset_rotation` in `include/CYD32_Display.h` and rebuild. Original hologram app only used the touch interrupt, so its success doesn't verify X/Y mapping.
- **Lost notes:** a full flash erase, changing partition layout, or LittleFS format removes saved notes. There is no export/backup feature; don't store irreplaceable notes only here. Flashing new merged firmware without erase may also reinitialize filesystems if partitions differ.
- **Wrong year later:** FY2027 P4 is the hardcoded initial view, not an automatically advancing 'today' selection. Fiscal-year transitions assume 52 weeks each.

## Verification status

Verified against the actual original hologram source code and its physical display pinout. Static project consistency tests and browser simulation tests can be performed without hardware. **ESP32 firmware is source-only until GitHub Actions builds it; it has not been compiled or flashed onto a physical 3.2″ CYD in this environment.**
