#include "esp_err.h"
#include "ftp.c"
#include "nvs_flash.h"
#include "wifi.c"
#include <esp_log.h>
#include <esp_spiffs.h>
#include <stdio.h>

static const char *FS_TAG = "Filesystem setup";

esp_err_t init_spiffs(char *mount_point, char *partition_label) {
  esp_vfs_spiffs_conf_t spiffs_conf = {
      .base_path = mount_point,
      .partition_label = partition_label,
      .max_files = 1,
      .format_if_mount_failed = true,
  };

  esp_err_t result = esp_vfs_spiffs_register(&spiffs_conf);
  if (result != ESP_OK) {
    ESP_LOGE(FS_TAG, "Failed to initialize SPIFFS: %s",
             esp_err_to_name(result));
    return result;
  }

  // Free space calculation
  size_t total = 0, used = 0;
  result = esp_spiffs_info(spiffs_conf.partition_label, &total, &used);
  if (result != ESP_OK) {
    ESP_LOGE(FS_TAG, "Failed to obtain partition info: %s",
             esp_err_to_name(result));
    return result;
  } else {
    ESP_LOGI(FS_TAG, "Partition size: total: %d, used %d", total, used);
  }

  return result;
}

void app_main(void) {
  esp_err_t status = WIFI_FAILURE;

  // Initialize storage
  esp_err_t res = nvs_flash_init();
  if (res == ESP_ERR_NVS_NO_FREE_PAGES ||
      res == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    res = nvs_flash_init();
  }
  ESP_ERROR_CHECK(res);

  // Connect to AP
  status = connect_wifi();
  if (WIFI_SUCCESS != status) {
    ESP_LOGE(WIFI_TAG, "Failed to associate to AP, dying...");
    return;
  }

  // SPIFFS Filesystem setup
  if (init_spiffs("/storage", NULL) != ESP_OK)
    return;

  // Read a file from the storage
  FILE *fd = fopen("/storage/text.txt", "r");
  if (fd == NULL) {
    ESP_LOGE(FS_TAG, "Failed to open file");
    return;
  }

  char line[32];
  fgets(line, sizeof(line), fd);

  fclose(fd);
  printf("%s\n", line);

  status = connect_ftp_server();
  if (WIFI_SUCCESS != status) {
    ESP_LOGE(FTP_TAG, "Error occured in FTP Client, dying...");
    return;
  }
}
