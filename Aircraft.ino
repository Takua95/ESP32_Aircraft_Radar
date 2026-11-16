
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <esp_sleep.h>

#define MAX_PLANES 32
#define TRAIL_POINTS 5
#define DARKGREY 0x7BEF

extern Adafruit_ST7789 tft;

const char* ssid     = "ATTYSwThfR";
const char* password = "wttpvirm=yt3";

unsigned long lastDebugPrint = 0;

int brightnessLevel = 4;

const uint8_t brightnessTable[] =
{
    0,    // off
    25,   // very dim
    64,   // dim
    128,  // medium
    255   // full
};

unsigned long screenButtonStart = 0;

struct TrailPoint
{
    float bearing;
    float distance;
};

struct AircraftHistory
{
    String callsign;
    TrailPoint trail[TRAIL_POINTS];
};

AircraftHistory history[MAX_PLANES];

struct Aircraft
{
    String callsign;

    float lat;
    float lon;

    float distance;
    float bearing;

    int altitudeFt;
    int speedMph;
    int heading;

    TrailPoint trail[TRAIL_POINTS];
};

Aircraft planes[MAX_PLANES];
int planeCount = 0;

const float HOME_LAT = 32.677919;
const float HOME_LON = -96.654376;

const float BOX_LAT = 0.1448;
const float BOX_LON = 0.1724;

const float RADAR_RANGE = 10.0f;

//==================================================

void updateProgressBar()
{
    float pct =
        getUpdatePercent();

    int width =
        (int)(pct * 130);

    tft.fillRect(
        2,
        213,
        130,
        4,
        ST77XX_BLACK);

    tft.drawRect(
        2,
        213,
        130,
        4,
        DARKGREY);

    tft.fillRect(
        2,
        213,
        width,
        4,
        ST77XX_GREEN);
}

//==================================================

AircraftHistory* findHistory(
    const String& callsign)
{
    for(int i = 0;
        i < MAX_PLANES;
        i++)
    {
        if(history[i].callsign ==
           callsign)
        {
            return &history[i];
        }
    }

    for(int i = 0;
        i < MAX_PLANES;
        i++)
    {
        if(history[i].callsign ==
           "")
        {
            history[i].callsign =
                callsign;

            return &history[i];
        }
    }

    return nullptr;
}

//==================================================

float degToRad(float deg)
{
    return deg * PI / 180.0;
}

//==================================================

float getDistanceMiles(
    float lat1,
    float lon1,
    float lat2,
    float lon2)
{
    const float R = 3958.8;

    float dLat =
        degToRad(lat2 - lat1);

    float dLon =
        degToRad(lon2 - lon1);

    float a =
        sin(dLat / 2) *
        sin(dLat / 2)
        +
        cos(degToRad(lat1))
        *
        cos(degToRad(lat2))
        *
        sin(dLon / 2)
        *
        sin(dLon / 2);

    float c =
        2 *
        atan2(
            sqrt(a),
            sqrt(1 - a));

    return R * c;
}

//==================================================

float getBearing(
    float lat1,
    float lon1,
    float lat2,
    float lon2)
{
    float dLon =
        degToRad(
            lon2 - lon1);

    float y =
        sin(dLon) *
        cos(
            degToRad(lat2));

    float x =
        cos(
            degToRad(lat1))
        *
        sin(
            degToRad(lat2))
        -
        sin(
            degToRad(lat1))
        *
        cos(
            degToRad(lat2))
        *
        cos(dLon);

    float b =
        atan2(y, x)
        *
        180.0
        /
        PI;

    if (b < 0)
        b += 360;

    return b;
}

//==================================================

String headingText(int deg)
{
    deg %= 360;

    if (deg < 23)  return "N";
    if (deg < 68)  return "NE";
    if (deg < 113) return "E";
    if (deg < 158) return "SE";
    if (deg < 203) return "S";
    if (deg < 248) return "SW";
    if (deg < 293) return "W";
    if (deg < 338) return "NW";

    return "N";
}

//==================================================

void sortPlanes()
{
    for (int i = 0;
         i < planeCount - 1;
         i++)
    {
        for (int j = i + 1;
             j < planeCount;
             j++)
        {
            if (planes[j].distance <
                planes[i].distance)
            {
                Aircraft tmp =
                    planes[i];

                planes[i] =
                    planes[j];

                planes[j] =
                    tmp;
            }
        }
    }
}

//==================================================

void fetchAircraft()
{
    planeCount = 0;

    if (WiFi.status() !=
        WL_CONNECTED)
        return;

    String url =
        "https://opensky-network.org/api/states/all?"
        "lamin=" +
        String(
            HOME_LAT - BOX_LAT,
            6)
        +
        "&lomin=" +
        String(
            HOME_LON - BOX_LON,
            6)
        +
        "&lamax=" +
        String(
            HOME_LAT + BOX_LAT,
            6)
        +
        "&lomax=" +
        String(
            HOME_LON + BOX_LON,
            6);

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;

    if (!http.begin(
            client,
            url))
    {
        return;
    }

    int code =
        http.GET();

    Serial.print("HTTP=");
    Serial.println(code);

    if (code != 200)
    {
        http.end();
        return;
    }

    String payload =
        http.getString();

    Serial.print("Payload=");
    Serial.println(payload.length());

    http.end();

    JsonDocument doc;

    auto err =
        deserializeJson(
            doc,
            payload);

    if (err)
        return;

    JsonArray states =
        doc["states"]
        .as<JsonArray>();

    for (JsonVariant v :
         states)
    {
        JsonArray s =
            v.as<JsonArray>();

        if (s.isNull())
            continue;

        if (s.size() < 11)
            continue;

        if (s[5].isNull())
            continue;

        if (s[6].isNull())
            continue;

        if (planeCount >=
            MAX_PLANES)
            break;

        float lon =
            s[5]
            .as<float>();

        float lat =
            s[6]
            .as<float>();

        Aircraft &p =
            planes[
                planeCount];

        p.lat = lat;
        p.lon = lon;

        p.distance =
            getDistanceMiles(
                HOME_LAT,
                HOME_LON,
                lat,
                lon);

        p.bearing =
            getBearing(
                HOME_LAT,
                HOME_LON,
                lat,
                lon);

        p.callsign =
            s[1].isNull()
            ? "UNKNOWN"
            : s[1]
                .as<String>();

        AircraftHistory* h =
            findHistory(
                p.callsign);
        
        if(h != nullptr)
        {
            for(int t =
                    TRAIL_POINTS - 1;
                t > 0;
                t--)
            {
                h->trail[t] =
                    h->trail[t - 1];
            }

            h->trail[0].distance =
                p.distance;

            h->trail[0].bearing =
                p.bearing;
        }

        p.callsign.trim();

        if (p.callsign.length()
            == 0)
        {
            p.callsign =
                "UNKNOWN";
        }

        p.altitudeFt =
            s[7].isNull()
            ? 0
            : (int)(
                s[7]
                .as<float>()
                *
                3.28084);

        p.speedMph =
            s[9].isNull()
            ? 0
            : (int)(
                s[9]
                .as<float>()
                *
                2.23694);

        p.heading =
            s[10].isNull()
            ? 0
            : (int)(
                s[10]
                .as<float>());

        planeCount++;
    }

    sortPlanes();

    Serial.print("PlaneCount=");
    Serial.println(planeCount);
}

//==================================================

enum ViewMode
{
    LIST_VIEW,
    RADAR_VIEW
};

extern ViewMode currentView;

//==================================================

char headingArrow(int hdg)
{
    hdg %= 360;

    if(hdg < 23)  return '^';
    if(hdg < 68)  return '/';
    if(hdg < 113) return '>';
    if(hdg < 158) return '\\';
    if(hdg < 203) return 'v';
    if(hdg < 248) return '/';
    if(hdg < 293) return '<';
    if(hdg < 338) return '\\';

    return '^';
}

//==================================================

void drawText(
    const String& s,
    int x,
    int y,
    uint16_t color,
    uint8_t size)
{
    tft.setTextColor(color);
    tft.setTextSize(size);
    tft.setCursor(x, y);
    tft.print(s);
}

//==================================================

int getBatteryPercent()
{
    float v = getBatteryVoltage();

    if(v >= 4.20) return 100;
    if(v <= 3.30) return 0;

    return (int)(
        (v - 3.30) *
        100.0 /
        (4.20 - 3.30));
}

void drawBattery(
    int x,
    int y)
{
    int pct =
        getBatteryPercent();

    uint16_t color;

    if(pct > 60)
        color = ST77XX_GREEN;
    else if(pct > 25)
        color = ST77XX_YELLOW;
    else
        color = ST77XX_RED;

    tft.drawRect(
        x,
        y,
        16,
        8,
        color);

    tft.fillRect(
        x + 16,
        y + 2,
        2,
        4,
        color);

    int fillWidth =
        map(
            pct,
            0,
            100,
            0,
            14);

    tft.fillRect(
        x + 1,
        y + 1,
        fillWidth,
        6,
        color);
}

//==================================================
// LIST VIEW
//==================================================

void drawListScreen()
{
    tft.fillScreen(ST77XX_BLACK);

    drawText(
        "FLIGHTS",
        2,
        2,
        ST77XX_CYAN,
        1);

    drawText(
        String(planeCount),
        84,
        2,
        ST77XX_YELLOW,
        1);

    drawBattery(
        112,
        2);
        
    tft.drawFastHLine(
        0,
        14,
        135,
        DARKGREY);

    int shown =
        min(5, planeCount);

    for(int i = 0;
        i < shown;
        i++)
    {
        int y =
            20 +
            (i * 40);

        drawText(
            planes[i].callsign,
            2,
            y,
            ST77XX_YELLOW,
            1);

        drawText(
            String(
                planes[i].distance,
                1)
            + "mi",
            2,
            y + 12,
            ST77XX_WHITE,
            1);

        drawText(
            String(
                planes[i].altitudeFt)
            + "ft",
            50,
            y + 12,
            ST77XX_WHITE,
            1);

        drawText(
            String(
                planes[i].speedMph)
            + "mph",
            2,
            y + 24,
            ST77XX_CYAN,
            1);

        drawText(
            headingText(
                planes[i].heading),
            95,
            y + 24,
            ST77XX_GREEN,
            1);
    }

    tft.drawFastHLine(
        0,
        220,
        135,
        DARKGREY);

    drawText(
        "MODE",
        5,
        228,
        DARKGREY,
        1);

    drawText(
        "DIM/SLP",
        90,
        228,
        DARKGREY,
        1);
}

//==================================================

void drawAircraftIcon(
    int cx,
    int cy,
    int heading,
    uint16_t color)
{
    float theta =
        (heading - 90)
        * PI
        / 180.0;

    const int noseLen = 5;
    const int wingLen = 3;
    const int tailLen = 2;

    int noseX =
        cx + cos(theta) * noseLen;

    int noseY =
        cy + sin(theta) * noseLen;

    int tailX =
        cx - cos(theta) * tailLen;

    int tailY =
        cy - sin(theta) * tailLen;

    float wingTheta =
        theta + PI / 2.0;

    int leftWingX =
        cx + cos(wingTheta) * wingLen;

    int leftWingY =
        cy + sin(wingTheta) * wingLen;

    int rightWingX =
        cx - cos(wingTheta) * wingLen;

    int rightWingY =
        cy - sin(wingTheta) * wingLen;

    tft.drawLine(
        tailX,
        tailY,
        noseX,
        noseY,
        color);

    tft.drawLine(
        leftWingX,
        leftWingY,
        rightWingX,
        rightWingY,
        color);
}

//==================================================
// RADAR VIEW
//==================================================

void drawRadarScreen()
{
    tft.fillScreen(ST77XX_BLACK);

    int cx = 67;
    int cy = 74;

    int radius = 55;

    drawText(
        "RADAR",
        2,
        2,
        ST77XX_CYAN,
        1);

    tft.drawCircle(
        cx,
        cy,
        radius,
        ST77XX_GREEN);

    tft.drawCircle(
        cx,
        cy,
        radius / 2,
        DARKGREY);

    tft.drawFastVLine(
        cx,
        cy - radius,
        radius * 2,
        DARKGREY);

    tft.drawFastHLine(
        cx - radius,
        cy,
        radius * 2,
        DARKGREY);

    drawText("N", 65, 8, ST77XX_WHITE, 1);
    drawText("S", 65, 134, ST77XX_WHITE, 1);
    drawText("W", 4, 70, ST77XX_WHITE, 1);
    drawText("E", 126, 70, ST77XX_WHITE, 1);

    // HOME POSITION

    tft.fillCircle(
        cx,
        cy,
        3,
        ST77XX_GREEN);

    int shown =
        min(
            planeCount,
            20);

    for(int i = 0;
        i < shown;
        i++)
    {
        // TRAILS

        AircraftHistory* h =
            findHistory(
                planes[i].callsign);

        if(h != nullptr)
        {
            for(int t = TRAIL_POINTS - 1;
                t >= 0;
                t--)
            {
                float distance =
                    h->trail[t].distance;

                if(distance <= 0)
                    continue;
                
                if(distance > RADAR_RANGE)
                    distance = RADAR_RANGE;

                float r =
                    (distance / RADAR_RANGE) *
                    radius;

                float theta =
                    (h->trail[t].bearing - 90.0f)
                    *
                    PI
                    /
                    180.0f;

                int x =
                    cx +
                    cos(theta) * r;

                int y =
                    cy +
                    sin(theta) * r;

                uint16_t trailColor;

                switch(t)
                {
                    case 0:
                        trailColor =
                            ST77XX_WHITE;
                        break;

                    case 1:
                        trailColor =
                            0xBDF7;
                        break;

                    case 2:
                        trailColor =
                            0x7BEF;
                        break;

                    case 3:
                        trailColor =
                            0x39E7;
                        break;

                    default:
                        trailColor =
                            0x2104;
                        break;
                }

                tft.drawPixel(
                    x,
                    y,
                    trailColor);
            }
        }

        // CURRENT AIRCRAFT POSITION

        float dist =
            planes[i].distance;

        if(dist > RADAR_RANGE)
            dist = RADAR_RANGE;

        float r =
            (dist / RADAR_RANGE)
            *
            radius;

        float theta =
            (planes[i].bearing - 90.0f)
            *
            PI
            /
            180.0f;

        int x =
            cx +
            cos(theta) * r;

        int y =
            cy +
            sin(theta) * r;

        uint16_t color;

        if(planes[i].altitudeFt < 5000)
        {
            color =
                ST77XX_GREEN;
        }
        else if(
            planes[i].altitudeFt < 20000)
        {
            color =
                ST77XX_YELLOW;
        }
        else
        {
            color =
                ST77XX_RED;
        }

        drawAircraftIcon(
            x,
            y,
            planes[i].heading,
            color);

        if(i == 0)
        {
            tft.drawCircle(
                x,
                y,
                6,
                ST77XX_WHITE);
        }

        if(i < 3)
        {
            drawText(
                planes[i].callsign,
                x + 4,
                y - 4,
                ST77XX_WHITE,
                1);
        }
    }

    // NEAREST AIRCRAFT INFO

    if(planeCount > 0)
    {
        drawText(
            planes[0].callsign,
            2,
            142,
            ST77XX_CYAN,
            1);

        drawText(
            String(
                planes[0].distance,
                1)
            + "mi",
            2,
            156,
            ST77XX_WHITE,
            1);

        drawText(
            String(
                planes[0].altitudeFt)
            + "ft",
            55,
            156,
            ST77XX_WHITE,
            1);

        drawText(
            String(
                planes[0].speedMph)
            + "mph",
            2,
            170,
            ST77XX_YELLOW,
            1);

        drawText(
            headingText(
                planes[0].heading)
            + " "
            + String(
                planes[0].heading)
            + "°",
            75,
            170,
            ST77XX_GREEN,
            1);
    }

    drawText(
        String(planeCount)
        + " AC",
        2,
        190,
        ST77XX_YELLOW,
        1);

    drawBattery(
        112,
        2);

    tft.drawFastHLine(
        0,
        220,
        135,
        DARKGREY);

    drawText(
        "MODE",
        5,
        228,
        DARKGREY,
        1);

    drawText(
        "DIM/SLP",
        90,
        228,
        DARKGREY,
        1);
}

//==================================================
// MASTER DRAW
//==================================================

void drawScreen()
{
    if(currentView ==
       LIST_VIEW)
    {
        drawListScreen();
    }
    else
    {
        drawRadarScreen();
    }
}

//==================================================
// DISPLAY PINS
//==================================================

#define TFT_CS     5
#define TFT_DC    16
#define TFT_RST   23
#define TFT_MOSI  19
#define TFT_SCLK  18
#define TFT_BL     4

//==================================================
// BUTTONS
//==================================================

#define BTN_MODE   35
#define BTN_SCREEN 0

//==================================================

SPIClass spi(VSPI);
Adafruit_ST7789 tft(
    &spi,
    TFT_CS,
    TFT_DC,
    TFT_RST);

//==================================================

bool backlightOn = true;

ViewMode currentView =
    LIST_VIEW;

unsigned long lastUpdate = 0;
unsigned long lastBarUpdate = 0;
unsigned long lastScreenUpdate = 0;

const unsigned long UPDATE_INTERVAL =
    90000UL;

//==================================================

float getUpdatePercent()
{
    unsigned long elapsed =
        millis() - lastUpdate;

    if(elapsed > UPDATE_INTERVAL)
        elapsed = UPDATE_INTERVAL;

    return
        (float)(UPDATE_INTERVAL - elapsed)
        /
        UPDATE_INTERVAL;
}

//==================================================
// BATTERY
//==================================================

float getBatteryVoltage()
{
    long sum = 0;

    for(int i = 0;
        i < 16;
        i++)
    {
        sum +=
            analogRead(34);

        delay(2);
    }

    float raw =
        sum / 16.0;

    return
        (raw / 4095.0)
        *
        2.0
        *
        3.3;
}

//==================================================

void enterDeepSleep()
{
    tft.fillScreen(
        ST77XX_BLACK);

    drawText(
        "Sleeping...",
        20,
        110,
        ST77XX_YELLOW,
        1);

    delay(500);

    ledcWrite(
        TFT_BL,
        0);

    esp_sleep_enable_ext0_wakeup(
        GPIO_NUM_0,
        LOW);

    esp_deep_sleep_start();
}

//==================================================
// SETUP
//==================================================

void setup()
{
    Serial.begin(115200);

    esp_sleep_wakeup_cause_t wakeup =
        esp_sleep_get_wakeup_cause();

    if(wakeup ==
      ESP_SLEEP_WAKEUP_EXT0)
    {
        Serial.println(
            "Wake From Deep Sleep");
    }

    analogReadResolution(12);

    pinMode(
        BTN_MODE,
        INPUT_PULLUP);

    pinMode(
        BTN_SCREEN,
        INPUT_PULLUP);

    ledcAttach(TFT_BL, 5000, 8);

    ledcWrite(
        TFT_BL,
        brightnessTable[4]);
        

    spi.begin(
        TFT_SCLK,
        -1,
        TFT_MOSI,
        TFT_CS);

    tft.init(
        135,
        240);

    tft.setRotation(2);

    tft.fillScreen(
        ST77XX_BLACK);

    drawText(
        "Connecting...",
        15,
        110,
        ST77XX_WHITE,
        1);

    WiFi.mode(
        WIFI_STA);

    WiFi.begin(
        ssid,
        password);

    while(
        WiFi.status()
        !=
        WL_CONNECTED)
    {
        delay(500);

        Serial.print(".");
    }

    Serial.println();
    Serial.println(
        "WiFi Connected");

    fetchAircraft();

    drawScreen();

    lastUpdate =
        millis();
}

//==================================================
// LOOP
//==================================================

void loop()
{

    if(
        millis()
        -
        lastDebugPrint
        >
        1000)
    {
        Serial.print("Current diff=");
        Serial.println(
            millis() - lastUpdate);

        lastDebugPrint =
            millis();
    }

    // MODE BUTTON

    if(
        digitalRead(
            BTN_MODE)
        ==
        LOW)
    {
        currentView =
            currentView
            ==
            LIST_VIEW
            ?
            RADAR_VIEW
            :
            LIST_VIEW;

        drawScreen();

        delay(350);
    }

    // BACKLIGHT BUTTON

    if(
        digitalRead(
            BTN_SCREEN)
        ==
        LOW)
    {
        if(
            screenButtonStart
            ==
            0)
        {
            screenButtonStart =
                millis();
        }

        if(
            millis()
            -
            screenButtonStart
            >
            2000)
        {
            enterDeepSleep();
        }
    }
    else
    {
        if(
            screenButtonStart
            !=
            0)
        {
            unsigned long held =
                millis()
                -
                screenButtonStart;

            screenButtonStart =
                0;

            if(
                held
                <
                2000)
            {
                brightnessLevel--;

                if(
                    brightnessLevel
                    <
                    1)
                {
                    brightnessLevel
                        =
                        4;
                }

                ledcWrite(
                    TFT_BL,
                    brightnessTable[
                        brightnessLevel]);
            }
        }
    }
    // progress bar

    if(
        millis()
        -
        lastBarUpdate
        >
        500)
    {
        updateProgressBar();

        lastBarUpdate =
            millis();
    }

    // AUTO UPDATE

    Serial.print("Current diff=");
    Serial.println(
        millis() - lastUpdate);

    if(
        millis()
        -
        lastUpdate
        >
        UPDATE_INTERVAL)
    {
        Serial.println("API Refresh");

        fetchAircraft();

        drawScreen();

        lastUpdate =
            millis();
    }
}