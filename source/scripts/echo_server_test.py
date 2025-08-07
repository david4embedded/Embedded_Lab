import socket
import time

# Server information
HOST = '192.168.1.3'
PORT = 7

def run_client():
   """Simple TCP echo client."""
   with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
      try:
         print(f"Connecting to {HOST}:{PORT}...")
         s.connect((HOST, PORT))
         print("Connected!")
         
         message = "Hello, Echo Server!"
         print(f"Sending: '{message}'")
         s.sendall(message.encode('utf-8'))
         
         # Receive data from the server
         data = s.recv(1024)
         print(f"Received echo: '{data.decode('utf-8')}'")
         
      except ConnectionRefusedError:
         print("Connection failed. Is the server running and is the IP address correct?")
      except Exception as e:
         print(f"An error occurred: {e}")
      finally:
         print("Closing socket.")

if __name__ == "__main__":
   run_client()