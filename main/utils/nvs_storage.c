#include "nvs_storage.h"

#define LENGTH 16

//初始化nvs_flash
esp_err_t init_nvs() {
    esp_err_t err = nvs_flash_init();
    if (nvs_flash_init() != ESP_OK)
        ESP_LOGE("TAG", "Failed to open file for writing. Error: %s", strerror(errno));

    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

//写数据
esp_err_t write_data_to_nvs(int16_t data, const char *NVS_NAMESPACE, const char *KEY_DATA) {
    // 检查是否需要初始化 NVS
    if (nvs_flash_init() != ESP_OK) {
        // 如果未初始化，则尝试初始化
        if (nvs_flash_erase() != ESP_OK || nvs_flash_init() != ESP_OK) {
            // 如果初始化失败，返回错误
            return ESP_FAIL;
        }
    }

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE("TAG", "Failed to open file for writing. Error: %s", strerror(errno));
        return err;
    }

    err = nvs_set_i16(nvs_handle, KEY_DATA, data);
    if (err != ESP_OK) {
        ESP_LOGE("TAG", "Failed to open file for writing. Error: %s", strerror(errno));
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE("TAG", "Failed to open file for writing. Error: %s", strerror(errno));
        nvs_close(nvs_handle);
        return err;
    }

    nvs_close(nvs_handle);
    ESP_LOGI("TAG", "write nvs successed...");
    return ESP_OK;
}

//读数据
esp_err_t read_data_from_nvs(const char *NVS_NAMESPACE, const char *KEY_DATA) {
    int16_t data = 0;
    // 检查是否需要初始化 NVS
    if (nvs_flash_init() != ESP_OK) {
        // 如果未初始化，则尝试初始化
        if (nvs_flash_erase() != ESP_OK || nvs_flash_init() != ESP_OK) {
            // 如果初始化失败，返回错误
            return ESP_FAIL;
        }
    }

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) return err;

    err = nvs_get_i16(nvs_handle, KEY_DATA, &data);

    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(nvs_handle);
        return err;
    }

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(nvs_handle);
        return 0;
    }

    esp_err_to_name(err);
    nvs_close(nvs_handle);
    return data;
}

//擦除特定namespace下内容
esp_err_t erase_specific_nvs_namespace(const char *namespace) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_flash_init();

    // 检查是否需要初始化 NVS
    if (nvs_flash_init() != ESP_OK) {
        // 如果未初始化，则尝试初始化
        if (nvs_flash_erase() != ESP_OK || nvs_flash_init() != ESP_OK) {
            // 如果初始化失败，返回错误
            return ESP_FAIL;
        }
    }
    //如果满了就全部擦除
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    if (err != ESP_OK) {
        return err;
    }

    err = nvs_open(namespace, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_erase_all(nvs_handle);
    if (err != ESP_OK) {
        nvs_close(nvs_handle);
        return err;
    }

    nvs_close(nvs_handle);
    return ESP_OK;
}