#!/usr/bin/env python3
"""Minimal PPP-over-CMUX probe for a Telit LE310Q1 UART0 connection.

Requires cmux_probe.py from the previous CMUX link test in the same directory.
The probe verifies:
  1. CMUX DLCI 0/1/2 establishment
  2. ATD*99***<cid># and CONNECT on DLCI 1
  3. PPP LCP and IPCP negotiation on DLCI 1
  4. CMUX AT traffic on DLCI 2 while PPP is open

It intentionally does not send application IP traffic. It is a one-shot
interoperability test, not a general-purpose CMUX or PPP implementation.
"""

from __future__ import annotations

import argparse
import hashlib
import logging
import struct
import sys
import time
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path

import serial

import cmux_probe

PPP_FLAG = 0x7E
PPP_ESCAPE = 0x7D
PPP_ESCAPE_XOR = 0x20
PPP_GOOD_FCS = 0xF0B8
PPP_INITIAL_FCS = 0xFFFF

PPP_PROTOCOL_LCP = 0xC021
PPP_PROTOCOL_PAP = 0xC023
PPP_PROTOCOL_CHAP = 0xC223
PPP_PROTOCOL_IPCP = 0x8021

PPP_CODE_CONFIGURE_REQUEST = 1
PPP_CODE_CONFIGURE_ACK = 2
PPP_CODE_CONFIGURE_NAK = 3
PPP_CODE_CONFIGURE_REJECT = 4
PPP_CODE_TERMINATE_REQUEST = 5
PPP_CODE_TERMINATE_ACK = 6

PAP_CODE_AUTHENTICATE_REQUEST = 1
PAP_CODE_AUTHENTICATE_ACK = 2
PAP_CODE_AUTHENTICATE_NAK = 3

CHAP_CODE_CHALLENGE = 1
CHAP_CODE_RESPONSE = 2
CHAP_CODE_SUCCESS = 3
CHAP_CODE_FAILURE = 4
CHAP_ALGORITHM_MD5 = 5

LCP_OPTION_MRU = 1
LCP_OPTION_AUTH_PROTOCOL = 3
IPCP_OPTION_IP_ADDRESS = 3
IPCP_OPTION_PRIMARY_DNS = 129
IPCP_OPTION_SECONDARY_DNS = 131


class PPPError(RuntimeError):
    """Raised when the probe cannot complete PPP link negotiation."""


def format_error_for_log(error: BaseException) -> str:
    """Keep residual binary UART traffic printable in an ASCII console and log."""

    return str(error).encode("ascii", errors="backslashreplace").decode("ascii")


@dataclass(frozen=True)
class PPPPacket:
    """A validated asynchronous PPP packet with address/control retained."""

    protocol: int
    payload: bytes


class PPPStreamDecoder:
    """Reassembles escaped serial PPP packets across CMUX UIH payloads."""

    def __init__(self) -> None:
        self._in_frame = False
        self._escaped = False
        self._frame = bytearray()

    def feed(self, data: bytes, logger: logging.Logger) -> list[PPPPacket]:
        packets: list[PPPPacket] = []

        for byte_value in data:
            if byte_value == PPP_FLAG:
                if self._in_frame and self._frame:
                    packet = decode_ppp_packet(bytes(self._frame), logger)
                    if packet is not None:
                        packets.append(packet)
                self._in_frame = True
                self._escaped = False
                self._frame.clear()
                continue

            if not self._in_frame:
                continue

            if self._escaped:
                self._frame.append(byte_value ^ PPP_ESCAPE_XOR)
                self._escaped = False
            elif byte_value == PPP_ESCAPE:
                self._escaped = True
            else:
                self._frame.append(byte_value)

        return packets


def configure_logging(port_name: str) -> logging.Logger:
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    log_path = Path.cwd() / f"cmux_ppp_probe_{port_name}_{timestamp}.log"

    logger = logging.getLogger("cmux_ppp_probe")
    logger.setLevel(logging.DEBUG)
    logger.handlers.clear()
    formatter = logging.Formatter("%(asctime)s %(levelname)s %(message)s")

    console_handler = logging.StreamHandler()
    console_handler.setLevel(logging.INFO)
    console_handler.setFormatter(formatter)
    logger.addHandler(console_handler)

    file_handler = logging.FileHandler(log_path, encoding="ascii")
    file_handler.setLevel(logging.DEBUG)
    file_handler.setFormatter(formatter)
    logger.addHandler(file_handler)

    logger.info("Raw frame log: %s", log_path)
    return logger


def update_ppp_fcs(fcs_value: int, byte_value: int) -> int:
    fcs_value ^= byte_value
    for _ in range(8):
        if fcs_value & 0x01:
            fcs_value = (fcs_value >> 1) ^ 0x8408
        else:
            fcs_value >>= 1
    return fcs_value


def calculate_ppp_fcs(data: bytes) -> int:
    fcs_value = PPP_INITIAL_FCS
    for byte_value in data:
        fcs_value = update_ppp_fcs(fcs_value, byte_value)
    return fcs_value ^ PPP_INITIAL_FCS


def escape_ppp_bytes(data: bytes) -> bytes:
    escaped = bytearray()
    for byte_value in data:
        if byte_value < 0x20 or byte_value in (PPP_FLAG, PPP_ESCAPE):
            escaped.append(PPP_ESCAPE)
            escaped.append(byte_value ^ PPP_ESCAPE_XOR)
        else:
            escaped.append(byte_value)
    return bytes(escaped)


def build_ppp_packet(protocol: int, payload: bytes) -> bytes:
    body = b"\xff\x03" + protocol.to_bytes(2, "big") + payload
    fcs_value = calculate_ppp_fcs(body)
    raw_packet = body + fcs_value.to_bytes(2, "little")
    return bytes([PPP_FLAG]) + escape_ppp_bytes(raw_packet) + bytes([PPP_FLAG])


def decode_ppp_packet(raw_packet: bytes, logger: logging.Logger) -> PPPPacket | None:
    if len(raw_packet) < 6:
        logger.debug("Ignoring short PPP packet: %s", raw_packet.hex(" "))
        return None

    fcs_value = PPP_INITIAL_FCS
    for byte_value in raw_packet:
        fcs_value = update_ppp_fcs(fcs_value, byte_value)
    if fcs_value != PPP_GOOD_FCS:
        logger.warning("Ignoring PPP packet with invalid FCS: %s", raw_packet.hex(" "))
        return None

    if raw_packet[0:2] != b"\xff\x03":
        logger.warning("Ignoring PPP packet with compressed address/control: %s", raw_packet.hex(" "))
        return None

    return PPPPacket(
        protocol=int.from_bytes(raw_packet[2:4], "big"),
        payload=raw_packet[4:-2],
    )


def build_control_packet(code: int, identifier: int, data: bytes = b"") -> bytes:
    return struct.pack("!BBH", code, identifier, len(data) + 4) + data


def parse_control_packet(payload: bytes) -> tuple[int, int, bytes]:
    if len(payload) < 4:
        raise PPPError(f"PPP control packet too short: {payload.hex(' ')}")

    code, identifier, declared_length = struct.unpack("!BBH", payload[:4])
    if declared_length < 4 or declared_length > len(payload):
        raise PPPError(f"Invalid PPP control packet length: {declared_length}")
    return code, identifier, payload[4:declared_length]


def parse_options(data: bytes) -> list[tuple[int, bytes]]:
    options: list[tuple[int, bytes]] = []
    offset = 0

    while offset < len(data):
        if offset + 2 > len(data):
            raise PPPError("Truncated PPP option header")
        option_type = data[offset]
        option_length = data[offset + 1]
        if option_length < 2 or offset + option_length > len(data):
            raise PPPError("Invalid PPP option length")
        options.append((option_type, data[offset + 2:offset + option_length]))
        offset += option_length

    return options


def format_ipv4(value: bytes) -> str:
    if len(value) != 4:
        return value.hex(" ")
    return ".".join(str(byte_value) for byte_value in value)


def send_cmux_frame(serial_port: serial.Serial, frame: bytes, logger: logging.Logger) -> None:
    logger.debug("CMUX TX raw: %s", frame.hex(" "))
    serial_port.write(frame)
    serial_port.flush()


def send_ppp_packet(serial_port: serial.Serial, protocol: int, payload: bytes, logger: logging.Logger) -> None:
    ppp_frame = build_ppp_packet(protocol, payload)
    logger.info("PPP TX: protocol=%04X payload=%s", protocol, payload.hex(" "))
    send_cmux_frame(
        serial_port,
        cmux_probe.build_frame(1, cmux_probe.CONTROL_UIH, ppp_frame),
        logger,
    )


def read_cmux_frames(
    serial_port: serial.Serial,
    receive_buffer: bytearray,
    timeout_seconds: float,
    logger: logging.Logger,
) -> list[cmux_probe.CMUXFrame]:
    deadline = time.monotonic() + timeout_seconds
    frames: list[cmux_probe.CMUXFrame] = []

    while time.monotonic() < deadline:
        chunk = serial_port.read(serial_port.in_waiting or 1)
        if not chunk:
            continue
        logger.debug("CMUX RX raw: %s", chunk.hex(" "))
        receive_buffer.extend(chunk)
        frames.extend(cmux_probe.parse_available_frames(receive_buffer, logger))
        if frames:
            return frames

    return frames


def wait_for_cmux_frame(
    serial_port: serial.Serial,
    receive_buffer: bytearray,
    predicate: object,
    timeout_seconds: float,
    logger: logging.Logger,
    description: str,
) -> cmux_probe.CMUXFrame:
    deadline = time.monotonic() + timeout_seconds

    while time.monotonic() < deadline:
        frames = read_cmux_frames(serial_port, receive_buffer, 0.25, logger)
        for frame in frames:
            logger.info("CMUX RX: DLCI=%d control=%02X payload=%r", frame.dlci, frame.control, frame.payload)
            if predicate(frame):
                return frame

    raise PPPError(f"Timed out waiting for {description}")


def wait_for_connect(
    serial_port: serial.Serial,
    receive_buffer: bytearray,
    timeout_seconds: float,
    logger: logging.Logger,
) -> None:
    wait_for_cmux_frame(
        serial_port,
        receive_buffer,
        lambda frame: frame.dlci == 1
        and (frame.control & ~cmux_probe.CONTROL_PF) == cmux_probe.CONTROL_UIH
        and b"CONNECT" in frame.payload,
        timeout_seconds,
        logger,
        "CONNECT on DLCI 1",
    )


def poll_ppp_packets(
    serial_port: serial.Serial,
    receive_buffer: bytearray,
    ppp_decoder: PPPStreamDecoder,
    timeout_seconds: float,
    logger: logging.Logger,
) -> list[PPPPacket]:
    packets: list[PPPPacket] = []
    for frame in read_cmux_frames(serial_port, receive_buffer, timeout_seconds, logger):
        logger.info("CMUX RX: DLCI=%d control=%02X payload=%r", frame.dlci, frame.control, frame.payload)
        if frame.dlci != 1 or (frame.control & ~cmux_probe.CONTROL_PF) != cmux_probe.CONTROL_UIH:
            continue
        for packet in ppp_decoder.feed(frame.payload, logger):
            logger.info("PPP RX: protocol=%04X payload=%s", packet.protocol, packet.payload.hex(" "))
            packets.append(packet)
    return packets


def acknowledge_peer_request(
    serial_port: serial.Serial,
    protocol: int,
    identifier: int,
    data: bytes,
    logger: logging.Logger,
) -> None:
    send_ppp_packet(
        serial_port,
        protocol,
        build_control_packet(PPP_CODE_CONFIGURE_ACK, identifier, data),
        logger,
    )


def extract_authentication_option(lcp_options: bytes) -> bytes | None:
    for option_type, option_value in parse_options(lcp_options):
        if option_type == LCP_OPTION_AUTH_PROTOCOL:
            return option_value
    return None


def negotiate_lcp(
    serial_port: serial.Serial,
    receive_buffer: bytearray,
    timeout_seconds: float,
    logger: logging.Logger,
) -> bytes | None:
    local_identifier = 1
    local_options = bytes([LCP_OPTION_MRU, 4]) + (1024).to_bytes(2, "big")
    send_ppp_packet(
        serial_port,
        PPP_PROTOCOL_LCP,
        build_control_packet(PPP_CODE_CONFIGURE_REQUEST, local_identifier, local_options),
        logger,
    )

    local_acked = False
    peer_acked = False
    peer_authentication_option: bytes | None = None
    decoder = PPPStreamDecoder()
    deadline = time.monotonic() + timeout_seconds

    while time.monotonic() < deadline:
        for packet in poll_ppp_packets(serial_port, receive_buffer, decoder, 0.25, logger):
            if packet.protocol != PPP_PROTOCOL_LCP:
                continue

            code, identifier, data = parse_control_packet(packet.payload)
            if code == PPP_CODE_CONFIGURE_REQUEST:
                peer_authentication_option = extract_authentication_option(data) or peer_authentication_option
                acknowledge_peer_request(serial_port, PPP_PROTOCOL_LCP, identifier, data, logger)
                peer_acked = True
            elif code == PPP_CODE_CONFIGURE_ACK and identifier == local_identifier:
                local_acked = True
            elif code in (PPP_CODE_CONFIGURE_NAK, PPP_CODE_CONFIGURE_REJECT) and identifier == local_identifier:
                raise PPPError(f"Peer rejected LCP options: code={code} data={data.hex(' ')}")
            elif code == PPP_CODE_TERMINATE_REQUEST:
                send_ppp_packet(
                    serial_port,
                    PPP_PROTOCOL_LCP,
                    build_control_packet(PPP_CODE_TERMINATE_ACK, identifier, data),
                    logger,
                )
                raise PPPError("Peer terminated LCP")

        if local_acked and peer_acked:
            logger.info("PPP LCP opened")
            if peer_authentication_option is not None:
                logger.info("Peer requires authentication option %s", peer_authentication_option.hex(" "))
            return peer_authentication_option

    raise PPPError("Timed out opening PPP LCP")


def authenticate_pap(
    serial_port: serial.Serial,
    receive_buffer: bytearray,
    username: str,
    password: str,
    timeout_seconds: float,
    logger: logging.Logger,
) -> None:
    username_bytes = username.encode("ascii")
    password_bytes = password.encode("ascii")
    if len(username_bytes) > 255 or len(password_bytes) > 255:
        raise PPPError("PAP username and password must each be at most 255 bytes")

    identifier = 1
    request_data = bytes([len(username_bytes)]) + username_bytes + bytes([len(password_bytes)]) + password_bytes
    send_ppp_packet(
        serial_port,
        PPP_PROTOCOL_PAP,
        build_control_packet(PAP_CODE_AUTHENTICATE_REQUEST, identifier, request_data),
        logger,
    )

    decoder = PPPStreamDecoder()
    deadline = time.monotonic() + timeout_seconds
    while time.monotonic() < deadline:
        for packet in poll_ppp_packets(serial_port, receive_buffer, decoder, 0.25, logger):
            if packet.protocol != PPP_PROTOCOL_PAP:
                continue
            code, response_identifier, data = parse_control_packet(packet.payload)
            if response_identifier != identifier:
                continue
            if code == PAP_CODE_AUTHENTICATE_ACK:
                logger.info("PAP authentication succeeded")
                return
            if code == PAP_CODE_AUTHENTICATE_NAK:
                raise PPPError(f"PAP authentication failed: {data!r}")

    raise PPPError("Timed out waiting for PAP authentication response")


def authenticate_chap_md5(
    serial_port: serial.Serial,
    receive_buffer: bytearray,
    username: str,
    password: str,
    timeout_seconds: float,
    logger: logging.Logger,
) -> None:
    username_bytes = username.encode("ascii")
    password_bytes = password.encode("ascii")
    decoder = PPPStreamDecoder()
    deadline = time.monotonic() + timeout_seconds

    while time.monotonic() < deadline:
        for packet in poll_ppp_packets(serial_port, receive_buffer, decoder, 0.25, logger):
            if packet.protocol != PPP_PROTOCOL_CHAP:
                continue

            code, identifier, data = parse_control_packet(packet.payload)
            if code == CHAP_CODE_CHALLENGE:
                if not data:
                    raise PPPError("CHAP challenge has no value-size field")
                value_size = data[0]
                if value_size == 0 or len(data) < value_size + 1:
                    raise PPPError("CHAP challenge has an invalid value length")

                challenge = data[1:value_size + 1]
                response_value = hashlib.md5(bytes([identifier]) + password_bytes + challenge).digest()
                response_data = bytes([len(response_value)]) + response_value + username_bytes
                send_ppp_packet(
                    serial_port,
                    PPP_PROTOCOL_CHAP,
                    build_control_packet(CHAP_CODE_RESPONSE, identifier, response_data),
                    logger,
                )
                logger.info("CHAP-MD5 response sent for user %r", username)
            elif code == CHAP_CODE_SUCCESS:
                logger.info("CHAP-MD5 authentication succeeded")
                return
            elif code == CHAP_CODE_FAILURE:
                raise PPPError(f"CHAP-MD5 authentication failed: {data!r}")

    raise PPPError("Timed out waiting for CHAP-MD5 challenge or success")


def build_ipcp_options(addresses: dict[int, bytes]) -> bytes:
    return b"".join(bytes([option_type, 6]) + addresses[option_type] for option_type in addresses)


def update_ipcp_addresses(addresses: dict[int, bytes], options: bytes, logger: logging.Logger) -> bool:
    changed = False
    for option_type, option_value in parse_options(options):
        if option_type in (IPCP_OPTION_IP_ADDRESS, IPCP_OPTION_PRIMARY_DNS, IPCP_OPTION_SECONDARY_DNS) and len(option_value) == 4:
            if addresses.get(option_type) != option_value:
                addresses[option_type] = option_value
                logger.info("IPCP option %d offered as %s", option_type, format_ipv4(option_value))
                changed = True
    return changed


def negotiate_ipcp(
    serial_port: serial.Serial,
    receive_buffer: bytearray,
    timeout_seconds: float,
    logger: logging.Logger,
) -> dict[int, bytes]:
    addresses = {
        IPCP_OPTION_IP_ADDRESS: b"\x00\x00\x00\x00",
        IPCP_OPTION_PRIMARY_DNS: b"\x00\x00\x00\x00",
        IPCP_OPTION_SECONDARY_DNS: b"\x00\x00\x00\x00",
    }
    local_identifier = 1
    local_acked = False
    peer_acked = False

    send_ppp_packet(
        serial_port,
        PPP_PROTOCOL_IPCP,
        build_control_packet(PPP_CODE_CONFIGURE_REQUEST, local_identifier, build_ipcp_options(addresses)),
        logger,
    )

    decoder = PPPStreamDecoder()
    deadline = time.monotonic() + timeout_seconds
    while time.monotonic() < deadline:
        for packet in poll_ppp_packets(serial_port, receive_buffer, decoder, 0.25, logger):
            if packet.protocol != PPP_PROTOCOL_IPCP:
                continue

            code, identifier, data = parse_control_packet(packet.payload)
            if code == PPP_CODE_CONFIGURE_REQUEST:
                acknowledge_peer_request(serial_port, PPP_PROTOCOL_IPCP, identifier, data, logger)
                peer_acked = True
            elif code == PPP_CODE_CONFIGURE_ACK and identifier == local_identifier:
                local_acked = True
            elif code == PPP_CODE_CONFIGURE_NAK and identifier == local_identifier:
                if not update_ipcp_addresses(addresses, data, logger):
                    raise PPPError(f"IPCP NAK did not offer usable addresses: {data.hex(' ')}")
                local_identifier = (local_identifier + 1) & 0xFF
                send_ppp_packet(
                    serial_port,
                    PPP_PROTOCOL_IPCP,
                    build_control_packet(PPP_CODE_CONFIGURE_REQUEST, local_identifier, build_ipcp_options(addresses)),
                    logger,
                )
            elif code == PPP_CODE_CONFIGURE_REJECT and identifier == local_identifier:
                raise PPPError(f"Peer rejected IPCP options: {data.hex(' ')}")

        if local_acked and peer_acked:
            logger.info(
                "PPP IPCP opened: local_ip=%s primary_dns=%s secondary_dns=%s",
                format_ipv4(addresses[IPCP_OPTION_IP_ADDRESS]),
                format_ipv4(addresses[IPCP_OPTION_PRIMARY_DNS]),
                format_ipv4(addresses[IPCP_OPTION_SECONDARY_DNS]),
            )
            return addresses

    raise PPPError("Timed out opening PPP IPCP")


def send_at_on_dlci_two(
    serial_port: serial.Serial,
    receive_buffer: bytearray,
    logger: logging.Logger,
) -> None:
    send_cmux_frame(
        serial_port,
        cmux_probe.build_frame(2, cmux_probe.CONTROL_UIH, b"AT+CEREG?\r"),
        logger,
    )
    response_frame = wait_for_cmux_frame(
        serial_port,
        receive_buffer,
        lambda frame: frame.dlci == 2
        and (frame.control & ~cmux_probe.CONTROL_PF) == cmux_probe.CONTROL_UIH
        and b"OK" in frame.payload,
        5.0,
        logger,
        "AT+CEREG? response on DLCI 2",
    )
    logger.info("AT channel remains active during PPP: %r", response_frame.payload)


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

        cmux_probe.send_at(serial_port, "AT", logger)
        cmux_probe.send_at(serial_port, "ATE0", logger)
        cmux_probe.send_at(serial_port, "AT&K3", logger)
        cmux_probe.send_at(serial_port, "AT+CMUX=0,0,5,1280", logger)

        receive_buffer = bytearray()
        for dlci in (0, 1, 2):
            cmux_probe.open_dlci(serial_port, receive_buffer, dlci, arguments.timeout, logger)

        dial_command = f"ATD*99***{arguments.cid}#"
        logger.info("Dialing PPP on DLCI 1: %s", dial_command)
        send_cmux_frame(
            serial_port,
            cmux_probe.build_frame(1, cmux_probe.CONTROL_UIH, (dial_command + "\r").encode("ascii")),
            logger,
        )
        wait_for_connect(serial_port, receive_buffer, arguments.timeout, logger)
        logger.info("PPP data mode entered on DLCI 1")

        authentication_option = negotiate_lcp(serial_port, receive_buffer, arguments.ppp_timeout, logger)
        if authentication_option is not None:
            if len(authentication_option) < 2:
                raise PPPError(f"Malformed LCP authentication option: {authentication_option.hex(' ')}")
            authentication_protocol = int.from_bytes(authentication_option[:2], "big")
            if arguments.username is None or arguments.password is None:
                raise PPPError("PPP authentication is required. Re-run with --username and --password.")
            if authentication_protocol == PPP_PROTOCOL_PAP:
                authenticate_pap(
                    serial_port,
                    receive_buffer,
                    arguments.username,
                    arguments.password,
                    arguments.ppp_timeout,
                    logger,
                )
            elif authentication_protocol == PPP_PROTOCOL_CHAP:
                chap_algorithm = authentication_option[2] if len(authentication_option) >= 3 else None
                if chap_algorithm != CHAP_ALGORITHM_MD5:
                    raise PPPError(f"Unsupported CHAP algorithm: {chap_algorithm}")
                authenticate_chap_md5(
                    serial_port,
                    receive_buffer,
                    arguments.username,
                    arguments.password,
                    arguments.ppp_timeout,
                    logger,
                )
            else:
                raise PPPError(f"Unsupported PPP authentication protocol {authentication_protocol:04X}")

        addresses = negotiate_ipcp(serial_port, receive_buffer, arguments.ppp_timeout, logger)
        send_at_on_dlci_two(serial_port, receive_buffer, logger)
        logger.info("PPP-over-CMUX probe succeeded. Negotiated IP: %s", format_ipv4(addresses[IPCP_OPTION_IP_ADDRESS]))

        if arguments.leave_open:
            logger.warning("PPP and CMUX channels left open by request. Reset the EVB to return to plain AT mode.")
            return

        logger.warning("Closing CMUX channels. Reset the EVB if plain AT mode does not return.")
        for dlci in (2, 1, 0):
            cmux_probe.close_dlci(serial_port, receive_buffer, dlci, arguments.timeout, logger)


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Probe PPP negotiation over Telit LE310Q1 CMUX DLCI 1.")
    parser.add_argument("--port", required=True, help="FTDI Main UART COM port, for example COM22")
    parser.add_argument("--cid", type=int, default=1, help="PDP context identifier used in ATD*99***<cid>#")
    parser.add_argument("--baudrate", type=int, default=115200, help="Plain UART baud rate before CMUX activation")
    parser.add_argument("--timeout", type=float, default=5.0, help="Seconds to wait for CMUX control responses")
    parser.add_argument("--ppp-timeout", type=float, default=30.0, help="Seconds to wait for each PPP negotiation stage")
    parser.add_argument("--username", help="PAP username, only when the network requests PAP")
    parser.add_argument("--password", help="PAP password, only when the network requests PAP")
    parser.add_argument("--leave-open", action="store_true", help="Do not close CMUX channels after a successful probe")
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    logger = configure_logging(arguments.port)

    try:
        run_probe(arguments, logger)
    except (PPPError, cmux_probe.CMUXError, serial.SerialException, ValueError) as error:
        logger.error("PPP-over-CMUX probe failed: %s", format_error_for_log(error))
        logger.error("The modem may remain in CMUX or PPP data mode. Reset or power-cycle the EVB, then verify plain AT again.")
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
