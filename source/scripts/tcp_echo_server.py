"""TCP echo server for receiving messages through a public TCP tunnel."""

import argparse
import socket
import threading
from datetime import datetime


DEFAULT_HOST = "0.0.0.0"
DEFAULT_PORT = 9000
RECEIVE_BUFFER_SIZE = 4096


def format_payload(payload: bytes) -> str:
    """Return a readable representation without losing non-text bytes."""
    try:
        return payload.decode("utf-8")
    except UnicodeDecodeError:
        return payload.hex(" ")


def handle_client(client_socket: socket.socket, client_address: tuple[str, int]) -> None:
    """Log and echo all data until the connected client closes the socket."""
    peer = f"{client_address[0]}:{client_address[1]}"
    print(f"[{datetime.now().isoformat(timespec='seconds')}] Connected: {peer}")

    try:
        while payload := client_socket.recv(RECEIVE_BUFFER_SIZE):
            timestamp = datetime.now().isoformat(timespec="seconds")
            print(f"[{timestamp}] Received from {peer} ({len(payload)} bytes): {format_payload(payload)}")
            client_socket.sendall(payload)
    except ConnectionError as error:
        print(f"Connection error from {peer}: {error}")
    finally:
        client_socket.close()
        print(f"[{datetime.now().isoformat(timespec='seconds')}] Disconnected: {peer}")


def run_server(host: str, port: int) -> None:
    """Listen for TCP clients and serve each connection in a daemon thread."""
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server_socket:
        server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server_socket.bind((host, port))
        server_socket.listen()

        print(f"TCP echo server listening on {host}:{port}")
        print("Use this port as the local target for the public TCP tunnel. Press Ctrl+C to stop.")

        try:
            while True:
                client_socket, client_address = server_socket.accept()
                client_thread = threading.Thread(
                    target=handle_client,
                    args=(client_socket, client_address),
                    daemon=True,
                )
                client_thread.start()
        except KeyboardInterrupt:
            print("\nTCP echo server stopped.")


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run a TCP echo server.")
    parser.add_argument("--host", default=DEFAULT_HOST, help=f"Bind host (default: {DEFAULT_HOST})")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help=f"Bind port (default: {DEFAULT_PORT})")
    return parser.parse_args()


if __name__ == "__main__":
    arguments = parse_arguments()
    run_server(arguments.host, arguments.port)