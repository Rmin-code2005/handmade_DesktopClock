# Smart Desktop Clock / ساعت رومیزی هوشمند

[English](#english) | [فارسی](#فارسی)

---

## English

### Overview

Smart Desktop Clock is an ESP32-based desktop clock with a 240×280 TFT display and a companion Windows desktop controller. The ESP device can show the current online time for a selected region and can also display two user-provided images stored on the ESP flash filesystem.

The Windows app lets you connect to the ESP over serial, set Wi-Fi network credentials, choose the region/local time, and upload two slideshow images.

### Main Features

- **Online time display:** the ESP connects to Wi-Fi and syncs time from `timeapi.io` for the configured timezone.
- **Region selection:** the desktop app includes common regions/countries such as Iran, Germany, United States (New York), United Kingdom, Turkey, UAE, Saudi Arabia, India, China, Japan, Russia, Pakistan, and Iraq.
- **Windows configuration app:** a Tkinter UI for selecting the serial port, Wi-Fi SSID/password, region, and images.
- **Two installable ESP images:** upload Image 1 and Image 2 from the Windows app; images are resized to 240×280 JPEG before being sent.
- **Slideshow mode:** uploaded images are saved on SPIFFS and shown periodically on the display.
- **Persistent settings:** Wi-Fi and timezone/city label are saved in ESP flash preferences.

### Repository Structure

```text
.
├── main.py                         # Windows desktop controller app
├── requirments.txt                 # Python dependencies
└── smart_desk_clock/
    ├── platformio.ini              # PlatformIO ESP32 project config
    ├── include/
    │   └── config.h                # ESP clock defaults and API config
    └── src/
        └── main.cpp                # ESP32 firmware
```

### Hardware / Firmware Target

The firmware is configured for:

- ESP32 development board (`esp32dev` in PlatformIO)
- 240×280 ST7789 TFT display
- Arduino framework
- TFT pins defined in `smart_desk_clock/platformio.ini`

Default TFT pin mapping:

| Signal | ESP32 Pin |
| --- | --- |
| MOSI | 23 |
| SCLK | 18 |
| CS | 15 |
| DC | 2 |
| RST | 4 |
| Backlight | 32 |

### ESP Firmware Setup

1. Install [PlatformIO](https://platformio.org/) or the PlatformIO extension for VS Code.
2. Open the `smart_desk_clock` folder as the PlatformIO project.
3. Connect the ESP32 board by USB.
4. Build and upload the firmware:

```bash
cd smart_desk_clock
pio run --target upload
```

5. Open the serial monitor at `115200` baud if you want to see ESP logs:

```bash
pio device monitor
```

### Windows App Setup

1. Install Python 3.11+ on Windows.
2. Install the Python dependencies from the repository root:

```bash
pip install -r requirments.txt
```

3. Run the controller app:

```bash
python main.py
```

### How to Use

1. Flash the ESP firmware.
2. Connect the ESP to the Windows PC over USB.
3. Run `python main.py`.
4. Click **Refresh** and select the ESP serial port.
5. Click **Connect**.
6. Enter your Wi-Fi SSID and password, or use **Auto Fill** on Windows.
7. Click **Send WiFi** so the ESP can connect to the network.
8. Choose a region and click **Send Time**.
9. Browse for Image 1 and Image 2, then click **Send** for each image or **Send Both Images**.
10. The ESP will show the clock and periodically switch to the saved images.

### Serial Protocol

The Windows app talks to the ESP over USB serial at `115200` baud.

| Command | Purpose |
| --- | --- |
| `WIFI:ssid,password` | Send Wi-Fi credentials to the ESP |
| `TIME:hour,minute,second,label` | Set local time and display label |
| `SETTZ:timezone` | Save timezone string on ESP |
| `IMG:index,size` + raw JPEG bytes + `END_IMG` | Upload image 1 or 2 |

### Notes

- Uploaded images are converted by the Windows app to 240×280 JPEG with quality `75`.
- The ESP stores uploaded images as `/img1.jpg` and `/img2.jpg` in SPIFFS.
- The firmware uses `https://timeapi.io/api/Time/current/zone?timeZone=` as the online time API base URL.
- The dependency file is named `requirments.txt` in this repository.

---

## فارسی

### معرفی

ساعت رومیزی هوشمند یک پروژه مبتنی بر ESP32 با نمایشگر TFT سایز 240×280 است که همراه با یک برنامه دسکتاپ ویندوزی ارائه می‌شود. دستگاه ESP می‌تواند ساعت آنلاین منطقه انتخاب‌شده را نمایش دهد و همچنین دو تصویری را که کاربر روی حافظه ESP نصب/آپلود می‌کند، نمایش دهد.

با برنامه ویندوزی می‌توانید از طریق Serial به ESP وصل شوید، منطقه زمانی، SSID و رمز Wi‑Fi، و دو تصویر مخصوص نمایش روی ESP را تنظیم و ارسال کنید.

### قابلیت‌های اصلی

- **نمایش ساعت آنلاین:** ESP به Wi‑Fi وصل می‌شود و زمان را برای منطقه زمانی تنظیم‌شده از `timeapi.io` دریافت می‌کند.
- **انتخاب منطقه:** برنامه دسکتاپ شامل چند کشور/منطقه رایج مثل ایران، آلمان، آمریکا/نیویورک، انگلیس، ترکیه، امارات، عربستان، هند، چین، ژاپن، روسیه، پاکستان و عراق است.
- **برنامه تنظیمات ویندوز:** رابط Tkinter برای انتخاب پورت Serial، وارد کردن SSID/رمز Wi‑Fi، انتخاب منطقه و انتخاب تصاویر.
- **دو تصویر قابل نصب روی ESP:** می‌توانید Image 1 و Image 2 را از برنامه ویندوز انتخاب کنید؛ تصاویر قبل از ارسال به JPEG با اندازه 240×280 تبدیل می‌شوند.
- **حالت اسلایدشو:** تصاویر آپلودشده در SPIFFS ذخیره می‌شوند و به‌صورت دوره‌ای روی نمایشگر نشان داده می‌شوند.
- **ذخیره تنظیمات:** اطلاعات Wi‑Fi و برچسب منطقه/زمان در حافظه ESP ذخیره می‌شود.

### ساختار مخزن

```text
.
├── main.py                         # برنامه کنترلر دسکتاپ ویندوز
├── requirments.txt                 # وابستگی‌های پایتون
└── smart_desk_clock/
    ├── platformio.ini              # تنظیمات پروژه ESP32 در PlatformIO
    ├── include/
    │   └── config.h                # تنظیمات پیش‌فرض ساعت و API
    └── src/
        └── main.cpp                # فریمور ESP32
```

### سخت‌افزار / هدف فریمور

فریمور برای موارد زیر تنظیم شده است:

- برد توسعه ESP32 (`esp32dev` در PlatformIO)
- نمایشگر TFT مدل ST7789 با اندازه 240×280
- فریم‌ورک Arduino
- پایه‌های نمایشگر در فایل `smart_desk_clock/platformio.ini` تعریف شده‌اند

اتصال پیش‌فرض پایه‌های TFT:

| سیگنال | پایه ESP32 |
| --- | --- |
| MOSI | 23 |
| SCLK | 18 |
| CS | 15 |
| DC | 2 |
| RST | 4 |
| Backlight | 32 |

### راه‌اندازی فریمور ESP

1. PlatformIO یا افزونه PlatformIO برای VS Code را نصب کنید.
2. پوشه `smart_desk_clock` را به‌عنوان پروژه PlatformIO باز کنید.
3. برد ESP32 را با USB به سیستم وصل کنید.
4. فریمور را build و upload کنید:

```bash
cd smart_desk_clock
pio run --target upload
```

5. برای دیدن لاگ‌ها، Serial Monitor را با baud rate برابر `115200` باز کنید:

```bash
pio device monitor
```

### راه‌اندازی برنامه ویندوز

1. Python 3.11 یا نسخه جدیدتر را روی ویندوز نصب کنید.
2. وابستگی‌های پایتون را از ریشه مخزن نصب کنید:

```bash
pip install -r requirments.txt
```

3. برنامه کنترلر را اجرا کنید:

```bash
python main.py
```

### روش استفاده

1. ابتدا فریمور را روی ESP آپلود کنید.
2. ESP را با USB به کامپیوتر ویندوزی وصل کنید.
3. دستور `python main.py` را اجرا کنید.
4. روی **Refresh** کلیک کنید و پورت Serial مربوط به ESP را انتخاب کنید.
5. روی **Connect** کلیک کنید.
6. SSID و رمز Wi‑Fi را وارد کنید، یا در ویندوز از **Auto Fill** استفاده کنید.
7. روی **Send WiFi** کلیک کنید تا ESP به شبکه وصل شود.
8. منطقه موردنظر را انتخاب کنید و روی **Send Time** کلیک کنید.
9. برای Image 1 و Image 2 تصویر انتخاب کنید و برای هرکدام **Send** یا برای هر دو **Send Both Images** را بزنید.
10. ESP ساعت را نشان می‌دهد و به‌صورت دوره‌ای تصاویر ذخیره‌شده را نمایش می‌دهد.

### پروتکل Serial

برنامه ویندوز از طریق USB Serial با baud rate برابر `115200` با ESP ارتباط برقرار می‌کند.

| دستور | کاربرد |
| --- | --- |
| `WIFI:ssid,password` | ارسال SSID و رمز Wi‑Fi به ESP |
| `TIME:hour,minute,second,label` | تنظیم ساعت محلی و برچسب نمایشی |
| `SETTZ:timezone` | ذخیره رشته منطقه زمانی روی ESP |
| `IMG:index,size` + raw JPEG bytes + `END_IMG` | آپلود تصویر 1 یا 2 |

### نکات

- تصاویر آپلودشده توسط برنامه ویندوز به JPEG با اندازه 240×280 و کیفیت `75` تبدیل می‌شوند.
- ESP تصاویر را با نام‌های `/img1.jpg` و `/img2.jpg` در SPIFFS ذخیره می‌کند.
- فریمور از آدرس پایه `https://timeapi.io/api/Time/current/zone?timeZone=` برای دریافت ساعت آنلاین استفاده می‌کند.
- نام فایل وابستگی‌ها در این مخزن `requirments.txt` است.
