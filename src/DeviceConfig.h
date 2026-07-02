#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#include <Arduino.h>
#include <EEPROM.h>

#define CONFIG_MAGIC 0x494B5541UL // "IKUA"
#define CONFIG_VERSION 2
#define CONFIG_EEPROM_SIZE 320

struct DeviceConfig
{
    uint32_t magic;
    uint8_t version;
    char wifiSsid[33];
    char wifiPassword[65];
    char ikuaiUsername[33];
    char ikuaiPassword[65];
    char ikuaiAddress[41];
};

static DeviceConfig deviceConfig;

static void setDefaultConfig(DeviceConfig& cfg)
{
    memset(&cfg, 0, sizeof(cfg));
    cfg.magic = CONFIG_MAGIC;
    cfg.version = CONFIG_VERSION;
    strncpy(cfg.wifiSsid, "", sizeof(cfg.wifiSsid) - 1);
    strncpy(cfg.wifiPassword, "", sizeof(cfg.wifiPassword) - 1);
    strncpy(cfg.ikuaiAddress, "10.1.1.1", sizeof(cfg.ikuaiAddress) - 1);
    strncpy(cfg.ikuaiUsername, "admin", sizeof(cfg.ikuaiUsername) - 1);
    strncpy(cfg.ikuaiPassword, "", sizeof(cfg.ikuaiPassword) - 1);
}

static bool isConfigValid(const DeviceConfig& cfg)
{
    return cfg.magic == CONFIG_MAGIC && (cfg.version == 1 || cfg.version == CONFIG_VERSION);
}

static void migrateConfig(DeviceConfig& cfg)
{
    if (cfg.version == 1)
    {
        strncpy(cfg.ikuaiAddress, "10.1.1.1", sizeof(cfg.ikuaiAddress) - 1);
        cfg.version = CONFIG_VERSION;
    }
}

static void trimConfigField(char* field, size_t capacity)
{
    if (capacity == 0)
    {
        return;
    }
    size_t len = strnlen(field, capacity);
    while (len > 0 && isspace(static_cast<unsigned char>(field[len - 1])))
    {
        field[--len] = '\0';
    }
    size_t start = 0;
    while (field[start] && isspace(static_cast<unsigned char>(field[start])))
    {
        start++;
    }
    if (start > 0)
    {
        memmove(field, field + start, strlen(field + start) + 1);
    }
}

static void normalizeIkuaiAddressField(char* field, size_t capacity)
{
    trimConfigField(field, capacity);
    if (field[0] == '\0')
    {
        return;
    }

    String addr(field);
    addr.trim();
    if (addr.startsWith("http://"))
    {
        addr = addr.substring(7);
    }
    else if (addr.startsWith("https://"))
    {
        addr = addr.substring(8);
    }
    int slash = addr.indexOf('/');
    if (slash >= 0)
    {
        addr = addr.substring(0, slash);
    }
    int colon = addr.indexOf(':');
    if (colon >= 0)
    {
        addr = addr.substring(0, colon);
    }
    addr.trim();
    strncpy(field, addr.c_str(), capacity - 1);
    field[capacity - 1] = '\0';
}

static void normalizeDeviceConfig(DeviceConfig& cfg)
{
    trimConfigField(cfg.wifiSsid, sizeof(cfg.wifiSsid));
    trimConfigField(cfg.wifiPassword, sizeof(cfg.wifiPassword));
    trimConfigField(cfg.ikuaiUsername, sizeof(cfg.ikuaiUsername));
    trimConfigField(cfg.ikuaiPassword, sizeof(cfg.ikuaiPassword));
    normalizeIkuaiAddressField(cfg.ikuaiAddress, sizeof(cfg.ikuaiAddress));
}

static void loadDeviceConfig()
{
    EEPROM.begin(CONFIG_EEPROM_SIZE);
    EEPROM.get(0, deviceConfig);
    if (!isConfigValid(deviceConfig))
    {
        setDefaultConfig(deviceConfig);
    }
    else
    {
        migrateConfig(deviceConfig);
    }
    normalizeDeviceConfig(deviceConfig);
}

static bool saveDeviceConfig(const DeviceConfig& cfg)
{
    DeviceConfig normalized = cfg;
    normalizeDeviceConfig(normalized);
    EEPROM.put(0, normalized);
    deviceConfig = normalized;
    return EEPROM.commit();
}

static bool factoryResetConfig()
{
    DeviceConfig cfg;
    setDefaultConfig(cfg);
    deviceConfig = cfg;
    return saveDeviceConfig(cfg);
}

static bool needsWifiSetup()
{
    return deviceConfig.wifiSsid[0] == '\0' && deviceConfig.wifiPassword[0] == '\0';
}

static bool hasWifiConfig()
{
    return deviceConfig.wifiSsid[0] != '\0';
}

static const char* getIkuaiServerAddress()
{
    if (deviceConfig.ikuaiAddress[0] != '\0')
    {
        return deviceConfig.ikuaiAddress;
    }
    return "10.1.1.1";
}

#endif
