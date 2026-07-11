#ifndef _MAIN_H
#define _MAIN_H

// MQTT服务器配置（精准匹配华为云）
#define SERVER_IP_ADDR          "b95325cee1.st1.iotda-device.cn-north-4.myhuaweicloud.com"
#define SERVER_IP_PORT           1883

// MQTT主题配置
#define MQTT_CMDTOPIC_SUB       "$oc/devices/6a3a6da1cbb0cf6bb96829a4_WHYwhy/sys/commands/#"
#define MQTT_DATATOPIC_PUB      "$oc/devices/6a3a6da1cbb0cf6bb96829a4_WHYwhy/sys/properties/report"
#define MQTT_CLIENT_RESPONSE    "$oc/devices/6a3a6da1cbb0cf6bb96829a4_WHYwhy/sys/commands/response/request_id=%s"

#define IOT
// 认证信息
#ifdef IOT
#define CLIENT_ID               "6a3a6da1cbb0cf6bb96829a4_WHYwhy_0_0_2026062312"
#define DEVICEID                "6a3a6da1cbb0cf6bb96829a4_WHYwhy"
#define CLIENTPASSWORD          "b1ce70df86af38ecd9bc63142b4b87ece13a2d305fa2f5fa2ec144e317eec06c"
#endif

// WiFi 热点账号密码配置
#define CONFIG_WIFI_SSID        "123456"      
#define CONFIG_WIFI_PWD         "why060324"        

#endif // _MAIN_H