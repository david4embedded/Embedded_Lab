import serial
import time
import argparse

# --- 설정값 ---
# AT 펌웨어 기본 통신 속도
BAUD_RATE = 115200

# AT 명령어 끝에 붙는 문자열
EOL = b'\r\n'

def send_at_command(ser, command, timeout=1):
   """
   AT 명령어를 전송하고 응답을 받는 함수
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
      # 시리얼 포트 열기
      print(f"Opening {SERIAL_PORT} port...")
      ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=5)
      print("COM port opened successfully.")
      
      # AT 명령어를 보내기 전에 잠시 대기
      time.sleep(2)
      
      # 1. AT 명령어 테스트
      print("Testing an AT command...")
      response = send_at_command(ser, b'AT')
      if b'OK' not in response:
         print("ERROR: ESP8266 not responding to AT command.")
         return

      # 2. Wi-Fi 모드 설정 (station 모드)
      print("Setting Wi-Fi mode to station...")
      response = send_at_command(ser, b'AT+CWMODE=1')
      if b'OK' not in response:
         print("ERROR: Failed to set Wi-Fi mode.")
         return
         
      # 3. Wi-Fi에 연결
      print(f"Connecting to Wi-Fi SSID...")
      join_command = f'AT+CWJAP="{wifi_ssid}","{wifi_password}"'.encode('utf-8')
      response = send_at_command(ser, join_command, timeout=10)
      
      if b'WIFI CONNECTED' in response and b'WIFI GOT IP' in response:
         print("SUCCESS: Connected to Wi-Fi!")
      else:
         print("ERROR: Failed to connect to Wi-Fi.")
         return

      # 4. 할당받은 IP 주소 확인
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
