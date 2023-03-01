# Compressor Monitor

Monitor / shut off your dive compressor with an ESP32 board.

![Assembled Front](assets/front.jpg)

## Parts list:

- ESP32 board (https://www.aliexpress.com/item/32801296418.html) as the controller;
- pressure sensor (https://www.aliexpress.com/item/32397492502.html), connected to ADC through a resistive voltage divider;
- relay module (https://www.aliexpress.com/item/1005002409810818.html), to disable the engine's ignition when over the pressure limit;
- display module (https://www.aliexpress.com/item/32840857700.html);
- voltage regulator (https://www.aliexpress.com/item/32266661300.html), can be replaced with any 5V source;
- piezo buzzer (https://www.aliexpress.com/item/32352820863.html), with a NPN transistor as a driver;
- buttons for input, connectors, case.

A detailed schematic will follow once I get around to it.

## Update 11 April 2021

I have now added web monitoring: Add your WiFi SSID / password into src/config.h, and the monitor will connect to the wireless network on startup. Then enter the IP address displayed on the screen into your mobile / desktop browser, and the monitoring page will load:

![Web Based Monitoring](assets/compressor_monitor_web.png)

## Update 2 March 2023

My experience is showing that monitoring and in particularly alerting works badly for the web based UI due to the monitoring thread being killed by the browser if the tab the UI is running is not active. The same goes for attempts to implement monitoring in an Android application - the power usage optimiser on modern phones tends to kill any background monitoring threads.
For this reason I have implemented a [simple UI client on an inexpensive M5Stick development kit](m5_client/).

In addition to this I have added a crude estimation of the time left until the currently filling tank reaches the configured pressure limit. This is also used to start warning at a set time interval before the tank is full (default: 2 minutes).
