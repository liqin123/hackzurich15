/*
The MIT License (MIT)

Copyright (c) 2015 alu96

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "esp_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"

#include "lwip/sockets.h"

#include "uart.h"
#include "monitor_mode.h"

#include "user_config.h"
#include "gpio.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
#define DBG(str, ...) printf(str "\n", ##__VA_ARGS__)

#define printmac(buf, i) printf("\t%02X:%02X:%02X:%02X:%02X:%02X", buf[i+0], buf[i+1], buf[i+2], buf[i+3], buf[i+4], buf[i+5])

xTimerHandle timer;

void promisc_cb(uint8 *buf, uint16 len) {
    gpio_signal(1);

    printf("%d -> %3d: %d \n", system_get_time(), wifi_get_channel(), len);

    if (len == 128) {
        struct sniffer_buf2 *sb = (struct sniffer_buf2 *) buf;
        printf("p rssi:%d rate: %d ch:%d ", sb->rx_ctrl.rssi, sb->rx_ctrl.rate, sb->rx_ctrl.channel);
        printf("cnt:%d data: ", sb->cnt);
        int i;
        for (i = 0; i < sb->len; i++) {
            printf("%02X", sb->buf[i]);
        }
        printf("\n");
    } else if (len == 12) {
        struct RxControl *rx_ctrl = (struct RxControl*) buf;
        printf("rx rssi:%d rate: %d ch:%d \n",rx_ctrl->rssi, rx_ctrl->rate, rx_ctrl->channel);
    } else {
        struct sniffer_buf *sb = (struct sniffer_buf*) buf;
        printf("p rssi:%d rate: %d ch:%d \n", sb->rx_ctrl.rssi, sb->rx_ctrl.rate, sb->rx_ctrl.channel);
        printf("cnt:%d head: ", sb->cnt);
        int i;
        for (i = 0; i < 32; i++) {
            printf("%02X", sb->buf[i]);
        }
        printf("\n");
    }

    /*
    printmac(buf, 4);
    printmac(buf, 10);
    printmac(buf, 16);
    printf("\n");
     */

    gpio_signal(1);
}

void channelHop(xTimerHandle pxTimer) {
    // 1 - 13 channel hopping
    uint8 new_channel = 10; //wifi_get_channel() % 13 + 1;
    DBG("  --- hop to %d", new_channel);
    wifi_set_channel(new_channel);
}

void main_task(void *pvParameters) {
    DBG("main task...");

    gpio_init();

    wifi_promiscuous_enable(1);

    vTaskSuspend(NULL);
}

/******************************************************************************
 * FunctionName : user_init
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void user_init(void) {
    system_update_cpu_freq(160);
    uart_init_new(BAUD, NULL);

    printf("setting cpu freq to 160 MHz\n");

    printf("SDK version:%s\n", system_get_sdk_version());

#ifdef DOUBLE_CLK_FREQ
    system_update_cpu_freq(160);
#endif

    DBG("----- monitor mode test");

    DBG(" ---- set opmode");
    if (!wifi_set_opmode(0x1)) {
        DBG(" ---- > failed to set opmode");
    }
    DBG(" ---- done");

    DBG(" ---- monitor mode setup");

    vTaskDelay(100 / portTICK_RATE_MS);
    wifi_station_disconnect();
    vTaskDelay(100 / portTICK_RATE_MS);

    wifi_set_promiscuous_rx_cb(promisc_cb);

    timer = xTimerCreate("channel_hopping", CHANNEL_HOP_INTERVAL / portTICK_RATE_MS, pdTRUE, (void *) 0, channelHop);

    if (timer) {
        if (xTimerStart(timer, 0) != pdPASS) {
            DBG(" ---- failed to start timer");
        }
    } else {
        DBG(" ---- failed to create timer");
    }

    DBG(" ---- done");

    xTaskCreate(main_task, "main", 256, NULL, 2, NULL);

    DBG(" ---- vTaskStartScheduler");

    //vTaskStartScheduler();
}

#pragma clang diagnostic pop