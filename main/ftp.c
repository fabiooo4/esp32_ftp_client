#include "esp_bit_defs.h"
#include "esp_err.h"
#include "esp_log.h"
#include "ftplib.h"

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
  // Open FTP server
  ESP_LOGI(FTP_TAG, "FTP Server IP: %s", FTP_SERVER_IP);
  ESP_LOGI(FTP_TAG, "FTP User     : %s", FTP_USER);

  static NetBuf_t *ftpClientNetBuf = NULL;

  int connect = FtpConnect(FTP_SERVER_IP, FTP_SERVER_PORT, &ftpClientNetBuf);

  ESP_LOGI(FTP_TAG, "connect=%d", connect);
  if (connect == 0) {
    ESP_LOGE(FTP_TAG, "FTP server connect fail");
    return FTP_FAILURE;
  }

  // Login FTP server
  int login = FtpLogin(FTP_USER, FTP_PASSWORD, ftpClientNetBuf);

  ESP_LOGI(FTP_TAG, "login=%d", login);

  if (login == 0) {
    ESP_LOGE(FTP_TAG, "FTP server login fail");
    return FTP_FAILURE;
  }

  return FTP_SUCCESS;
}
