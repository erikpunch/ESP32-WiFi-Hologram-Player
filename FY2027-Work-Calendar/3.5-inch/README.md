# FY2027 Work Calendar — 3.5-inch ESP32 CYD

A Costco-inspired **red/blue work calendar** for the 480x320 display on the **ESP32-3248S035R / ESP32-3248S035C**, using **LovyanGFX** and **LittleFS**. No official Costco logo or proprietary font is included.

## Fiscal calendar rules

- FY2027 **Period 1 Week 1**: Monday **August 31, 2026**.
- FY2027 **Period 4 Week 1**: Monday **November 23, 2026**.
- Thirteen 4-week periods (364 days), Monday–Sunday, no conventional calendar months or Sunday-first layout.
- Period 13 begins **August 2, 2027**; the next 52-week FY starts **August 30, 2027**.
- **Important:** This is a *52-week model*. If your employer uses a 53-week fiscal year, the anchor in `src/main.cpp` needs adjustment for that year before the next FY. A 13x4 grid cannot represent a 53rd week without an exception.

## Controls

- **Tap** a date: opens its note editor (up to 96 characters, uppercase, with on-screen keyboard).
- **Hold** a date 650 ms: opens **BLUE / RED / CLEAR** picker.
- **< / >**: previous/next fiscal period, automatically wraps P13/P01 to other fiscal years.
- Red dot on a date: note exists.
- Save: writes both note and date colors to internal LittleFS. An error indicator appears if persistence fails.
- Works entirely offline. No clock, Wi-Fi, SD card, external services, or time synchronization needed. Period 4 is the initial screen at every power-on; the device does **not** automatically track today's date.

## Hardware variants — check the back of your board

| PlatformIO env | Printed model | Touch | Panel |
|---|---|---|---|
| `cyd35r` | ESP32-3248S035R | XPT2046 resistive SPI | ST7796 320x480 |
| `cyd35c` | ESP32-3248S035C | GT911 capacitive I2C | ST7796 320x480 |

Expected SPI GPIOs are display MOSI 13, MISO 12, CLK 14, CS 15, DC 2, backlight 27. Resistive touch CS is 33; capacitive I2C SDA is 33 and SCL is 32. Some boards sold as a 3.5-inch CYD use different display controllers, GT911 I2C addresses, and touch calibration. Those would require code changes. **Neither build has been tested on the user's physical board.**

## Build without Arduino IDE

### Option A: GitHub Actions (automated)

1. These files are uploaded under `FY2027-Work-Calendar/3.5-inch` in your existing repository; the build workflow lives at `.github/workflows/fy2027-cyd-calendars.yml` in the repository root.
2. Open **Actions > Build FY2027 CYD Calendars > Run workflow**.
3. When the workflow completes, download artifact **FY2027-CYD-Calendars-Browser-Flasher**, then open its `3.5-inch` folder.
4. It contains `firmware/cyd35r-merged.bin` and `firmware/cyd35c-merged.bin` plus the site for browser install.
5. Use a compatible ESP32 browser flasher for the merged `.bin` at address **0x0000**; or deploy the artifact's files together to an **HTTPS** host (such as GitHub Pages) and open `index.html` in Chrome/Edge.

Do not simply open `index.html` as `file://`: ESP Web Tools needs HTTPS and must be able to fetch the adjacent manifest and firmware files.

### Option B: PlatformIO CLI on Linux/Windows

Install PlatformIO and run `pio run -e cyd35r` (or `pio run -e cyd35c`). It builds `.pio/build/<variant>/firmware.bin` for address **0x10000**. For *browser flashing*, build the merged image using `esptool.py` as in `.github/workflows/build-firmware.yml`.

### Testing before flashing

Open `web/preview.html` locally. It is a **working browser simulation** of the calendar behavior, storing browser-side data in localStorage. It does not put data onto your CYD or generate `.bin` files.

## Data protection and limitations

- Notes and colors are on **internal flash**, not a remote account. Flash erase, factory reset, failed filesystem mount/format, or changing partition layout can delete them. **There is currently no backup/export feature**. Do not use this as the only record for irreplaceable work information.
- The sketch may format LittleFS on first boot if it cannot mount the filesystem. Do not reflash an existing populated device using `erase all` unless you have backed up data externally.
- NVS/LittleFS data persists only on the individual board, not between devices.
- 500 stored nonempty dates maximum, each up to 96 ASCII characters; this is designed for brief notes on a compact touchscreen.
- The app does not synchronize with Outlook/Google Calendar or display automatically calculated current date.
- Touch calibration varies between board revisions; adjust the touch `x_min/x_max/y_min/y_max` values in `include/CYD35_Display.h` if it is mirrored or misaligned.
- Uses a bold sans-serif supplied by the display library. It approximates the requested typography, **not** an official Costco-branded font or logo.

## Status

Source and HTML simulator are provided. The source **has not been compiled or hardware-tested** here because this environment lacks the ESP32 compiler and cannot install PlatformIO. Use GitHub Actions to obtain a firmware binary and check the correct touch variant before flashing.
