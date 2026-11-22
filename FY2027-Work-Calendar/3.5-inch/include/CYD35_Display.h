#pragma once
// ST7796 / 480x320 landscape. Shared-display SPI on HSPI.
// R = XPT2046 SPI touch, C = GT911 I2C touch.
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

class CYD35Display : public lgfx::LGFX_Device {
  lgfx::Panel_ST7796 panel;
  lgfx::Bus_SPI bus;
  lgfx::Light_PWM backlight;
#if defined(CYD_CAPACITIVE)
  lgfx::Touch_GT911 touch;
#else
  lgfx::Touch_XPT2046 touch;
#endif
public:
  CYD35Display() {
    {
      auto cfg = bus.config();
      cfg.spi_host = HSPI_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 24000000; // conservative for common board revisions
      cfg.freq_read = 16000000;
      cfg.spi_3wire = true;
      cfg.use_lock = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = 14;
      cfg.pin_mosi = 13;
      cfg.pin_miso = 12;
      cfg.pin_dc = 2;
      bus.config(cfg);
      panel.setBus(&bus);
    }
    {
      auto cfg = panel.config();
      cfg.pin_cs = 15;
      cfg.pin_rst = -1;
      cfg.pin_busy = -1;
      cfg.panel_width = 320;
      cfg.panel_height = 480;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits = 1;
      cfg.readable = true;
      cfg.invert = false;
      cfg.rgb_order = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = true;
      panel.config(cfg);
    }
    {
      auto cfg = backlight.config();
      cfg.pin_bl = 27;
      cfg.invert = false;
      cfg.freq = 1200;
      cfg.pwm_channel = 7;
      backlight.config(cfg);
      panel.setLight(&backlight);
    }
    {
      auto cfg = touch.config();
#if defined(CYD_CAPACITIVE)
      cfg.x_min = 0;
      cfg.x_max = 319;
      cfg.y_min = 0;
      cfg.y_max = 479;
      cfg.pin_int = -1;
      cfg.pin_rst = 25;
      cfg.bus_shared = false;
      cfg.offset_rotation = 0;
      cfg.i2c_port = 1;
      cfg.i2c_addr = 0x5D; // some batches use 0x14
      cfg.pin_sda = 33;
      cfg.pin_scl = 32;
      cfg.freq = 400000;
#else
      cfg.spi_host = HSPI_HOST;
      cfg.bus_shared = true;
      cfg.freq = 1000000;
      cfg.pin_int = -1; // polling works without IRQ / is safer on revisions
      cfg.pin_sclk = 14;
      cfg.pin_mosi = 13;
      cfg.pin_miso = 12;
      cfg.pin_cs = 33;
      cfg.x_min = 200;
      cfg.x_max = 3800;
      cfg.y_min = 3900;
      cfg.y_max = 300;
      cfg.offset_rotation = 0;
#endif
      touch.config(cfg);
      panel.setTouch(&touch);
    }
    setPanel(&panel);
  }
};
