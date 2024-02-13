#include <string.h>
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <errno.h>
#include <esp_log.h>

#ifndef LEDCONTROL_NVS_STORAGE_H
#define LEDCONTROL_NVS_STORAGE_H

esp_err_t init_nvs();

esp_err_t erase_specific_nvs_namespace(const char *namespace);

esp_err_t write_data_to_nvs(int16_t data, const char *NVS_NAMESPACE, const char *KEY_DATA);

esp_err_t read_data_from_nvs(const char *NVS_NAMESPACE, const char *KEY_DATA);

#endif //LEDCONTROL_NVS_STORAGE_H
