# WirelessAdapter

A wireless communication system built on two ESP32 using the ESP-NOW protocol. This project provides a transmitter-receiver pair for wireless data transmission.

## Overview

This repository contains firmware for creating a wireless adapter system consisting of:

- **Wireless Transmitter**: Receives data from an HID, and packages and sends data wirelessly to the RX using ESP-NOW
- **Wireless Receiver**: Receives data wirelessly using ESP-NOW and delivers input-reports to a host PC.
- **ESP-NOW MAC Utility**: Helper utility to retrieve device MAC addresses for pairing
- **Custom Components**: Custom components and configuration shared across RX and TX architecture.

## Features

- **ESP-NOW Protocol**: Fast, wireless communication between ESP32 devices
- **Low Latency**: Below 5ms RTT on Average
- **Bidirectional Communication**: Both transmitter and receiver components
- **Modular Design**: Structured with custom components and decoupled architecture for easier extension
- **CMake Build System**: Standard ESP-IDF build configuration

## Project Structure

```
WirelessAdapter/
├── wireless_transmitter-2.0/   # Transmitter firmware
│   ├── main/                   # Main application code
│   ├── CMakeLists.txt          # Build configuration
│   └── structure.puml          # Architecture diagram
├── wireless_receiver-2.0/      # Receiver firmware
│   ├── main/                   # Main application code
│   ├── CMakeLists.txt          # Build configuration
│   └── structure.puml          # Architecture diagram
├── espnow_getmac/              # MAC address utility
│   ├── main/                   # Main application code
│   └── CMakeLists.txt          # Build configuration
└── custom_components/          # Reusable components
```

## Prerequisites

- **ESP-IDF**: Espressif IoT Development Framework (version 4.x or 5.x)
- **ESP32 Development Board**: Any ESP32 series board that supports both ESP_NOW & USB_OTG
- **USB Cable**: For flashing and serial communication
- **Python 3.x**: Required by ESP-IDF

## Installation

### 1. Install ESP-IDF

Follow the [official ESP-IDF installation guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html) for your operating system.

### 2. Clone this Repository

```bash
git clone https://github.com/AndrewTheeFirst/WirelessAdapter.git
cd WirelessAdapter
```

## Usage

### Getting MAC Addresses

Before pairing devices, you need to know their MAC addresses:

```bash
cd espnow_getmac
idf.py build flash monitor
```

Note the MAC address displayed in the serial output.

### Building and Flashing the Transmitter

```bash
cd wireless_transmitter-2.0
idf.py build flash monitor
```

### Building and Flashing the Receiver

```bash
cd wireless_receiver-2.0
idf.py build flash monitor
```

## Configuration

### Pairing Devices

1. Flash the `espnow_getmac` utility to both ESP32 boards to obtain their MAC addresses
2. Update the MAC addresses in the transmitter and receiver code
3. Rebuild and reflash both devices

## Architecture

The project uses PlantUML diagrams (`structure.puml`) in each component directory to document the architecture. View these files with a PlantUML viewer or plugin.

## Author

Andrew ([@AndrewTheeFirst](https://github.com/AndrewTheeFirst))

## Acknowledgments

- ESP-IDF framework by Espressif Systems
- ESP-NOW protocol documentation and examples
