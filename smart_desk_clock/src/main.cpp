#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <Preferences.h>
#include <SPIFFS.h>
#include <TJpg_Decoder.h>
#include <WebServer.h>
#include "config.h"
#include "web_ui.h"

Preferences prefs;
TFT_eSPI    tft;
WebServer   webServer(WEB_PORT);

File     webUploadFile;
int      webUploadSlot = 0;
size_t   webUploadBytes = 0;
bool     webUploadFailed = false;

uint16_t C_BG, C_ACCENT, C_WHITE, C_GRAY, C_DARK, C_GREEN, C_LINE, C_AMBIENT;

bool wifiConnected = false;

int gHour   = 0;
int gMinute = 0;
int gSecond = 0;
int gDay    = 1;
int gMonth  = 1;
int gYear   = 2026;

String gDayName   = "Loading";
String gCityLabel = "TEHRAN";
String gTimezone  = TIME_API_DEFAULT_TZ;

String rSSID = WIFI_SSID;
String rPASS = WIFI_PASS;

uint32_t lastSecondTick  = 0;
uint32_t lastApiSync     = 0;
uint32_t lastSlideSwitch = 0;
uint32_t lastWiFiRetry   = 0;

// ── slideshow ──
bool     imgAvailable[2]  = {false, false};
int      currentSlide     = -1;   // -1 = clock mode
bool     showingSlide     = false;
uint32_t slideShowTimer   = 0;

const uint32_t SLIDE_INTERVAL_MS = 300000UL;  // هر ۵ دقیقه
const uint32_t SLIDE_DURATION_MS = 10000UL;   // هر عکس ۱۰ ثانیه

const char* IMG_PATH[2] = {"/img1.jpg", "/img2.jpg"};

const char* MONTHS[] =
{
    "",
    "JAN","FEB","MAR","APR","MAY","JUN",
    "JUL","AUG","SEP","OCT","NOV","DEC"
};

// ════════════════════════════════════════════
//  TJpg callback: هر tile رو مستقیم روی TFT می‌کشه
// ════════════════════════════════════════════
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap)
{
    if (y >= tft.height()) return 0;
    tft.pushImage(x, y, w, h, bitmap);
    return 1;
}

// ════════════════════════════════════════════
//  Flash: ذخیره و بازیابی
// ════════════════════════════════════════════
void saveToFlash()
{
    prefs.begin("espctl", false);
    prefs.putString("ssid",     rSSID);
    prefs.putString("pass",     rPASS);
    prefs.putString("timezone", gTimezone);
    prefs.putString("city",     gCityLabel);
    prefs.end();
    Serial.println("OK:saved to flash");
}

void loadFromFlash()
{
    prefs.begin("espctl", true);
    rSSID      = prefs.getString("ssid",     WIFI_SSID);
    rPASS      = prefs.getString("pass",     WIFI_PASS);
    gTimezone  = prefs.getString("timezone", TIME_API_DEFAULT_TZ);
    gCityLabel = prefs.getString("city",     "TEHRAN");
    prefs.end();

    if (rSSID.length() > 0)
        Serial.printf("Flash loaded: ssid=%s tz=%s\n", rSSID.c_str(), gTimezone.c_str());
    else
        Serial.println("Flash: no saved data");
}

// ════════════════════════════════════════════
void initColors()
{
    C_BG      = tft.color565(10,  10,  15);
    C_ACCENT  = tft.color565(99,  102, 241);
    C_WHITE   = tft.color565(241, 241, 255);
    C_GRAY    = tft.color565(136, 136, 136);
    C_DARK    = tft.color565(30,  30,  46);
    C_GREEN   = tft.color565(34,  197, 94);
    C_LINE    = tft.color565(42,  42,  58);
    C_AMBIENT = tft.color565(18,  14,  45);
}

void connectWiFi()
{
    if (rSSID.length() == 0) return;

    Serial.printf("Connecting to %s\n", rSSID.c_str());
    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(rSSID.c_str(), rPASS.c_str());

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT_MS)
    {
        delay(250);
        Serial.print(".");
    }

    wifiConnected = (WiFi.status() == WL_CONNECTED);
    Serial.println();

    if (wifiConnected)
    {
        Serial.println("WiFi Connected");
        Serial.println(WiFi.localIP());
    }
    else Serial.println("WiFi Failed");
}

bool syncTime()
{
    if (!wifiConnected) return false;

    String url = String(TIME_API_BASE) + gTimezone;
    HTTPClient http;
    http.begin(url);

    int code = http.GET();
    if (code != 200)
    {
        Serial.printf("HTTP Error %d\n", code);
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    JsonDocument doc;
    if (deserializeJson(doc, payload)) { Serial.println("JSON Error"); return false; }

    gHour    = doc["hour"]      | 0;
    gMinute  = doc["minute"]    | 0;
    gSecond  = doc["seconds"]   | 0;
    gDay     = doc["day"]       | 1;
    gMonth   = doc["month"]     | 1;
    gYear    = doc["year"]      | 2026;
    gDayName = String((const char*)doc["dayOfWeek"]);

    Serial.printf("Time Sync OK %02d:%02d:%02d\n", gHour, gMinute, gSecond);
    return true;
}

void tickClock()
{
    if (millis() - lastSecondTick < 1000) return;
    lastSecondTick = millis();
    gSecond++;
    if (gSecond >= 60) { gSecond = 0; gMinute++; }
    if (gMinute >= 60) { gMinute = 0; gHour++;   }
    if (gHour   >= 24) { gHour   = 0;             }
}

void drawUI()
{
    tft.fillScreen(C_BG);
    tft.fillCircle(SCREEN_WIDTH / 2, -10, 65, C_AMBIENT);

    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(1);
    tft.setTextColor(C_GRAY, C_BG);

    String dayShort = gDayName.substring(0, 3);
    dayShort.toUpperCase();
    tft.drawString(dayShort, SCREEN_WIDTH / 2, 58);

    char dateBuf[16];
    sprintf(dateBuf, "%02d %s %04d", gDay, MONTHS[gMonth], gYear);
    tft.setTextColor(C_ACCENT, C_BG);
    tft.drawString(dateBuf, SCREEN_WIDTH / 2, 74);

    tft.drawFastHLine(SCREEN_WIDTH / 2 - 18, 88, 36, C_LINE);

    char timeBuf[9];
    sprintf(timeBuf, "%02d:%02d:%02d", gHour, gMinute, gSecond);
    tft.setTextColor(C_WHITE, C_BG);
    tft.setTextSize(3);
    tft.drawString(timeBuf, SCREEN_WIDTH / 2, 148);

    int barX = SCREEN_WIDTH / 2 - 60, barY = 178, barW = 120, barH = 3;
    tft.fillRoundRect(barX, barY, barW, barH, 1, C_DARK);
    int fill = map(gSecond, 0, 59, 0, barW);
    if (fill > 0) tft.fillRoundRect(barX, barY, fill, barH, 1, C_ACCENT);

    // Network info: show the hotspot/router-assigned STA IP just above the UTC label.
    // Keep it deliberately small and subtle so it does not compete with the clock.
    tft.setTextSize(1);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(C_GRAY, C_BG);

    String ipLabel = wifiConnected ? WiFi.localIP().toString() : String("--");
    tft.drawString(ipLabel.c_str(), SCREEN_WIDTH / 2, 237);

    uint16_t dotColor = wifiConnected ? C_GREEN : C_GRAY;
    tft.fillCircle(SCREEN_WIDTH / 2 - 45, 256, 4, dotColor);
    tft.setTextDatum(ML_DATUM);
    tft.drawString(gCityLabel.c_str(), SCREEN_WIDTH / 2 - 37, 256);
}

// ════════════════════════════════════════════
//  Slideshow
// ════════════════════════════════════════════
void showImage(int idx)
{
    if (!imgAvailable[idx]) return;

    tft.fillScreen(C_BG);
    TJpgDec.drawFsJpg(0, 0, IMG_PATH[idx]);
    Serial.printf("slide: showing img%d\n", idx + 1);
}

void updateSlideshow()
{
    // آیا هیچ عکسی داریم؟
    bool anyImg = imgAvailable[0] || imgAvailable[1];
    if (!anyImg) return;

    uint32_t now = millis();

    if (!showingSlide)
    {
        // وقتشه که slideshow شروع بشه؟
        if (now - lastSlideSwitch >= SLIDE_INTERVAL_MS)
        {
            // پیدا کردن اولین عکس موجود
            for (int i = 0; i < 2; i++)
            {
                int next = (currentSlide + 1 + i) % 2;
                if (imgAvailable[next])
                {
                    currentSlide  = next;
                    showingSlide  = true;
                    slideShowTimer = now;
                    showImage(currentSlide);
                    return;
                }
            }
        }
    }
    else
    {
        // ۱۰ ثانیه گذشت؟
        if (now - slideShowTimer >= SLIDE_DURATION_MS)
        {
            // عکس بعدی هست؟
            bool nextFound = false;
            for (int i = 1; i <= 2; i++)
            {
                int next = (currentSlide + i) % 2;
                if (imgAvailable[next] && next != currentSlide)
                {
                    currentSlide   = next;
                    slideShowTimer = now;
                    showImage(currentSlide);
                    nextFound = true;
                    break;
                }
            }

            if (!nextFound)
            {
                // فقط یه عکس داریم یا هر دو نشون داده شدن → برگرد به ساعت
                showingSlide   = false;
                lastSlideSwitch = now;
                drawUI();
            }
        }
    }
}

// ════════════════════════════════════════════
//  دریافت تصویر از Serial
//  پروتکل: IMG:index,size\n  +  [bytes]  +  END_IMG\n
// ════════════════════════════════════════════
void receiveImage(int idx, uint32_t size)
{
    Serial.printf("OK:ready to receive img%d (%u bytes)\n", idx, size);

    File f = SPIFFS.open(IMG_PATH[idx - 1], FILE_WRITE);
    if (!f)
    {
        Serial.println("ERR:spiffs open failed");
        return;
    }

    uint8_t  buf[256];
    uint32_t received = 0;

    // timeout: 30 ثانیه
    uint32_t deadline = millis() + 30000UL;

    while (received < size && millis() < deadline)
    {
        int avail = Serial.available();
        if (avail <= 0) { delay(5); continue; }

        int toRead = min((uint32_t)min(avail, (int)sizeof(buf)), size - received);
        int n      = Serial.readBytes(buf, toRead);
        f.write(buf, n);
        received += n;
    }

    f.close();

    // بلعیدن END_IMG\n
    uint32_t t = millis() + 2000;
    while (millis() < t)
    {
        if (Serial.available())
        {
            String tail = Serial.readStringUntil('\n');
            if (tail.indexOf("END_IMG") >= 0) break;
        }
        delay(5);
    }

    if (received == size)
    {
        imgAvailable[idx - 1] = true;
        Serial.printf("OK:img%d saved (%u bytes)\n", idx, received);

        // نمایش فوری بعد از دریافت به مدت ۱۰ ثانیه
        currentSlide   = idx - 1;
        showingSlide   = true;
        slideShowTimer = millis();
        showImage(currentSlide);
    }
    else
    {
        Serial.printf("ERR:img%d incomplete %u/%u\n", idx, received, size);
        imgAvailable[idx - 1] = false;
    }
}


// ════════════════════════════════════════════
//  Embedded Web control panel
// ════════════════════════════════════════════
String jsonEscape(const String& value)
{
    String out;
    out.reserve(value.length() + 8);
    for (size_t i = 0; i < value.length(); i++)
    {
        char c = value[i];
        if (c == '\\' || c == '"') { out += '\\'; out += c; }
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else out += c;
    }
    return out;
}

void sendJson(int code, const String& body)
{
    webServer.sendHeader("Cache-Control", "no-store");
    webServer.send(code, "application/json", body);
}

void startSetupAccessPoint()
{
    WiFi.mode(WIFI_AP_STA);
    bool ok = WiFi.softAP(WEB_AP_SSID, WEB_AP_PASS);
    if (ok)
    {
        Serial.printf("Web setup AP: %s  IP=%s\n", WEB_AP_SSID, WiFi.softAPIP().toString().c_str());
    }
    else Serial.println("ERR: failed to start setup AP");
}

void handleWebStatus()
{
    String body = "{";
    body += "\"wifi_connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false");
    body += ",\"ssid\":\"" + jsonEscape(rSSID) + "\"";
    body += ",\"sta_ip\":\"" + jsonEscape(WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : String("")) + "\"";
    body += ",\"ap_ip\":\"" + jsonEscape(WiFi.softAPIP().toString()) + "\"";
    body += ",\"timezone\":\"" + jsonEscape(gTimezone) + "\"";
    body += ",\"city_label\":\"" + jsonEscape(gCityLabel) + "\"";
    body += ",\"img1\":" + String(imgAvailable[0] ? "true" : "false");
    body += ",\"img2\":" + String(imgAvailable[1] ? "true" : "false");
    body += ",\"time\":\"" + String(gHour < 10 ? "0" : "") + String(gHour) + ":" + String(gMinute < 10 ? "0" : "") + String(gMinute) + ":" + String(gSecond < 10 ? "0" : "") + String(gSecond) + "\"";
    body += "}";
    sendJson(200, body);
}

void handleWebWifi()
{
    if (!webServer.hasArg("ssid")) { sendJson(400, "{\"message\":\"SSID is required\"}"); return; }

    String newSsid = webServer.arg("ssid");
    newSsid.trim();
    if (newSsid.length() == 0 || newSsid.length() > 32)
    {
        sendJson(400, "{\"message\":\"Invalid SSID\"}");
        return;
    }

    String oldSsid = rSSID;
    String newPass = webServer.hasArg("password") ? webServer.arg("password") : String("");
    rSSID = newSsid;
    if (newPass.length() > 0 || oldSsid != newSsid) rPASS = newPass;

    saveToFlash();
    WiFi.disconnect(false, false);
    delay(150);
    connectWiFi();
    if (wifiConnected) syncTime();
    if (!showingSlide) drawUI();

    if (wifiConnected)
        sendJson(200, "{\"message\":\"Wi-Fi connected and settings saved\"}");
    else
        sendJson(200, "{\"message\":\"Settings saved, but Wi-Fi connection failed. Setup AP is still available.\"}");
}

void handleWebTimezone()
{
    if (!webServer.hasArg("timezone")) { sendJson(400, "{\"message\":\"timezone is required\"}"); return; }
    String tz = webServer.arg("timezone");
    tz.trim();
    if (tz.length() == 0 || tz.length() > 64) { sendJson(400, "{\"message\":\"Invalid timezone\"}"); return; }

    gTimezone = tz;
    if (webServer.hasArg("label"))
    {
        gCityLabel = webServer.arg("label");
        gCityLabel.trim();
        if (gCityLabel.length() > 20) gCityLabel = gCityLabel.substring(0, 20);
    }

    // Browser-provided local time mirrors the Python controller and works even offline.
    if (webServer.hasArg("hour") && webServer.hasArg("minute") && webServer.hasArg("second"))
    {
        gHour = constrain(webServer.arg("hour").toInt(), 0, 23);
        gMinute = constrain(webServer.arg("minute").toInt(), 0, 59);
        gSecond = constrain(webServer.arg("second").toInt(), 0, 59);
        lastSecondTick = millis();
    }

    saveToFlash();
    bool synced = wifiConnected && syncTime();
    if (!showingSlide) drawUI();
    sendJson(200, synced ? "{\"message\":\"Region saved and online time synced\"}" : "{\"message\":\"Region saved; browser local time applied\"}");
}

void handleWebSync()
{
    if (!wifiConnected) { sendJson(409, "{\"message\":\"Wi-Fi is not connected\"}"); return; }
    if (!syncTime()) { sendJson(502, "{\"message\":\"Online time sync failed\"}"); return; }
    if (!showingSlide) drawUI();
    sendJson(200, "{\"message\":\"Time synced successfully\"}");
}

void handleWebImageUpload(int slot)
{
    HTTPUpload& upload = webServer.upload();
    const char* path = IMG_PATH[slot - 1];

    if (upload.status == UPLOAD_FILE_START)
    {
        webUploadSlot = slot;
        webUploadBytes = 0;
        webUploadFailed = false;
        if (SPIFFS.exists(path)) SPIFFS.remove(path);
        webUploadFile = SPIFFS.open(path, FILE_WRITE);
        if (!webUploadFile) webUploadFailed = true;
    }
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        if (webUploadSlot != slot || webUploadFailed) return;
        if (webUploadBytes + upload.currentSize > WEB_MAX_IMAGE_BYTES)
        {
            webUploadFailed = true;
            if (webUploadFile) webUploadFile.close();
            SPIFFS.remove(path);
            return;
        }
        if (!webUploadFile || webUploadFile.write(upload.buf, upload.currentSize) != upload.currentSize)
        {
            webUploadFailed = true;
            if (webUploadFile) webUploadFile.close();
            SPIFFS.remove(path);
            return;
        }
        webUploadBytes += upload.currentSize;
    }
    else if (upload.status == UPLOAD_FILE_END)
    {
        if (webUploadFile) webUploadFile.close();
        if (!webUploadFailed && webUploadBytes > 2)
        {
            File check = SPIFFS.open(path, FILE_READ);
            bool jpeg = false;
            if (check)
            {
                int a = check.read(), b = check.read();
                jpeg = (a == 0xFF && b == 0xD8);
                check.close();
            }
            if (!jpeg) { webUploadFailed = true; SPIFFS.remove(path); }
        }
        else { webUploadFailed = true; SPIFFS.remove(path); }

        imgAvailable[slot - 1] = !webUploadFailed;
        if (imgAvailable[slot - 1])
        {
            currentSlide = slot - 1;
            showingSlide = true;
            slideShowTimer = millis();
            showImage(currentSlide);
        }
    }
    else if (upload.status == UPLOAD_FILE_ABORTED)
    {
        webUploadFailed = true;
        if (webUploadFile) webUploadFile.close();
        SPIFFS.remove(path);
        imgAvailable[slot - 1] = false;
    }
}

void finishWebImageUpload(int slot)
{
    if (webUploadSlot != slot || webUploadFailed || !imgAvailable[slot - 1])
    {
        sendJson(400, "{\"message\":\"Image upload failed or exceeded 256 KB\"}");
        return;
    }
    sendJson(200, "{\"message\":\"Image saved to ESP flash\"}");
}

void handleDeleteWebImage(int slot)
{
    const char* path = IMG_PATH[slot - 1];
    if (SPIFFS.exists(path)) SPIFFS.remove(path);
    imgAvailable[slot - 1] = false;
    if (showingSlide && currentSlide == slot - 1)
    {
        showingSlide = false;
        currentSlide = -1;
        lastSlideSwitch = millis();
        drawUI();
    }
    sendJson(200, "{\"message\":\"Image deleted\"}");
}

void handleGetWebImage(int slot)
{
    const char* path = IMG_PATH[slot - 1];
    if (!SPIFFS.exists(path)) { webServer.send(404, "text/plain", "Not found"); return; }
    File f = SPIFFS.open(path, FILE_READ);
    webServer.sendHeader("Cache-Control", "no-store");
    webServer.streamFile(f, "image/jpeg");
    f.close();
}

void handleWebRestart()
{
    sendJson(200, "{\"message\":\"ESP restarting\"}");
    delay(250);
    ESP.restart();
}

void handleWebEraseStorage()
{
    prefs.begin("espctl", false);
    prefs.clear();
    prefs.end();
    SPIFFS.format();
    imgAvailable[0] = imgAvailable[1] = false;
    sendJson(200, "{\"message\":\"Flash storage erased; ESP restarting\"}");
    delay(300);
    ESP.restart();
}

void startWebServer()
{
    webServer.on("/", HTTP_GET, []() { webServer.send_P(200, "text/html; charset=utf-8", WEB_UI_HTML); });
    webServer.on("/api/status", HTTP_GET, handleWebStatus);
    webServer.on("/api/wifi", HTTP_POST, handleWebWifi);
    webServer.on("/api/timezone", HTTP_POST, handleWebTimezone);
    webServer.on("/api/sync", HTTP_POST, handleWebSync);

    webServer.on("/api/image/1", HTTP_POST, []() { finishWebImageUpload(1); }, []() { handleWebImageUpload(1); });
    webServer.on("/api/image/2", HTTP_POST, []() { finishWebImageUpload(2); }, []() { handleWebImageUpload(2); });
    webServer.on("/api/image/1", HTTP_DELETE, []() { handleDeleteWebImage(1); });
    webServer.on("/api/image/2", HTTP_DELETE, []() { handleDeleteWebImage(2); });
    webServer.on("/image/1", HTTP_GET, []() { handleGetWebImage(1); });
    webServer.on("/image/2", HTTP_GET, []() { handleGetWebImage(2); });
    webServer.on("/api/restart", HTTP_POST, handleWebRestart);
    webServer.on("/api/erase-storage", HTTP_POST, handleWebEraseStorage);
    webServer.onNotFound([]() { webServer.send(404, "application/json", "{\"message\":\"Not found\"}"); });

    webServer.begin();
    Serial.println("Web server started on port 80");
}

// ════════════════════════════════════════════
//  Serial parser
// ════════════════════════════════════════════
void handleSerial()
{
    if (!Serial.available()) return;

    String line = Serial.readStringUntil('\n');
    line.trim();

    // ── WIFI ──
    if (line.startsWith("WIFI:"))
    {
        String data = line.substring(5);
        int comma   = data.indexOf(',');
        if (comma < 0) { Serial.println("ERR:bad WIFI format"); return; }

        rSSID = data.substring(0, comma);
        rPASS = data.substring(comma + 1);

        Serial.printf("OK:WIFI ssid=%s\n", rSSID.c_str());
        WiFi.disconnect(false, false);
        delay(300);
        wifiConnected = false;

        connectWiFi();
        if (wifiConnected) { syncTime(); saveToFlash(); }
        drawUI();
        return;
    }

    // ── TIME ──
    if (line.startsWith("TIME:"))
    {
        String data = line.substring(5);
        int c1 = data.indexOf(',');
        int c2 = data.indexOf(',', c1 + 1);
        int c3 = data.indexOf(',', c2 + 1);
        if (c1 < 0 || c2 < 0 || c3 < 0) { Serial.println("ERR:bad TIME format"); return; }

        gHour      = data.substring(0,      c1).toInt();
        gMinute    = data.substring(c1 + 1, c2).toInt();
        gSecond    = data.substring(c2 + 1, c3).toInt();
        gCityLabel = data.substring(c3 + 1);
        gCityLabel.trim();

        lastSecondTick = millis();
        Serial.printf("OK:TIME %02d:%02d:%02d label=%s\n",
                      gHour, gMinute, gSecond, gCityLabel.c_str());
        saveToFlash();
        if (!showingSlide) drawUI();
        return;
    }

    // ── SETTZ ──
    if (line.startsWith("SETTZ:"))
    {
        gTimezone = line.substring(6);
        gTimezone.trim();
        Serial.printf("OK:timezone=%s\n", gTimezone.c_str());
        saveToFlash();
        return;
    }

    // ── IMG ──
    // فرمت: IMG:index,size
    if (line.startsWith("IMG:"))
    {
        String data  = line.substring(4);
        int    comma = data.indexOf(',');
        if (comma < 0) { Serial.println("ERR:bad IMG format"); return; }

        int      idx  = data.substring(0, comma).toInt();
        uint32_t size = data.substring(comma + 1).toInt();

        if (idx < 1 || idx > 2 || size == 0)
        {
            Serial.println("ERR:invalid IMG params");
            return;
        }

        receiveImage(idx, size);
        return;
    }

    Serial.println("ERR:unknown command");
}

// ════════════════════════════════════════════
void setup()
{
    Serial.begin(115200);

    tft.init();
    tft.setRotation(ROTATION);
    initColors();

    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
    tft.fillScreen(C_BG);

    // SPIFFS
    if (!SPIFFS.begin(true))
        Serial.println("SPIFFS mount failed");
    else
    {
        // بررسی عکس‌های ذخیره‌شده قبلی
        for (int i = 0; i < 2; i++)
            imgAvailable[i] = SPIFFS.exists(IMG_PATH[i]);

        Serial.printf("SPIFFS OK  img1=%d img2=%d\n",
                      imgAvailable[0], imgAvailable[1]);
    }

    // TJpg decoder
    TJpgDec.setJpgScale(1);
    TJpgDec.setCallback(tft_output);
    TJpgDec.setSwapBytes(true);

    loadFromFlash();

    startSetupAccessPoint();
    if (rSSID.length() > 0) { connectWiFi(); syncTime(); }
    startWebServer();

    drawUI();

    lastSecondTick  = millis();
    lastApiSync     = millis();
    lastSlideSwitch = millis();

    Serial.println("READY");
}

void loop()
{
    static int lastSec = -1;

    handleSerial();
    webServer.handleClient();
    tickClock();

    // آپدیت ثانیه (فقط وقتی slide نشون نمی‌دیم)
    if (!showingSlide && gSecond != lastSec)
    {
        lastSec = gSecond;
        drawUI();
    }

    updateSlideshow();

    // sync هر ۶ ساعت
    if (millis() - lastApiSync > 21600000UL)
    {
        lastApiSync = millis();
        syncTime();
    }

    wifiConnected = (WiFi.status() == WL_CONNECTED);
    if (!wifiConnected && rSSID.length() > 0 && millis() - lastWiFiRetry >= WIFI_RETRY_MS)
    {
        lastWiFiRetry = millis();
        connectWiFi();
    }

    delay(50);
}