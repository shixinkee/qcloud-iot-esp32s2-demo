/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2019 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */
#include <string.h>
#include <esp_log.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"

#include "nvs_flash.h"
#include "nvs.h"

#include "esp_err.h"
#include "esp_log.h"

#define FACTORY_QUICK_REBOOT_TIMEOUT        5000
#define FACTORY_QUICK_REBOOT_MAX_TIMES      5
#define FACTORY_QUICK_REBOOT_TIMES          "q_rt"

#define qcloud_KV_RST                       "qcloud.rst"

#define NVS_PARTITION_NAME  "nvs"
#define NVS_KV              "iotkit-kv"
#define STA_SSID_KEY             "stassid"
#define STA_PASSWORD_KEY         "pswd"


static const char *TAG = "factory_rst";

static bool s_kv_init_flag;

esp_err_t HAL_Kv_Init(void)
{
    esp_err_t ret = ESP_OK;

    do {
        if (s_kv_init_flag == false) {
            ret = nvs_flash_init_partition(NVS_PARTITION_NAME);

            if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
                ESP_ERROR_CHECK(nvs_flash_erase_partition(NVS_PARTITION_NAME));
                ret = nvs_flash_init_partition(NVS_PARTITION_NAME);
            } else if (ret != ESP_OK) {
                ESP_LOGE(TAG, "NVS Flash init %s failed!", NVS_PARTITION_NAME);
                break;
            }

            s_kv_init_flag = true;
        }
    } while (0);

    return ret;
}

int HAL_Kv_Del(const char *key)
{
    nvs_handle handle;
    esp_err_t ret;

    if (key == NULL) {
        ESP_LOGE(TAG, "HAL_Kv_Del Null key");
        return ESP_FAIL;
    }

    if (HAL_Kv_Init() != ESP_OK) {
        return ESP_FAIL;
    }

    ret = nvs_open_from_partition(NVS_PARTITION_NAME, NVS_KV, NVS_READWRITE, &handle);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs open %s failed with %x", NVS_KV, ret);
        return ESP_FAIL;
    }

    ret = nvs_erase_key(handle, key);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs erase key %s failed with %x", key, ret);
    } else {
        nvs_commit(handle);
    }

    nvs_close(handle);

    return ret;
}

int HAL_Kv_Get(const char *key, void *val, int *buffer_len)
{
    nvs_handle handle;
    esp_err_t ret;

    if (key == NULL || val == NULL || buffer_len == NULL) {
        ESP_LOGE(TAG, "HAL_Kv_Get Null params");
        return ESP_FAIL;
    }

    if (HAL_Kv_Init() != ESP_OK) {
        return ESP_FAIL;
    }

    ret = nvs_open_from_partition(NVS_PARTITION_NAME, NVS_KV, NVS_READONLY, &handle);

    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "nvs open %s failed with %x", NVS_KV, ret);
        return ESP_FAIL;
    }

    ret = nvs_get_blob(handle, key, val, (size_t *) buffer_len);

    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "nvs get blob %s failed with %x", key, ret);
    }

    nvs_close(handle);

    return ret;
}

int HAL_Kv_Set(const char *key, const void *val, int len, int sync)
{
    nvs_handle handle;
    esp_err_t ret;

    if (key == NULL || val == NULL || len <= 0) {
        ESP_LOGE(TAG, "HAL_Kv_Set NULL params");
        return ESP_FAIL;
    }

    if (HAL_Kv_Init() != ESP_OK) {
        return ESP_FAIL;
    }

    ret = nvs_open_from_partition(NVS_PARTITION_NAME, NVS_KV, NVS_READWRITE, &handle);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs open %s failed with %x", NVS_KV, ret);
        return ESP_FAIL;
    }

    ret = nvs_set_blob(handle, key, val, len);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs erase key %s failed with %x", key, ret);
    } else {
        nvs_commit(handle);
    }

    nvs_close(handle);

    return ret;
}

static esp_err_t factory_restore_handle(void)
{
    esp_err_t ret = ESP_OK;
    int quick_reboot_times = 0;

    /**< If the device restarts within the instruction time, the event_mdoe value will be incremented by one */
    int length = sizeof(int);
    ret = HAL_Kv_Get(FACTORY_QUICK_REBOOT_TIMES, &quick_reboot_times, &length);

    quick_reboot_times++;

    ret = HAL_Kv_Set(FACTORY_QUICK_REBOOT_TIMES, &quick_reboot_times, sizeof(int), 0);

    if (quick_reboot_times >= FACTORY_QUICK_REBOOT_MAX_TIMES) {
        char rst = 0x01;

        /*  since we cannot report reset status to cloud in this stage, just set the reset flag.
            when connects to cloud, awss will do the reset report. */
        ret = HAL_Kv_Set(qcloud_KV_RST, &rst, sizeof(rst), 0);  
        ret = HAL_Kv_Del(FACTORY_QUICK_REBOOT_TIMES);

        ESP_LOGW(TAG, "factory restore");
          //擦除配网信息
         HAL_Kv_Del(STA_SSID_KEY); 
         HAL_Kv_Del(STA_PASSWORD_KEY);
    } else {
        ESP_LOGI(TAG, "quick reboot times %d, don't need to restore", quick_reboot_times);
    }

    return ret;
}

static void factory_restore_timer_handler(void *timer)
{
    if (!xTimerStop(timer, 0)) {
        ESP_LOGE(TAG, "xTimerStop timer %p", timer);
    }

    if (!xTimerDelete(timer, 0)) {
        ESP_LOGE(TAG, "xTimerDelete timer %p", timer);
    }

    /* erase reboot times record */
    HAL_Kv_Del(FACTORY_QUICK_REBOOT_TIMES);

    ESP_LOGI(TAG, "Quick reboot timeout, clear reboot times");
}

esp_err_t factory_restore_init(void)
{
#ifdef CONFIG_IDF_TARGET_ESP32
    if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_UNDEFINED) {
        HAL_Kv_Del(FACTORY_QUICK_REBOOT_TIMES);
        return ESP_OK;
    }
#endif

    TimerHandle_t timer = xTimerCreate("factory_clear", FACTORY_QUICK_REBOOT_TIMEOUT / portTICK_RATE_MS,
                                       false, NULL, factory_restore_timer_handler);

    xTimerStart(timer, portMAX_DELAY);

    return factory_restore_handle();
}
