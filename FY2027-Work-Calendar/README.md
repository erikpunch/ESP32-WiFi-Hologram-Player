# FY2027 CYD Fiscal Work Calendar

Two standalone ESP32 CYD touchscreen calendar projects with **13 fiscal periods**, **four Monday-first weeks per period**, a **tap-to-edit note** editor, and a **long-press Blue / Red / Clear** palette.

The dates are anchored to FY2027 Period 4 Week 1 = **Monday, November 23, 2026**. FY2027 Period 1 Week 1 = August 31, 2026. Calendars are offline and notes/colors are saved locally with LittleFS. No real-time clock or cloud sync; both initially open to Period 4, FY2027. The 52-week model does not handle 53-week exceptions.

| Folder | Hardware | Display | Touch | Build target |
|---|---|---|---|---|
| [3.2-inch](3.2-inch/) | QD3223 / E32R32P (original hologram projector) | ST7789 landscape 320×240 | XPT2046 | `cyd32` |
| [3.5-inch](3.5-inch/) | ESP32-3248S035R | ST7796 landscape 480×320 | XPT2046 | `cyd35r` |
| [3.5-inch](3.5-inch/) | ESP32-3248S035C | ST7796 landscape 480×320 | GT911 | `cyd35c` |

## Build without Arduino IDE

1. Open [Actions](../../actions/workflows/fy2027-cyd-calendars.yml), select **Build FY2027 CYD Calendars**, and use **Run workflow** (or see the latest run triggered by a commit).
2. Download the `FY2027-CYD-Calendars-Browser-Flasher` artifact from a successful workflow run. It contains separate folders for each board and merged `.bin` files.
3. Use an ESP32 web serial flasher to program the relevant merged `.bin` at offset `0x0000`. The included `index.html` installers require HTTPS hosting to work; opening via `file://` will not flash.
4. Hardware touch calibration and display orientation may require adjustment. **No hardware flash or compilation has been verified yet.**

These projects are **separate firmware**; flashing one over the hologram player replaces that application. Keep copies of your notes: full erasure/repartitioning can delete local LittleFS data.
