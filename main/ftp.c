#include "esp_bit_defs.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/idf_additions.h"
#include "ftplib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
  // Connect to FTP server
  if (!FtpConnect(FTP_SERVER_IP, FTP_SERVER_PORT, &ftp_connection)) {
    error("Connection failed");
    return FTP_FAILURE;
  }
  ESP_LOGI(FTP_TAG, "Connected to FTP: %s", FtpGetLastResponse(ftp_connection));

  // Login to FTP server
  if (!FtpLogin(FTP_USER, FTP_PASSWORD, ftp_connection)) {
    error("Login failed");
    return FTP_FAILURE;
  }
  ESP_LOGI(FTP_TAG, "Logged in to FTP: %s", FtpGetLastResponse(ftp_connection));

  // ls
  ESP_LOGI(FTP_TAG, "Listing remote directory:");
  if (!FtpDir(NULL, ".", ftp_connection)) {
    error("Failed to list remote directory");
    return FTP_FAILURE;
  }

  // Create a directory
  ESP_LOGI(FTP_TAG, "Creating directory testDir");
  if (!FtpMakeDir("testDir", ftp_connection)) {
    ESP_LOGI(FTP_TAG, "MakeDir failed: %s", FtpGetLastResponse(ftp_connection));
  }

  // Put the file in spiffs to the remote server
  ESP_LOGI(FTP_TAG, "Putting file (text.txt) to remote server (./text.txt)");
  if (!FtpPut("/storage/text.txt", "text.txt", FTPLIB_ASCII, ftp_connection)) {
    error("Failed to put \"text.txt\" to remote server \"./text.txt\"");
    return FTP_FAILURE;
  }

  ESP_LOGI(FTP_TAG,
           "Putting file (text.txt) to remote server (./testDir/textDir.txt)");
  if (!FtpPut("/storage/text.txt", "testDir/textDir.txt", FTPLIB_ASCII,
              ftp_connection)) {
    error("Failed to put \"text.txt\" to remote server \"./testDir/text.txt\"");
    return FTP_FAILURE;
  }

  // ls
  ESP_LOGI(FTP_TAG, "Listing remote directory:");
  if (!FtpDir(NULL, ".", ftp_connection)) {
    error("Failed to list remote directory");
    return FTP_FAILURE;
  }

  // Change directory
  ESP_LOGI(FTP_TAG, "Changing directory to testDir");
  if (!FtpChangeDir("testDir", ftp_connection)) {
    error("Failed to change directory");
    return FTP_FAILURE;
  }

  // pwd
  char pwd_buf[32] = {0};
  if (!FtpPwd(pwd_buf, sizeof(pwd_buf), ftp_connection)) {
    error("Failed to get PWD");
    return FTP_FAILURE;
  }
  ESP_LOGI(FTP_TAG, "Current directory: %s", pwd_buf);

  // ls
  ESP_LOGI(FTP_TAG, "Listing remote directory:");
  if (!FtpDir(NULL, ".", ftp_connection)) {
    error("Failed to list remote directory");
    return FTP_FAILURE;
  }

  // Print contents of a file
  ESP_LOGI(FTP_TAG, "Getting file (textDir.txt):");
  if (!FtpGet(NULL, "textDir.txt", FTPLIB_ASCII, ftp_connection)) {
    error("Failed to get file");
    return FTP_FAILURE;
  }

  // Remove file
  ESP_LOGI(FTP_TAG, "Removing file (textDir.txt)");
  if (!FtpDelete("textDir.txt", ftp_connection)) {
    error("Failed to delete file");
  }

  // Go to parent directory
  ESP_LOGI(FTP_TAG, "Going to parent directory");
  if (!FtpChangeDirUp(ftp_connection)) {
    error("Failed to go to parent directory");
    return FTP_FAILURE;
  }

  // pwd
  if (!FtpPwd(pwd_buf, sizeof(pwd_buf), ftp_connection)) {
    error("Failed to get PWD");
    return FTP_FAILURE;
  }
  ESP_LOGI(FTP_TAG, "Current directory: %s", pwd_buf);

  // Remove directory
  ESP_LOGI(FTP_TAG, "Removing directory (testDir)");
  if (!FtpRemoveDir("testDir", ftp_connection)) {
    error("Failed to remove directory");
    return FTP_FAILURE;
  }

  // Rename file
  ESP_LOGI(FTP_TAG, "Renaming file (text.txt) to (renamed.txt)");
  if (!FtpRename("text.txt", "renamed.txt", ftp_connection)) {
    error("Failed to rename file");
    return FTP_FAILURE;
  }

  // ls
  ESP_LOGI(FTP_TAG, "Listing remote directory:");
  if (!FtpDir(NULL, ".", ftp_connection)) {
    error("Failed to list remote directory");
    return FTP_FAILURE;
  }

  // Access file
  ESP_LOGI(FTP_TAG, "Accessing file (renamed.txt)");
  NetBuf_t *remote_file = NULL;
  if (!FtpAccess("renamed.txt", FTPLIB_FILE_WRITE, FTPLIB_ASCII, ftp_connection,
                 &remote_file)) {
    error("Failed to access file");
    return FTP_FAILURE;
  }

  // Edit file
  ESP_LOGI(FTP_TAG, "Editing file (renamed.txt)");
  if (FtpWrite("Hello World\n", 12, remote_file) < 12) {
    error("Failed to write to file");
    return FTP_FAILURE;
  }

  // Close file
  FtpClose(remote_file);

  // ls
  ESP_LOGI(FTP_TAG, "Listing remote directory:");
  if (!FtpDir(NULL, ".", ftp_connection)) {
    error("Failed to list remote directory");
    return FTP_FAILURE;
  }

  // Get file and check contents
  ESP_LOGI(FTP_TAG, "Getting file (renamed.txt)");
  if (!FtpGet("/storage/remoteGet.txt", "renamed.txt", FTPLIB_ASCII,
              ftp_connection)) {
    error("Failed to get file");
    return FTP_FAILURE;
  }

  // Compare the contents of "/storage/remoteGet.txt" with "Hello World\n"
  FILE *fd = fopen("/storage/remoteGet.txt", "r");
  if (fd == NULL) {
    ESP_LOGE(FTP_TAG, "Failed to open file");
    return FTP_FAILURE;
  }

  char line[13];
  fgets(line, sizeof(line), fd);
  fclose(fd);

  if (strcmp(line, "Hello World\n") != 0) {
    ESP_LOGE(FTP_TAG, "File contents do not match");
    return FTP_FAILURE;
  } else {
    ESP_LOGI(FTP_TAG, "File contents match edited content");
  }
  ESP_LOGI(FTP_TAG, "File contents: %s", line);

  // Remove file
  ESP_LOGI(FTP_TAG, "Removing file (renamed.txt)");
  if (!FtpDelete("renamed.txt", ftp_connection)) {
    error("Failed to delete file");
  }

  // Close FTP connection
  FtpQuit(ftp_connection);

  return FTP_SUCCESS;
}
