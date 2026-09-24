#!/usr/bin/env python3
"""Minimal GSM 07.10 / 3GPP TS 27.010 CMUX probe for Telit LE310Q1 UART0.

This probe intentionally stops before PPP. It verifies that the physical UART can
enter CMUX mode, opens DLCI 0/1/2, then exchanges AT/OK on DLCI 2.
"""

from __future__ import annotations

import argparse
import logging
import sys
import time
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Callable

import serial

FLAG = 0xF9
ADDRESS_EA = 0x01
ADDRESS_COMMAND = 0x02
CONTROL_SABM = 0x2F
CONTROL_UA = 0x63
CONTROL_DISC = 0x43
CONTROL_UIH = 0xEF
CONTROL_PF = 0x10


class CMUXError(RuntimeError):
    """Raised when the modem does not complete the expected CMUX exchange."""


@dataclass(frozen=True)
class CMUXFrame:
    """A validated CMUX frame without its opening and closing flag bytes."""

    dlci: int
    control: int
    payload: bytes


def configure_logging(port_name: str) -> logging.Logger:
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    log_path = Path.cwd() / f"cmux_probe_{port_name}_{timestamp}.log"

    logger = logging.getLogger("cmux_probe")
    logger.setLevel(logging.DEBUG)
    formatter = logging.Formatter("%(asctime)s %(levelname)s %(message)s")

    console_handler = logging.StreamHandler()
    console_handler.setLevel(logging.INFO)
    console_handler.setFormatter(formatter)

    file_handler = logging.FileHandler(log_path, encoding="ascii")
    file_handler.setLevel(logging.DEBUG)
    file_handler.setFormatter(formatter)

    logger.handlers.clear()
    logger.addHandler(console_handler)
    logger.addHandler(file_handler)
    logger.info("Raw frame log: %s", log_path)
    return logger


def update_fcs(fcs_value: int, byte_value: int) -> int:
    """Apply the GSM 07.10 reflected CRC-8 update to one byte."""

    fcs_value ^= byte_value
    for _ in range(8):
        if fcs_value & 0x01:
            fcs_value = (fcs_value >> 1) ^ 0xE0
        else:
            fcs_value >>= 1
    return fcs_value


def calculate_fcs(header: bytes) -> int:
    """Return the transmitted FCS for address/control/length bytes."""

    fcs_value = 0xFF
    for byte_value in header:
        fcs_value = update_fcs(fcs_value, byte_value)
    return (0xFF - fcs_value) & 0xFF


def encode_length(payload_length: int) -> bytes:
    if payload_length <= 127:
        return bytes([(payload_length << 1) | 0x01])

    if payload_length > 0x7FFF:
        raise ValueError("CMUX payload length exceeds the 15-bit Basic option limit")

    return bytes([
        (payload_length << 1) & 0xFE,
        (payload_length >> 7) & 0xFF,
    ])


def build_frame(dlci: int, control: int, payload: bytes = b"") -> bytes:
    if not 0 <= dlci <= 63:
        raise ValueError("DLCI must be in range 0..63")

    address = (dlci << 2) | ADDRESS_COMMAND | ADDRESS_EA
    length = encode_length(len(payload))
    header = bytes([address, control]) + length
    return bytes([FLAG]) + header + payload + bytes([calculate_fcs(header), FLAG])


def parse_available_frames(receive_buffer: bytearray, logger: logging.Logger) -> list[CMUXFrame]:
    frames: list[CMUXFrame] = []

    while True:
        try:
            opening_index = receive_buffer.index(FLAG)
        except ValueError:
            receive_buffer.clear()
            break

        if opening_index:
            del receive_buffer[:opening_index]

        if len(receive_buffer) < 5:
            break

        first_length = receive_buffer[3]
        length_size = 1 if first_length & ADDRESS_EA else 2
        if len(receive_buffer) < 1 + 2 + length_size + 1 + 1:
            break

        payload_length = first_length >> 1
        if length_size == 2:
            payload_length |= receive_buffer[4] << 7

        frame_size = 1 + 2 + length_size + payload_length + 1 + 1
        if len(receive_buffer) < frame_size:
            break

        if receive_buffer[frame_size - 1] != FLAG:
            logger.warning("Discarding malformed CMUX frame start: %s", receive_buffer[:8].hex(" "))
            del receive_buffer[0]
            continue

        frame_bytes = bytes(receive_buffer[:frame_size])
        del receive_buffer[:frame_size]

        header_end = 3 + length_size
        header = frame_bytes[1:header_end]
        received_fcs = frame_bytes[-2]
        expected_fcs = calculate_fcs(header)
        if received_fcs != expected_fcs:
            logger.warning(
                "Ignoring frame with invalid FCS: expected=%02X received=%02X raw=%s",
                expected_fcs,
                received_fcs,
                frame_bytes.hex(" "),
            )
            continue

        payload_start = header_end
        payload_end = payload_start + payload_length
        frames.append(
            CMUXFrame(
                dlci=(frame_bytes[1] >> 2) & 0x3F,
                control=frame_bytes[2],
                payload=frame_bytes[payload_start:payload_end],
            )
        )

    return frames


def read_at_response(serial_port: serial.Serial, timeout_seconds: float) -> bytes:
    deadline = time.monotonic() + timeout_seconds
    response = bytearray()

    while time.monotonic() < deadline:
        chunk = serial_port.read(serial_port.in_waiting or 1)
        if chunk:
            response.extend(chunk)
            if b"OK\r\n" in response:
                return bytes(response)
            if b"ERROR\r\n" in response:
                raise CMUXError(f"AT command failed: {response.decode('ascii', errors='replace')!r}")

    raise CMUXError(f"Timed out waiting for AT response: {response.decode('ascii', errors='replace')!r}")


def send_at(serial_port: serial.Serial, command: str, logger: logging.Logger) -> None:
    logger.info("AT TX: %s", command)
    serial_port.write((command + "\r").encode("ascii"))
    serial_port.flush()
    response = read_at_response(serial_port, timeout_seconds=5.0)
    logger.info("AT RX: %s", response.decode("ascii", errors="replace").replace("\r", "\\r").replace("\n", "\\n"))


def wait_for_frame(
    serial_port: serial.Serial,
    receive_buffer: bytearray,
    predicate: Callable[[CMUXFrame], bool],
    timeout_seconds: float,
    logger: logging.Logger,
    description: str,
) -> CMUXFrame:
    deadline = time.monotonic() + timeout_seconds

    while time.monotonic() < deadline:
        chunk = serial_port.read(serial_port.in_waiting or 1)
        if not chunk:
            continue

        logger.debug("CMUX RX raw: %s", chunk.hex(" "))
        receive_buffer.extend(chunk)
        for frame in parse_available_frames(receive_buffer, logger):
            logger.info(
                "CMUX RX: DLCI=%d control=%02X payload=%r",
                frame.dlci,
                frame.control,
                frame.payload,
            )
            if predicate(frame):
                return frame

    raise CMUXError(f"Timed out waiting for {description}")


def send_frame(serial_port: serial.Serial, frame: bytes, logger: logging.Logger) -> None:
    logger.info("CMUX TX raw: %s", frame.hex(" "))
    serial_port.write(frame)
    serial_port.flush()


def open_dlci(
    serial_port: serial.Serial,
    receive_buffer: bytearray,
    dlci: int,
    timeout_seconds: float,
    logger: logging.Logger,
) -> None:
    send_frame(serial_port, build_frame(dlci, CONTROL_SABM | CONTROL_PF), logger)
    wait_for_frame(
        serial_port,
        receive_buffer,
        lambda frame: frame.dlci == dlci and (frame.control & ~CONTROL_PF) == CONTROL_UA,
        timeout_seconds,
        logger,
        f"UA for DLCI {dlci}",
    )
    logger.info("DLCI %d opened", dlci)


def close_dlci(
    serial_port: serial.Serial,
    receive_buffer: bytearray,
    dlci: int,
    timeout_seconds: float,
    logger: logging.Logger,
) -> None:
    send_frame(serial_port, build_frame(dlci, CONTROL_DISC | CONTROL_PF), logger)
    wait_for_frame(
        serial_port,
        receive_buffer,
        lambda frame: frame.dlci == dlci and (frame.control & ~CONTROL_PF) == CONTROL_UA,
        timeout_seconds,
        logger,
        f"UA for DISC on DLCI {dlci}",
    )
    logger.info("DLCI %d closed", dlci)


def run_probe(arguments: argparse.Namespace, logger: logging.Logger) -> None:
    logger.info("Opening %s at %d bps with RTS/CTS", arguments.port, arguments.baudrate)

    with serial.Serial(
        port=arguments.port,
        baudrate=arguments.baudrate,
        bytesize=serial.EIGHTBITS,
        parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
        timeout=0.1,
        write_timeout=2.0,
        rtscts=True,
        dsrdtr=False,
    ) as serial_port:
        serial_port.reset_input_buffer()
        serial_port.reset_output_buffer()

        send_at(serial_port, "AT", logger)
        send_at(serial_port, "ATE0", logger)
        send_at(serial_port, "AT&K3", logger)
        send_at(serial_port, "AT+CMUX=0,0,5,1280", logger)

        receive_buffer = bytearray()
        for dlci in (0, 1, 2):
            open_dlci(serial_port, receive_buffer, dlci, arguments.timeout, logger)

        send_frame(serial_port, build_frame(2, CONTROL_UIH, b"AT\r"), logger)
        response_frame = wait_for_frame(
            serial_port,
            receive_buffer,
            lambda frame: frame.dlci == 2 and (frame.control & ~CONTROL_PF) == CONTROL_UIH and b"OK" in frame.payload,
            arguments.timeout,
            logger,
            "DLCI 2 UIH response containing OK",
        )
        logger.info("CMUX probe succeeded: DLCI 2 response=%r", response_frame.payload)

        if arguments.leave_open:
            logger.warning("CMUX channels left open by request. Reset the EVB to return to plain AT mode.")
            return

        for dlci in (2, 1, 0):
            close_dlci(serial_port, receive_buffer, dlci, arguments.timeout, logger)

        logger.info("CMUX channels closed cleanly. Power-cycle or reset the EVB if plain AT mode does not return.")


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Probe Telit LE310Q1 CMUX over an FTDI Main UART.")
    parser.add_argument("--port", required=True, help="FTDI Main UART COM port, for example COM22")
    parser.add_argument("--baudrate", type=int, default=115200, help="Plain UART baud rate before CMUX activation")
    parser.add_argument("--timeout", type=float, default=5.0, help="Seconds to wait for each CMUX response")
    parser.add_argument("--leave-open", action="store_true", help="Do not send DISC frames after a successful probe")
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    logger = configure_logging(arguments.port)

    try:
        run_probe(arguments, logger)
    except (CMUXError, serial.SerialException, ValueError) as error:
        logger.error("CMUX probe failed: %s", error)
        logger.error("The modem may still be in CMUX mode. Reset or power-cycle the EVB, then verify plain AT again.")
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
