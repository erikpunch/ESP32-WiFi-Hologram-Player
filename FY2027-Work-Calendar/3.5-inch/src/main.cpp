/*
 FY2027 Work Calendar for Sunton ESP32-3248S035R / S035C (3.5 inch CYD)
 480x320 landscape, four-week fiscal periods, Monday-first.
 Tap day = edit its note; hold day >= 650 ms = Blue / Red / Clear.
 All changes persisted to LittleFS (no Wi-Fi, RTC, or SD card required).
 Costco-inspired palette; no official logo or proprietary font embedded.
*/
#include <Arduino.h>
#include <LittleFS.h>
#include <string.h>
#include "CYD35_Display.h"

CYD35Display tft;
static uint16_t BLUE, RED, NAVY, PALE, INK, BORDER, WHITE, GREY;
constexpr int DISPLAY_W = 480, DISPLAY_H = 320;
constexpr int MAX_EVENTS = 500;
constexpr int NOTE_MAX = 96;
constexpr uint32_t STORAGE_MAGIC = 0x43414C32UL; // "CAL2"
constexpr uint16_t STORAGE_VERSION = 1;
constexpr uint32_t HOLD_MS = 650;

struct CalendarEntry {
  int32_t day;                 // days since 1970-01-01
  uint8_t color;               // 0=none, 1=blue, 2=red
  char note[NOTE_MAX + 1];     // ASCII, nul-terminated
};
struct FileHeader { uint32_t magic; uint16_t version; uint16_t count; };
static CalendarEntry entries[MAX_EVENTS];
static int entryCount = 0;
static bool fsReady = false, saveOK = true;
static int fiscalYear = 2027, period = 4;
static int32_t chosenDay = 0;
static String draft;
static bool numberKeys = false;
enum Screen { CALENDAR, NOTE_EDITOR, COLOR_PICKER };
static Screen screen = CALENDAR;

// Civil-date arithmetic adapted from Howard Hinnant's public-domain algorithms.
static int32_t daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = unsigned(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + int(doe) - 719468;
}
static void civilFromDays(int32_t z, int &y, unsigned &m, unsigned &d) {
  z += 719468;
  const int era = (z >= 0 ? z : z - 146096) / 146097;
  const unsigned doe = unsigned(z - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  y = int(yoe) + era * 400;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  d = doy - (153 * mp + 2) / 5 + 1;
  m = mp + (mp < 10 ? 3 : -9);
  y += m <= 2;
}
static int32_t firstDay() {
  // FY2027 P4 W1 = Mon 2026-11-23 -> FY2027 P1 W1 = Mon 2026-08-31.
  // Each modeled FY is 13 x 4 = 52 weeks (a 53-week FY needs a rule update).
  return daysFromCivil(2026, 8, 31) + (fiscalYear - 2027) * 364;
}
static int32_t currentPeriodStart() { return firstDay() + (period - 1) * 28; }
static const char *monthName(unsigned m) {
  static const char *names[] = {"", "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
  return (m >= 1 && m <= 12) ? names[m] : "???";
}
static String dateText(int32_t day, bool year = true) {
  int y; unsigned m,d; civilFromDays(day,y,m,d);
  char b[40];
  if (year) snprintf(b,sizeof(b),"%s %02u, %04d",monthName(m),d,y);
  else snprintf(b,sizeof(b),"%s %u",monthName(m),d);
  return String(b);
}
static int indexOf(int32_t day) {
  for (int i=0; i<entryCount; ++i) if (entries[i].day == day) return i;
  return -1;
}
static uint8_t colorOf(int32_t day) { int i=indexOf(day); return i < 0 ? 0 : entries[i].color; }
static const char *noteOf(int32_t day) { int i=indexOf(day); return i < 0 ? "" : entries[i].note; }
static bool storeFile() {
  if (!fsReady) return false;
  File f = LittleFS.open("/calendar.tmp", "w");
  if (!f) return false;
  FileHeader head = {STORAGE_MAGIC, STORAGE_VERSION, (uint16_t)entryCount};
  bool good = (f.write(reinterpret_cast<const uint8_t*>(&head), sizeof(head)) == sizeof(head));
  if (good && entryCount > 0)
    good = (f.write(reinterpret_cast<const uint8_t*>(entries), sizeof(CalendarEntry)*entryCount)
            == sizeof(CalendarEntry)*entryCount);
  f.flush(); f.close();
  if (!good) { LittleFS.remove("/calendar.tmp"); return false; }
  // LittleFS rename on ESP32 replaces a file. Keep a backup during replacement.
  LittleFS.remove("/calendar.bak");
  if (LittleFS.exists("/calendar.dat")) LittleFS.rename("/calendar.dat", "/calendar.bak");
  if (!LittleFS.rename("/calendar.tmp", "/calendar.dat")) {
    if (LittleFS.exists("/calendar.bak")) LittleFS.rename("/calendar.bak", "/calendar.dat");
    return false;
  }
  return true;
}
static void loadFile() {
  if (!fsReady) return;
  String path = LittleFS.exists("/calendar.dat") ? "/calendar.dat" : "/calendar.bak";
  File f = LittleFS.open(path, "r");
  if (!f) return;
  FileHeader head;
  bool valid = f.readBytes(reinterpret_cast<char*>(&head), sizeof(head)) == sizeof(head)
     && head.magic == STORAGE_MAGIC && head.version == STORAGE_VERSION && head.count <= MAX_EVENTS;
  if (valid) {
    const size_t bytes = head.count * sizeof(CalendarEntry);
    valid = f.readBytes(reinterpret_cast<char*>(entries), bytes) == bytes;
    if (valid) { entryCount = head.count; for(int i=0;i<entryCount;++i) entries[i].note[NOTE_MAX]=0; }
  }
  if (!valid) { entryCount = 0; saveOK = false; }
  f.close();
}
static void setEntry(int32_t day, uint8_t color, const String &note) {
  int idx = indexOf(day);
  if (color == 0 && note.length() == 0) {
    if (idx >= 0) { entries[idx] = entries[--entryCount]; saveOK = storeFile(); }
    return;
  }
  if (idx < 0) {
    if (entryCount >= MAX_EVENTS) { saveOK = false; return; }
    idx = entryCount++;
  }
  entries[idx].day = day;
  entries[idx].color = color;
  size_t n = min((size_t)NOTE_MAX, (size_t)note.length());
  memcpy(entries[idx].note, note.c_str(), n);
  entries[idx].note[n] = '\0';
  saveOK = storeFile();
}
static void applyColor(uint8_t color) { setEntry(chosenDay,color,String(noteOf(chosenDay))); }
static bool inside(int x,int y,int a,int b,int w,int h) { return x>=a && x<a+w && y>=b && y<b+h; }
static void centered(const String &s,int x,int y,uint16_t fg,int font=2) {
  tft.setFont(font==4 ? &fonts::Font4 : &fonts::Font2);
  tft.setTextDatum(middle_center);
  tft.setTextColor(fg);
  tft.drawString(s,x,y);
}
static void button(int x,int y,int w,int h,const String &label,uint16_t bg,uint16_t fg,int font=2) {
  tft.fillRoundRect(x,y,w,h,6,bg);
  centered(label,x+w/2,y+h/2,fg,font);
}
static bool dateCellAt(int x,int y,int &row,int &col) {
  if (!inside(x,y,8,91,462,190)) return false;
  col=(x-8)/66; row=(y-91)/48;
  if (col < 0 || col > 6 || row < 0 || row > 3) return false;
  return x < 8 + col*66+64 && y < 91+row*48+46;
}
static void drawCalendar() {
  tft.fillScreen(WHITE);
  tft.fillRect(0,0,480,6,RED);
  tft.fillRect(0,6,480,57,BLUE);
  tft.setTextDatum(top_left);
  tft.setTextColor(WHITE);
  tft.setFont(&fonts::Font4);
  char head[32]; snprintf(head,sizeof(head),"FY%d   P%02d",fiscalYear,period);
  tft.drawString(head,12,9);
  tft.setFont(&fonts::Font2);
  String range = dateText(currentPeriodStart(),false) + " - " + dateText(currentPeriodStart()+27,false);
  tft.drawString(range,14,42);
  button(367,13,48,42,"<",NAVY,WHITE,4);
  button(424,13,48,42,">",NAVY,WHITE,4);

  tft.fillRect(0,63,480,25,NAVY);
  static const char *days[] = {"MON","TUE","WED","THU","FRI","SAT","SUN"};
  for (int c=0;c<7;++c) centered(days[c],8+c*66+32,76,WHITE);
  const int32_t start = currentPeriodStart();
  for(int r=0;r<4;++r) for(int c=0;c<7;++c) {
    int x=8+c*66, y=91+r*48; int32_t day=start+r*7+c;
    int yr; unsigned mo,da; civilFromDays(day,yr,mo,da);
    uint8_t col = colorOf(day);
    uint16_t bg = (col==1) ? BLUE : (col==2) ? RED : PALE;
    uint16_t fg = col ? WHITE : INK;
    tft.fillRoundRect(x,y,64,46,4,bg);
    if(!col) tft.drawRoundRect(x,y,64,46,4,BORDER);
    char num[5]; snprintf(num,sizeof(num),"%u",da);
    centered(num,x+31,y+18,fg,4);
    if(c==0) {char wk[6];snprintf(wk,sizeof(wk),"W%d",r+1);
      tft.setFont(&fonts::Font2);tft.setTextDatum(top_left);tft.setTextColor(fg);tft.drawString(wk,x+4,y+33);}
    if(da==1) {tft.setFont(&fonts::Font2);tft.setTextDatum(top_left);
      tft.setTextColor(fg);tft.drawString(monthName(mo),x+3,y+1);}
    if(noteOf(day)[0]) tft.fillCircle(x+54,y+37,4,col ? WHITE : RED);
  }
  tft.fillRect(0,283,480,1,BORDER);
  tft.setFont(&fonts::Font2);tft.setTextDatum(top_left);tft.setTextColor(INK);
  tft.drawString("TAP DATE: NOTE     HOLD DATE: COLOR",12,288);
  if(!fsReady || !saveOK) {tft.setTextColor(RED);tft.drawString("STORAGE ERROR - DATA NOT SAVED",10,304);}
  else {tft.setTextColor(BLUE);tft.drawString("13 PERIODS  /  4 WEEKS EACH",12,304);}
}
static void drawColorPicker() {
  drawCalendar();
  tft.fillRoundRect(28,77,424,188,12,WHITE);
  tft.drawRoundRect(28,77,424,188,12,NAVY);
  centered("COLOR  " + dateText(chosenDay),240,107,NAVY,4);
  button(43,146,122,55,"BLUE",BLUE,WHITE,4);
  button(179,146,122,55,"RED",RED,WHITE,4);
  button(315,146,122,55,"CLEAR",GREY,INK,4);
  button(168,219,144,32,"CANCEL",NAVY,WHITE);
}
static void drawKeyboard() {
  static const char *letters[]={"QWERTYUIOP","ASDFGHJKL","ZXCVBNM"};
  static const char *symbols[]={"1234567890","-/. :&#(),","?!+=%$@_;"};
  for(int row=0;row<3;++row) {
    String keys = numberKeys ? String(symbols[row]) : String(letters[row]);
    const int count=keys.length();
    int gap=(row==0?3:row==1?3:5), width=(row==0?44:row==1?47:47);
    int start=(480 - (count*width + (count-1)*gap))/2;
    int top=119 + row*43;
    for(int c=0;c<count;++c) button(start+c*(width+gap),top,width,37,String(keys[c]),PALE,INK,2);
  }
  button(5,250,76,48,numberKeys?"ABC":"123",NAVY,WHITE);
  button(88,250,240,48,"SPACE",PALE,INK);
  button(335,250,140,48,"BACK",RED,WHITE);
}
static void drawEditor() {
  tft.fillScreen(WHITE);
  tft.fillRect(0,0,480,51,BLUE); tft.fillRect(0,0,480,5,RED);
  button(6,7,68,37,"BACK",NAVY,WHITE);
  centered(dateText(chosenDay),235,25,WHITE,4);
  button(395,7,79,37,"SAVE",NAVY,WHITE);
  tft.fillRoundRect(8,55,464,57,5,PALE);
  tft.drawRoundRect(8,55,464,57,5,BORDER);
  tft.setFont(&fonts::Font2); tft.setTextColor(INK); tft.setTextDatum(top_left);
  if(!draft.length()) {tft.setTextColor(0x8410);tft.drawString("TYPE NOTE USING KEYBOARD...",17,67);}
  else {
    // Three printable lines of 45 characters, or up to NOTE_MAX in total.
    for(int line=0;line<3;++line) {
      const int begin=line*45;
      if(begin >= (int)draft.length()) break;
      tft.drawString(draft.substring(begin,min((int)draft.length(),begin+45)),17,59+line*17);
    }
  }
  drawKeyboard();
  char counter[20]; snprintf(counter,sizeof(counter),"%u/%d",(unsigned)draft.length(),NOTE_MAX);
  centered(counter,418,309,NAVY);
}
static void navigatePeriod(int n) {
  period+=n;
  if(period<1) {period=13; --fiscalYear;}
  else if(period>13) {period=1; ++fiscalYear;}
  drawCalendar();
}
static void handleKeyboard(int x,int y) {
  if(inside(x,y,5,250,76,48)) { numberKeys=!numberKeys;drawEditor();return; }
  if(inside(x,y,88,250,240,48)) { if(draft.length()<NOTE_MAX)draft+=' ';drawEditor();return; }
  if(inside(x,y,335,250,140,48)) { if(draft.length())draft.remove(draft.length()-1);drawEditor();return; }
  static const char *letters[]={"QWERTYUIOP","ASDFGHJKL","ZXCVBNM"};
  static const char *symbols[]={"1234567890","-/. :&#(),","?!+=%$@_;"};
  for(int row=0;row<3;++row) {
    String keys=numberKeys ? String(symbols[row]) : String(letters[row]);
    int count=keys.length(), gap=(row==0?3:row==1?3:5), width=(row==0?44:row==1?47:47);
    int start=(480 - (count*width+(count-1)*gap))/2;
    for(int c=0;c<count;++c) if(inside(x,y,start+c*(width+gap),119+row*43,width,37)) {
      if(draft.length()<NOTE_MAX) draft+=(char)keys[c];
      drawEditor(); return;
    }
  }
}
static void tap(int x,int y) {
  if(screen==CALENDAR) {
    if(inside(x,y,367,13,48,42)) { navigatePeriod(-1); return; }
    if(inside(x,y,424,13,48,42)) { navigatePeriod(1); return; }
    int r,c;
    if(dateCellAt(x,y,r,c)) {
      chosenDay=currentPeriodStart()+r*7+c;
      draft=String(noteOf(chosenDay)); numberKeys=false;
      screen=NOTE_EDITOR;drawEditor();
    }
    return;
  }
  if(screen==NOTE_EDITOR) {
    if(inside(x,y,6,7,68,37)) {screen=CALENDAR;drawCalendar();return;}
    if(inside(x,y,395,7,79,37)) {
      draft.trim();setEntry(chosenDay,colorOf(chosenDay),draft);
      screen=CALENDAR;drawCalendar();return;
    }
    if(y>=119) handleKeyboard(x,y);
    return;
  }
  if(screen==COLOR_PICKER) {
    uint8_t choice=0;
    if(inside(x,y,43,146,122,55))choice=1;
    else if(inside(x,y,179,146,122,55))choice=2;
    else if(inside(x,y,315,146,122,55))choice=3;
    if(choice) applyColor(choice==3?0:choice);
    screen=CALENDAR;drawCalendar();
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);
  tft.begin();tft.setRotation(1);tft.setBrightness(245);
  WHITE=tft.color565(255,255,255);
  BLUE=tft.color565(0,93,171);       // brand reference blue #005DAB
  RED=tft.color565(229,25,55);       // brand reference red  #E51937
  NAVY=tft.color565(0,47,95);
  PALE=tft.color565(238,243,248);
  INK=tft.color565(24,41,63);
  BORDER=tft.color565(209,221,231);
  GREY=tft.color565(220,223,226);
  fsReady=LittleFS.begin(false);
  if(!fsReady) fsReady=LittleFS.begin(true); // first-time format; see README backup warning
  if(fsReady) loadFile();
  saveOK=fsReady && saveOK;
  drawCalendar();
  Serial.printf("FY2027 Work Calendar, board %s, entries=%d, LittleFS=%d\n",
#if defined(CYD_CAPACITIVE)
    "C (capacitive)",
#else
    "R (resistive)",
#endif
    entryCount,fsReady);
}
void loop() {
  static bool down=false, held=false;
  static uint32_t began=0;
  static int originX=0,originY=0;
  uint16_t x=0,y=0;
  bool now=tft.getTouch(&x,&y);
  if(now && !down) {
    down=true;held=false;began=millis();originX=x;originY=y;
  } else if(now && down && !held && screen==CALENDAR && millis()-began>=HOLD_MS) {
    int r,c;
    if(dateCellAt(originX,originY,r,c)) {
      chosenDay=currentPeriodStart()+r*7+c;
      screen=COLOR_PICKER;drawColorPicker();held=true;
    }
  } else if(!now && down) {
    if(!held && millis()-began >= 25) tap(originX,originY);
    down=false;held=false;
  }
  delay(15);
}
