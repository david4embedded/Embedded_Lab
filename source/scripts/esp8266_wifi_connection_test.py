import serial
import time
import argparse

# --- Configuration values ---
# Default baud rate for AT firmware
BAUD_RATE = 115200

# String appended to AT commands
EOL = b'\r\n'

def send_at_command(ser, command, timeout=1):
   """
   Sends an AT command and retrieves the response.
   """
   print(f"    ESP8266 AT Sending: {command}")
   ser.write(command + EOL)
   time.sleep(timeout)
   response = ser.read_all()
   print(f"    ESP8266 AT Received: {response.decode('utf-8')}")
   return response

def main():
   parser = argparse.ArgumentParser(description="Connect to Wi-Fi using ESP8266.")
   parser.add_argument("port", help="Serial Port")
   parser.add_argument("ssid", help="Wi-Fi SSID")
   parser.add_argument("password", help="Wi-Fi Password")
   args = parser.parse_args()

   SERIAL_PORT = args.port
   wifi_ssid = args.ssid
   wifi_password = args.password

   try:
      # Open the serial port
      print(f"Opening {SERIAL_PORT} port...")
      ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=5)
      print("COM port opened successfully.")
      
      # Wait briefly before sending AT commands
      time.sleep(2)
      
      # 1. Test AT command
      print("Testing an AT command...")
      response = send_at_command(ser, b'AT')
      if b'OK' not in response:
         print("ERROR: ESP8266 not responding to AT command.")
         return

      # 2. Set Wi-Fi mode (station mode)
      print("Setting Wi-Fi mode to station...")
      response = send_at_command(ser, b'AT+CWMODE=1')
      if b'OK' not in response:
         print("ERROR: Failed to set Wi-Fi mode.")
         return
         
      # 3. Connect to Wi-Fi
      print(f"Connecting to Wi-Fi SSID...")
      join_command = f'AT+CWJAP="{wifi_ssid}","{wifi_password}"'.encode('utf-8')
      response = send_at_command(ser, join_command, timeout=10)
      
      if b'WIFI CONNECTED' in response and b'WIFI GOT IP' in response:
         print("SUCCESS: Connected to Wi-Fi!")
      else:
         print("ERROR: Failed to connect to Wi-Fi.")
         return

      # 4. Check assigned IP address
      print("Checking assigned IP address...")
      response = send_at_command(ser, b'AT+CIFSR')
      
   except serial.SerialException as e:
      print(f"Serial port error: {e}")
   except Exception as e:
      print(f"An error occurred: {e}")
   finally:
      if 'ser' in locals() and ser.is_open:
         ser.close()
         print(f"Serial port {SERIAL_PORT} closed.")

if __name__ == "__main__":
   main()
