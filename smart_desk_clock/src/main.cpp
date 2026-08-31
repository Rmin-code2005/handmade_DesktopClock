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

// ============================================================
// Global objects
// ============================================================

Preferences prefs;
TFT_eSPI tft;
WebServer webServer(WEB_PORT);

File webUploadFile;

int webUploadSlot = 0;
size_t webUploadBytes = 0;
bool webUploadFailed = false;


// ============================================================
// ESP Setup Access Point
// ============================================================
//
// This Wi-Fi is always available.
//
// Connect to:
//      armin's watch
//
// Password:
//      12345678
//
// Open:
//      http://192.168.4.1
//

const char* SETUP_AP_SSID = "armin's watch";
const char* SETUP_AP_PASS = "12345678";


// ============================================================
// Colors
// ============================================================

uint16_t C_BG;
uint16_t C_ACCENT;
uint16_t C_WHITE;
uint16_t C_GRAY;
uint16_t C_DARK;
uint16_t C_GREEN;
uint16_t C_LINE;
uint16_t C_AMBIENT;

uint16_t C_RED;
uint16_t C_YELLOW;


// ============================================================
// Wi-Fi state
// ============================================================

bool wifiConnected = false;

// برای تشخیص تغییر وضعیت WiFi
bool lastKnownWiFiState = false;

// آیا صفحه offline الان روی LCD است؟
bool offlineScreenVisible = false;


// ============================================================
// Clock
// ============================================================

int gHour = 0;
int gMinute = 0;
int gSecond = 0;

int gDay = 1;
int gMonth = 1;
int gYear = 2026;

String gDayName = "Loading";
String gCityLabel = "TEHRAN";
String gTimezone = TIME_API_DEFAULT_TZ;


// ============================================================
// Saved router Wi-Fi
// ============================================================

String rSSID = WIFI_SSID;
String rPASS = WIFI_PASS;


// ============================================================
// Timers
// ============================================================

uint32_t lastSecondTick = 0;
uint32_t lastApiSync = 0;
uint32_t lastSlideSwitch = 0;
uint32_t lastWiFiRetry = 0;


// ============================================================
// Slideshow
// ============================================================

bool imgAvailable[2] = {
    false,
    false
};

int currentSlide = -1;

bool showingSlide = false;

uint32_t slideShowTimer = 0;


// هر 5 دقیقه
const uint32_t SLIDE_INTERVAL_MS = 300000UL;

// هر عکس 10 ثانیه
const uint32_t SLIDE_DURATION_MS = 10000UL;


const char* IMG_PATH[2] = {
    "/img1.jpg",
    "/img2.jpg"
};


// ============================================================
// Months
// ============================================================

const char* MONTHS[] =
{
    "",
    "JAN",
    "FEB",
    "MAR",
    "APR",
    "MAY",
    "JUN",
    "JUL",
    "AUG",
    "SEP",
    "OCT",
    "NOV",
    "DEC"
};


// ============================================================
// Offline setup HTML
// ============================================================

const char OFFLINE_SETUP_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">

<head>

<meta charset="UTF-8">

<meta
    name="viewport"
    content="width=device-width,initial-scale=1,maximum-scale=1"
>

<title>Armin's Watch Setup</title>

<style>

* {
    box-sizing: border-box;
}

body {

    margin: 0;

    min-height: 100vh;

    display: flex;
    justify-content: center;
    align-items: center;

    padding: 24px;

    background:
        radial-gradient(
            circle at top,
            #24234a 0,
            #10101a 45%,
            #07070c 100%
        );

    color: #f3f3ff;

    font-family:
        -apple-system,
        BlinkMacSystemFont,
        "Segoe UI",
        Arial,
        sans-serif;
}

.card {

    width: 100%;
    max-width: 390px;

    background: rgba(22, 22, 34, 0.96);

    border: 1px solid #303048;

    border-radius: 24px;

    padding: 28px;

    box-shadow:
        0 25px 70px rgba(0,0,0,.45);
}

.logo {

    width: 52px;
    height: 52px;

    margin: 0 auto 18px auto;

    display: flex;
    align-items: center;
    justify-content: center;

    border-radius: 16px;

    background:
        linear-gradient(
            135deg,
            #6366f1,
            #8b5cf6
        );

    font-size: 25px;
}

h1 {

    margin: 0;

    text-align: center;

    font-size: 23px;
}

.subtitle {

    text-align: center;

    color: #9999ad;

    font-size: 14px;

    margin-top: 8px;
    margin-bottom: 28px;
}

.status {

    padding: 13px 15px;

    margin-bottom: 22px;

    border-radius: 14px;

    background: rgba(239, 68, 68, .08);

    border: 1px solid rgba(239, 68, 68, .20);

    color: #fca5a5;

    font-size: 13px;

    text-align: center;
}

label {

    display: block;

    margin:
        15px 0
        7px 1px;

    font-size: 13px;

    color: #b9b9ca;
}

input {

    width: 100%;

    padding: 14px 15px;

    border-radius: 13px;

    border: 1px solid #35354b;

    background: #101018;

    color: white;

    font-size: 15px;

    outline: none;
}

input:focus {

    border-color: #6366f1;

    box-shadow:
        0 0 0 3px rgba(99,102,241,.13);
}

button {

    width: 100%;

    margin-top: 22px;

    border: 0;

    border-radius: 13px;

    padding: 14px;

    background:
        linear-gradient(
            135deg,
            #6366f1,
            #7c3aed
        );

    color: white;

    font-size: 15px;
    font-weight: 600;

    cursor: pointer;
}

button:disabled {

    opacity: .55;
}

.info {

    margin-top: 24px;

    padding-top: 20px;

    border-top: 1px solid #2d2d40;

    color: #8c8c9f;

    font-size: 12px;

    line-height: 1.7;
}

.info strong {

    color: #dadae9;
}

#message {

    display: none;

    margin-top: 18px;

    padding: 12px;

    border-radius: 12px;

    text-align: center;

    font-size: 13px;
}

.success {

    display: block !important;

    background: rgba(34,197,94,.1);

    border: 1px solid rgba(34,197,94,.25);

    color: #86efac;
}

.error {

    display: block !important;

    background: rgba(239,68,68,.1);

    border: 1px solid rgba(239,68,68,.25);

    color: #fca5a5;
}

</style>

</head>

<body>

<div class="card">

    <div class="logo">
        ⌚
    </div>

    <h1>
        Armin's Watch
    </h1>

    <div class="subtitle">
        Internet setup
    </div>

    <div class="status">
        Internet connection lost
    </div>

    <form id="wifiForm">

        <label>
            WiFi Name
        </label>

        <input
            id="ssid"
            name="ssid"
            type="text"
            maxlength="32"
            placeholder="Enter WiFi name"
            required
        >

        <label>
            Password
        </label>

        <input
            id="password"
            name="password"
            type="password"
            maxlength="64"
            placeholder="Enter WiFi password"
        >

        <button
            id="connectButton"
            type="submit"
        >
            Connect to Internet
        </button>

    </form>

    <div id="message"></div>

    <div class="info">

        Setup network:
        <strong>armin's watch</strong>

        <br>

        Password:
        <strong>12345678</strong>

        <br>

        Local address:
        <strong>192.168.4.1</strong>

    </div>

</div>


<script>

const form =
    document.getElementById("wifiForm");

const button =
    document.getElementById("connectButton");

const message =
    document.getElementById("message");


form.addEventListener(
    "submit",

    async function(event) {

        event.preventDefault();

        const ssid =
            document
                .getElementById("ssid")
                .value
                .trim();

        const password =
            document
                .getElementById("password")
                .value;


        if (!ssid) {

            message.className =
                "error";

            message.textContent =
                "WiFi name is required.";

            return;
        }


        button.disabled = true;

        button.textContent =
            "Connecting...";

        message.className = "";

        message.style.display =
            "none";


        try {

            const data =
                new URLSearchParams();

            data.append(
                "ssid",
                ssid
            );

            data.append(
                "password",
                password
            );


            const response =
                await fetch(
                    "/api/wifi",
                    {
                        method: "POST",

                        headers: {
                            "Content-Type":
                                "application/x-www-form-urlencoded"
                        },

                        body:
                            data.toString()
                    }
                );


            const result =
                await response.json();


            if (!response.ok) {

                throw new Error(
                    result.message ||
                    "Connection failed"
                );
            }


            message.className =
                "success";

            message.textContent =
                result.message;


            setTimeout(
                async () => {

                    try {

                        const status =
                            await fetch(
                                "/api/status",
                                {
                                    cache:
                                        "no-store"
                                }
                            );

                        const json =
                            await status.json();


                        if (
                            json.wifi_connected &&
                            json.sta_ip
                        ) {

                            message.className =
                                "success";

                            message.innerHTML =
                                "Connected successfully.<br>" +
                                "Watch IP: <strong>" +
                                json.sta_ip +
                                "</strong>";

                        }

                    }

                    catch (_) {}

                },

                2500
            );

        }

        catch (error) {

            message.className =
                "error";

            message.textContent =
                error.message;

        }

        finally {

            button.disabled =
                false;

            button.textContent =
                "Connect to Internet";

        }
    }
);

</script>

</body>
</html>
)rawliteral";


// ============================================================
// TJpg callback
// ============================================================

bool tft_output(
    int16_t x,
    int16_t y,
    uint16_t w,
    uint16_t h,
    uint16_t* bitmap
)
{
    if (y >= tft.height())
        return 0;

    tft.pushImage(
        x,
        y,
        w,
        h,
        bitmap
    );

    return 1;
}


// ============================================================
// Flash
// ============================================================

void saveToFlash()
{
    prefs.begin(
        "espctl",
        false
    );

    prefs.putString(
        "ssid",
        rSSID
    );

    prefs.putString(
        "pass",
        rPASS
    );

    prefs.putString(
        "timezone",
        gTimezone
    );

    prefs.putString(
        "city",
        gCityLabel
    );

    prefs.end();

    Serial.println(
        "OK:saved to flash"
    );
}


void loadFromFlash()
{
    prefs.begin(
        "espctl",
        true
    );

    rSSID =
        prefs.getString(
            "ssid",
            WIFI_SSID
        );

    rPASS =
        prefs.getString(
            "pass",
            WIFI_PASS
        );

    gTimezone =
        prefs.getString(
            "timezone",
            TIME_API_DEFAULT_TZ
        );

    gCityLabel =
        prefs.getString(
            "city",
            "TEHRAN"
        );

    prefs.end();


    if (rSSID.length() > 0)
    {
        Serial.printf(
            "Flash loaded: ssid=%s tz=%s\n",
            rSSID.c_str(),
            gTimezone.c_str()
        );
    }
    else
    {
        Serial.println(
            "Flash: no saved WiFi"
        );
    }
}


// ============================================================
// Colors
// ============================================================

void initColors()
{
    C_BG =
        tft.color565(
            10,
            10,
            15
        );

    C_ACCENT =
        tft.color565(
            99,
            102,
            241
        );

    C_WHITE =
        tft.color565(
            241,
            241,
            255
        );

    C_GRAY =
        tft.color565(
            136,
            136,
            136
        );

    C_DARK =
        tft.color565(
            30,
            30,
            46
        );

    C_GREEN =
        tft.color565(
            34,
            197,
            94
        );

    C_LINE =
        tft.color565(
            42,
            42,
            58
        );

    C_AMBIENT =
        tft.color565(
            18,
            14,
            45
        );

    C_RED =
        tft.color565(
            239,
            68,
            68
        );

    C_YELLOW =
        tft.color565(
            250,
            204,
            21
        );
}


// ============================================================
// Setup Access Point
// ============================================================

void startSetupAccessPoint()
{
    WiFi.mode(
        WIFI_AP_STA
    );


    // optional:
    // ثابت کردن IP خود ESP روی 192.168.4.1

    IPAddress localIP(
        192,
        168,
        4,
        1
    );

    IPAddress gateway(
        192,
        168,
        4,
        1
    );

    IPAddress subnet(
        255,
        255,
        255,
        0
    );


    WiFi.softAPConfig(
        localIP,
        gateway,
        subnet
    );


    bool ok =
        WiFi.softAP(
            SETUP_AP_SSID,
            SETUP_AP_PASS
        );


    if (ok)
    {
        Serial.println();
        Serial.println(
            "================================"
        );

        Serial.printf(
            "Setup WiFi: %s\n",
            SETUP_AP_SSID
        );

        Serial.printf(
            "Password: %s\n",
            SETUP_AP_PASS
        );

        Serial.printf(
            "Local IP: %s\n",
            WiFi
                .softAPIP()
                .toString()
                .c_str()
        );

        Serial.println(
            "================================"
        );

        Serial.println();
    }
    else
    {
        Serial.println(
            "ERR: failed to start setup AP"
        );
    }
}


// ============================================================
// Connect router Wi-Fi
// ============================================================

void connectWiFi()
{
    if (rSSID.length() == 0)
    {
        wifiConnected = false;
        return;
    }


    Serial.printf(
        "Connecting to %s\n",
        rSSID.c_str()
    );


    // AP + STA simultaneously
    WiFi.mode(
        WIFI_AP_STA
    );


    WiFi.begin(
        rSSID.c_str(),
        rPASS.c_str()
    );


    uint32_t start =
        millis();


    while (
        WiFi.status() != WL_CONNECTED &&
        millis() - start < WIFI_TIMEOUT_MS
    )
    {
        delay(250);

        webServer.handleClient();

        Serial.print(".");
    }


    wifiConnected =
        (
            WiFi.status() ==
            WL_CONNECTED
        );


    Serial.println();


    if (wifiConnected)
    {
        Serial.println(
            "WiFi Connected"
        );

        Serial.print(
            "Router IP: "
        );

        Serial.println(
            WiFi.localIP()
        );

        Serial.print(
            "Setup AP still available: "
        );

        Serial.println(
            WiFi.softAPIP()
        );
    }
    else
    {
        Serial.println(
            "WiFi Failed"
        );

        Serial.print(
            "Open setup page: http://"
        );

        Serial.println(
            WiFi.softAPIP()
        );
    }
}


// ============================================================
// Time sync
// ============================================================

bool syncTime()
{
    wifiConnected =
        (
            WiFi.status() ==
            WL_CONNECTED
        );


    if (!wifiConnected)
        return false;


    String url =
        String(TIME_API_BASE) +
        gTimezone;


    HTTPClient http;

    http.begin(url);


    int code =
        http.GET();


    if (code != 200)
    {
        Serial.printf(
            "HTTP Error %d\n",
            code
        );

        http.end();

        return false;
    }


    String payload =
        http.getString();


    http.end();


    JsonDocument doc;


    if (
        deserializeJson(
            doc,
            payload
        )
    )
    {
        Serial.println(
            "JSON Error"
        );

        return false;
    }


    gHour =
        doc["hour"] | 0;

    gMinute =
        doc["minute"] | 0;

    gSecond =
        doc["seconds"] | 0;

    gDay =
        doc["day"] | 1;

    gMonth =
        doc["month"] | 1;

    gYear =
        doc["year"] | 2026;


    const char* dayName =
        doc["dayOfWeek"];


    if (dayName)
        gDayName =
            String(dayName);


    Serial.printf(
        "Time Sync OK %02d:%02d:%02d\n",
        gHour,
        gMinute,
        gSecond
    );


    return true;
}


// ============================================================
// Clock tick
// ============================================================

void tickClock()
{
    if (
        millis() -
        lastSecondTick <
        1000
    )
    {
        return;
    }


    lastSecondTick =
        millis();


    gSecond++;


    if (gSecond >= 60)
    {
        gSecond = 0;
        gMinute++;
    }


    if (gMinute >= 60)
    {
        gMinute = 0;
        gHour++;
    }


    if (gHour >= 24)
    {
        gHour = 0;
    }
}


// ============================================================
// Normal clock screen
// ============================================================

void drawUI()
{
    offlineScreenVisible =
        false;


    tft.fillScreen(
        C_BG
    );


    tft.fillCircle(
        SCREEN_WIDTH / 2,
        -10,
        65,
        C_AMBIENT
    );


    tft.setTextDatum(
        MC_DATUM
    );


    tft.setTextSize(1);


    tft.setTextColor(
        C_GRAY,
        C_BG
    );


    String dayShort =
        gDayName.substring(
            0,
            3
        );


    dayShort.toUpperCase();


    tft.drawString(
        dayShort,
        SCREEN_WIDTH / 2,
        58
    );


    char dateBuf[16];


    int month =
        constrain(
            gMonth,
            1,
            12
        );


    sprintf(
        dateBuf,
        "%02d %s %04d",
        gDay,
        MONTHS[month],
        gYear
    );


    tft.setTextColor(
        C_ACCENT,
        C_BG
    );


    tft.drawString(
        dateBuf,
        SCREEN_WIDTH / 2,
        74
    );


    tft.drawFastHLine(
        SCREEN_WIDTH / 2 - 18,
        88,
        36,
        C_LINE
    );


    char timeBuf[9];


    sprintf(
        timeBuf,
        "%02d:%02d:%02d",
        gHour,
        gMinute,
        gSecond
    );


    tft.setTextColor(
        C_WHITE,
        C_BG
    );


    tft.setTextSize(3);


    tft.drawString(
        timeBuf,
        SCREEN_WIDTH / 2,
        148
    );


    int barX =
        SCREEN_WIDTH / 2 - 60;

    int barY =
        178;

    int barW =
        120;

    int barH =
        3;


    tft.fillRoundRect(
        barX,
        barY,
        barW,
        barH,
        1,
        C_DARK
    );


    int fill =
        map(
            gSecond,
            0,
            59,
            0,
            barW
        );


    if (fill > 0)
    {
        tft.fillRoundRect(
            barX,
            barY,
            fill,
            barH,
            1,
            C_ACCENT
        );
    }


    // --------------------------------------------------------
    // Router / hotspot assigned IP
    // --------------------------------------------------------

    tft.setTextSize(1);

    tft.setTextDatum(
        MC_DATUM
    );

    tft.setTextColor(
        C_GRAY,
        C_BG
    );


    String ipLabel =
        wifiConnected
            ? WiFi
                .localIP()
                .toString()
            : String("--");


    tft.drawString(
        ipLabel.c_str(),
        SCREEN_WIDTH / 2,
        237
    );


    // --------------------------------------------------------
    // City / UTC label
    // --------------------------------------------------------

    uint16_t dotColor =
        wifiConnected
            ? C_GREEN
            : C_GRAY;


    tft.fillCircle(
        SCREEN_WIDTH / 2 - 45,
        256,
        4,
        dotColor
    );


    tft.setTextDatum(
        ML_DATUM
    );


    tft.setTextColor(
        C_GRAY,
        C_BG
    );


    tft.drawString(
        gCityLabel.c_str(),
        SCREEN_WIDTH / 2 - 37,
        256
    );
}


// ============================================================
// Offline LCD screen
// ============================================================
//
// When the watch cannot connect to the internet,
// users get everything they need directly on the LCD.
//
// ============================================================

void drawOfflineScreen()
{
    // اگر قبلا رسم شده، دوباره نکش
    if (offlineScreenVisible)
        return;


    offlineScreenVisible =
        true;


    // slideshow باید متوقف شود
    showingSlide =
        false;

    currentSlide =
        -1;


    tft.fillScreen(
        C_BG
    );


    tft.setTextDatum(
        MC_DATUM
    );


    // --------------------------------------------------------
    // Small red status dot
    // --------------------------------------------------------

    tft.fillCircle(
        SCREEN_WIDTH / 2,
        31,
        4,
        C_RED
    );


    // --------------------------------------------------------
    // Internet connection lost
    // --------------------------------------------------------

    tft.setTextSize(1);


    tft.setTextColor(
        C_RED,
        C_BG
    );


    tft.drawString(
        "INTERNET CONNECTION LOST",
        SCREEN_WIDTH / 2,
        52
    );


    tft.drawFastHLine(
        SCREEN_WIDTH / 2 - 34,
        68,
        68,
        C_LINE
    );


    // --------------------------------------------------------
    // Instruction
    // --------------------------------------------------------

    tft.setTextColor(
        C_GRAY,
        C_BG
    );


    tft.drawString(
        "Connect to watch WiFi",
        SCREEN_WIDTH / 2,
        88
    );


    // --------------------------------------------------------
    // Wi-Fi name
    // --------------------------------------------------------

    tft.setTextColor(
        C_GRAY,
        C_BG
    );


    tft.drawString(
        "WiFi Name",
        SCREEN_WIDTH / 2,
        112
    );


    tft.setTextColor(
        C_WHITE,
        C_BG
    );


    tft.setTextSize(2);


    tft.drawString(
        SETUP_AP_SSID,
        SCREEN_WIDTH / 2,
        132
    );


    // --------------------------------------------------------
    // Password
    // --------------------------------------------------------

    tft.setTextSize(1);


    tft.setTextColor(
        C_GRAY,
        C_BG
    );


    tft.drawString(
        "Password",
        SCREEN_WIDTH / 2,
        158
    );


    tft.setTextColor(
        C_WHITE,
        C_BG
    );


    tft.setTextSize(2);


    tft.drawString(
        SETUP_AP_PASS,
        SCREEN_WIDTH / 2,
        178
    );


    // --------------------------------------------------------
    // Browser instruction
    // --------------------------------------------------------

    tft.setTextSize(1);


    tft.setTextColor(
        C_GRAY,
        C_BG
    );


    tft.drawString(
        "Open in browser",
        SCREEN_WIDTH / 2,
        207
    );


    // --------------------------------------------------------
    // Local IP
    // --------------------------------------------------------

    String localIP =
        WiFi
            .softAPIP()
            .toString();


    tft.setTextColor(
        C_GREEN,
        C_BG
    );


    tft.setTextSize(2);


    tft.drawString(
        localIP.c_str(),
        SCREEN_WIDTH / 2,
        231
    );


    tft.setTextSize(1);


    tft.setTextColor(
        C_GRAY,
        C_BG
    );


    tft.drawString(
        "http://192.168.4.1",
        SCREEN_WIDTH / 2,
        256
    );


    Serial.println(
        "Offline screen displayed"
    );
}


// ============================================================
// Slideshow
// ============================================================

void showImage(int idx)
{
    // هنگام قطع اینترنت صفحه setup مهم‌تر است
    if (!wifiConnected)
        return;


    if (!imgAvailable[idx])
        return;


    offlineScreenVisible =
        false;


    tft.fillScreen(
        C_BG
    );


    TJpgDec.drawFsJpg(
        0,
        0,
        IMG_PATH[idx]
    );


    Serial.printf(
        "slide: showing img%d\n",
        idx + 1
    );
}


void updateSlideshow()
{
    // در حالت Offline اسلایدشو اجرا نشود
    if (!wifiConnected)
        return;


    bool anyImg =
        imgAvailable[0] ||
        imgAvailable[1];


    if (!anyImg)
        return;


    uint32_t now =
        millis();


    if (!showingSlide)
    {
        if (
            now -
            lastSlideSwitch >=
            SLIDE_INTERVAL_MS
        )
        {
            for (
                int i = 0;
                i < 2;
                i++
            )
            {
                int next =
                    (
                        currentSlide +
                        1 +
                        i
                    ) % 2;


                if (
                    imgAvailable[next]
                )
                {
                    currentSlide =
                        next;

                    showingSlide =
                        true;

                    slideShowTimer =
                        now;

                    showImage(
                        currentSlide
                    );

                    return;
                }
            }
        }
    }
    else
    {
        if (
            now -
            slideShowTimer >=
            SLIDE_DURATION_MS
        )
        {
            bool nextFound =
                false;


            for (
                int i = 1;
                i <= 2;
                i++
            )
            {
                int next =
                    (
                        currentSlide +
                        i
                    ) % 2;


                if (
                    imgAvailable[next] &&
                    next != currentSlide
                )
                {
                    currentSlide =
                        next;

                    slideShowTimer =
                        now;

                    showImage(
                        currentSlide
                    );

                    nextFound =
                        true;

                    break;
                }
            }


            if (!nextFound)
            {
                showingSlide =
                    false;

                lastSlideSwitch =
                    now;

                drawUI();
            }
        }
    }
}


// ============================================================
// Serial Image Receive
// ============================================================

void receiveImage(
    int idx,
    uint32_t size
)
{
    Serial.printf(
        "OK:ready to receive img%d (%u bytes)\n",
        idx,
        size
    );


    File f =
        SPIFFS.open(
            IMG_PATH[idx - 1],
            FILE_WRITE
        );


    if (!f)
    {
        Serial.println(
            "ERR:spiffs open failed"
        );

        return;
    }


    uint8_t buf[256];

    uint32_t received =
        0;


    uint32_t deadline =
        millis() +
        30000UL;


    while (
        received < size &&
        millis() < deadline
    )
    {
        webServer.handleClient();


        int avail =
            Serial.available();


        if (avail <= 0)
        {
            delay(5);
            continue;
        }


        int toRead =
            min(
                (uint32_t)
                min(
                    avail,
                    (int)sizeof(buf)
                ),

                size - received
            );


        int n =
            Serial.readBytes(
                buf,
                toRead
            );


        f.write(
            buf,
            n
        );


        received +=
            n;
    }


    f.close();


    // END_IMG
    uint32_t t =
        millis() +
        2000;


    while (millis() < t)
    {
        if (Serial.available())
        {
            String tail =
                Serial.readStringUntil(
                    '\n'
                );


            if (
                tail.indexOf(
                    "END_IMG"
                ) >= 0
            )
            {
                break;
            }
        }


        delay(5);
    }


    if (received == size)
    {
        imgAvailable[idx - 1] =
            true;


        Serial.printf(
            "OK:img%d saved (%u bytes)\n",
            idx,
            received
        );


        if (wifiConnected)
        {
            currentSlide =
                idx - 1;

            showingSlide =
                true;

            slideShowTimer =
                millis();

            showImage(
                currentSlide
            );
        }
    }
    else
    {
        Serial.printf(
            "ERR:img%d incomplete %u/%u\n",
            idx,
            received,
            size
        );


        imgAvailable[idx - 1] =
            false;
    }
}


// ============================================================
// Web helpers
// ============================================================

String jsonEscape(
    const String& value
)
{
    String out;

    out.reserve(
        value.length() +
        8
    );


    for (
        size_t i = 0;
        i < value.length();
        i++
    )
    {
        char c =
            value[i];


        if (
            c == '\\' ||
            c == '"'
        )
        {
            out += '\\';
            out += c;
        }

        else if (c == '\n')
        {
            out += "\\n";
        }

        else if (c == '\r')
        {
            out += "\\r";
        }

        else
        {
            out += c;
        }
    }


    return out;
}


void sendJson(
    int code,
    const String& body
)
{
    webServer.sendHeader(
        "Cache-Control",
        "no-store"
    );


    webServer.send(
        code,
        "application/json",
        body
    );
}


// ============================================================
// Root page
// ============================================================

void handleWebRoot()
{
    wifiConnected =
        (
            WiFi.status() ==
            WL_CONNECTED
        );


    // Offline:
    // simple Wi-Fi setup
    if (!wifiConnected)
    {
        webServer.send_P(
            200,
            "text/html; charset=utf-8",
            OFFLINE_SETUP_HTML
        );

        return;
    }


    // Online:
    // original full web panel
    webServer.send_P(
        200,
        "text/html; charset=utf-8",
        WEB_UI_HTML
    );
}


// ============================================================
// Web status
// ============================================================

void handleWebStatus()
{
    wifiConnected =
        (
            WiFi.status() ==
            WL_CONNECTED
        );


    String body =
        "{";


    body +=
        "\"wifi_connected\":" +
        String(
            wifiConnected
                ? "true"
                : "false"
        );


    body +=
        ",\"ssid\":\"" +
        jsonEscape(rSSID) +
        "\"";


    body +=
        ",\"sta_ip\":\"" +
        jsonEscape(
            wifiConnected
                ? WiFi
                    .localIP()
                    .toString()
                : String("")
        ) +
        "\"";


    body +=
        ",\"ap_ssid\":\"" +
        jsonEscape(
            String(
                SETUP_AP_SSID
            )
        ) +
        "\"";


    body +=
        ",\"ap_ip\":\"" +
        jsonEscape(
            WiFi
                .softAPIP()
                .toString()
        ) +
        "\"";


    body +=
        ",\"timezone\":\"" +
        jsonEscape(
            gTimezone
        ) +
        "\"";


    body +=
        ",\"city_label\":\"" +
        jsonEscape(
            gCityLabel
        ) +
        "\"";


    body +=
        ",\"img1\":" +
        String(
            imgAvailable[0]
                ? "true"
                : "false"
        );


    body +=
        ",\"img2\":" +
        String(
            imgAvailable[1]
                ? "true"
                : "false"
        );


    body +=
        ",\"time\":\"" +
        String(
            gHour < 10
                ? "0"
                : ""
        ) +
        String(gHour) +
        ":" +
        String(
            gMinute < 10
                ? "0"
                : ""
        ) +
        String(gMinute) +
        ":" +
        String(
            gSecond < 10
                ? "0"
                : ""
        ) +
        String(gSecond) +
        "\"";


    body +=
        "}";


    sendJson(
        200,
        body
    );
}


// ============================================================
// Change Wi-Fi from web
// ============================================================

void handleWebWifi()
{
    if (
        !webServer.hasArg(
            "ssid"
        )
    )
    {
        sendJson(
            400,
            "{\"message\":\"SSID is required\"}"
        );

        return;
    }


    String newSsid =
        webServer.arg(
            "ssid"
        );


    newSsid.trim();


    if (
        newSsid.length() == 0 ||
        newSsid.length() > 32
    )
    {
        sendJson(
            400,
            "{\"message\":\"Invalid SSID\"}"
        );

        return;
    }


    String newPass =
        webServer.hasArg(
            "password"
        )
            ? webServer.arg(
                "password"
            )
            : String("");


    // WPA/WPA2 maximum password size
    if (
        newPass.length() > 64
    )
    {
        sendJson(
            400,
            "{\"message\":\"Password is too long\"}"
        );

        return;
    }


    // ذخیره credentials
    rSSID =
        newSsid;

    rPASS =
        newPass;


    saveToFlash();


    // پاسخ را قبل از reconnect بده
    // تا مرورگر timeout نشود.
    sendJson(
        200,
        "{\"message\":\"WiFi settings saved. Watch is connecting now...\"}"
    );


    delay(150);


    WiFi.disconnect(
        false,
        false
    );


    delay(150);


    wifiConnected =
        false;


    offlineScreenVisible =
        false;


    drawOfflineScreen();


    // تلاش اتصال
    connectWiFi();


    if (wifiConnected)
    {
        syncTime();

        offlineScreenVisible =
            false;

        drawUI();


        Serial.println(
            "New WiFi connection successful"
        );
    }
    else
    {
        offlineScreenVisible =
            false;

        drawOfflineScreen();


        Serial.println(
            "New WiFi connection failed"
        );
    }
}


// ============================================================
// Timezone
// ============================================================

void handleWebTimezone()
{
    if (
        !webServer.hasArg(
            "timezone"
        )
    )
    {
        sendJson(
            400,
            "{\"message\":\"timezone is required\"}"
        );

        return;
    }


    String tz =
        webServer.arg(
            "timezone"
        );


    tz.trim();


    if (
        tz.length() == 0 ||
        tz.length() > 64
    )
    {
        sendJson(
            400,
            "{\"message\":\"Invalid timezone\"}"
        );

        return;
    }


    gTimezone =
        tz;


    if (
        webServer.hasArg(
            "label"
        )
    )
    {
        gCityLabel =
            webServer.arg(
                "label"
            );


        gCityLabel.trim();


        if (
            gCityLabel.length() >
            20
        )
        {
            gCityLabel =
                gCityLabel.substring(
                    0,
                    20
                );
        }
    }


    if (
        webServer.hasArg(
            "hour"
        ) &&
        webServer.hasArg(
            "minute"
        ) &&
        webServer.hasArg(
            "second"
        )
    )
    {
        gHour =
            constrain(
                webServer
                    .arg("hour")
                    .toInt(),
                0,
                23
            );


        gMinute =
            constrain(
                webServer
                    .arg("minute")
                    .toInt(),
                0,
                59
            );


        gSecond =
            constrain(
                webServer
                    .arg("second")
                    .toInt(),
                0,
                59
            );


        lastSecondTick =
            millis();
    }


    saveToFlash();


    bool synced =
        wifiConnected &&
        syncTime();


    if (
        !showingSlide &&
        wifiConnected
    )
    {
        drawUI();
    }


    sendJson(
        200,

        synced

            ? "{\"message\":\"Region saved and online time synced\"}"

            : "{\"message\":\"Region saved; browser local time applied\"}"
    );
}


// ============================================================
// Manual sync
// ============================================================

void handleWebSync()
{
    wifiConnected =
        (
            WiFi.status() ==
            WL_CONNECTED
        );


    if (!wifiConnected)
    {
        sendJson(
            409,
            "{\"message\":\"Wi-Fi is not connected\"}"
        );

        return;
    }


    if (!syncTime())
    {
        sendJson(
            502,
            "{\"message\":\"Online time sync failed\"}"
        );

        return;
    }


    if (!showingSlide)
    {
        drawUI();
    }


    sendJson(
        200,
        "{\"message\":\"Time synced successfully\"}"
    );
}


// ============================================================
// Image web upload
// ============================================================

void handleWebImageUpload(
    int slot
)
{
    HTTPUpload& upload =
        webServer.upload();


    const char* path =
        IMG_PATH[slot - 1];


    if (
        upload.status ==
        UPLOAD_FILE_START
    )
    {
        webUploadSlot =
            slot;

        webUploadBytes =
            0;

        webUploadFailed =
            false;


        if (
            SPIFFS.exists(
                path
            )
        )
        {
            SPIFFS.remove(
                path
            );
        }


        webUploadFile =
            SPIFFS.open(
                path,
                FILE_WRITE
            );


        if (!webUploadFile)
        {
            webUploadFailed =
                true;
        }
    }


    else if (
        upload.status ==
        UPLOAD_FILE_WRITE
    )
    {
        if (
            webUploadSlot != slot ||
            webUploadFailed
        )
        {
            return;
        }


        if (
            webUploadBytes +
            upload.currentSize >
            WEB_MAX_IMAGE_BYTES
        )
        {
            webUploadFailed =
                true;


            if (webUploadFile)
            {
                webUploadFile.close();
            }


            SPIFFS.remove(
                path
            );


            return;
        }


        if (
            !webUploadFile ||
            webUploadFile.write(
                upload.buf,
                upload.currentSize
            ) != upload.currentSize
        )
        {
            webUploadFailed =
                true;


            if (webUploadFile)
            {
                webUploadFile.close();
            }


            SPIFFS.remove(
                path
            );


            return;
        }


        webUploadBytes +=
            upload.currentSize;
    }


    else if (
        upload.status ==
        UPLOAD_FILE_END
    )
    {
        if (webUploadFile)
        {
            webUploadFile.close();
        }


        if (
            !webUploadFailed &&
            webUploadBytes > 2
        )
        {
            File check =
                SPIFFS.open(
                    path,
                    FILE_READ
                );


            bool jpeg =
                false;


            if (check)
            {
                int a =
                    check.read();

                int b =
                    check.read();


                jpeg =
                    (
                        a == 0xFF &&
                        b == 0xD8
                    );


                check.close();
            }


            if (!jpeg)
            {
                webUploadFailed =
                    true;

                SPIFFS.remove(
                    path
                );
            }
        }
        else
        {
            webUploadFailed =
                true;

            SPIFFS.remove(
                path
            );
        }


        imgAvailable[slot - 1] =
            !webUploadFailed;


        if (
            imgAvailable[slot - 1] &&
            wifiConnected
        )
        {
            currentSlide =
                slot - 1;

            showingSlide =
                true;

            slideShowTimer =
                millis();

            showImage(
                currentSlide
            );
        }
    }


    else if (
        upload.status ==
        UPLOAD_FILE_ABORTED
    )
    {
        webUploadFailed =
            true;


        if (webUploadFile)
        {
            webUploadFile.close();
        }


        SPIFFS.remove(
            path
        );


        imgAvailable[slot - 1] =
            false;
    }
}


void finishWebImageUpload(
    int slot
)
{
    if (
        webUploadSlot != slot ||
        webUploadFailed ||
        !imgAvailable[slot - 1]
    )
    {
        sendJson(
            400,
            "{\"message\":\"Image upload failed or exceeded 256 KB\"}"
        );

        return;
    }


    sendJson(
        200,
        "{\"message\":\"Image saved to ESP flash\"}"
    );
}


// ============================================================
// Delete image
// ============================================================

void handleDeleteWebImage(
    int slot
)
{
    const char* path =
        IMG_PATH[slot - 1];


    if (
        SPIFFS.exists(
            path
        )
    )
    {
        SPIFFS.remove(
            path
        );
    }


    imgAvailable[slot - 1] =
        false;


    if (
        showingSlide &&
        currentSlide ==
            slot - 1
    )
    {
        showingSlide =
            false;

        currentSlide =
            -1;

        lastSlideSwitch =
            millis();


        if (wifiConnected)
        {
            drawUI();
        }
        else
        {
            offlineScreenVisible =
                false;

            drawOfflineScreen();
        }
    }


    sendJson(
        200,
        "{\"message\":\"Image deleted\"}"
    );
}


// ============================================================
// Get image
// ============================================================

void handleGetWebImage(
    int slot
)
{
    const char* path =
        IMG_PATH[slot - 1];


    if (
        !SPIFFS.exists(
            path
        )
    )
    {
        webServer.send(
            404,
            "text/plain",
            "Not found"
        );

        return;
    }


    File f =
        SPIFFS.open(
            path,
            FILE_READ
        );


    webServer.sendHeader(
        "Cache-Control",
        "no-store"
    );


    webServer.streamFile(
        f,
        "image/jpeg"
    );


    f.close();
}


// ============================================================
// Restart
// ============================================================

void handleWebRestart()
{
    sendJson(
        200,
        "{\"message\":\"ESP restarting\"}"
    );


    delay(250);


    ESP.restart();
}


// ============================================================
// Erase flash
// ============================================================

void handleWebEraseStorage()
{
    prefs.begin(
        "espctl",
        false
    );


    prefs.clear();


    prefs.end();


    SPIFFS.format();


    imgAvailable[0] =
        false;

    imgAvailable[1] =
        false;


    sendJson(
        200,
        "{\"message\":\"Flash storage erased; ESP restarting\"}"
    );


    delay(300);


    ESP.restart();
}


// ============================================================
// Web Server
// ============================================================

void startWebServer()
{
    // Root:
    // offline -> simple setup
    // online  -> full web UI
    webServer.on(
        "/",
        HTTP_GET,
        handleWebRoot
    );


    webServer.on(
        "/api/status",
        HTTP_GET,
        handleWebStatus
    );


    webServer.on(
        "/api/wifi",
        HTTP_POST,
        handleWebWifi
    );


    webServer.on(
        "/api/timezone",
        HTTP_POST,
        handleWebTimezone
    );


    webServer.on(
        "/api/sync",
        HTTP_POST,
        handleWebSync
    );


    webServer.on(
        "/api/image/1",
        HTTP_POST,

        []()
        {
            finishWebImageUpload(
                1
            );
        },

        []()
        {
            handleWebImageUpload(
                1
            );
        }
    );


    webServer.on(
        "/api/image/2",
        HTTP_POST,

        []()
        {
            finishWebImageUpload(
                2
            );
        },

        []()
        {
            handleWebImageUpload(
                2
            );
        }
    );


    webServer.on(
        "/api/image/1",
        HTTP_DELETE,

        []()
        {
            handleDeleteWebImage(
                1
            );
        }
    );


    webServer.on(
        "/api/image/2",
        HTTP_DELETE,

        []()
        {
            handleDeleteWebImage(
                2
            );
        }
    );


    webServer.on(
        "/image/1",
        HTTP_GET,

        []()
        {
            handleGetWebImage(
                1
            );
        }
    );


    webServer.on(
        "/image/2",
        HTTP_GET,

        []()
        {
            handleGetWebImage(
                2
            );
        }
    );


    webServer.on(
        "/api/restart",
        HTTP_POST,
        handleWebRestart
    );


    webServer.on(
        "/api/erase-storage",
        HTTP_POST,
        handleWebEraseStorage
    );


    webServer.onNotFound(
        []()
        {
            // وقتی Offline هستیم،
            // حتی اگر کاربر آدرس اشتباه زد،
            // صفحه setup را نشان بده.
            if (
                WiFi.status() !=
                WL_CONNECTED
            )
            {
                webServer.sendHeader(
                    "Location",
                    "/",
                    true
                );


                webServer.send(
                    302,
                    "text/plain",
                    ""
                );


                return;
            }


            webServer.send(
                404,
                "application/json",
                "{\"message\":\"Not found\"}"
            );
        }
    );


    webServer.begin();


    Serial.println(
        "Web server started on port 80"
    );
}


// ============================================================
// Serial parser
// ============================================================

void handleSerial()
{
    if (!Serial.available())
        return;


    String line =
        Serial.readStringUntil(
            '\n'
        );


    line.trim();


    // --------------------------------------------------------
    // WIFI
    // --------------------------------------------------------

    if (
        line.startsWith(
            "WIFI:"
        )
    )
    {
        String data =
            line.substring(
                5
            );


        int comma =
            data.indexOf(
                ','
            );


        if (comma < 0)
        {
            Serial.println(
                "ERR:bad WIFI format"
            );

            return;
        }


        rSSID =
            data.substring(
                0,
                comma
            );


        rPASS =
            data.substring(
                comma + 1
            );


        Serial.printf(
            "OK:WIFI ssid=%s\n",
            rSSID.c_str()
        );


        // ذخیره حتی قبل از اتصال
        saveToFlash();


        WiFi.disconnect(
            false,
            false
        );


        delay(300);


        wifiConnected =
            false;


        offlineScreenVisible =
            false;


        drawOfflineScreen();


        connectWiFi();


        if (wifiConnected)
        {
            syncTime();

            drawUI();
        }
        else
        {
            offlineScreenVisible =
                false;

            drawOfflineScreen();
        }


        return;
    }


    // --------------------------------------------------------
    // TIME
    // --------------------------------------------------------

    if (
        line.startsWith(
            "TIME:"
        )
    )
    {
        String data =
            line.substring(
                5
            );


        int c1 =
            data.indexOf(
                ','
            );

        int c2 =
            data.indexOf(
                ',',
                c1 + 1
            );

        int c3 =
            data.indexOf(
                ',',
                c2 + 1
            );


        if (
            c1 < 0 ||
            c2 < 0 ||
            c3 < 0
        )
        {
            Serial.println(
                "ERR:bad TIME format"
            );

            return;
        }


        gHour =
            data.substring(
                0,
                c1
            ).toInt();


        gMinute =
            data.substring(
                c1 + 1,
                c2
            ).toInt();


        gSecond =
            data.substring(
                c2 + 1,
                c3
            ).toInt();


        gCityLabel =
            data.substring(
                c3 + 1
            );


        gCityLabel.trim();


        lastSecondTick =
            millis();


        Serial.printf(
            "OK:TIME %02d:%02d:%02d label=%s\n",
            gHour,
            gMinute,
            gSecond,
            gCityLabel.c_str()
        );


        saveToFlash();


        if (
            wifiConnected &&
            !showingSlide
        )
        {
            drawUI();
        }


        return;
    }


    // --------------------------------------------------------
    // SETTZ
    // --------------------------------------------------------

    if (
        line.startsWith(
            "SETTZ:"
        )
    )
    {
        gTimezone =
            line.substring(
                6
            );


        gTimezone.trim();


        Serial.printf(
            "OK:timezone=%s\n",
            gTimezone.c_str()
        );


        saveToFlash();


        return;
    }


    // --------------------------------------------------------
    // IMG
    // --------------------------------------------------------

    if (
        line.startsWith(
            "IMG:"
        )
    )
    {
        String data =
            line.substring(
                4
            );


        int comma =
            data.indexOf(
                ','
            );


        if (comma < 0)
        {
            Serial.println(
                "ERR:bad IMG format"
            );

            return;
        }


        int idx =
            data.substring(
                0,
                comma
            ).toInt();


        uint32_t size =
            data.substring(
                comma + 1
            ).toInt();


        if (
            idx < 1 ||
            idx > 2 ||
            size == 0
        )
        {
            Serial.println(
                "ERR:invalid IMG params"
            );

            return;
        }


        receiveImage(
            idx,
            size
        );


        return;
    }


    Serial.println(
        "ERR:unknown command"
    );
}


// ============================================================
// Setup
// ============================================================

void setup()
{
    Serial.begin(
        115200
    );


    // --------------------------------------------------------
    // LCD
    // --------------------------------------------------------

    tft.init();


    tft.setRotation(
        ROTATION
    );


    initColors();


    pinMode(
        TFT_BL,
        OUTPUT
    );


    digitalWrite(
        TFT_BL,
        HIGH
    );


    tft.fillScreen(
        C_BG
    );


    // --------------------------------------------------------
    // SPIFFS
    // --------------------------------------------------------

    if (
        !SPIFFS.begin(
            true
        )
    )
    {
        Serial.println(
            "SPIFFS mount failed"
        );
    }
    else
    {
        for (
            int i = 0;
            i < 2;
            i++
        )
        {
            imgAvailable[i] =
                SPIFFS.exists(
                    IMG_PATH[i]
                );
        }


        Serial.printf(
            "SPIFFS OK img1=%d img2=%d\n",
            imgAvailable[0],
            imgAvailable[1]
        );
    }


    // --------------------------------------------------------
    // JPEG
    // --------------------------------------------------------

    TJpgDec.setJpgScale(
        1
    );


    TJpgDec.setCallback(
        tft_output
    );


    TJpgDec.setSwapBytes(
        true
    );


    // --------------------------------------------------------
    // Load settings
    // --------------------------------------------------------

    loadFromFlash();


    // --------------------------------------------------------
    // IMPORTANT:
    // Start ESP Access Point FIRST.
    // Therefore setup page is ALWAYS available.
    // --------------------------------------------------------

    startSetupAccessPoint();


    // --------------------------------------------------------
    // Start web server BEFORE trying router Wi-Fi
    // --------------------------------------------------------

    startWebServer();


    // --------------------------------------------------------
    // Try saved router Wi-Fi
    // --------------------------------------------------------

    if (
        rSSID.length() >
        0
    )
    {
        connectWiFi();


        if (wifiConnected)
        {
            syncTime();
        }
    }


    // --------------------------------------------------------
    // Initial screen
    // --------------------------------------------------------

    wifiConnected =
        (
            WiFi.status() ==
            WL_CONNECTED
        );


    lastKnownWiFiState =
        wifiConnected;


    if (wifiConnected)
    {
        drawUI();
    }
    else
    {
        drawOfflineScreen();
    }


    lastSecondTick =
        millis();


    lastApiSync =
        millis();


    lastSlideSwitch =
        millis();


    lastWiFiRetry =
        millis();


    Serial.println();
    Serial.println(
        "READY"
    );

    Serial.println();
    Serial.println(
        "Watch setup WiFi is always available:"
    );

    Serial.printf(
        "SSID: %s\n",
        SETUP_AP_SSID
    );

    Serial.printf(
        "PASS: %s\n",
        SETUP_AP_PASS
    );

    Serial.printf(
        "OPEN: http://%s\n",
        WiFi
            .softAPIP()
            .toString()
            .c_str()
    );
}


// ============================================================
// Loop
// ============================================================

void loop()
{
    static int lastSec =
        -1;


    // --------------------------------------------------------
    // Serial
    // --------------------------------------------------------

    handleSerial();


    // --------------------------------------------------------
    // Web server
    // --------------------------------------------------------

    webServer.handleClient();


    // --------------------------------------------------------
    // Local clock
    // --------------------------------------------------------

    tickClock();


    // --------------------------------------------------------
    // Current Wi-Fi state
    // --------------------------------------------------------

    bool currentWiFiState =
        (
            WiFi.status() ==
            WL_CONNECTED
        );


    wifiConnected =
        currentWiFiState;


    // --------------------------------------------------------
    // Detect connection state changes
    // --------------------------------------------------------

    if (
        currentWiFiState !=
        lastKnownWiFiState
    )
    {
        lastKnownWiFiState =
            currentWiFiState;


        // ---------------------------
        // Connected
        // ---------------------------

        if (currentWiFiState)
        {
            Serial.println();
            Serial.println(
                "Internet connection restored"
            );


            Serial.print(
                "Network IP: "
            );


            Serial.println(
                WiFi.localIP()
            );


            offlineScreenVisible =
                false;


            showingSlide =
                false;


            currentSlide =
                -1;


            syncTime();


            drawUI();


            lastSec =
                gSecond;
        }


        // ---------------------------
        // Connection lost
        // ---------------------------

        else
        {
            Serial.println();
            Serial.println(
                "Internet connection lost"
            );


            Serial.printf(
                "Connect to: %s\n",
                SETUP_AP_SSID
            );


            Serial.printf(
                "Password: %s\n",
                SETUP_AP_PASS
            );


            Serial.printf(
                "Open: http://%s\n",
                WiFi
                    .softAPIP()
                    .toString()
                    .c_str()
            );


            showingSlide =
                false;


            currentSlide =
                -1;


            offlineScreenVisible =
                false;


            drawOfflineScreen();
        }
    }


    // --------------------------------------------------------
    // LCD
    // --------------------------------------------------------

    if (wifiConnected)
    {
        // فقط صفحه ساعت ثانیه‌ای refresh شود
        if (
            !showingSlide &&
            gSecond != lastSec
        )
        {
            lastSec =
                gSecond;


            drawUI();
        }


        updateSlideshow();
    }
    else
    {
        // صفحه offline فقط یک بار رسم می‌شود
        drawOfflineScreen();
    }


    // --------------------------------------------------------
    // Sync every 6 hours
    // --------------------------------------------------------

    if (
        wifiConnected &&
        millis() -
        lastApiSync >
        21600000UL
    )
    {
        lastApiSync =
            millis();


        syncTime();
    }


    // --------------------------------------------------------
    // Auto retry saved Wi-Fi
    // --------------------------------------------------------

    if (
        !wifiConnected &&
        rSSID.length() > 0 &&
        millis() -
        lastWiFiRetry >=
        WIFI_RETRY_MS
    )
    {
        lastWiFiRetry =
            millis();


        Serial.println(
            "Retrying saved WiFi..."
        );


        connectWiFi();


        wifiConnected =
            (
                WiFi.status() ==
                WL_CONNECTED
            );


        if (wifiConnected)
        {
            lastKnownWiFiState =
                true;


            syncTime();


            offlineScreenVisible =
                false;


            drawUI();


            lastSec =
                gSecond;
        }
        else
        {
            lastKnownWiFiState =
                false;


            offlineScreenVisible =
                false;


            drawOfflineScreen();
        }
    }


    delay(50);
}