# FIAT HELL LN ATM

FIAT HELL LN ATM is a cryptocurrency ATM project built on the ESP32 platform. It allows users to convert fiat currencies to cryptocurrency using the Lightning Network. This project is designed to be highly configurable, supporting multiple currencies and funding sources.

## Features

- **Multiple Currencies Support**: Configurable to support various fiat currencies.
- **Funding Sources**: Supports multiple funding sources such as LNbits and Blink.
- **Animation**: Enable or disable animations on the main screen.
- **Configurable Settings**: Easily configure aome basic settings through a dedicated settings screen.
- **Dynamic UI**: Adjusts UI elements based on configuration (e.g., single currency mode displays "START" button).

## Hardware Requirements

- ESP32 WT32 SC01
- NV10 USB+ note acceptor
- 12V to 5V Step Down Converter
- 12V battery [I'm using this one from LIDL][https://www.lidl.de/p/parkside-12v-akku-papk-12-b4-4-ah/p100362831], or any 12V power source
- 
- Dupont cables

## Software Requirements

- VSCode and PlatformIO
- LVGL (Light and Versatile Graphics Library)
- ArduinoJson library

## Getting Started

### Prerequisites

- Install VSCode and PlatformIO
- Install the required libraries:
  - LVGL
  - ArduinoJson
  - WiFiClientSecure

### Installation

1. Clone the repository:
```
    git clone https://github.com/webiumsk/fiat-hell-ln-atm.git
```

2. Open the project in VSCode.

3. Upload the project to your ESP32 board through PlatformIO.

### Configuration

Access the Config Portal to configure various parameters:

    Funding Source: Choose between LNbits and Blink.
    Enable/Disable Animation: Toggle animations on the main screen.

### Usage

    On startup, the ATM will connect to the configured WiFi network and initialize the main screen.
    Use the settings button on the top-right corner to access the settings screen.
    Tap the currency buttons to select a currency and proceed with the transaction.

### Troubleshooting

    ESP32 Not Starting: Ensure sufficient heap memory and check for stack overflow issues.
    WiFi Connection Issues: Verify WiFi credentials and network availability.
    UI Elements Not Visible: Check the configuration and ensure the display is properly connected.

## Contributing

Contributions are welcome! Please fork the repository and create a pull request with your changes.
License

This project is licensed under the MIT License - see the LICENSE file for details.
### Acknowledgements

    LVGL for the graphics library
    ArduinoJson for the JSON library
    ESP32-TUX for WiFi provisioning example

## Contact

For any inquiries, please contact your-email@example.com.
