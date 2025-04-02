# TLV storage

A simple TLV based storage solution for EEPROM-like storage devices.

## Composition

The solution is composed of 3 main parts:

* TLV storage management
* Data types definitions
* Demo storage application

## Storages types (data models)

### TLV data model properties

  - PRODUCT_ID - Unique identifier for the product
  - PRODUCT_NAME - Human-readable product name
  - SERIAL_NO - Serial number of the device
  - PCB_NAME - Name of the printed circuit board
  - PCB_REVISION - Revision number of the PCB
  - PCB_PRDATE - Production date in YY-MM-DD format
  - PCB_PRLOCATION - Production location identifier
  - PCB_SN - Serial number of the PCB
  - MAC_ADDR_* - MAC address for network interfaces, may be specified multiple
    times for different interfaces
  - XTAL_CALDATA - Board/radio XTAL calibration data
  - RADIO_CALDATA - Radio calibration data
  - RADIO_BRDDATA - Radio board data

### Fixed fields structure data model properties

  - PRODUCT_ID - Unique identifier for the product
  - PRODUCT_NAME - Human-readable product name
  - SERIAL_NO - Serial number of the device
  - PCB_NAME - Name of the printed circuit board
  - PCB_REVISION - Revision number of the PCB
  - PCB_PRDATE - Production date in YY-MM-DD format
  - PCB_PRLOCATION - Production location identifier
  - PCB_SN - Serial number of the PCB
  - MAC_ADDR - MAC address of device

### Legacy TLV data model properties

  - PRODUCT_ID - Unique identifier for the product
  - SERIAL_NO - Serial number of the device
  - PCB_NAME - Name of the printed circuit board
  - PCB_REVISION - Revision number of the PCB
  - PCB_PRDATE - Production date in YY-MM-DD format
  - PCB_PRLOCATION - Production location identifier
  - PCB_SN - Serial number of the PCB
  - MAC_ADDR_* - MAC address for network interfaces, may be specified multiple
    times for different interfaces
  - XTAL_CALDATA - Board/radio XTAL calibration data
  - RADIO_CALDATA - Radio calibration data

## Usage

List all supported properties:

  ```bash
  tlvs -l
  ```

Get all properties:

  ```bash
  tlvs -g
  ```

Set and get specific properties:

  ```bash
  tlvs -s PRODUCT_NAME="My Device" SERIAL_NO="SN12345"
  tlvs -g PRODUCT_NAME SERIAL_NO
  ```

Setting parametrized MAC address properties:

  ```bash
  tlvs -s MAC_ADDR_eth0=aa:bb:cc:dd:ee:ff MAC_ADDR_eth1=11:22:33:44:55:66
  tlvs -g MAC_ADDR_eth0 MAC_ADDR_eth1
  ```

Import and export properties from and to file:

  ```bash
  tlvs -s PRODUCT_NAME=@name-in.txt
  tlvs -g PRODUCT_NAME=@name-out.txt
  ```

Get properties using a configuration file:

  ```bash
  tlvs -g @config.txt
  ```

  Example of config.txt content:
  ```
  PRODUCT_NAME
  SERIAL_NO
  PCB_REVISION
  MAC_ADDR_eth0
  ```

Set properties using a configuration file:

  ```bash
  tlvs -s @config.txt
  ```

  Example of config.txt content:
  ```
  PRODUCT_NAME=Example Device
  PRODUCT_ID=EX-01
  SERIAL_NO=SN12345
  PCB_NAME=MainBoard
  PCB_REVISION=0002
  PCB_PRDATE=24-03-15
  MAC_ADDR_eth0=aa:bb:cc:dd:ee:ff
  MAC_ADDR_wlan0=11:22:33:44:55:66
  ```

Use existing storage or create a new one in a file. Size option is required
when creating a new storage (e.g., 8KB), but optional when using existing
storage as size is detected automatically:

  ```bash
  tlvs -F new_storage.bin -S 8192
  ```

## Build

Build the utility with optional debug output, custom storage file and size:

  ```bash
  make [DEBUG=1] [CONFIG_TLVS_FILE=/path/to/storage.bin] [CONFIG_TLVS_SIZE=size]
  ```

  | Option | Description |
  |--------|-------------|
  | `DEBUG=1` | Enables debug build, to produce detailed operation information |
  | `CONFIG_TLVS_FILE=/path/to/storage.bin` | Specifies a default storage file path |
  | `CONFIG_TLVS_SIZE=size` | Specifies a default storage size (in bytes) |
  | `CONFIG_TLVS_COMPRESSION=<0-9>` | Specifies compression level preset (default: 9 extreme) |
  | `CONFIG_TLVS_COMPRESSION_NONE=y` | Disables compression support (default LZMA compression) |

Install the utility with optional installation prefix:

  ```bash
  make install [PREFIX=/path/to/installation]
  ```

When compression is enabled packages is linked against liblzma, therefore
`liblzma-dev` should be available in libs search path or sysroot.

## Next

  - [ ] Add more compression algorithms options
  - [ ] Add support for compressing all fields
  - [ ] Add bindings for python and LUA languages
