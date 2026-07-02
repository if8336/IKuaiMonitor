#ifndef __NETDATA_H
#define __NETDATA_H

#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <MD5Builder.h>
#include <base64.h>
#include <string>
#include "DeviceConfig.h"

class NetChartData
{
public:
    int api;
    String id;
    String name;

    int view_update_every;
    int update_every;
    long first_entry;
    long last_entry;
    long before;
    long after;
    String group;
    String options_0;
    String options_1;
    char sessKey[48];
    double up_speed;
    double down_speed;
    double cpu_usage;
    double mem_usage;
    double temp_value;

    JsonArray dimension_names;
    JsonArray dimension_ids;
    JsonArray latest_values;
    JsonArray view_latest_values;
    int dimensions;
    int points;
    String format;
    JsonArray result;
    double min;
    double max;
    int lastLoginResult = -1;
};


// 根据用户名和密码生成爱快登录 JSON（passwd=MD5密码，pass=base64("salt_11"+密码)）
String buildIkuaiLoginPayload(const char* username, const char* password)
{
    MD5Builder md5;
    md5.begin();
    md5.add(password);
    md5.calculate();
    String passwdHash = md5.toString();
    passwdHash.toLowerCase();

    String passPlain = String("salt_11") + password;
    String passEncoded = base64::encode(passPlain);

    DynamicJsonDocument doc(256);
    doc["username"] = username;
    doc["passwd"] = passwdHash;
    doc["pass"] = passEncoded;
    doc["remember_password"] = true;

    String output;
    serializeJson(doc, output);
    return output;
}

static String readHttpResponse(WiFiClient& client, unsigned long timeoutMs = 8000)
{
    unsigned long waitStart = millis();
    while (!client.available() && client.connected() && millis() - waitStart < 3000)
    {
        delay(1);
        yield();
    }

    String response;
    response.reserve(512);
    const unsigned long deadline = millis() + timeoutMs;
    while ((client.connected() || client.available()) && millis() < deadline)
    {
        while (client.available())
        {
            response += static_cast<char>(client.read());
        }
        yield();
    }
    return response;
}

static void clearSessKey(NetChartData& data)
{
    data.sessKey[0] = '\0';
}

static bool hasSessKey(const NetChartData& data)
{
    return data.sessKey[0] != '\0';
}

static void setSessKey(NetChartData& data, const char* key)
{
    if (key == nullptr)
    {
        clearSessKey(data);
        return;
    }
    strncpy(data.sessKey, key, sizeof(data.sessKey) - 1);
    data.sessKey[sizeof(data.sessKey) - 1] = '\0';
}

static bool parseIkuaiLoginResponse(WiFiClient& client, NetChartData& data)
{
    clearSessKey(data);
    data.lastLoginResult = -1;
    const String response = readHttpResponse(client);

    if (response.length() == 0)
    {
        Serial.println("iKuai login response empty.");
        return false;
    }

    Serial.print("iKuai login raw response: ");
    const int previewLen = response.length() > 320 ? 320 : response.length();
    Serial.println(response.substring(0, previewLen));

    int headerEnd = response.indexOf("\r\n\r\n");
    int bodyOffset = 4;
    if (headerEnd < 0)
    {
        headerEnd = response.indexOf("\n\n");
        bodyOffset = 2;
    }

    const String headers = headerEnd >= 0 ? response.substring(0, headerEnd) : response;
    String body = headerEnd >= 0 ? response.substring(headerEnd + bodyOffset) : "";
    body.trim();

    String lowerHeaders = headers;
    lowerHeaders.toLowerCase();
    const int sessPos = lowerHeaders.indexOf("sess_key=");
    if (sessPos >= 0)
    {
        const int start = sessPos + 9;
        int end = headers.indexOf(';', start);
        if (end < 0)
        {
            end = headers.indexOf('\r', start);
        }
        if (end < 0)
        {
            end = headers.indexOf('\n', start);
        }
        if (end < 0)
        {
            end = headers.length();
        }
        const String sessVal = headers.substring(start, end);
        setSessKey(data, sessVal.c_str());
    }

    int loginResult = -1;
    if (body.length() > 0)
    {
        const int jsonStart = body.indexOf('{');
        if (jsonStart > 0)
        {
            body = body.substring(jsonStart);
        }
        Serial.print("iKuai login body: ");
        Serial.println(body);

        DynamicJsonDocument doc(512);
        const DeserializationError error = deserializeJson(doc, body);
        if (!error)
        {
            loginResult = doc["Result"] | -1;
            data.lastLoginResult = loginResult;
            Serial.print("iKuai login Result: ");
            Serial.println(loginResult);
            if (doc.containsKey("ErrMsg"))
            {
                Serial.print("iKuai login ErrMsg: ");
                Serial.println(doc["ErrMsg"].as<const char*>());
            }
            if (!hasSessKey(data) && doc["Data"].containsKey("sess_key"))
            {
                setSessKey(data, doc["Data"]["sess_key"] | "");
            }
        }
        else
        {
            Serial.print("iKuai login JSON parse failed: ");
            Serial.println(error.c_str());
        }
    }
    else
    {
        Serial.println("iKuai login body empty.");
    }

    Serial.print("iKuai sess_key: ");
    Serial.println(hasSessKey(data) ? data.sessKey : "(empty)");

    if (loginResult == 10000 && hasSessKey(data))
    {
        return true;
    }
    if (loginResult > 0 && loginResult != 10000)
    {
        clearSessKey(data);
        return false;
    }

    return hasSessKey(data);
}

// 爱快返回 10001 表示账号或密码错误，不应重复尝试以免锁定账号
static bool isIkuaiCredentialError(int loginResult)
{
    return loginResult == 10001;
}

void parseNetDataResponse(WiFiClient& client, NetChartData& data)
{
    // Stream& input;

    DynamicJsonDocument doc(4096);

    DeserializationError error = deserializeJson(doc, client);

    if (error)
    {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
    }

    data.api = doc["api"]; // 1
    data.id = doc["id"].as<String>();
    data.name = doc["name"].as<String>();
    data.view_update_every = doc["view_update_every"]; // 1
    data.update_every = doc["update_every"]; // 1
    data.first_entry = doc["first_entry"]; // 1691505357
    data.last_entry = doc["last_entry"]; // 1691505905
    data.after = doc["after"]; // 1691505903
    data.before = doc["before"]; // 1691505904
    data.group = doc["group"].as<String>(); // "average"

    data.options_0 = doc["options"][0].as<String>(); // "jsonwrap"
    data.options_1 = doc["options"][1].as<String>(); // "natural-points"

    data.dimension_names = doc["dimension_names"];
    data.dimension_ids = doc["dimension_ids"];
    data.latest_values = doc["latest_values"];
    data.view_latest_values = doc["view_latest_values"];

    data.dimensions = doc["dimensions"]; // 3
    data.points = doc["points"]; // 2
    data.format = doc["format"].as<String>(); // "array"

    data.min = doc["min"]; // 2.1594684
    data.max = doc["max"]; // 3.7468776

    // 计算内存使用率（仅当数据包含足够维度时）
    JsonArray memData = doc["result"]["data"][0];
    data.mem_usage = 0;
    if (memData.size() >= 5)
    {
        double free = memData[1];
        double used = memData[2];
        double cached = memData[3];
        double buffers = memData[4];
        double total = free + used + buffers + cached;
        if (total > 0)
        {
            data.mem_usage = 100 * used / total;
        }
    }
}

/**
 * 从软路由NetData获取监控信息
 * ChartID:
 *  system.cpu - CPU占用率信息
 *  sensors.temp_thermal_zone0_thermal_thermal_zone - CPU 温度信息
 */

bool getNetDataInfoWithDimension(String chartID, NetChartData& data, String dimensions_filter)
{
    int NETDATA_PORT = 19999;
    // String reqRes = "/api/v0/data?chart=sensors.temp_thermal_zone0_thermal_thermal_zone0&format=json&points=9&group=average&gtime=0&options=s%7Cjsonwrap%7Cnonzero&after=-10";
    String reqRes = "/api/v1/data?chart=" + chartID +
        "&format=json&points=1&group=average&gtime=0&options=s%7Cjsonwrap%7Cnonzero&after=-2";
    reqRes = reqRes + "&dimensions=" + dimensions_filter;

    WiFiClient client;
    bool ret = false;

    // 建立http请求信息
    String httpRequest = String("GET ") + reqRes + " HTTP/0.1\r\n" + "Host: " + getIkuaiServerAddress() + "\r\n" +
        "Connection: close\r\n\r\n";

    // 尝试连接服务器
    if (client.connect(getIkuaiServerAddress(), NETDATA_PORT))
    {
        // 向服务器发送http请求信息
        client.print(httpRequest);
        Serial.println("Sending request: ");
        Serial.println(httpRequest);

        // 获取并显示服务器响应状态行
        String status_response = client.readStringUntil('\n');
        Serial.print("status_response: ");
        Serial.println(status_response);
        // 使用find跳过HTTP响应头
        if (client.find("\r\n\r\n"))
        {
            Serial.println("Found Header End. Start Parsing.");
        }

        // 利用ArduinoJson库解析NetData返回的信息
        parseNetDataResponse(client, data);
        ret = true;
    }
    else
    {
        Serial.println(" connection failed!");
    }
    // 断开客户端与服务器连接工作
    client.stop();
    return ret;
}

static double parseIkuaiPercentField(JsonVariantConst variant)
{
    if (variant.isNull())
    {
        return 0.0;
    }
    if (variant.is<const char*>())
    {
        return strtod(variant.as<const char*>(), nullptr);
    }
    if (variant.is<double>() || variant.is<float>() || variant.is<int>() || variant.is<long>())
    {
        return variant.as<double>();
    }
    return 0.0;
}

static double parseIkuaiCpuUsage(JsonVariantConst cpuField)
{
    if (cpuField.isNull())
    {
        return 0.0;
    }
    if (cpuField.is<JsonArrayConst>() && cpuField.size() > 0)
    {
        return parseIkuaiPercentField(cpuField[0]);
    }
    return parseIkuaiPercentField(cpuField);
}

static double parseIkuaiTempValue(JsonVariantConst tempField)
{
    if (tempField.isNull())
    {
        return 0.0;
    }
    if (tempField.is<JsonArrayConst>() && tempField.size() > 0)
    {
        return parseIkuaiPercentField(tempField[0]);
    }
    return parseIkuaiPercentField(tempField);
}

void parseIkuaiResponse(WiFiClient& client, NetChartData& data)
{
    if (!client.find("\r\n\r\n"))
    {
        Serial.println("iKuai response header not found.");
        return;
    }

    // sysstat 返回当前快照；stream 为瞬时速率，图表由本地每秒入队 10 点
    StaticJsonDocument<192> filter;
    filter["Data"]["cpu"] = true;
    filter["Data"]["cputemp"] = true;
    filter["Data"]["memory"]["used"] = true;
    filter["Data"]["stream"]["upload"] = true;
    filter["Data"]["stream"]["download"] = true;

    DynamicJsonDocument doc(1536);
    const DeserializationError error = deserializeJson(doc, client, DeserializationOption::Filter(filter));

    if (error)
    {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
        return;
    }

    data.cpu_usage = parseIkuaiCpuUsage(doc["Data"]["cpu"]);
    data.temp_value = parseIkuaiTempValue(doc["Data"]["cputemp"]);
    data.mem_usage = parseIkuaiPercentField(doc["Data"]["memory"]["used"]);
    data.up_speed = doc["Data"]["stream"]["upload"] | 0.0;
    data.down_speed = doc["Data"]["stream"]["download"] | 0.0;
}


void ParseCookie(WiFiClient& client, NetChartData& data)
{
    parseIkuaiLoginResponse(client, data);
}

bool GetIkuaiInfo(const char* chartID, NetChartData& data, const char* postData)
{
    const bool isLogin = strcmp(chartID, "login") == 0;
    constexpr int NETDATA_PORT = 80;

    WiFiClient client;
    bool ret = false;

    Serial.print("Connecting iKuai: ");
    Serial.println(getIkuaiServerAddress());

    if (!client.connect(getIkuaiServerAddress(), NETDATA_PORT))
    {
        Serial.println("iKuai connection failed!");
        return false;
    }

    client.print(F("POST /Action/"));
    client.print(chartID);
    client.print(F(" HTTP/1.1\r\nHost: "));
    client.print(getIkuaiServerAddress());
    client.print(F("\r\nCache-Control: no-cache\r\nContent-Length: "));
    client.print(strlen(postData));
    client.print(F("\r\nContent-Type: application/json;charset=UTF-8\r\n"
                    "Accept: application/json, text/plain, */*\r\n"
                    "Connection: close\r\n"));
    if (!isLogin && hasSessKey(data))
    {
        client.print(F("Cookie: sess_key="));
        client.print(data.sessKey);
        client.print(F("\r\n"));
    }
    client.print(F("\r\n"));
    client.print(postData);

    if (isLogin)
    {
        Serial.print("Login payload: ");
        Serial.println(postData);
        ret = parseIkuaiLoginResponse(client, data);
    }
    else
    {
        (void)client.readStringUntil('\n');
        parseIkuaiResponse(client, data);
        ret = true;
    }

    client.stop();
    return ret;
}


bool getNetDataInfo(String chartID, NetChartData& data)
{
    return getNetDataInfoWithDimension(chartID, data, "");
}

#endif
