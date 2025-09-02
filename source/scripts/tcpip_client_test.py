# -*- coding: utf-8 -*-

import socket
import argparse

def main():
   """
   A client script that connects to an echo server and exchanges messages.
   """
   parser = argparse.ArgumentParser(description="TCP client script with host, port, and message arguments.")
   parser.add_argument("--ip", type=str, help="The IP address and port to connect to (e.g., 192.168.1.3:8000).")
   parser.add_argument("--message", type=str, help="The message to send.")

   args = parser.parse_args()

   ip_address, port_str = args.ip.split(":")
   host = ip_address
   port = int(port_str)
   message = args.message

   # Create a socket
   # AF_INET is IPv4, SOCK_STREAM is TCP.
   with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
      print(f"Connecting to {host}:{port}...")
      try:
         # Connect to the server
         s.connect((host, port))
         print("Connected.")

         # Encode the message to bytes and send it
         message += "\r\n"
         
         s.sendall(message.encode('utf-8'))
         print(f"Sent: '{message}'")

         # Receive a response from the server
         timeout_sec = 5
         print(f"Waiting for response...within {timeout_sec} seconds")
         s.settimeout(timeout_sec)  # Set timeout to 5 seconds
         try:
            data = s.recv(1024)
         except socket.timeout:
            print("Timeout occurred while receiving data.")
            return
         except Exception as e:
            print(f"An error occurred while receiving data: {e}")
            return
         
         # Decode the received data to a string
         received_message = data.decode('utf-8')
         print(f"Received: '{received_message}'")

      except ConnectionRefusedError:
         print("Error: The server is not running or the port is incorrect.")
      except Exception as e:
         print(f"An unexpected error occurred: {e}")

if __name__ == "__main__":
   main()