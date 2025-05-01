#include "esp_bit_defs.h"
#include "esp_err.h"
#include "esp_log.h"
#include "ftplib.h"
#include <stdio.h>
#include <stdlib.h>

// Menuconfig ----------------------------
#define FTP_SERVER_IP CONFIG_FTP_SERVER_IP
#define FTP_SERVER_PORT CONFIG_FTP_SERVER_PORT
#define FTP_USER CONFIG_FTP_SERVER_USER
#define FTP_PASSWORD CONFIG_FTP_SERVER_PASSWORD
// Menuconfig ----------------------------

#define FTP_SUCCESS BIT0
#define FTP_FAILURE BIT1

static const char *FTP_TAG = "FTP";
static NetBuf_t *ftp_connection = NULL;

void error(char *message) {
  ESP_LOGE(FTP_TAG, "%s: %s", message, FtpGetLastResponse(ftp_connection));
}

esp_err_t connect_ftp_server(void) {
  // Open FTP server
  ESP_LOGI(FTP_TAG, "FTP Server IP: %s", FTP_SERVER_IP);
  ESP_LOGI(FTP_TAG, "FTP User     : %s", FTP_USER);

  // Connect to FTP server
  if (!FtpConnect(FTP_SERVER_IP, FTP_SERVER_PORT, &ftp_connection)) {
    error("Connection failed");
    return FTP_FAILURE;
  }
  ESP_LOGI(FTP_TAG, "Connected to FTP");
  ESP_LOGI(FTP_TAG, "%s", FtpGetLastResponse(ftp_connection));

  // Login to FTP server
  if (!FtpLogin(FTP_USER, FTP_PASSWORD, ftp_connection)) {
    error("Login failed");
    return FTP_FAILURE;
  }
  ESP_LOGI(FTP_TAG, "Logged in to FTP");
  ESP_LOGI(FTP_TAG, "%s", FtpGetLastResponse(ftp_connection));

  // ls
  if (!FtpDir(NULL, ".", ftp_connection)) {
    ESP_LOGE(FTP_TAG, "Failed to list remote directory");
    error("Failed to list remote directory");
    return FTP_FAILURE;
  }

  // Print the contents of a remote file
  char *textfile = "test.txt";
  printf("\nContents of %s:\n", textfile);
  if (!FtpGet(NULL, "test.txt", FTPLIB_ASCII, ftp_connection)) {
    ESP_LOGE(FTP_TAG, "Failed to retrieve the remote file");
    return FTP_FAILURE;
  }

  return FTP_SUCCESS;
}
