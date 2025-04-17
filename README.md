# FTP Client
This ESP32 project is a FTP client that can download files from a remote server
and store them in flash memory

# Menuconfig
## Set the correct flash size
In `Serial Flasher Config/Flash Size` set the amount of flash memory available
on your model of microcontroller.

## Custom partition table
In `Partition Table/Partition Table` set the option "Custom Partition Table CSV".
This option expects a "partitions.csv" file that defines the custom partitions.
To add a storage partition add the following line:
`storage,  data, spiffs, 0x110000, 0x100000,`
Spiffs stands for "SPI Flash File System" wich is a lightweight filesystem
that is designed to work with flash memory. The last two numbers are the
start address and the size of the partition.

### Create filesystem images from the contents of a host folder
To create default files for the filesystem, add the following line to
`main/CMakeLists.txt` file:
```cmake
spiffs_create_partition_image(<partition> <base_dir> [FLASH_IN_PROJECT] [DEPENDS dep dep dep...])
```
With this command the build configuration is automatically passed to the tool,
ensuring that the generated image is valid for that build.

#### Automatically flash filesystem image
Optionally, you can opt to have the image automatically flashed together with
the app binaries, partition tables, etc. on `idf.py flash` by specifying
`FLASH_IN_PROJECT`. For example:
```cmake
spiffs_create_partition_image(my_spiffs_partition my_folder FLASH_IN_PROJECT)
```

# References
- [ESP-IDF documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/storage/spiffs.html#)
