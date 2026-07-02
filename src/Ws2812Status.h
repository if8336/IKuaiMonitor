#ifndef WS2812_STATUS_H
#define WS2812_STATUS_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "DeviceConfig.h"
#include "WifiManager.h"

// SD2 小电视 WS2812 数据脚为 GPIO12(D6)
#ifndef WS2812_PIN
#define WS2812_PIN 12
#endif
#ifndef WS2812_COUNT
#define WS2812_COUNT 1
#endif
#define WS2812_BRIGHTNESS 64
#define WS2812_GREEN_DURATION_MS 5000

static Adafruit_NeoPixel ws2812(WS2812_COUNT, WS2812_PIN, NEO_GRB + NEO_KHZ800);
static bool ws2812Ready = false;
static bool greenTimerActive = false;
static unsigned long greenStartedAt = 0;
static uint32_t ws2812LastColor = 0;

enum Ws2812LedState
{
    WS2812_OFF = 0,
    WS2812_RED,
    WS2812_YELLOW,
    WS2812_GREEN
};

// WS2812 时序要求极高，发送期间必须屏蔽 WiFi 等中断
static inline void ws2812ShowSafe()
{
    ESP.wdtFeed();
    noInterrupts();
    ws2812.show();
    interrupts();
}

static void ws2812SetColor(uint8_t r, uint8_t g, uint8_t b)
{
    if (!ws2812Ready)
    {
        return;
    }
    const uint32_t color = ws2812.Color(r, g, b);
    if (color == ws2812LastColor)
    {
        return;
    }
    ws2812LastColor = color;
    for (uint16_t i = 0; i < WS2812_COUNT; i++)
    {
        ws2812.setPixelColor(i, color);
    }
    ws2812ShowSafe();
}

static void ws2812TurnOff()
{
    ws2812SetColor(0, 0, 0);
}

static Ws2812LedState resolveWs2812TargetState(bool ikuaiMode, bool ikuaiLoggedIn)
{
    if (!isWifiReady() || needsWifiSetup() || isWifiConnectionFailed() || isWifiSignalLost())
    {
        return WS2812_RED;
    }
    if (ikuaiMode && !ikuaiLoggedIn)
    {
        return WS2812_YELLOW;
    }
    return WS2812_GREEN;
}

inline void initWs2812Status()
{
    pinMode(WS2812_PIN, OUTPUT);
    ws2812.begin();
    ws2812.setBrightness(WS2812_BRIGHTNESS);
    ws2812.clear();
    ws2812ShowSafe();
    ws2812Ready = true;
    greenTimerActive = false;
    ws2812LastColor = 0;
}

inline void updateWs2812Status(bool ikuaiMode, bool ikuaiLoggedIn)
{
    if (!ws2812Ready)
    {
        return;
    }

    const Ws2812LedState target = resolveWs2812TargetState(ikuaiMode, ikuaiLoggedIn);

    if (target != WS2812_GREEN)
    {
        greenTimerActive = false;
        if (target == WS2812_RED)
        {
            ws2812SetColor(255, 0, 0);
        }
        else if (target == WS2812_YELLOW)
        {
            ws2812SetColor(255, 180, 0);
        }
        return;
    }

    if (!greenTimerActive)
    {
        greenTimerActive = true;
        greenStartedAt = millis();
        ws2812SetColor(0, 255, 0);
        return;
    }

    if (millis() - greenStartedAt >= WS2812_GREEN_DURATION_MS)
    {
        ws2812TurnOff();
    }
}

#endif
