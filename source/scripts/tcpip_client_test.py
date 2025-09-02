# -*- coding: utf-8 -*-

import socket
import argparse

def main():
   """
   Echo 서버에 연결하고 메시지를 주고받는 클라이언트 스크립트.
   """
   parser = argparse.ArgumentParser(description="TCP client script with host, port, and message arguments.")
   parser.add_argument("--ip", type=str, help="The IP address and port to connect to (e.g., 192.168.1.3:8000).")
   parser.add_argument("--message", type=str, help="The message to send.")

   args = parser.parse_args()

   ip_address, port_str = args.ip.split(":")
   host = ip_address
   port = int(port_str)
   message = args.message

   # 소켓 생성
   # AF_INET은 IPv4를, SOCK_STREAM은 TCP를 의미합니다.
   with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
      print(f"Connecting to {host}:{port}...")
      try:
         # 서버에 연결
         s.connect((host, port))
         print("Connected.")

         # 메시지를 바이트로 인코딩하여 전송
         message += "\r\n"
         
         s.sendall(message.encode('utf-8'))
         print(f"Sent: '{message}'")

         # 서버로부터 응답 받기
         timeout_sec = 5
         print(f"Waiting for response...within {timeout_sec} seconds")
         s.settimeout(timeout_sec)  # 5초 타임아웃 설정
         try:
            data = s.recv(1024)
         except socket.timeout:
            print("Timeout occurred while receiving data.")
            return
         except Exception as e:
            print(f"An error occurred while receiving data: {e}")
            return
         
         # 받은 데이터를 문자열로 디코딩
         received_message = data.decode('utf-8')
         print(f"Received: '{received_message}'")

      except ConnectionRefusedError:
         print("Error: The server is not running or the port is incorrect.")
      except Exception as e:
         print(f"An unexpected error occurred: {e}")

if __name__ == "__main__":
   main()