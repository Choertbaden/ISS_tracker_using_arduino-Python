ISS Tracker with Arduino

This project fetches the current position of the International Space Station (ISS) from the N2YO API
 and sends the data to an Arduino over serial. The Arduino can then use this data (e.g., to control servos and track the ISS in real-time).

The system now includes a home coordinate handshake: the Arduino will not track ISS data until it confirms the "home" latitude and longitude have been set by the Python script.

*--------------------------------------------------------------------------------------*

Features

- Retrieves ISS position (latitude, longitude, elevation).
- Sends data to Arduino in CSV format via serial.
- Home coordinates handshake ensures Arduino starts tracking only after confirming home.
- Reads and prints Arduino's response (servo angles, status flags).
- Built-in error handling with automatic retries.
- Pop-up notifications when connection fails or is restored.

*--------------------------------------------------------------------------------------*

Requirements

- Python 3.x
- Python libraries:
- requests (pip install requests)
- pyserial (pip install pyserial)
- tkinter (usually built-in)
- Arduino (Uno, Mega, or similar)
- Three standard servos (SG90 or similar)

*--------------------------------------------------------------------------------------*

Hardware Setup

1. Servo Wiring:

- Pan (horizontal) → Arduino pin 10
- Tilt (latitude) → Arduino pin 6
- Elevation → Arduino pin 5
- Servos also require 5V power and GND connected to Arduino.

2. Powering Arduino:

- Ensure the Arduino is powered before launching the Python script.
- USB power is sufficient for small servos. For multiple or high-torque servos, use an external 5V power source.

3. Serial Connection:

- Connect Arduino via USB.
- Note the serial port:
  -- Windows: COMx (e.g., COM3)
  -- Linux/Mac: /dev/ttyUSB0 or similar

*--------------------------------------------------------------------------------------*

Configuration

Before running the script, configure your API key, location, and serial port.

1. Get your Latitude & Longitude

- Open Google Maps (https://www.google.com/maps)
- Right-click your location and select "What's here?"
- Copy the coordinates in decimal format, e.g.:
-  -25.653974, 28.243838

Set in Python script:
LAT = -25.653974
LON = 28.243838

2. Get a Free API Key

- Go to the N2YO API signup page (https://www.n2yo.com/api/)
- Register for a free account
- Copy your personal API key and replace in the Python script:
  API_KEY = "YOUR_API_KEY_HERE"

3. Set Arduino Serial Port
   
  SERIAL_PORT = "COM3"   # Windows
  SERIAL_PORT = "/dev/ttyUSB0"  # Linux/Mac  

*--------------------------------------------------------------------------------------*

Running the System

1. Upload Arduino sketch (provided in this repo) to your Arduino.
2. Power Arduino. Ensure it is ready and connected via USB.
3. Run the Python script:
    - python iss_tracker.py
  
*--------------------------------------------------------------------------------------*

Home Handshake

- The Python script sends the home coordinates to Arduino first.
- Arduino replies with "HOME_OK" when it has successfully stored home.
- ISS tracking begins only after home is confirmed.
- If Arduino is not connected, Python will retry sending home until confirmation is received.

*--------------------------------------------------------------------------------------*

Data Format

Python → Arduino:
 - latitude,longitude,elevation,timestamp

Home handshake message (first message):
 - latitude,longitude,0,HOME

Arduino → Python (debug):
 - pan_angle,tilt_angle,elev_angle,reverse_flag

Example:
 - Sent: -25.65,28.24,417.5,1693155421
 - Arduino -> Pan: 123.4, Tilt: 45.6, Elev: 30.2, Reverse: 0

*--------------------------------------------------------------------------------------*

Notes

- The system uses ISS NORAD ID 25544 but can be adapted for other satellites by changing SAT_ID.
- Free N2YO accounts have request limits; the script polls every 4 seconds by default.
- Ensure the Arduino sketch is uploaded and powered before running Python.
- The handshake ensures the first ISS position does not overwrite home coordinates.

*--------------------------------------------------------------------------------------*

Arduino Sketch Overview

- Pins used: 10 (pan), 6 (tilt), 5 (elevation)
- Servos move smoothly using a weighted average to reduce jitter
- Home handshake ensures Arduino stores the "home" coordinates before tracking ISS
- Debug values are sent back to Python for monitoring
