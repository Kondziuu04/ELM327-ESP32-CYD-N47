ESP32 OBD-II TFT Display

A compact OBD-II data display system designed to show real-time vehicle diagnostics
on a TFT screen. The project communicates with the vehicle via an ELM327 Bluetooth
adapter and displays selected engine parameters.

⸻

Features

	•	Real-time OBD-II data acquisition via Bluetooth (ELM327)
	•	TFT-based data visualization (ESP32-CYD / external TFT during experiments)
	•	Standard OBD-II PIDs (limited subset)
	•	BMW-specific extended PIDs
	•	Tested on BMW E87 118d

Displayed parameters include:

	•	Engine coolant temperature
	•	Engine oil temperature
	•	Manifold absolute pressure (boost estimation)
	•	Battery voltage

⸻

Important Notes

ELM327 Bluetooth Configuration

The Bluetooth configuration for the ELM327 adapter is hardcoded in the source code.

Bluetooth PIN
To pair with the ELM327 adapter, a fixed PIN is used. If your adapter uses a different PIN,
you must modify it directly in main.cpp:

esp_bt_gap_set_pin(ESP_BT_PIN_TYPE_FIXED, 4, (uint8_t *)"1234");

Dynamic pairing or runtime PIN configuration is not implemented.

ELM327 MAC Address
The MAC address of the ELM327 adapter must also be hardcoded in the source code:

uint8_t elmMAC[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

You must replace this address with the MAC address of your own ELM327 device for the
Bluetooth connection to work correctly.

⸻

Engine Compatibility

⚠️ BMW-specific PIDs implemented in this project are valid only for BMW N47 diesel engines.
	•	BMW extended PIDs were tested exclusively on BMW N47
	•	Other BMW engines or manufacturers are not supported
	•	Standard OBD-II fallback is limited and not guaranteed to work on all ECUs

If you plan to use this project with another engine type, PID definitions and parsing logic must be adapted.

⸻

TFT Configuration Note

A working User_Setup.h configuration for the TFT_eSPI library
is provided in this repository.

⸻

Project Status

⚠️ Project archived due to hardware constraints

The project was intentionally stopped after the failure of the ESP32-CYD board used in the original hardware setup.
The project is considered finished in its current form and is planned to be reimplemented and 
continued on an STM32-based platform, with a properly matched display and power architecture.

⸻
