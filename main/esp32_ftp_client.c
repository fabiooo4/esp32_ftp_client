#include "esp_err.h"
#include <esp_log.h>
#include <esp_spiffs.h>
#include <stdio.h>

static const char *FS_TAG = "Filesystem setup";

void app_main(void) {
  // Filesystem setup
  esp_vfs_spiffs_conf_t spiffs_conf = {
      .base_path = "/storage",
      .partition_label = NULL,
      .max_files = 1,
      .format_if_mount_failed = true,
  };

  esp_err_t result = esp_vfs_spiffs_register(&spiffs_conf);
  if (result != ESP_OK) {
    ESP_LOGE(FS_TAG, "Failed to initialize SPIFFS: %s",
             esp_err_to_name(result));
    return;
  }

  // Free space calculation
  size_t total = 0, used = 0;
  result = esp_spiffs_info(spiffs_conf.partition_label, &total, &used);
  if (result != ESP_OK) {
    ESP_LOGE(FS_TAG, "Failed to obtain partition info: %s",
             esp_err_to_name(result));
    return;
  } else {
    ESP_LOGI(FS_TAG, "Partition size: total: %d, used %d", total, used);
  }

  // Read a file from the storage
  FILE *fd = fopen("/storage/text.txt", "r");
  if (fd == NULL) {
    ESP_LOGE(FS_TAG, "Failed to open file");
    return;
  }

  char line[13];
  fgets(line, sizeof(line), fd);

  fclose(fd);
  printf("%s\n", line);
}
