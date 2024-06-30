#ifndef __NETDATA_H
#define __NETDATA_H

#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <string>

const char* SERVER_ADDRESS = "10.1.1.1";

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
    static String cookie;
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
};

// 在类外部定义静态成员变量 cookie，初始化为 ""
String NetChartData::cookie = "";

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

    JsonArray db_points_per_tier = doc["db_points_per_tier"];
    data.result = doc["result"];
    data.min = doc["min"]; // 2.1594684
    data.max = doc["max"]; // 3.7468776
    double free = doc["result"]["data"][0][1];
    double used = doc["result"]["data"][0][2];
    double cached = doc["result"]["data"][0][3];
    double buffers = doc["result"]["data"][0][4];

    data.mem_usage = 100 * (used) / (free + used + buffers + cached);
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
    String httpRequest = String("GET ") + reqRes + " HTTP/0.1\r\n" + "Host: " + SERVER_ADDRESS + "\r\n" +
        "Connection: close\r\n\r\n";

    // 尝试连接服务器
    if (client.connect(SERVER_ADDRESS, NETDATA_PORT))
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

void parseIkuaiResponse(WiFiClient& client, NetChartData& data)
{
    // Stream& input;

    DynamicJsonDocument doc(4096);

    if (client.find("\r\n\r\n"))
    {
        Serial.println("Found Header End. Start Parsing.");
    }
    Serial.println("开始解析JSON:");
    DeserializationError error = deserializeJson(doc, client);

    if (error)
    {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());

        return;
    }
    String Result = doc["Result"];
    Serial.println("Result");
    Serial.println(Result);


    String cpuStr = doc["Data"]["cpu"][0]; // 1
    data.cpu_usage = strtod(cpuStr.c_str(),NULL);
    Serial.println("cpu");
    Serial.println(data.cpu_usage);

    data.temp_value = doc["Data"]["cputemp"][0]; // 1
    Serial.println("cputemp");
    Serial.println(data.temp_value);

    String memStr = doc["Data"]["memory"]["used"]; // 1
    data.mem_usage = strtod(memStr.c_str(),NULL);

    Serial.println("memory");
    Serial.println(data.mem_usage);

    data.up_speed = doc["Data"]["stream"]["upload"]; // 1

    Serial.println("upload");
    Serial.println(data.up_speed);
    data.down_speed = doc["Data"]["stream"]["download"]; // 1

    Serial.println("download");
    Serial.println(data.down_speed);
}


void ParseCookie(WiFiClient client, NetChartData data)
{
    // Read response and extract cookie
    Serial.println("----解析cookie");
    while (client.available() || client.connected())
    {
        String line = client.readStringUntil('\n');
        if (line.startsWith("Set-Cookie:"))
        {
            int start = line.indexOf('=');
            int end = line.indexOf(';');
            const String cookie = line.substring(start + 1, end);
            Serial.println("Cookie: ");
            NetChartData::cookie = cookie;
            Serial.println(cookie);
            client.stop();
        }
    }
}

/**
 * 从爱快软路由获取信息
 * ChartID:
 *  system.cpu - CPU占用率信息
 *  sensors.temp_thermal_zone0_thermal_thermal_zone - CPU 温度信息
 */

bool GetIkuaiInfo(String chartID, NetChartData& data, String postData)
{
    int NETDATA_PORT = 80;
    String reqRes = "/Action/" + chartID;

    WiFiClient client;
    bool ret = false;

    // 建立http请求信息
    String httpRequest = String("POST ") + reqRes + " HTTP/1.1\r\n" + "Host: " + SERVER_ADDRESS + "\r\n" +
        "Cache-Control: no-cache\r\n" +
        "Content-Length: " + postData.length() + "\r\n" +
        "Content-Type: text/plain\r\n" +
        "Cookie: sess_key=" + NetChartData::cookie + "\r\n" +
        "Accept: application/json, text/plain, */*\r\n" +
        "Connection: close\r\n\r\n" +
        postData;


    // 尝试连接服务器
    if (client.connect(SERVER_ADDRESS, NETDATA_PORT))
    {
        // 向服务器发送http请求信息
        Serial.println("Sending request: ");
        client.print(httpRequest);
        Serial.print(httpRequest);
        // 获取并显示服务器响应状态行
        String status_response = client.readStringUntil('\n');
        Serial.print("status_response: ");
        Serial.println(status_response);
        if (data.cookie == "")
        {
            Serial.println("---------没有cookie=======");

            ParseCookie(client, data);
        }
        else
        {
            parseIkuaiResponse(client, data);
        }


        // 利用ArduinoJson库解析NetData返回的信息

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


bool getNetDataInfo(String chartID, NetChartData& data)
{
    return getNetDataInfoWithDimension(chartID, data, "");
}

#endif
