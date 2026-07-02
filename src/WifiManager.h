#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "DeviceConfig.h"

static const char* AP_PASSWORD = "ikuai123";
static const unsigned long WIFI_RETRY_INTERVAL_MS = 10000;
static const unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000;

static bool apModeEnabled = false;
static bool staConnecting = false;
static bool wifiConnectionFailed = false;
static bool wasWifiConnected = false;
static bool wifiSignalLost = false;
static unsigned long staConnectStart = 0;
static unsigned long lastWifiRetry = 0;
static char displayIpBuf[16] = "0.0.0.0";
static bool displayIpDirty = true;

static void refreshDisplayIpBuf()
{
    if (!displayIpDirty)
    {
        return;
    }
    displayIpDirty = false;
    if (WiFi.status() == WL_CONNECTED)
    {
        snprintf(displayIpBuf, sizeof(displayIpBuf), "%s", WiFi.localIP().toString().c_str());
    }
    else if (apModeEnabled)
    {
        snprintf(displayIpBuf, sizeof(displayIpBuf), "%s", WiFi.softAPIP().toString().c_str());
    }
    else
    {
        strncpy(displayIpBuf, "192.168.4.1", sizeof(displayIpBuf) - 1);
        displayIpBuf[sizeof(displayIpBuf) - 1] = '\0';
    }
}

static void markDisplayIpDirty()
{
    displayIpDirty = true;
}

static String getApSsid()
{
    return String("IKuaiMonitor-") + String(ESP.getChipId(), HEX);
}

static void startApMode()
{
    if (apModeEnabled)
    {
        return;
    }
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(getApSsid().c_str(), AP_PASSWORD);
    apModeEnabled = true;
    markDisplayIpDirty();
    Serial.print("AP started: ");
    Serial.println(getApSsid());
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
}

static void retryStaConnect()
{
    if (!hasWifiConfig())
    {
        return;
    }
    WiFi.mode(WIFI_AP_STA);
    WiFi.disconnect();
    yield();
    WiFi.begin(deviceConfig.wifiSsid, deviceConfig.wifiPassword);
    staConnecting = true;
    staConnectStart = millis();
    Serial.print("Retrying WiFi: ");
    Serial.println(deviceConfig.wifiSsid);
}

static void beginStaConnect()
{
    if (!hasWifiConfig())
    {
        startApMode();
        return;
    }
    if (staConnecting)
    {
        return;
    }
    retryStaConnect();
    startApMode();
}

static bool isWifiReady()
{
    return WiFi.status() == WL_CONNECTED;
}

static bool isWifiConnectionFailed()
{
    return wifiConnectionFailed;
}

static bool isWifiSignalLost()
{
    return wifiSignalLost;
}

static void stopApMode()
{
    if (!apModeEnabled)
    {
        return;
    }
    WiFi.softAPdisconnect(true);
    apModeEnabled = false;
    markDisplayIpDirty();
    Serial.println("AP stopped to save RAM.");
}

// WiFi 稳定且无需配网时关闭 AP，可释放约 1~2KB 堆内存
static void manageApPower(bool needConfigPortal)
{
    if (needConfigPortal)
    {
        if (!apModeEnabled)
        {
            startApMode();
        }
        return;
    }
    if (isWifiReady() && apModeEnabled)
    {
        stopApMode();
    }
}

static const char* getDisplayIp()
{
    refreshDisplayIpBuf();
    return displayIpBuf;
}

static String getConfigPortalUrl()
{
    return String("http://") + getDisplayIp();
}

// 非阻塞 WiFi 管理：首次连接失败回退 AP；运行中断线则定时重连
static bool updateWifiConnection()
{
    if (isWifiReady())
    {
        wasWifiConnected = true;
        wifiSignalLost = false;
        wifiConnectionFailed = false;
        staConnecting = false;
        markDisplayIpDirty();
        manageApPower(needsWifiSetup());
        return true;
    }

    if (needsWifiSetup())
    {
        startApMode();
        return false;
    }

    if (wasWifiConnected && hasWifiConfig() && !wifiConnectionFailed)
    {
        wifiSignalLost = true;
        manageApPower(true);

        if (staConnecting && millis() - staConnectStart > WIFI_CONNECT_TIMEOUT_MS)
        {
            staConnecting = false;
        }

        if (!staConnecting && millis() - lastWifiRetry >= WIFI_RETRY_INTERVAL_MS)
        {
            lastWifiRetry = millis();
            retryStaConnect();
        }
        return false;
    }

    if (wifiConnectionFailed)
    {
        startApMode();
        return false;
    }

    if (!staConnecting)
    {
        beginStaConnect();
        return false;
    }

    if (millis() - staConnectStart > WIFI_CONNECT_TIMEOUT_MS)
    {
        Serial.println("WiFi connection timeout, fallback to AP mode.");
        staConnecting = false;
        wifiConnectionFailed = true;
        WiFi.disconnect();
        startApMode();
    }
    return false;
}

static void initWifiManager()
{
    WiFi.persistent(false);
    WiFi.setAutoReconnect(true);
    loadDeviceConfig();
    if (hasWifiConfig())
    {
        beginStaConnect();
    }
    else
    {
        startApMode();
    }
}

#endif
