import serial
import time
import argparse

# --- 설정값 ---
# ESP8266이 연결된 시리얼 포트
# Windows: 'COM3', macOS/Linux: '/dev/ttyUSB0' 등
SERIAL_PORT = 'COM12' 

# AT 펌웨어 기본 통신 속도
BAUD_RATE = 115200

# AT 명령어 끝에 붙는 문자열
EOL = b'\r\n'

def send_at_command(ser, command, timeout=1):
   """
   AT 명령어를 전송하고 응답을 받는 함수
   """
   print(f"Sending: {command}")
   ser.write(command + EOL)
   time.sleep(timeout)
   response = ser.read_all()
   print(f"Received: {response.decode('utf-8')}")
   return response

def main():
   parser = argparse.ArgumentParser(description="Connect to Wi-Fi using ESP8266.")
   parser.add_argument("ssid", help="Wi-Fi SSID")
   parser.add_argument("password", help="Wi-Fi Password")
   args = parser.parse_args()

   wifi_ssid = args.ssid
   wifi_password = args.password

   try:
      # 시리얼 포트 열기
      ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=5)
      print(f"Serial port {SERIAL_PORT} opened successfully.")
      
      # AT 명령어를 보내기 전에 잠시 대기
      time.sleep(2)
      
      # 1. AT 명령어 테스트
      response = send_at_command(ser, b'AT')
      if b'OK' not in response:
         print("ERROR: ESP8266 not responding to AT command.")
         return

      # 2. Wi-Fi 모드 설정 (station 모드)
      response = send_at_command(ser, b'AT+CWMODE=1')
      if b'OK' not in response:
         print("ERROR: Failed to set Wi-Fi mode.")
         return
         
      # 3. Wi-Fi에 연결
      join_command = f'AT+CWJAP="{wifi_ssid}","{wifi_password}"'.encode('utf-8')
      response = send_at_command(ser, join_command, timeout=10)
      
      if b'WIFI CONNECTED' in response and b'WIFI GOT IP' in response:
         print("SUCCESS: Connected to Wi-Fi!")
      else:
         print("ERROR: Failed to connect to Wi-Fi.")
         print("Please check your SSID and Password.")
         return

      # 4. 할당받은 IP 주소 확인
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