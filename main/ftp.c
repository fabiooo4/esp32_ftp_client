#include "esp_bit_defs.h"
#include "esp_err.h"
#include "esp_log.h"

// Menuconfig ----------------------------
#define FTP_SERVER_IP CONFIG_FTP_SERVER_IP
#define FTP_SERVER_PORT CONFIG_FTP_SERVER_PORT
#define FTP_USER CONFIG_FTP_SERVER_USER
#define FTP_PASSWORD CONFIG_FTP_SERVER_PASSWORD
// Menuconfig ----------------------------

#define FTP_SUCCESS BIT0
#define FTP_FAILURE BIT1

static const char *FTP_TAG = "FTP";

esp_err_t connect_ftp_server(void) {
  ESP_LOGE(FTP_TAG, "Todo: Implement FTP client");
  return FTP_FAILURE;
}
