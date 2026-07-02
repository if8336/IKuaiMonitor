#ifndef WEB_CONFIG_H
#define WEB_CONFIG_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "DeviceConfig.h"
#include "WifiManager.h"

static ESP8266WebServer configServer(80);
static bool webConfigRoutesReady = false;
static bool webConfigActive = false;

// 流式输出，避免在堆上拼接整页 HTML（ESP8266 RAM 仅 ~80KB）
static void sendEscapedHtml(const char* text)
{
    if (text == nullptr)
    {
        return;
    }
    for (const char* p = text; *p; ++p)
    {
        switch (*p)
        {
        case '&':
            configServer.sendContent("&amp;");
            break;
        case '<':
            configServer.sendContent("&lt;");
            break;
        case '>':
            configServer.sendContent("&gt;");
            break;
        case '"':
            configServer.sendContent("&quot;");
            break;
        default:
        {
            char ch[2] = {*p, '\0'};
            configServer.sendContent(ch);
            break;
        }
        }
        yield();
    }
}

static void sendStatusBadge()
{
    if (needsWifiSetup())
    {
        configServer.sendContent(F("<span class='b w'>等待配网</span>"));
    }
    else if (isWifiConnectionFailed())
    {
        configServer.sendContent(F("<span class='b w'>WiFi失败</span>"));
    }
    else if (WiFi.status() == WL_CONNECTED)
    {
        configServer.sendContent(F("<span class='b o'>WiFi已连接</span>"));
    }
    else
    {
        configServer.sendContent(F("<span class='b'>连接中</span>"));
    }
}

static void sendConfigPage()
{
    configServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
    configServer.send(200, "text/html; charset=utf-8", "");

    configServer.sendContent(F(
        "<!DOCTYPE html><html><head><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>IKuaiMonitor</title><style>"
        "body{font-family:sans-serif;margin:0;padding:16px;background:#111;color:#eee}"
        "h1{font-size:1.2rem;margin:0 0 4px}p{margin:0 0 12px;color:#aaa;font-size:.9rem}"
        ".m{font-size:.82rem;color:#888;margin-bottom:12px}"
        ".b{display:inline-block;padding:2px 8px;border-radius:8px;background:#333;font-size:.75rem}"
        ".o{background:#14532d}.w{background:#7c2d12}"
        "label{display:block;margin-top:10px;font-size:.85rem;color:#bbb}"
        "input{width:100%;padding:8px;margin-top:4px;border:1px solid #444;border-radius:6px;"
        "background:#1a1a1a;color:#fff;font-size:1rem;box-sizing:border-box}"
        "button{width:100%;padding:10px;margin-top:14px;border:none;border-radius:6px;"
        "font-size:1rem;font-weight:600;cursor:pointer}"
        ".s{background:#2563eb;color:#fff}.r{background:transparent;color:#f87171;border:1px solid #7f1d1d}"
        "</style></head><body>"
        "<h1>IKuaiMonitor</h1><p>爱快监视器配置</p><div class='m'>IP: "));
    yield();

    if (WiFi.status() == WL_CONNECTED)
    {
        configServer.sendContent(WiFi.localIP().toString());
    }
    else
    {
        configServer.sendContent(WiFi.softAPIP().toString());
    }
    configServer.sendContent(F(" "));
    sendStatusBadge();
    yield();

    configServer.sendContent(F(
        "</div><form method='POST' action='/save'>"
        "<label>WiFi 名称</label><input name='wifi_ssid' value='"));
    sendEscapedHtml(deviceConfig.wifiSsid);
    configServer.sendContent(F("'>"
        "<label>WiFi 密码</label><input name='wifi_password' type='password' value='"));
    sendEscapedHtml(deviceConfig.wifiPassword);
    configServer.sendContent(F("'>"
        "<label>爱快地址</label><input name='ikuai_address' placeholder='10.1.1.1' value='"));
    sendEscapedHtml(deviceConfig.ikuaiAddress);
    configServer.sendContent(F("'>"
        "<label>爱快用户名</label><input name='ikuai_username' value='"));
    sendEscapedHtml(deviceConfig.ikuaiUsername);
    configServer.sendContent(F("'>"
        "<label>爱快密码</label><input name='ikuai_password' type='password' value='"));
    sendEscapedHtml(deviceConfig.ikuaiPassword);
    yield();

    configServer.sendContent(F(
        "'><button class='s' type='submit'>保存并重启</button></form>"
        "<form method='POST' action='/factory-reset' onsubmit=\"return confirm('确定恢复出厂？');\">"
        "<button class='r' type='submit'>恢复出厂</button></form>"
        "</body></html>"));
}

static void sendRestartPage(const char* message)
{
    configServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
    configServer.send(200, "text/html; charset=utf-8", "");
    configServer.sendContent(F(
        "<!DOCTYPE html><html><head><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<style>body{font-family:sans-serif;text-align:center;padding:40px 16px;"
        "background:#111;color:#eee}</style></head><body><h2>"));
    configServer.sendContent(message);
    configServer.sendContent(F("</h2><p>设备正在重启...</p></body></html>"));
}

static void handleConfigSave()
{
    DeviceConfig newConfig = deviceConfig;
    newConfig.magic = CONFIG_MAGIC;
    newConfig.version = CONFIG_VERSION;

    if (configServer.hasArg("wifi_ssid"))
    {
        strncpy(newConfig.wifiSsid, configServer.arg("wifi_ssid").c_str(), sizeof(newConfig.wifiSsid) - 1);
    }
    if (configServer.hasArg("wifi_password"))
    {
        strncpy(newConfig.wifiPassword, configServer.arg("wifi_password").c_str(), sizeof(newConfig.wifiPassword) - 1);
    }
    if (configServer.hasArg("ikuai_address"))
    {
        strncpy(newConfig.ikuaiAddress, configServer.arg("ikuai_address").c_str(), sizeof(newConfig.ikuaiAddress) - 1);
    }
    if (configServer.hasArg("ikuai_username"))
    {
        strncpy(newConfig.ikuaiUsername, configServer.arg("ikuai_username").c_str(), sizeof(newConfig.ikuaiUsername) - 1);
    }
    if (configServer.hasArg("ikuai_password"))
    {
        strncpy(newConfig.ikuaiPassword, configServer.arg("ikuai_password").c_str(), sizeof(newConfig.ikuaiPassword) - 1);
    }

    deviceConfig = newConfig;
    if (saveDeviceConfig(deviceConfig))
    {
        sendRestartPage("配置已保存");
        delay(500);
        ESP.restart();
    }
    else
    {
        configServer.send(500, "text/plain; charset=utf-8", "保存失败");
    }
}

static void handleFactoryReset()
{
    factoryResetConfig();
    sendRestartPage("已恢复出厂设置");
    delay(500);
    ESP.restart();
}

static void handleNotFound()
{
    sendConfigPage();
}

static void registerWebConfigRoutes()
{
    if (webConfigRoutesReady)
    {
        return;
    }
    configServer.on("/", HTTP_GET, sendConfigPage);
    configServer.on("/save", HTTP_POST, handleConfigSave);
    configServer.on("/factory-reset", HTTP_POST, handleFactoryReset);
    configServer.onNotFound(handleNotFound);
    webConfigRoutesReady = true;
}

static void startWebConfig()
{
    if (webConfigActive)
    {
        return;
    }
    registerWebConfigRoutes();
    configServer.begin();
    webConfigActive = true;
    Serial.println("Web config server started on port 80");
}

static void stopWebConfig()
{
    if (!webConfigActive)
    {
        return;
    }
    configServer.stop();
    webConfigActive = false;
    Serial.println("Web config server stopped");
}

static bool isWebConfigActive()
{
    return webConfigActive;
}

static void initWebConfig()
{
    registerWebConfigRoutes();
}

static void handleWebConfig()
{
    if (!webConfigActive)
    {
        return;
    }
    configServer.handleClient();
}

static bool isWebConfigBusy()
{
    return webConfigActive && configServer.client().connected();
}

#endif
