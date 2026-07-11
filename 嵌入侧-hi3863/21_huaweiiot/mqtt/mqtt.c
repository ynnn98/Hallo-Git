/*
 * Copyright (c) 2024 Beijing HuaQingYuanJian Education Technology Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "soc_osal.h"
#include "app_init.h"
#include "cmsis_os2.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "MQTTClientPersistence.h"
#include "MQTTClient.h"
#include "errcode.h"
#include "../wifi/wifi_connect.h"
#include "../app_main.h"
#include "../dht11/dht11.h"
#include "../led/led.h"
#include "../motor/motor.h"
#include "../co2/co2.h"
#include "../servo2/servo2.h"

#ifndef unused
#define unused(var)     (void)(var)
#endif

osThreadId_t mqtt_init_task_id; // mqtt subscribe task

#define KEEP_ALIVE_INTERVAL 120
#define DELAY_TIME_MS 100


char g_send_buffer[260] = {0}; // publish data buffer
char g_response_id[100] = {0}; // save command id buffer

MQTTClient_message pubmsg = MQTTClient_message_initializer;
MQTTClient_deliveryToken token;

// global static buffer
static char topicBuf[256] = {0};
static char dataBuf[1024] = {0};
#define     JSON_Tree_Format   "{ \n "                  \
                                "\"services\": [{ \n"       \
                                "\"service_id\": \"王昊洋\", \n" \
                                "\"properties\": { \n" \
                                    "\"Temp\":  \"%4.2f\", \n" \
                                    "\"Humi\":  \"%4.2f\", \n" \
                                    "\"Lumi\":  \"%d\", \n"   \
                                    "\"Dist\":  \"%d\" ,\n"   \
                                    "\"LampSt\":  \"%s\", \n" \
                                    "\"Fengd\":  \"%s\", \n"  \
                                    "\"CO2\":  \"%d\", \n"    \
                                    "\"DangGuangBan\":  \"%s\", \n" \
                                    "\"Bump\":  \"%s\" \n"  \
                                    "}, \n"   \
                                "\"event_time\": \"\" \n" \
                                "} \n"  \
                                "] \n"  \
                                "}\n"


char A_JSON_Tree[640] = {0};    // JSON tree buffer

char g_response_buf[] =
    "{\"result_code\": 0,\"response_name\": \"smartPet\",\"paras\": {\"result\": \"success\"}}"; // response json
uint8_t g_cmdFlag;
MQTTClient client;
volatile MQTTClient_deliveryToken deliveredToken;
extern int MQTTClient_init(void);
static osal_mutex g_mux_id;

extern DHT11_Data_TypeDef DHT11_Data;
extern char LampSt[4];
extern int lampState;

extern int32_t distance;
extern uint16_t alsData;
extern uint16_t ldr_value;
extern uint16_t co2_value;      // CO2 (PPM)

extern char FanSt[4];
extern int fanState;
extern int windSpeed;
extern char Fengd[4];
extern char DangGuangBan[4];
extern char BumpSt[4];
extern int bumpState;
extern void pump_on(void);
extern void pump_off(void);

// Create JSON tree
//===================================================================================================
static void  Setup_JSON_Tree_JX(void)
{

    printf(" into setup json\n");
    memset(A_JSON_Tree, 0, 640);
    sprintf(A_JSON_Tree, JSON_Tree_Format,
            DHT11_Data.temperature, DHT11_Data.humidity,
            ldr_value, distance,
            LampSt,
            Fengd,
            co2_value,
            DangGuangBan,
            BumpSt);

    printf("\r\n-------------------- create JSON tree -------------------\r\n");
    printf("%s", A_JSON_Tree);
    printf("\r\n--------------------create JSON tree  -------------------\r\n");
}


/* callback: connection lost */
void connlost(void *context, char *cause)
{
    unused(context);
    printf("Connection lost: %s\n", cause);
}
int mqtt_subscribe(const char *topic)
{
    printf("subscribe start\r\n");
    MQTTClient_subscribe(client, topic, 1);
    return 0;
}

int mqtt_publish(const char *topic, char *msg)
{
    int ret = 0;
    pubmsg.payload = msg;
    pubmsg.payloadlen = (int)strlen(msg);
    pubmsg.qos = 1;
    pubmsg.retained = 0;
    ret = MQTTClient_publishMessage(client, topic, &pubmsg, &token);

    if (ret != MQTTCLIENT_SUCCESS) {
        printf("mqtt publish failed\r\n");
        return ret;
    }
    return ret;
}

/* callback: message delivered */
void delivered(void *context, MQTTClient_deliveryToken dt)
{
    unused(context);
    deliveredToken = dt;
}

void parse_after_equal(const char *input, char *output)
{
    const char *equalsign = strchr(input, '=');
    if (equalsign != NULL) {
        strcpy(output, equalsign + 1);
    }
}

/* callback: message arrived */
int messageArrived(void *context, char *topic_name, int topic_len, MQTTClient_message *message)
{
    unused(context);
    uint16_t data_len = message->payloadlen;

    char *tmpT = NULL;
    if (topic_len >= (int)sizeof(topicBuf)) {
        printf("Topic length exceeds buffer size!\n");
        topic_len = sizeof(topicBuf) - 1;
    }
    if (data_len >= sizeof(dataBuf)) {
        printf("Data length exceeds buffer size!\n");
        data_len = sizeof(dataBuf) - 1;
    }

    memset(topicBuf, 0, sizeof(topicBuf));
    memset(dataBuf, 0, sizeof(dataBuf));
    memcpy(topicBuf, topic_name, topic_len);
    topicBuf[topic_len] = '\0';
    memcpy(dataBuf, (char *)message->payload, data_len);
    dataBuf[data_len] = '\0';

    printf("[MQTT] Receive topic: %s, data: %s \r\n", topicBuf, dataBuf);
    printf("[MQTT] Topic len: %d, Data len: %d\r\n", topic_len, data_len);

    // 1. Light control
    tmpT = strstr(dataBuf, "Light");
    if(tmpT != NULL)
    {
        if(strstr(dataBuf, "ON") != NULL) {
            printf("[MQTT] Command: led on\r\n");
            lampState = 0;
            led_on(1);
        }
        if(strstr(dataBuf, "OFF") != NULL) {
            printf("[MQTT] Command: led off\r\n");
            lampState = 1;
            led_off(1);
        }
    }

    // 2. Conditioner control
    tmpT = strstr(dataBuf, "Condi");
    if(tmpT != NULL)
    {
        if(strstr(dataBuf, "ON") != NULL) printf("[MQTT] Command: condition on\r\n");
        else printf("[MQTT] Command: condition off\r\n");
    }

    // 3. TV control
    tmpT = strstr(dataBuf, "TV");
    if(tmpT != NULL)
    {
        printf("[MQTT] Command: tv command comein\r\n");
        if(strstr(dataBuf, "ON") != NULL) printf("[MQTT] Command: tv now on\r\n");
        else printf("[MQTT] Command: tv now off\r\n");
    }

    // 4. Wash control
    tmpT = strstr(dataBuf, "Wash");
    if(tmpT != NULL)
    {
        printf("[MQTT] Command: Wash command comein\r\n");
        if(strstr(dataBuf, "ON") != NULL) printf("[MQTT] Command: Wash now on\r\n");
        else printf("[MQTT] Command: Wash now off\r\n");
    }

    // 5. BeepWarning control
    tmpT = strstr(dataBuf, "BeepWarning");
    if(tmpT != NULL)
    {
        printf("[MQTT] Command: BeepWarning command comein\r\n");
        if(strstr(dataBuf, "ON") != NULL) printf("[MQTT] Command: BeepWarning now on\r\n");
        else printf("[MQTT] Command: BeepWarning now off\r\n");
    }

    // 6. Control mode
    tmpT = strstr(dataBuf, "CtlMode");
    if(tmpT != NULL)
    {
        if(strstr(dataBuf, "AUTO") != NULL) printf("[MQTT] Command: auto control mode\r\n");
        if(strstr(dataBuf, "HUMAN") != NULL) printf("[MQTT] Command: human control mode\r\n");
        if(strstr(dataBuf, "VOICE") != NULL) printf("[MQTT] Command: voice control mode\r\n");
    }

    // 7. Light degree
    tmpT = strstr(dataBuf, "LightDegree");
    if(tmpT != NULL)
    {
        printf("[MQTT] Command: LightDegree\r\n");
        printf("[MQTT] DataBuf: %s\r\n", dataBuf);
    }

    // 8. Fan control (10档)
    tmpT = strstr(dataBuf, "Fan");
    if(tmpT != NULL)
    {
        printf("[MQTT] Command: Fan command comein\r\n");
        if(strstr(dataBuf, "ON") != NULL) {
            printf("[MQTT] Command: Fan ON\r\n");
            fanState = 0;
            windSpeed = 5;  // 默认5档
            snprintf_s(Fengd, sizeof(Fengd), sizeof(Fengd)-1, "%d", windSpeed);
            motor_set_speed(windSpeed * 10, MOTOR_FORWARD);
        }
        else if(strstr(dataBuf, "OFF") != NULL) {
            printf("[MQTT] Command: Fan OFF\r\n");
            fanState = 1;
            windSpeed = 0;
            memset(Fengd, 0, sizeof(Fengd));
            fan_off();
        }
        else {
            // 尝试解析档位: "Fan_3", "Fan_5" 等
            for (int i = 1; i <= 10; i++) {
                char levelStr[4];
                snprintf_s(levelStr, sizeof(levelStr), sizeof(levelStr)-1, "%d", i);
                if (strstr(dataBuf, levelStr) != NULL) {
                    windSpeed = i;
                    snprintf_s(Fengd, sizeof(Fengd), sizeof(Fengd)-1, "%d", windSpeed);
                    motor_set_speed(windSpeed * 10, MOTOR_FORWARD);
                    printf("[MQTT] Fan speed set to %d\r\n", windSpeed);
                    fanState = 0;
                    break;
                }
            }
        }
    }

    // 10. Smoke alarm
    tmpT = strstr(dataBuf, "Smoke");
    if(tmpT != NULL)
    {
        printf("[MQTT] Command: Smoke alarm command comein\r\n");
        if(strstr(dataBuf, "ON") != NULL) printf("[MQTT] Command: Smoke alarm ON\r\n");
        else printf("[MQTT] Command: Smoke alarm OFF\r\n");
    }

    // 11. Door (servo) control
    tmpT = strstr(dataBuf, "Door");
    if(tmpT != NULL)
    {
        printf("[MQTT] Command: Door command comein\r\n");
        if(strstr(dataBuf, "ON") != NULL) {
            printf("[MQTT] Command: Door ON\r\n");
            Door_ON();
        }
        if(strstr(dataBuf, "OFF") != NULL) {
            printf("[MQTT] Command: Door OFF\r\n");
            Door_OFF();
        }
    }

    // 12. Fengdegree 风档位指令（华为云命令下发格式）
    tmpT = strstr(dataBuf, "Fengdegree");
    if(tmpT != NULL)
    {
        printf("[MQTT] Command: Fengdegree comein\r\n");
        // 从JSON中解析 fengdegree 数值，如 {"fengdegree": 9}
        char *valPtr = strstr(dataBuf, "fengdegree");
        if(valPtr != NULL) {
            char *colon = strchr(valPtr, ':');
            if(colon != NULL) {
                int degree = atoi(colon + 1);
                if(degree >= 1 && degree <= 10) {
                    windSpeed = degree;
                    snprintf_s(Fengd, sizeof(Fengd), sizeof(Fengd)-1, "%d", windSpeed);
                    motor_set_speed(windSpeed * 10, MOTOR_FORWARD);
                    fanState = 0;
                    printf("[MQTT] Fan speed set to %d via Fengdegree\r\n", windSpeed);
                } else {
                    // 0或非法值 → 关风扇
                    fanState = 1;
                    windSpeed = 0;
                    memset(Fengd, 0, sizeof(Fengd));
                    fan_off();
                    printf("[MQTT] Fan OFF via Fengdegree\r\n");
                }
            }
        }
    }

    // 13. Bump 水泵指令（云端下发 ON/OFF）
    tmpT = strstr(dataBuf, "Bump");
    if(tmpT != NULL)
    {
        printf("[MQTT] Command: Bump command comein\r\n");
        if(strstr(dataBuf, "ON") != NULL) {
            bumpState = 1;
            pump_on();
            printf("[MQTT] Bump ON\r\n");
        }
        if(strstr(dataBuf, "OFF") != NULL) {
            bumpState = 0;
            pump_off();
            printf("[MQTT] Bump OFF\r\n");
        }
    }

    // parse command id
    parse_after_equal(topic_name, g_response_id);
    sprintf(g_send_buffer, MQTT_CLIENT_RESPONSE, g_response_id);

    g_cmdFlag = 1;

    memset((char *)message->payload, 0, message->payloadlen);
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topic_name);

    return 1;
}

static errcode_t mqtt_connect(void)
{
    int ret;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    MQTTClient_init();
    ret = MQTTClient_create(&client, SERVER_IP_ADDR, CLIENT_ID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    if (ret != MQTTCLIENT_SUCCESS) {
        printf("Failed to create MQTT client, return code %d\n", ret);
        return ERRCODE_FAIL;
    }
    conn_opts.keepAliveInterval = KEEP_ALIVE_INTERVAL;
    conn_opts.cleansession = 1;
#ifdef IOT
    conn_opts.username = DEVICEID;
    conn_opts.password = CLIENTPASSWORD;
#endif
    MQTTClient_setCallbacks(client, NULL, connlost, messageArrived, delivered);

    if ((ret = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        printf("Failed to connect, return code %d\n", ret);
        MQTTClient_destroy(&client);
        return ERRCODE_FAIL;
    }
    printf("Connected to MQTT broker!\n");
    osDelay(DELAY_TIME_MS);
    mqtt_subscribe(MQTT_CMDTOPIC_SUB);

    return ERRCODE_SUCC;
}

void mqtt_init_task(const char *argument)
{
    unused(argument);
    osDelay(DELAY_TIME_MS);

    // 初始化互斥锁
    osal_mutex_init(&g_mux_id);

    // MQTT连接失败则重试
    while (mqtt_connect() != ERRCODE_SUCC) {
        printf("[MQTT] Connection failed, retry after 3s...\r\n");
        osDelay(30);  // 等待3秒后重试 (30 * 100ms)
    }

    while(1) {
        osDelay(DELAY_TIME_MS);
        if (g_cmdFlag) {
            sprintf(g_send_buffer, MQTT_CLIENT_RESPONSE, g_response_id);
            osal_mutex_lock_timeout(&g_mux_id, 10);
            mqtt_publish(g_send_buffer, g_response_buf);
            osal_mutex_unlock(&g_mux_id);
            g_cmdFlag = 0;
            memset(g_response_id, 0, sizeof(g_response_id) / sizeof(g_response_id[0]));
        }

        printf("construct json tree\r\n");
        Setup_JSON_Tree_JX();
        mqtt_publish(MQTT_DATATOPIC_PUB, A_JSON_Tree);
        memset(A_JSON_Tree, 0, 640);
        osDelay(DELAY_TIME_MS);
    }
}

void mqtt_app_start(void)
{
    osThreadAttr_t options;
    options.name = "mqtt_init_task";
    options.attr_bits = 0;
    options.cb_mem = NULL;
    options.cb_size = 0;
    options.stack_mem = NULL;
    options.stack_size = 0x6000;
    options.priority = osPriorityNormal;

    mqtt_init_task_id = osThreadNew((osThreadFunc_t)mqtt_init_task, NULL, &options);
    if (mqtt_init_task_id != NULL) {
        printf("ID = %d, Create mqtt_init_task_id is OK!", mqtt_init_task_id);
    }
}
