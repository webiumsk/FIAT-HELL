# FIAT HELL LN ATM

FIAT HELL LN ATM is a bitcoin ATM project built on the ESP32 platform. It allows users to convert fiat currencies to bitcoin using the Lightning Network. This project is designed to be highly configurable, supporting multiple currencies and funding sources.
Its based on [Ben's FOSSA DIY ATM](https://github.com/lnbits/fossa)

![FIAT HELL - LN ATM](https://storage.googleapis.com/geyser-images-distribution-prod-us/cdc6a4fc-218b-497d-9b7e-85b5b9b7ed2b_signal-2024-06-05-145915_005/image_large.webp)

## Features

- **Multiple Currencies Support**: Configurable to support various fiat currencies (Up to 3).
- **Funding Sources**: Supports two funding sources LNbits and Blink.
- **Works Offline**: With [LNbits](https://lnbits.com) or **Online**: With [Blink](https://blink.sv) or [LNbits](https://lnbits.com)
- **Animation**: Enable or disable animations on the main screen.
- **Configurable Settings**: Easily configure some basic settings through a dedicated settings screen.
- **Dynamic UI**: Adjusts UI elements based on configuration (e.g., single currency mode displays "START" button).

## Hardware Requirements

- ESP32 WT32-SC01 - [Aliexpress](https://www.aliexpress.com/item/1005003191471709.html)
- NV10 USB+ note acceptor [EBAY](https://www.ebay.com/sch/i.html?_from=R40&_trksid=p3519243.m570.l1313&_nkw=nv10+usb%2B&_sacat=0)
- 12V to 5V Step Down Converter [Aliexpress](https://www.aliexpress.com/item/1005006407431542.html)
- 12V battery [I'm using this one from LIDL](https://www.lidl.de/p/parkside-12v-akku-papk-12-b4-4-ah/p100362831), or any 12V power source
- [3D printed battery holder](https://www.thingiverse.com/thing:4445077/makes)
- CH3.96 connectors [Aliexpress](https://www.aliexpress.com/item/32959512753.html)
- Dupont cables [Aliexpress](https://www.aliexpress.com/item/1005004022062472.html)
  - M-M 20cm 2x
  - M-F 20cm 4x

## Software Requirements

- VSCode and PlatformIO

## Getting Started

### Prerequisites

- Install VSCode and PlatformIO
- PlatformIO will download and install all necessary libraries defined in platformio.ini

#### LNbits

You need a running instance of LNbits

#### Blink

Install Blink wallet on your phone, and use their API.

### Installation

1. Clone the repository:
```
    git clone https://github.com/webiumsk/fiat-hell.git
```

2. Open the project in VSCode.

3. Upload the project to your ESP32 board through PlatformIO. 

### Configuration

Access the Config Portal to configure various parameters:
1. Tap the screen during the initial loading animation
2. On your phone or PC, connect to the device (find device with name "LN ATM-xxx")
3. Set your preferences, currencies and wallets
4. Change the AP password!
5. Change the Settings screen PIN code!
6. "Reset" will restart the ATM

**Some prefences can be set during the runtime.**
Tap the dot in top right corner of main screen.
Type your PIN (configured through the Config Portal)

- Funding Source: Choose between LNbits and Blink.
- Exchange: Default is CoinGecko, but for some exotic currencies, like Hondurasan Lempira, you need switch to ExchangeApi
- Enable/Disable Animation: Toggle animation of the Title on the main screen.
- From this screen you can reset the ATM or run the Config Portal 

### Usage

On startup, the ATM will connect to the configured WiFi network and initialize the main screen.
Use the settings button on the top-right corner to access the settings screen.
Tap the currency buttons to select a currency and proceed with the transaction.

### Troubleshooting

    ESP32 Not Starting: Ensure sufficient heap memory and check for stack overflow issues.
    WiFi Connection Issues: Verify WiFi credentials and network availability.
    UI Elements Not Visible: Check the configuration and ensure the display is properly connected.

### WIP

    This project is work in progress. 

## Contributing

Contributions are welcome! Please fork the repository and create a pull request with your changes.
License

This project is licensed under the MIT License - see the LICENSE file for details.

## Contact

For any inquiries, please contact dr750@protonmail.com
