# ESP32-WiFi-Hologram-Player

this is a repository for all my code for the esp32 hologram player


GIF to RGB565 Little-Endian Converter

A free, browser-based tool for converting animated GIF files into raw 16-bit RGB565 little-endian binary video for ESP32 microcontrollers, Arduino projects, TFT LCD displays, and DIY hologram projectors.

Convert GIF animations directly in your browser without installing Python, FFmpeg, or other software.

Features

- GIF to RGB565 conversion
- Little-endian byte ordering (LSB first)
- 16-bit RGB565 pixel encoding (5-bit red, 6-bit green, 5-bit blue)
- Adjustable resolution and frame rate
- Animation preview, resizing, rotation, and mirroring
- Raw ".rgb" and ".rgbv" binary downloads
- Offline browser operation

RGB565 Little-Endian Format

RGB565 stores each pixel in 16 bits. In little-endian encoding, the least significant byte is written first.

For example, pure red is encoded as "0xF800", stored as bytes "00 F8".

This format is useful for microcontrollers and TFT displays that read raw RGB565 pixel data from SD cards.

Designed for

ESP32 CYD (Cheap Yellow Display), ST7789 TFT screens, Arduino graphics projects, embedded video playback, and DIY holographic display systems.

Free to use. No installation. Runs locally in your browser.
