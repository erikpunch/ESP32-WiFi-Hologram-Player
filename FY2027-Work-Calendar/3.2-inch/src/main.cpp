/* FY2027 work calendar: 3.2-inch QD3223 / ESP32-32E. Compiles for landscape 320x240. */
#include <Arduino.h>
#include <LittleFS.h>
#include <string.h>
#include "CYD32_Display.h"

CYD32Display tft;
static uint16_t BLUE, RED, NAVY, PALE, INK, BORDER, WHITE, GREY;
constexpr int DISPLAY_W = 320, DISPLAY_H = 240;
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

// Compact 320x240 landscape layout for 3.2-inch E32R32P / QD3223 CYD.
static void centered(const String &s, int x, int y, uint16_t color, int font=2) {
  tft.setFont(font==4 ? &fonts::Font4 : &fonts::Font2);
  tft.setTextDatum(middle_center);
  tft.setTextColor(color);
  tft.drawString(s,x,y);
}
static void button(int x,int y,int w,int h,const String &label,uint16_t bg,uint16_t fg,int font=2) {
  tft.fillRoundRect(x,y,w,h,4,bg);
  centered(label,x+w/2,y+h/2,fg,font);
}
static bool dateCellAt(int x,int y,int &row,int &col) {
  if(!inside(x,y,4,67,315,152)) return false;
  col=(x-4)/45;
  row=(y-67)/38;
  if(col<0 || col>6 || row<0 || row>3) return false;
  return (x < 4+col*45+44) && (y < 67+row*38+36);
}
static void drawCalendar() {
  tft.fillScreen(WHITE);
  tft.fillRect(0,0,320,4,RED);
  tft.fillRect(0,4,320,40,BLUE);
  tft.setTextDatum(top_left);tft.setTextColor(WHITE);
  tft.setFont(&fonts::Font4);
  char title[24];snprintf(title,sizeof(title),"FY%02d P%02d", fiscalYear%100,period);
  tft.drawString(title,7,5);
  tft.setFont(&fonts::Font2);
  tft.drawString(dateText(currentPeriodStart(),false)+" - "+dateText(currentPeriodStart()+27,false),8,27);
  button(239,8,36,31,"<",NAVY,WHITE,4);
  button(281,8,36,31,">",NAVY,WHITE,4);
  tft.fillRect(0,44,320,20,NAVY);
  static const char* weekdays[]={"MON","TUE","WED","THU","FRI","SAT","SUN"};
  for(int c=0;c<7;++c) centered(weekdays[c],4+c*45+22,54,WHITE,2);
  const int32_t start=currentPeriodStart();
  for(int r=0;r<4;++r) for(int c=0;c<7;++c) {
    int x=4+c*45,y=67+r*38;
    const int32_t day=start+r*7+c;
    int yr;unsigned mo,da;civilFromDays(day,yr,mo,da);
    const uint8_t state=colorOf(day);
    const uint16_t bg=state==1?BLUE:state==2?RED:PALE;
    const uint16_t fg=state?WHITE:INK;
    tft.fillRoundRect(x,y,44,36,3,bg);
    if(!state)tft.drawRoundRect(x,y,44,36,3,BORDER);
    char n[4];snprintf(n,sizeof(n),"%u",da);
    centered(n,x+22,y+15,fg,4);
    // Date markers are deliberately small, retaining 44px-wide tap targets.
    tft.setFont(&fonts::Font0);tft.setTextColor(fg);tft.setTextDatum(top_left);
    if(c==0) { char w[4];snprintf(w,sizeof(w),"W%d",r+1);tft.drawString(w,x+2,y+27); }
    if(da==1) tft.drawString(monthName(mo),x+2,y+1);
    if(noteOf(day)[0])tft.fillCircle(x+37,y+29,3,state?WHITE:RED);
  }
  tft.fillRect(0,219,320,1,BORDER);
  tft.setTextDatum(top_left);tft.setTextColor(BLUE);tft.setFont(&fonts::Font2);
  if(!fsReady || !saveOK) {tft.setTextColor(RED);tft.drawString("STORAGE ERROR: NOT SAVED",7,222);}
  else tft.drawString("TAP: NOTE   HOLD: COLOR   13 PERIODS",6,222);
}
static void drawColorPicker(){
  drawCalendar();
  tft.fillRoundRect(11,57,298,126,8,WHITE);
  tft.drawRoundRect(11,57,298,126,8,NAVY);
  centered("COLOR: "+dateText(chosenDay,false),160,77,NAVY,4);
  button(20,100,89,43,"BLUE",BLUE,WHITE,2);
  button(116,100,88,43,"RED",RED,WHITE,2);
  button(211,100,89,43,"CLEAR",GREY,INK,2);
  button(110,151,100,25,"CANCEL",NAVY,WHITE,2);
}
static void drawKeyboard(){
  static const char* letters[]={"QWERTYUIOP","ASDFGHJKL","ZXCVBNM"};
  static const char* symbols[]={"1234567890","-/. :&#()","?!+=%$@"};
  for(int row=0;row<3;++row){
    String keys=numberKeys?String(symbols[row]):String(letters[row]);
    const int count=keys.length();
    const int width=row==0?28:row==1?30:37;
    const int gap=3;
    const int x0=(320-(count*width+(count-1)*gap))/2;
    const int y=93+row*33;
    for(int c=0;c<count;++c) button(x0+c*(width+gap),y,width,29,String(keys[c]),PALE,INK,2);
  }
  button(4,194,54,42,numberKeys?"ABC":"123",NAVY,WHITE);
  button(64,194,173,42,"SPACE",PALE,INK);
  button(243,194,73,42,"BACK",RED,WHITE);
}
static void drawEditor(){
  tft.fillScreen(WHITE);
  tft.fillRect(0,0,320,4,RED);tft.fillRect(0,4,320,34,BLUE);
  button(4,7,45,27,"BACK",NAVY,WHITE);
  centered(dateText(chosenDay),159,21,WHITE,2);
  button(269,7,47,27,"SAVE",NAVY,WHITE);
  tft.fillRoundRect(5,42,310,46,4,PALE);
  tft.drawRoundRect(5,42,310,46,4,BORDER);
  tft.setFont(&fonts::Font2);tft.setTextDatum(top_left);tft.setTextColor(INK);
  if(!draft.length()){
    tft.setTextColor(0x8410);tft.drawString("TYPE A NOTE...",11,52);
  } else {
    for(int line=0;line<3;++line){
      const int begin=line*32;
      if(begin>=(int)draft.length())break;
      tft.drawString(draft.substring(begin,min((int)draft.length(),begin+32)),9,43+line*14);
    }
  }
  drawKeyboard();
  char count[16];snprintf(count,sizeof(count),"%u/%d",(unsigned)draft.length(),NOTE_MAX);
  tft.setTextDatum(top_left);tft.setFont(&fonts::Font0);tft.setTextColor(NAVY);
  tft.drawString(count,278,87);
}
static void navigatePeriod(int direction){
  period+=direction;
  if(period<1){period=13;--fiscalYear;}
  else if(period>13){period=1;++fiscalYear;}
  drawCalendar();
}
static void handleKeyboard(int x,int y){
  if(inside(x,y,4,194,54,42)){numberKeys=!numberKeys;drawEditor();return;}
  if(inside(x,y,64,194,173,42)){if(draft.length()<NOTE_MAX)draft+=' ';drawEditor();return;}
  if(inside(x,y,243,194,73,42)){if(draft.length())draft.remove(draft.length()-1);drawEditor();return;}
  static const char* letters[]={"QWERTYUIOP","ASDFGHJKL","ZXCVBNM"};
  static const char* symbols[]={"1234567890","-/. :&#()","?!+=%$@"};
  for(int row=0;row<3;++row){
    String keys=numberKeys?String(symbols[row]):String(letters[row]);
    const int count=keys.length(),width=row==0?28:row==1?30:37,gap=3;
    const int x0=(320-(count*width+(count-1)*gap))/2;
    for(int c=0;c<count;++c)if(inside(x,y,x0+c*(width+gap),93+row*33,width,29)){
      if(draft.length()<NOTE_MAX)draft+=(char)keys[c];
      drawEditor();return;
    }
  }
}
static void tap(int x,int y){
  if(screen==CALENDAR){
    if(inside(x,y,239,8,36,31)){navigatePeriod(-1);return;}
    if(inside(x,y,281,8,36,31)){navigatePeriod(1);return;}
    int r,c;
    if(dateCellAt(x,y,r,c)){
      chosenDay=currentPeriodStart()+r*7+c;
      draft=String(noteOf(chosenDay));numberKeys=false;
      screen=NOTE_EDITOR;drawEditor();
    }
  }else if(screen==NOTE_EDITOR){
    if(inside(x,y,4,7,45,27)){screen=CALENDAR;drawCalendar();return;}
    if(inside(x,y,269,7,47,27)){
      draft.trim();setEntry(chosenDay,colorOf(chosenDay),draft);
      screen=CALENDAR;drawCalendar();return;
    }
    if(y>=93)handleKeyboard(x,y);
  }else if(screen==COLOR_PICKER){
    uint8_t choice=255;
    if(inside(x,y,20,100,89,43))choice=1;
    else if(inside(x,y,116,100,88,43))choice=2;
    else if(inside(x,y,211,100,89,43))choice=0;
    if(choice!=255)applyColor(choice);
    screen=CALENDAR;drawCalendar();
  }
}
void setup(){
  Serial.begin(115200);
  delay(100);
  tft.begin();tft.setRotation(1);tft.setBrightness(245);
  WHITE=tft.color565(255,255,255);
  BLUE=tft.color565(0,93,171);
  RED=tft.color565(229,25,55);
  NAVY=tft.color565(0,47,95);
  PALE=tft.color565(238,243,248);
  INK=tft.color565(24,41,63);
  BORDER=tft.color565(209,221,231);
  GREY=tft.color565(220,223,226);
  fsReady=LittleFS.begin(false);
  // A completely unformatted device is initialized automatically. This can erase
  // data if a different firmware has used an incompatible partition layout.
  if(!fsReady)fsReady=LittleFS.begin(true);
  if(fsReady)loadFile();
  saveOK=fsReady&&saveOK;
  drawCalendar();
  Serial.printf("FY2027 QD3223 calendar; entries=%d; LittleFS=%d\n",entryCount,fsReady);
}
void loop(){
  static bool down=false,held=false;
  static uint32_t began=0;
  static int originX=0,originY=0;
  uint16_t x=0,y=0;
  const bool now=tft.getTouch(&x,&y);
  if(now&&!down){down=true;held=false;began=millis();originX=x;originY=y;}
  else if(now&&down&&!held&&screen==CALENDAR&&millis()-began>=HOLD_MS){
    int r,c;
    if(dateCellAt(originX,originY,r,c)){
      chosenDay=currentPeriodStart()+r*7+c;
      screen=COLOR_PICKER;drawColorPicker();held=true;
    }
  } else if(!now&&down){
    if(!held&&millis()-began>=25)tap(originX,originY);
    down=false;held=false;
  }
  delay(15);
}
