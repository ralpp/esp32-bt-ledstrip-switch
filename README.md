
# ESP32 Local Wi-Fi Switch for Bluetooth LED Strip Controllers
This project uses an **ESP32** as a local Wi-Fi server to control multiple Bluetooth LED strip controllers from a single interface.

Instead of using separate mobile apps for each controller, the ESP32 acts as a main hub that sends payloads to each LED controller.

The system currently controls:
- 3 × MELK-based LED controllers  
- 1 × LEDNETWF-based LED controller  
- 1 × MRSTAR-based LED controller  

The Switch is controlled via a local web server, which can be used in iOS shortcuts to sync it with other Wi-Fi-controlled devices. 

# How to use
1. Replace Wi-Fi credentials (ESP32 only runs on 2.4G networks)
```cpp
	const char* ssid = "";
	const char* password = ""; 
```
2.  Replace BT MAC addresses
```cpp
	BLEAddress melkAddrs[3] = {
	  BLEAddress("BE:69:FF:06:67:11"),
	  BLEAddress("BE:67:00:78:14:8A"),
	  BLEAddress("BE:67:00:53:11:A5")
	};

	BLEAddress mrStar("E4:98:BB:F3:19:E2");
	BLEAddress lednetAddr("08:65:F0:92:5F:4E")
```
3. Compile on ESP32, check on Serial Monitor on which IP it used.
