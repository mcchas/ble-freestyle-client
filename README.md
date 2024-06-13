# wt32-freestyle-client

## Overview

The **wt32-freestyle-client** is a BLE client for the [Gainsborough Freestyle Trilock](https://www.gainsboroughhardware.com.au/en/featured/gainsborough-freestyle.html) smart door lock. Made by Allegion, this client may also work with other Allegion locks.

Originally developed for a common WROOM32 ESP32, I transitioned to an Ethernet-based board to improve BLE connection consistency, as the ESP32 can't run both WiFi and BLE simultaneously.

Using this client, you can lock/unlock the door in under 2 seconds, compared to the cloud API's 20-30 second response time.

This can remove the need for cloud connectivity for your lock.

## Getting Started

Designed for the [WT32-ETH01](https://github.com/egnor/wt32-eth01) development board (an ESP32 with Ethernet). Other boards using the same PHY will work, other hardware would take some adaptations.

### Programming the WT32

   - Open the project with Visual Studio Code and PlatformIO to fetch dependencies and build.
   - Connect WT32 (TX, RX, GND & VCC) to a 3.3V TTL USB UART adapter.

      <img src="https://community-assets.home-assistant.io/original/3X/9/1/911e031e4933ea72762ae0a9fb87e723f9138b93.jpeg" alt="wiring" width="400"/>

   - Connect GPIO0 to GND while powering on the board to enter programming mode.
   - Hit the upload button in PlatformIO.

### Setup

1. **Network Connection:**
   - Connect the RJ45 port to a _DHCP-enabled_ LAN and power on the WT32.
   - The IP address will be shown in the serial log.

2. **Web Configuration:**
   - Open the IP address in a browser.
   - If prompted for a username and password, leave them blank.
   - Navigate to the setup page and fill in the configuration details.

### Configuration Options

| Option        | Description                                         |
|---------------|-----------------------------------------------------|
| Hostname      | Any name for this device                            |
| AES Key       | The base64 encoded value of the Offline Key for your lock |
| BLE Mac       | The Bluetooth MAC address of your lock              |
| MQTT Server   | MQTT server IP address                              |
| MQTT Topic    | Topic to use, defaults to `trilock`                 |
| HTTP Username | Username for web/api/ota, default is blank          |
| HTTP Password | Password for web/api/ota, default is blank          |

## Disclaimer

This is a reverse engineering project and is not affiliated with Gainsborough or Allegion.

## To Do

- Improve this README
- Support repurposing the Freestyle Bridge (built on ESP8266 + EFR32)
- Retrieve keys to decrypt Sync channel messages (e.g., door open/closed notifications, config etc)
- Support fragmented packets (not needed for lock/unlock)
