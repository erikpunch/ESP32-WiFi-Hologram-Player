/*
  E32R32P TOUCH-TO-NEXT RAW VIDEO PLAYER — AUTO SCAN
  ---------------------------------------------------
  Automatically finds every valid .rgb or .rgbv animation
  in the ROOT of the SD card.

  Touch anywhere to advance to the next animation.

  Required raw format:
    320 x 240
    RGB565 little-endian
    10 FPS
    No header
    153600 bytes per frame

  Notes:
  - Files with only one frame are ignored.
  - Up to 50 animations are supported.
  - Files are played alphabetically by filename.
*/
#define TFT_SCK     14
#define TFT_MOSI    13
#define TFT_MISO    12
#define TFT_BL      27
#define TOUCH_IRQ   36

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <Arduino_GFX_Library.h>
#include <WiFi.h>
#include <WebServer.h>

#define TFT_DC       2
#define TFT_CS      15

#define SD_CS        5
#define SD_SCK      18
#define SD_MOSI     23
#define SD_MISO     19

static constexpr int VIDEO_W = 320;
static constexpr int VIDEO_H = 240;
static constexpr int FPS = 10;
static constexpr uint32_t FRAME_TIME_MS = 1000 / FPS;
static constexpr size_t BYTES_PER_LINE = VIDEO_W * 2;
static constexpr size_t BYTES_PER_FRAME =
  static_cast<size_t>(VIDEO_W) * VIDEO_H * 2;

static constexpr int MAX_VIDEOS = 50;
static constexpr int MAX_PATH_LENGTH = 96;

char videoFiles[MAX_VIDEOS][MAX_PATH_LENGTH];
int videoCount = 0;
int currentVideo = -1;

Arduino_DataBus *bus = new Arduino_ESP32SPI(
  TFT_DC, TFT_CS, TFT_SCK, TFT_MOSI, TFT_MISO, HSPI
);

#ifdef FREENOVE_E32R35T
Arduino_GFX *gfx = new Arduino_ST7796(bus, GFX_NOT_DEFINED, 1, false);
#else
Arduino_GFX *gfx = new Arduino_ST7789(
  bus,
  GFX_NOT_DEFINED,
  1,
  true,
  240,
  320,
  0, 0, 0, 0
);

#endif

SPIClass sdSPI(VSPI);
File videoFile;

// Wi-Fi access point: connect your phone/PC directly to the ESP32.
static const char *WIFI_AP_NAME = "HologramPlayer";
static const char *WIFI_AP_PASSWORD = "hologram";  // 8+ characters
WebServer server(80);
File uploadFile;
bool uploadInProgress = false;

// Headerless native clips need an explicit size marker because frame lengths overlap.
#ifdef FREENOVE_E32R35T
bool nativeClip(const char *name)
{
  const char *ext = strrchr(name, '.');
  return ext && ext - name >= 8 && strncasecmp(ext - 8, ".480x320", 8) == 0;
}
uint16_t lineBuffer[480];
uint16_t scaledLine[426];
bool nativePlayback = false;
#else
uint16_t lineBuffer[VIDEO_W];
#endif
uint32_t nextFrameAt = 0;

bool touchWasDown = false;
uint32_t lastTouchChange = 0;
static constexpr uint32_t TOUCH_DEBOUNCE_MS = 120;

void drawMessage(const char *line1, const char *line2 = nullptr)
{
  gfx->fillScreen(RGB565_BLACK);
  gfx->setTextColor(RGB565_WHITE);

  gfx->setTextSize(2);
  gfx->setCursor(10, 90);
  gfx->println(line1);

  if (line2 != nullptr) {
    gfx->setTextSize(1);
    gfx->setCursor(10, 125);
    gfx->println(line2);
  }
}

bool hasVideoExtension(const char *name)
{
  const char *dot = strrchr(name, '.');
  if (dot == nullptr) return false;

  return strcasecmp(dot, ".rgb") == 0 ||
         strcasecmp(dot, ".rgbv") == 0;
}

void sortVideoList()
{
  for (int i = 0; i < videoCount - 1; ++i) {
    for (int j = i + 1; j < videoCount; ++j) {
      if (strcasecmp(videoFiles[i], videoFiles[j]) > 0) {
        char temp[MAX_PATH_LENGTH];
        strncpy(temp, videoFiles[i], MAX_PATH_LENGTH);
        temp[MAX_PATH_LENGTH - 1] = '\0';

        strncpy(videoFiles[i], videoFiles[j], MAX_PATH_LENGTH);
        videoFiles[i][MAX_PATH_LENGTH - 1] = '\0';

        strncpy(videoFiles[j], temp, MAX_PATH_LENGTH);
        videoFiles[j][MAX_PATH_LENGTH - 1] = '\0';
      }
    }
  }
}

void scanForVideos()
{
  videoCount = 0;

  File root = SD.open("/");
  if (!root || !root.isDirectory()) {
    Serial.println("Could not open SD root directory.");
    return;
  }

  File entry = root.openNextFile();

  while (entry && videoCount < MAX_VIDEOS) {
    if (!entry.isDirectory()) {
      const char *entryName = entry.name();
      const size_t fileSize = entry.size();

      size_t frameBytes = BYTES_PER_FRAME;
#ifdef FREENOVE_E32R35T
      if (nativeClip(entryName)) frameBytes = 480 * 320 * 2;
#endif
      // At least 2 full frames, and no partial frame.
      const bool validSize =
        fileSize >= (frameBytes * 2) &&
        (fileSize % frameBytes) == 0;

      if (hasVideoExtension(entryName) && validSize) {
        if (entryName[0] == '/') {
          strncpy(
            videoFiles[videoCount],
            entryName,
            MAX_PATH_LENGTH - 1
          );
        } else {
          snprintf(
            videoFiles[videoCount],
            MAX_PATH_LENGTH,
            "/%s",
            entryName
          );
        }

        videoFiles[videoCount][MAX_PATH_LENGTH - 1] = '\0';

        Serial.printf(
          "Found animation %d: %s | %u frames\n",
          videoCount + 1,
          videoFiles[videoCount],
          static_cast<unsigned>(fileSize / frameBytes)
        );

        videoCount++;
      }
    }

    entry.close();
    entry = root.openNextFile();
  }

  root.close();
  sortVideoList();

  Serial.printf("Total animations found: %d\n", videoCount);

  if (videoCount == MAX_VIDEOS) {
    Serial.println("Maximum animation limit reached: 50");
  }
}

void closeCurrentVideo()
{
  if (videoFile) {
    videoFile.close();
  }
}

bool openVideoByIndex(int index)
{
  if (videoCount == 0 || index < 0 || index >= videoCount) {
    return false;
  }

  closeCurrentVideo();

  videoFile = SD.open(videoFiles[index], FILE_READ);
  if (!videoFile) {
    Serial.printf("Could not open: %s\n", videoFiles[index]);
    return false;
  }

  size_t frameBytes = BYTES_PER_FRAME;
#ifdef FREENOVE_E32R35T
  nativePlayback = nativeClip(videoFiles[index]);
  if (nativePlayback) frameBytes = 480 * 320 * 2;
#endif
  const size_t frameCount = videoFile.size() / frameBytes;

  currentVideo = index;
  nextFrameAt = millis();

  gfx->fillScreen(RGB565_BLACK);

  Serial.printf(
    "Now playing %d/%d: %s | %u frames\n",
    currentVideo + 1,
    videoCount,
    videoFiles[currentVideo],
    static_cast<unsigned>(frameCount)
  );

  return true;
}

bool openNextVideo()
{
  if (videoCount == 0) {
    return false;
  }

  for (int attempt = 1; attempt <= videoCount; ++attempt) {
    const int nextIndex = (currentVideo + attempt) % videoCount;

    if (openVideoByIndex(nextIndex)) {
      return true;
    }
  }

  return false;
}

bool drawOneFrame()
{
  if (!videoFile) {
    return false;
  }

  int width = VIDEO_W, height = VIDEO_H;
#ifdef FREENOVE_E32R35T
  if (nativePlayback) { width = 480; height = 320; }
#endif
  const size_t rowBytes = width * sizeof(uint16_t);
  if (videoFile.available() < width * height * 2) {
    videoFile.seek(0);
  }

  for (int y = 0; y < height; ++y) {
    const size_t bytesRead = videoFile.read(
      reinterpret_cast<uint8_t *>(lineBuffer),
      rowBytes
    );

    if (bytesRead != rowBytes) {
      Serial.printf(
        "Read error on line %d. Restarting animation.\n",
        y
      );

      videoFile.seek(0);
      return false;
    }

#ifdef FREENOVE_E32R35T
    if (!nativePlayback) {
      for (int x = 0; x < 426; ++x) scaledLine[x] = lineBuffer[x * 3 / 4];
      for (int row = y * 4 / 3; row < (y + 1) * 4 / 3; ++row) {
        gfx->draw16bitRGBBitmap(27, row, scaledLine, 426, 1);
      }
      continue;
    }
#endif
    gfx->draw16bitRGBBitmap(0, y, lineBuffer, width, 1);
  }

  return true;
}

void handleTouch()
{
  const bool touchDown = digitalRead(TOUCH_IRQ) == LOW;
  const uint32_t now = millis();

  if (touchDown != touchWasDown &&
      now - lastTouchChange >= TOUCH_DEBOUNCE_MS) {

    lastTouchChange = now;
    touchWasDown = touchDown;

    if (touchDown) {
      Serial.println("Touch detected: next animation");
      openNextVideo();
    }
  }
}


String htmlEscape(const String &s)
{
  String out;
  out.reserve(s.length() + 16);
  for (size_t i = 0; i < s.length(); ++i) {
    char c = s[i];
    if (c == '&') out += F("&amp;");
    else if (c == '<') out += F("&lt;");
    else if (c == '>') out += F("&gt;");
    else if (c == '"') out += F("&quot;");
    else out += c;
  }
  return out;
}

void sendControlPage()
{
  String page;
  page.reserve(7000);

  page += F(
    "<!doctype html><html><head>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Hologram Player</title>"
    "<style>"
    "body{font-family:Arial,sans-serif;background:#111;color:#eee;max-width:760px;margin:auto;padding:20px}"
    "h1{color:#67e8f9}.card{background:#1d1d1d;padding:16px;margin:14px 0;border-radius:12px}"
    "button,input[type=submit]{background:#0891b2;color:white;border:0;padding:10px 14px;border-radius:8px;margin:4px}"
    "input[type=file]{max-width:100%}.file{display:flex;gap:8px;align-items:center;justify-content:space-between;border-top:1px solid #333;padding:9px 0}"
    ".name{overflow-wrap:anywhere}.danger{background:#b91c1c}.small{color:#aaa;font-size:.9em}"
    "</style></head><body><h1>Hologram Player</h1>"
  );

  page += F("<div class='card'><b>Touchscreen:</b> still advances to the next animation.<br>");
  page += F("<b>Wi-Fi:</b> HologramPlayer<br><b>Address:</b> http://192.168.4.1</div>");

  page += F(
    "<div class='card'><h2>Upload animation</h2>"
    "<form method='POST' action='/upload' enctype='multipart/form-data'>"
    "<input type='file' name='file' accept='.rgb,.rgbv' required>"
    "<input type='submit' value='Upload to SD'></form>"
    "<p class='small'>Valid player format: 320x240 RGB565 little-endian; .rgb or .rgbv. "
#ifdef FREENOVE_E32R35T
    "For native 480x320 clips use a name ending in .480x320.rgb or .480x320.rgbv. "
#endif
    "The player will rescan the SD card after upload.</p></div>"
  );

  page += F("<div class='card'><h2>Animations</h2>");
  if (videoCount == 0) {
    page += F("<p>No valid animations found.</p>");
  } else {
    for (int i = 0; i < videoCount; ++i) {
      String path = String(videoFiles[i]);
      String shown = path;
      if (shown.startsWith("/")) shown.remove(0, 1);

      page += F("<div class='file'><span class='name'>");
      if (i == currentVideo) page += F("&#9654; ");
      page += htmlEscape(shown);
      page += F("</span><span>");

      page += F("<form style='display:inline' method='POST' action='/play'>"
                "<input type='hidden' name='i' value='");
      page += String(i);
      page += F("'><input type='submit' value='Play'></form>");

      page += F("<form style='display:inline' method='POST' action='/delete' "
                "onsubmit=\"return confirm('Delete this file?')\">"
                "<input type='hidden' name='file' value='");
      page += htmlEscape(path);
      page += F("'><input class='danger' type='submit' value='Delete'></form>");

      page += F("</span></div>");
    }
  }
  page += F("</div>");

  page += F(
    "<div class='card'>"
    "<form style='display:inline' method='POST' action='/next'><input type='submit' value='Next animation'></form>"
    "<form style='display:inline' method='POST' action='/rescan'><input type='submit' value='Rescan SD'></form>"
    "</div></body></html>"
  );

  server.send(200, "text/html", page);
}

void handleUploadData()
{
  HTTPUpload &upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    uploadInProgress = true;
    closeCurrentVideo();

    String filename = upload.filename;
    filename.replace("\\", "/");
    int slash = filename.lastIndexOf('/');
    if (slash >= 0) filename = filename.substring(slash + 1);
    if (!filename.startsWith("/")) filename = "/" + filename;

    Serial.printf("Wi-Fi upload starting: %s\n", filename.c_str());

    if (SD.exists(filename.c_str())) {
      SD.remove(filename.c_str());
    }

    uploadFile = SD.open(filename.c_str(), FILE_WRITE);
    if (!uploadFile) {
      Serial.println("Could not create upload file.");
    }
  }
  else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile) {
      uploadFile.write(upload.buf, upload.currentSize);
    }
  }
  else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) {
      uploadFile.close();
    }

    Serial.printf("Wi-Fi upload complete: %u bytes\n",
                  static_cast<unsigned>(upload.totalSize));

    uploadInProgress = false;
    scanForVideos();

    if (videoCount > 0) {
      openVideoByIndex(0);
    }
  }
  else if (upload.status == UPLOAD_FILE_ABORTED) {
    if (uploadFile) uploadFile.close();
    uploadInProgress = false;
    Serial.println("Wi-Fi upload aborted.");

    scanForVideos();
    if (videoCount > 0) openVideoByIndex(0);
  }
}

void setupWebServer()
{
  server.on("/", HTTP_GET, sendControlPage);

  server.on("/next", HTTP_POST, []() {
    openNextVideo();
    server.sendHeader("Location", "/");
    server.send(303);
  });

  server.on("/play", HTTP_POST, []() {
    if (server.hasArg("i")) {
      int i = server.arg("i").toInt();
      openVideoByIndex(i);
    }
    server.sendHeader("Location", "/");
    server.send(303);
  });

  server.on("/rescan", HTTP_POST, []() {
    closeCurrentVideo();
    scanForVideos();
    if (videoCount > 0) openVideoByIndex(0);
    server.sendHeader("Location", "/");
    server.send(303);
  });

  server.on("/delete", HTTP_POST, []() {
    if (server.hasArg("file")) {
      String path = server.arg("file");
      if (!path.startsWith("/")) path = "/" + path;

      closeCurrentVideo();
      if (SD.exists(path.c_str())) {
        Serial.printf("Deleting: %s\n", path.c_str());
        SD.remove(path.c_str());
      }

      scanForVideos();
      if (videoCount > 0) openVideoByIndex(0);
      else currentVideo = -1;
    }

    server.sendHeader("Location", "/");
    server.send(303);
  });

  server.on(
    "/upload",
    HTTP_POST,
    []() {
      server.sendHeader("Location", "/");
      server.send(303);
    },
    handleUploadData
  );

  server.onNotFound([]() {
    server.send(404, "text/plain", "Not found");
  });

  server.begin();
  Serial.println("Hologram web server started.");
}

void startWiFi()
{
  WiFi.mode(WIFI_AP);
  bool ok = WiFi.softAP(WIFI_AP_NAME, WIFI_AP_PASSWORD);

  if (ok) {
    Serial.println();
    Serial.println("==================================");
    Serial.printf("Wi-Fi network: %s\n", WIFI_AP_NAME);
    Serial.printf("Wi-Fi password: %s\n", WIFI_AP_PASSWORD);
    Serial.print("Open in browser: http://");
    Serial.println(WiFi.softAPIP());
    Serial.println("==================================");
  } else {
    Serial.println("Could not start Wi-Fi access point.");
  }

  setupWebServer();
}


void setup()
{
  Serial.begin(115200);
  delay(300);

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  pinMode(TOUCH_IRQ, INPUT);

#ifdef FREENOVE_E32R35T
  // XPT2046 shares the display bus. Tap-to-next only reads its PENIRQ.
  pinMode(33, OUTPUT);
  digitalWrite(33, HIGH);
  constexpr int displayClock = 40000000;
#else
  constexpr int displayClock = 20000000;
#endif
  if (!gfx->begin(displayClock)) {
    while (true) {
      delay(1000);
    }
  }

  gfx->fillScreen(RGB565_BLACK);

  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

  if (!SD.begin(SD_CS, sdSPI, 12000000)) {
    drawMessage("SD CARD ERROR", "Use a FAT32-formatted card.");

    while (true) {
      delay(1000);
    }
  }

  scanForVideos();

  if (videoCount == 0) {
    drawMessage(
      "NO ANIMATIONS",
      "Add 320x240 .rgb or .rgbv files."
    );

    while (true) {
      delay(1000);
    }
  }

  openVideoByIndex(0);

  // Start Wi-Fi only after the known-good display and SD setup succeeds.
  startWiFi();
}

void loop()
{
  // Keep servicing browser requests while the animation continues playing.
  server.handleClient();

  if (uploadInProgress) {
    delay(1);
    return;
  }

  handleTouch();

  if (!videoFile) {
    delay(10);
    return;
  }

  const uint32_t now = millis();

  if (static_cast<int32_t>(now - nextFrameAt) >= 0) {
    drawOneFrame();
    nextFrameAt += FRAME_TIME_MS;

    if (static_cast<int32_t>(now - nextFrameAt) >
        static_cast<int32_t>(FRAME_TIME_MS * 2)) {
      nextFrameAt = millis() + FRAME_TIME_MS;
    }
  }
}
