"""
ISS Tracker Script with Arduino Home Handshake
-----------------------------------------------
This script fetches the International Space Station's (ISS) position
from the N2YO API and sends it to an Arduino over serial. The Arduino
will not track ISS data until it confirms the "home" coordinates are set.

Requirements:
- Python 3.x
- requests library (pip install requests)
- pyserial library (pip install pyserial)
- tkinter (built-in with most Python installations)

Before running:
1. Get a free API key from https://www.n2yo.com/api/
2. Replace the placeholders in CONFIG with your API key, location, and Arduino serial port.
"""

import time
import requests
import serial
import tkinter as tk
from tkinter import messagebox

# ============================================================
# CONFIGURATION (edit these for your setup)
# ============================================================

API_KEY = "YOUR_API_KEY_HERE"        # <-- Replace with your N2YO API key
SAT_ID = 25544                       # ISS NORAD ID (leave as 25544 for ISS)
LAT = 0.0                            # <-- Replace with your latitude (decimal degrees)
LON = 0.0                            # <-- Replace with your longitude (decimal degrees)
ALT = 0                              # Altitude in meters (0 is fine for most cases)

SERIAL_PORT = "COM3"                 # <-- Replace with your Arduino serial port
BAUD = 9600                          # Match this with Arduino sketch

UPDATE_INTERVAL = 4                  # Seconds between API updates
RETRY_INTERVAL = 30                  # Seconds between retries after failure
MAX_RETRIES = 10                     # Max retry attempts before pausing

# ============================================================
# SERIAL SETUP
# ============================================================

ser = serial.Serial(SERIAL_PORT, BAUD, timeout=1)
time.sleep(2)  # Give Arduino time to reset after opening port

# ============================================================
# FUNCTIONS
# ============================================================

def get_iss_pos():
    """
    Fetch the ISS position from the N2YO API.
    Returns a tuple: (latitude, longitude, elevation) or None if error.
    """
    url = f"https://api.n2yo.com/rest/v1/satellite/positions/{SAT_ID}/{LAT}/{LON}/{ALT}/1/&apiKey={API_KEY}"
    resp = requests.get(url)

    if resp.status_code != 200:
        print("API Error:", resp.status_code)
        return None

    data = resp.json()
    positions = data.get("positions")
    if not positions:
        return None

    pos = positions[0]
    return pos['satlatitude'], pos['satlongitude'], pos['elevation']

def show_popup(title, message):
    """
    Show a simple Tkinter popup message.
    Blocks execution until the user closes the window.
    """
    root = tk.Tk()
    root.withdraw()  # Hide main window
    messagebox.showinfo(title, message)
    root.destroy()

# ============================================================
# MAIN LOOP
# ============================================================

connection_restored_shown = False
home_set = False   # Flag to track if Arduino has confirmed home

while True:
    retries = 0

    # First, attempt to set "home" on Arduino
    while not home_set:
        try:
            line = f"{LAT},{LON},0,HOME\n"  # Home handshake message
            ser.write(line.encode())
            print(f"Sending HOME coordinates: {LAT},{LON}")
            time.sleep(1)  # Wait for Arduino response

            # Check Arduino response
            if ser.in_waiting > 0:
                response = ser.readline().decode('utf-8').strip()
                if response == "HOME_OK":
                    print("Arduino confirmed home coordinates.")
                    home_set = True
                    break
                else:
                    print("Arduino response:", response)

            retries += 1
            if retries >= MAX_RETRIES:
                show_popup("Arduino Not Responding",
                           f"Failed to set home coordinates after {MAX_RETRIES} retries.\n"
                           f"Ensure Arduino is connected and powered, then restart script.")
                retries = 0  # Reset retries after popup
            else:
                time.sleep(RETRY_INTERVAL)

        except Exception as e:
            print("Error sending home coordinates:", e)
            time.sleep(RETRY_INTERVAL)

    # After home is set, continue normal ISS tracking
    while home_set:
        retries = 0
        while retries < MAX_RETRIES:
            try:
                result = get_iss_pos()
                if result:
                    lat, lon, el = result
                    timestamp = int(time.time())
                    line = f"{lat},{lon},{el},{timestamp}\n"
                    ser.write(line.encode())
                    print(f"Sent ISS data: {line.strip()}")

                    # Read Arduino reply safely
                    try:
                        if ser.in_waiting > 0:
                            arduino_response = ser.readline().decode('utf-8').strip()
                            if arduino_response:
                                try:
                                    pan_angle, tilt_angle, elev_angle, reverse_flag = arduino_response.split(',')
                                    pan_angle   = float(pan_angle)
                                    tilt_angle  = float(tilt_angle)
                                    elev_angle  = float(elev_angle)
                                    reverse_flag = int(reverse_flag)
                                    print(f"Arduino -> Pan: {pan_angle:.1f}, Tilt: {tilt_angle:.1f}, Elev: {elev_angle:.1f}, Reverse: {reverse_flag}")
                                except ValueError:
                                    print("Malformed Arduino response:", arduino_response)
                    except Exception as e:
                        print("Arduino serial error, continuing:", e)

                    # Notify if connection was restored
                    if retries > 0 and not connection_restored_shown:
                        show_popup("Connection Restored", "ISS API connection restored.")
                        connection_restored_shown = True

                    break  # Success, exit retry loop

                else:
                    raise Exception("Failed to get ISS position")

            except Exception as e:
                print(f"Error fetching ISS position: {e}")
                retries += 1
                if retries >= MAX_RETRIES:
                    show_popup("Connection Failed",
                               f"Failed to get ISS position after {MAX_RETRIES} retries.\n"
                               f"Script will pause until this window is closed.")
                    connection_restored_shown = False
                    retries = 0
                else:
                    time.sleep(RETRY_INTERVAL)
                    continue

        # Normal update interval
        time.sleep(UPDATE_INTERVAL)
