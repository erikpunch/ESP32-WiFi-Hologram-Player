#pragma once
// Matches the original working E32R32P hologram projector display wiring:
// https://github.com/erikpunch/ESP32-WiFi-Hologram-Player/tree/main/firmware-source
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

class CYD32Display : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 panel;
  lgfx::Bus_SPI bus;
  lgfx::Light_PWM backlight;
  lgfx::Touch_XPT2046 touch;
public:
  CYD32Display(){
    {
      auto cfg=bus.config();
      cfg.spi_host=HSPI_HOST;
      cfg.spi_mode=0;
      cfg.freq_write=20000000; // same conservative display clock as projector
      cfg.freq_read=16000000;
      cfg.spi_3wire=false;
      cfg.use_lock=true;
      cfg.dma_channel=SPI_DMA_CH_AUTO;
      cfg.pin_sclk=14;
      cfg.pin_mosi=13;
      cfg.pin_miso=12;
      cfg.pin_dc=2;
      bus.config(cfg);
      panel.setBus(&bus);
    }
    {
      auto cfg=panel.config();
      cfg.pin_cs=15;
      cfg.pin_rst=-1;
      cfg.pin_busy=-1;
      cfg.panel_width=240;
      cfg.panel_height=320;
      cfg.memory_width=240;
      cfg.memory_height=320;
      cfg.offset_x=0;
      cfg.offset_y=0;
      cfg.offset_rotation=0;
      cfg.readable=false;
      cfg.invert=true;
      cfg.rgb_order=true; // E32R32P ST7789 BGR wiring; change if red/blue are swapped
      cfg.bus_shared=true;
      panel.config(cfg);
    }
    {
      auto cfg=backlight.config();
      cfg.pin_bl=27;
      cfg.invert=false;
      cfg.freq=1200;
      cfg.pwm_channel=7;
      backlight.config(cfg);
      panel.setLight(&backlight);
    }
    {
      auto cfg=touch.config();
      cfg.spi_host=HSPI_HOST;
      cfg.bus_shared=true;
      cfg.freq=1000000;
      cfg.pin_int=36;
      cfg.pin_sclk=14;
      cfg.pin_mosi=13;
      cfg.pin_miso=12;
      cfg.pin_cs=33;
      cfg.x_min=200;cfg.x_max=3800;
      cfg.y_min=3800;cfg.y_max=200;
      cfg.offset_rotation=0;
      touch.config(cfg);
      panel.setTouch(&touch);
    }
    setPanel(&panel);
  }
};
