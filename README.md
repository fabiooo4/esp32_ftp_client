# FTP Client

This ESP32 project is a FTP client that can download files from a remote server
and store them in flash memory.

## Prerequisites

- ESP32 development board
- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/v5.4.1/esp32/get-started/index.html#ide) v5.4.1 or later

## Installation

1. Clone the repository:
   ```
   git clone https://github.com/fabiooo4/esp32_ftp_client.git
   cd esp32_ftp_client
   ```
2. Build the project:
   ```
   idf.py build
   ```
3. Flash the project to the ESP32 (flash also builds the project):
   ```
   idf.py flash
   ```
4. Monitor the ESP32:
   ```
   idf.py monitor
   ```

# Configuration

- [FTP Client configuration](#ftp-client-configuration)
- [Default configuration](#default-configuration)

The project is configured using the ESP-IDF menuconfig tool. To open the
configuration menu, run the following command:

```
idf.py menuconfig
```

## FTP Client configuration

All the possible FTP Client configurations can be found in the `menuconfig`
under `FTP Client configuration`.

### Wifi station

In `Wifi station configuration` set the SSID and password of the wifi network
you want to connect to. You can also change other parameters.

### FTP Server

In `FTP Server configuration` set the IP and Port of the FTP server
you want to connect to.

If the FTP server requires authentication, set the username and password
in `FTP Server authentication` menu.

## Default configuration

#### Configuration options

- [Flash size](#set-the-correct-flash-size)
- [Custom partition table](#custom-partition-table)
  - [Create filesystem images from the contents of a host folder](#create-filesystem-images-from-the-contents-of-a-host-folder)
- [Wifi setup](#wifi-setup)

### Flash size

In `Serial Flasher Config/Flash Size` set the amount of flash memory available
on your model of microcontroller.

### Custom partition table

In `Partition Table/Partition Table` set the option "Custom Partition Table CSV".
This option expects a "partitions.csv" file that defines the custom partitions.

To add another storage partition add the configuration line to `partitions.csv`,
for example:

```csv
# Name,  Type, SubType, Offset,   Size,     Flags
storage, data, spiffs,  0x110000, 0x100000,
```

Spiffs stands for "SPI Flash File System" wich is a lightweight filesystem
that is designed to work with flash memory. The last two numbers are the
start address and the size of the partition.

#### Create filesystem images from the contents of a host folder

To add default files to the ESP32 filesystem from the project root folder, add
the following line to `main/CMakeLists.txt`:

```cmake
spiffs_create_partition_image(<partition> <base_dir> [FLASH_IN_PROJECT] [DEPENDS dep dep dep...])
```

With this command the build configuration is automatically passed to the tool,
ensuring that the generated image is valid for that build.

Optionally, you can opt to have the image automatically flashed together with
the app binaries, partition tables, etc. on `idf.py flash` by specifying
`FLASH_IN_PROJECT`. For example:

```cmake
spiffs_create_partition_image(my_spiffs_partition my_folder FLASH_IN_PROJECT)
```

## References

- [ESP-IDF Storage API](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/storage/spiffs.html#)
- [ESP-IDF Wifi Driver](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/wifi.html#wi-fi-driver)
