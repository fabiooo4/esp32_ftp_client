#include "esp_bit_defs.h"
#include "esp_err.h"
#include "esp_log.h"

#define FTP_SUCCESS BIT0
#define FTP_FAILURE BIT1

static const char *FTP_TAG = "FTP";

esp_err_t connect_ftp_server(void) {
  ESP_LOGE(FTP_TAG, "Todo: Implement FTP client");
  return FTP_FAILURE;
}
