#include <stdio.h>
#include <esp_log.h>
#include <esp_vfs_fat.h>
#include <errno.h>
#include "esp_wifi.h"
#include "lwip/sockets.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include "esp_camera.h"
#include "driver/gpio.h"
#include "utils/nvs_storage.h"

// AI-Thinker PIN Map
#define CAM_PIN_PWDN 32
#define CAM_PIN_RESET -1 //software reset will be performed
#define CAM_PIN_XCLK 0
#define CAM_PIN_SIOD 26
#define CAM_PIN_SIOC 27

#define CAM_PIN_D7 35
#define CAM_PIN_D6 34
#define CAM_PIN_D5 39
#define CAM_PIN_D4 36
#define CAM_PIN_D3 21
#define CAM_PIN_D2 19
#define CAM_PIN_D1 18
#define CAM_PIN_D0 5
#define CAM_PIN_VSYNC 25
#define CAM_PIN_HREF 23
#define CAM_PIN_PCLK 22
#define CAM_PIN_DATA1 4 //闪光灯和DATA1共用

#define NVS_NAMESPACE "Picture_Count"

#define NVS_PICTURE_COUNT_KEY "picture_count"

#define ESP_SSID "Yangser"
#define ESP_PASS "password"

IP地址和端口号
#define ESP_IP "192.168.4.1"
#define ESP_PORT 8080

static const char *TAG = "img2img";
static sdmmc_card_t *card;

static camera_config_t camera_config = {
        .pin_pwdn = CAM_PIN_PWDN,
        .pin_reset = CAM_PIN_RESET,
        .pin_xclk = CAM_PIN_XCLK,
        .pin_sccb_sda = CAM_PIN_SIOD,
        .pin_sccb_scl = CAM_PIN_SIOC,

        .pin_d7 = CAM_PIN_D7,
        .pin_d6 = CAM_PIN_D6,
        .pin_d5 = CAM_PIN_D5,
        .pin_d4 = CAM_PIN_D4,
        .pin_d3 = CAM_PIN_D3,
        .pin_d2 = CAM_PIN_D2,
        .pin_d1 = CAM_PIN_D1,
        .pin_d0 = CAM_PIN_D0,
        .pin_vsync = CAM_PIN_VSYNC,
        .pin_href = CAM_PIN_HREF,
        .pin_pclk = CAM_PIN_PCLK,

        .xclk_freq_hz = 20000000,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,

        .pixel_format = PIXFORMAT_JPEG,
        .frame_size = FRAMESIZE_240X240,

        .jpeg_quality = 20,
        .fb_count = 2, 
        .grab_mode = CAMERA_GRAB_LATEST,
        .fb_location = CAMERA_FB_IN_DRAM,
};

static esp_err_t init_camera(void) {
    //initialize the camera
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera Init Failed");
        return err;
    }

    return ESP_OK;
}

/*// 初始化 SD 卡
static esp_err_t init_sd_card() {
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
            .format_if_mount_failed = true,
            .max_files = 5
    };

    esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SD card VFAT filesystem. Error: %d", ret);
    }

    ESP_LOGI(TAG, "init sd_card successed");

    return ret;
}*/

void init_ap() {
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_t *ap = esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_AP);

    wifi_config_t ap_config = {
            .ap = {
                    .ssid = ESP_SSID,
                    .ssid_len = strlen(ESP_SSID),
                    .password = ESP_PASS,
                    .max_connection = 4,
                    .authmode = WIFI_AUTH_WPA_WPA2_PSK
            }
    };

    esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_config);

    esp_netif_ip_info_t ip_info;

    IP4_ADDR(&ip_info.ip, 192, 168, 4, 1);
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
    IP4_ADDR(&ip_info.gw, 192, 168, 4, 1);

    esp_netif_dhcps_stop(NULL);
    esp_netif_set_ip_info(ap, &ip_info);
    esp_netif_dhcps_start(NULL);

    esp_wifi_start();
}

int create_server_socket() {
    int server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);

    if (server_socket < 0) {
        ESP_LOGE(TAG, "Failed to create socket\n");
        return -1;
    }

    struct sockaddr_in server_addr;

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(ESP_PORT);
    server_addr.sin_addr.s_addr = inet_addr(ESP_IP);

    int bind_result = bind(server_socket, (struct sockaddr *) &server_addr, sizeof(server_addr));

    if (bind_result < 0) {
        ESP_LOGE(TAG, "Failed to bind socket\n");
        return -1;
    }

    int listen_result = listen(server_socket, 5);

    if (listen_result < 0) {
        ESP_LOGE(TAG, "Failed to listen socket\n");
        return -1;
    }

    return server_socket;
}

bool receive_message(int client_socket) {
    char buffer[10];
    int recv_len = recv(client_socket, buffer, sizeof(buffer), 0);

    if (recv_len < 0) {
        ESP_LOGE(TAG, "Failed to receive data\n");
        return false;
    }
    ESP_LOGI(TAG, "Received: %s\n", buffer);
    return true;
}

void send_message(int client_socket, const void *data, size_t data_len) {
    send(client_socket, &data_len, sizeof(size_t), 0);
    int send_len = send(client_socket, data, data_len, 0);
        
    if (send_len < 0) {
        ESP_LOGE(TAG, "Failed to send data\n");
        return;
    }
    if (send_len != data_len)
        ESP_LOGW(TAG, "Incomplete data sent: %zd out of %zu bytes\n", send_len, data_len);
    else
        ESP_LOGI(TAG, "数据发送成功\n");

    ESP_LOGI(TAG, "Sent data of length: %zu\n", data_len);
}

esp_err_t send_write_image(int client_socket) {

    /*char picture_name[29];
    sprintf(picture_name, "/sdcard/%d.jpg", read_data_from_nvs(NVS_NAMESPACE, NVS_PICTURE_COUNT_KEY));

    ESP_LOGI(TAG, "Picture Count: %s", picture_name);*/

    gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << CAM_PIN_DATA1),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,

    };
    gpio_config(&io_conf);

    gpio_set_level(CAM_PIN_DATA1, 1);
    vTaskDelay(pdMS_TO_TICKS(20));
    camera_fb_t *pic = esp_camera_fb_get();
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(CAM_PIN_DATA1, 0);

    vTaskDelay(pdMS_TO_TICKS(50));

    //init_sd_card(); //DATA1使用完必须初始化sd卡，否则无法访问

    if (!pic) {
        ESP_LOGE(TAG, "Camera Capture Failed");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Picture taken! Its size was: %zu bytes", pic->len);

    /*FILE *file = fopen(picture_name, "wb");

    if (file == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing. Error: %s", strerror(errno));
        esp_camera_fb_return(pic);
        return ESP_FAIL;
    }

    size_t bytes_written = fwrite(pic->buf, 1, pic->len, file);
    if (bytes_written != pic->len) {
        ESP_LOGE(TAG, "Write failed");
        fclose(file);
        esp_camera_fb_return(pic);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Write successed");*/

    send_message(client_socket, pic->buf, pic->len);
    //vTaskDelay(pdMS_TO_TICKS(10)); 加了后端会报错，不知道为什么

    //fclose(file);
    esp_camera_fb_return(pic);

    /*write_data_to_nvs(read_data_from_nvs(NVS_NAMESPACE, NVS_PICTURE_COUNT_KEY) + 1,
                      NVS_NAMESPACE, NVS_PICTURE_COUNT_KEY);*/

    return ESP_OK;
}

void app_main(void) {
    if (ESP_OK != init_camera() || ESP_OK != init_nvs())
        return;
    init_ap();
    int server_socket = -1;

    while (server_socket < 0) {
        server_socket = create_server_socket();
        if (server_socket < 0)
            ESP_LOGE(TAG, "Failed to create server socket, retrying...\n");
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    bool img_sent = false;

    while (true) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_socket = accept(server_socket, (struct sockaddr *) &client_addr, &addr_len);
            
        if (client_socket < 0)
            ESP_LOGE(TAG, "Failed to accept client socket\n");

        ESP_LOGI(TAG, "Client connected from %s:%d\n", inet_ntoa(client_addr.sin_addr),
                 ntohs(client_addr.sin_port));

        if (!img_sent) {
            send_write_image(client_socket);
            img_sent = true;
        }
        vTaskDelay(pdMS_TO_TICKS(3000));

        if (receive_message(client_socket)) {
            close(client_socket);
            img_sent = false;
            ESP_LOGI(TAG, "Client disconnected\n");
        }

    }

}
