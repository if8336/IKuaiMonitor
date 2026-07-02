#ifndef GUIDE_PAGE_H
#define GUIDE_PAGE_H

#include <lvgl.h>
#include "DeviceConfig.h"
#include "WifiManager.h"

LV_FONT_DECLARE(guide_font_16)

enum GuideMode
{
    GUIDE_NONE = 0,
    GUIDE_WIFI_SETUP,
    GUIDE_WIFI_FAILED,
    GUIDE_WIFI_LOST,
    GUIDE_IKUAI_FAILED,
    GUIDE_IKUAI_LOST
};

static lv_obj_t* guide_page = NULL;
static lv_obj_t* guide_title_label = NULL;
static lv_obj_t* guide_line1_label = NULL;
static lv_obj_t* guide_line2_label = NULL;
static lv_obj_t* guide_line3_label = NULL;
static lv_obj_t* guide_line4_label = NULL;
static lv_obj_t* guide_hint_label = NULL;
static GuideMode currentGuideMode = GUIDE_NONE;

static void setGuideLabelStyle(lv_obj_t* label, lv_color_t color)
{
    lv_obj_set_style_local_text_color(label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, color);
    lv_obj_set_style_local_text_font(label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, &guide_font_16);
    lv_label_set_align(label, LV_LABEL_ALIGN_CENTER);
    lv_label_set_long_mode(label, LV_LABEL_LONG_BREAK);
    lv_obj_set_width(label, 228);
}

static void initGuidePage(lv_obj_t* parentScreen)
{
    guide_page = lv_cont_create(parentScreen, NULL);
    lv_obj_set_size(guide_page, 240, 240);
    lv_obj_set_style_local_bg_color(guide_page, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x0d1117));
    lv_obj_set_style_local_border_color(guide_page, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x0d1117));
    lv_obj_set_style_local_radius(guide_page, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
    lv_obj_set_hidden(guide_page, true);

    guide_title_label = lv_label_create(guide_page, NULL);
    setGuideLabelStyle(guide_title_label, lv_color_hex(0xff5d18));
    lv_obj_align(guide_title_label, NULL, LV_ALIGN_IN_TOP_MID, 0, 16);

    guide_line1_label = lv_label_create(guide_page, NULL);
    setGuideLabelStyle(guide_line1_label, LV_COLOR_WHITE);
    lv_obj_align(guide_line1_label, NULL, LV_ALIGN_IN_TOP_MID, 0, 48);

    guide_line2_label = lv_label_create(guide_page, NULL);
    setGuideLabelStyle(guide_line2_label, lv_color_hex(0x63d0fc));
    lv_obj_align(guide_line2_label, guide_line1_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 6);

    guide_line3_label = lv_label_create(guide_page, NULL);
    setGuideLabelStyle(guide_line3_label, LV_COLOR_WHITE);
    lv_obj_align(guide_line3_label, guide_line2_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 6);

    guide_line4_label = lv_label_create(guide_page, NULL);
    setGuideLabelStyle(guide_line4_label, lv_color_hex(0x50ff7d));
    lv_obj_align(guide_line4_label, guide_line3_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 6);

    guide_hint_label = lv_label_create(guide_page, NULL);
    setGuideLabelStyle(guide_hint_label, lv_color_hex(0x838a99));
    lv_obj_align(guide_hint_label, NULL, LV_ALIGN_IN_BOTTOM_MID, 0, -12);
}

static void showGuidePage(GuideMode mode)
{
    if (guide_page == NULL || mode == GUIDE_NONE)
    {
        return;
    }
    currentGuideMode = mode;
    String apSsid = getApSsid();
    const char* portalIp = getDisplayIp();

    if (mode == GUIDE_WIFI_SETUP)
    {
        lv_label_set_text(guide_title_label, "首次配置");
        lv_label_set_text(guide_line1_label, "请连接热点");
        lv_label_set_text_fmt(guide_line2_label, "%s", apSsid.c_str());
        lv_label_set_text_fmt(guide_line3_label, "密码: %s", AP_PASSWORD);
        lv_label_set_text_fmt(guide_line4_label, "访问 %s", portalIp);
        lv_label_set_text(guide_hint_label, "浏览器打开地址配置网络");
    }
    else if (mode == GUIDE_WIFI_FAILED)
    {
        lv_label_set_text(guide_title_label, "WiFi连接失败");
        lv_label_set_text(guide_line1_label, "已切换配网热点");
        lv_label_set_text_fmt(guide_line2_label, "%s", apSsid.c_str());
        lv_label_set_text_fmt(guide_line3_label, "密码: %s", AP_PASSWORD);
        lv_label_set_text_fmt(guide_line4_label, "访问 %s", portalIp);
        lv_label_set_text(guide_hint_label, "请重新配置WiFi名称密码");
    }
    else if (mode == GUIDE_WIFI_LOST)
    {
        lv_label_set_text(guide_title_label, "WiFi异常");
        lv_label_set_text(guide_line1_label, "搜索不到WiFi...");
        lv_label_set_text_fmt(guide_line2_label, "目标: %s", deviceConfig.wifiSsid);
        lv_label_set_text(guide_line3_label, "后台正在尝试重连");
        lv_label_set_text(guide_line4_label, "");
        lv_label_set_text(guide_hint_label, "请检查路由器与信号");
    }
    else if (mode == GUIDE_IKUAI_FAILED)
    {
        lv_label_set_text(guide_title_label, "爱快登录失败");
        lv_label_set_text(guide_line1_label, "账号或密码错误");
        lv_label_set_text(guide_line2_label, "请访问");
        lv_label_set_text_fmt(guide_line3_label, "%s", portalIp);
        lv_label_set_text(guide_line4_label, "重新配置爱快凭据");
        lv_label_set_text(guide_hint_label, "请确认凭据以免账号被锁定");
    }
    else if (mode == GUIDE_IKUAI_LOST)
    {
        lv_label_set_text(guide_title_label, "爱快异常");
        lv_label_set_text(guide_line1_label, "无法连接爱快...");
        lv_label_set_text_fmt(guide_line2_label, "地址: %s", getIkuaiServerAddress());
        lv_label_set_text(guide_line3_label, "后台正在尝试重连");
        lv_label_set_text_fmt(guide_line4_label, "配置页 %s", portalIp);
        lv_label_set_text(guide_hint_label, "请检查爱快是否在线");
    }

    lv_obj_set_hidden(guide_page, false);
    lv_obj_move_foreground(guide_page);
}

static void hideGuidePage()
{
    if (guide_page != NULL)
    {
        lv_obj_set_hidden(guide_page, true);
    }
    currentGuideMode = GUIDE_NONE;
}

static GuideMode getGuideMode()
{
    return currentGuideMode;
}

#endif
