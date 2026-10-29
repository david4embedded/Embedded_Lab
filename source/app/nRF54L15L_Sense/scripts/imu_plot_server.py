#!/usr/bin/env python3
"""Read the board's IMU shell stream and serve a live browser plot."""

from __future__ import annotations

import argparse
import importlib
import json
import re
import threading
import webbrowser
from collections import deque
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from typing import Any


MAX_HISTORY = 30000

SAMPLE_PATTERN = re.compile(
    r"^\s*(?P<time>\d+)\s*\|\s*"
    r"(?P<ax>-?\d+)\s+(?P<ay>-?\d+)\s+(?P<az>-?\d+)\s*\|\s*"
    r"(?P<gx>-?\d+)\s+(?P<gy>-?\d+)\s+(?P<gz>-?\d+)\s*\|\s*"
    r"(?P<temp>-?\d+(?:\.\d+)?)\s*$"
)

PAGE = r"""<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>nRF54L15 Sense - IMU live plot</title>
  <style>
    :root { color-scheme: dark; font: 14px system-ui, sans-serif; background: #10151d; color: #e6edf3; }
    body { max-width: 1100px; margin: 0 auto; padding: 20px; }
    header { display: flex; align-items: baseline; justify-content: space-between; gap: 16px; flex-wrap: wrap; }
    h1 { font-size: 1.35rem; margin: 0 0 8px; }
    #status { color: #9daab8; }
    .controls { display: flex; align-items: center; gap: 8px; flex-wrap: wrap; margin: 14px 0; }
    button, input { color: inherit; background: #202a37; border: 1px solid #3a485a; border-radius: 5px; padding: 7px 10px; }
    button { cursor: pointer; }
    button:disabled { cursor: wait; opacity: .6; }
    input { width: 75px; }
    .chart { margin: 14px 0; padding: 12px; background: #171e28; border: 1px solid #293443; border-radius: 8px; }
    .chart h2 { font-size: 1rem; margin: 0 0 8px; }
    canvas { display: block; width: 100%; height: 190px; }
    .legend { color: #9daab8; margin-top: 6px; font-size: .85rem; }
    @media (max-width: 600px) { body { padding: 12px; } canvas { height: 160px; } }
  </style>
</head>
<body>
  <header>
    <div><h1>nRF54L15 Sense IMU</h1><div id="status">Connecting to serial reader…</div></div>
    <div id="latest">Waiting for samples</div>
  </header>
  <div class="controls">
    <button id="stream-toggle" type="button">Start stream</button>
    <button id="clear-graph" type="button">Clear graph</button>
    <button id="mode-toggle" type="button">Fixed window (10 s) → Show accumulated</button>
    <label for="window-seconds">Window (seconds)</label>
    <input id="window-seconds" type="number" min="1" max="3600" value="__WINDOW_SECONDS__">
    <label for="sample-interval">Sample interval (ms)</label>
    <input id="sample-interval" type="number" min="10" max="60000" step="1" value="__INTERVAL__">
  </div>
  <section class="chart"><h2>Acceleration (mg)</h2><canvas id="accel"></canvas><div class="legend">X: red · Y: green · Z: blue</div></section>
  <section class="chart"><h2>Angular velocity (mdps)</h2><canvas id="gyro"></canvas><div class="legend">X: red · Y: green · Z: blue</div></section>
  <section class="chart"><h2>Temperature (°C)</h2><canvas id="temperature"></canvas></section>
<script>
const samples = [];
const colors = ["#ff6b6b", "#65d98b", "#63a9ff"];
const status = document.getElementById("status");
const latest = document.getElementById("latest");
const streamToggle = document.getElementById("stream-toggle");
const clearGraph = document.getElementById("clear-graph");
const modeToggle = document.getElementById("mode-toggle");
const windowSeconds = document.getElementById("window-seconds");
const sampleInterval = document.getElementById("sample-interval");
let streaming = false;
let fixedWindow = true;
let renderScheduled = false;
let lastRenderAt = 0;
const MIN_RENDER_INTERVAL_MS = 1000 / 30;

function updateModeLabel() {
  modeToggle.textContent = fixedWindow
    ? `Fixed window (${windowDurationMs() / 1000} s) → Show accumulated`
    : "Accumulated → Show fixed window";
}

function updateStreaming(state) {
  streaming = state.streaming;
  streamToggle.textContent = streaming ? "Stop stream" : "Start stream";
  sampleInterval.disabled = streaming;
  if (Number.isInteger(state.interval)) sampleInterval.value = String(state.interval);
}

function windowDurationMs() {
  const seconds = Number(windowSeconds.value);
  return (Number.isFinite(seconds) ? Math.min(3600, Math.max(1, seconds)) : 10) * 1000;
}

function visibleSamples() {
  if (!fixedWindow || samples.length === 0) return samples;
  const end = samples[samples.length - 1].time;
  const start = end - windowDurationMs();
  return samples.filter(sample => sample.time >= start);
}

function chart(canvasId, title, keys, unit) {
  const canvas = document.getElementById(canvasId);
  const ctx = canvas.getContext("2d");
  const dpr = window.devicePixelRatio || 1;
  const width = canvas.clientWidth;
  const height = canvas.clientHeight;
  if (!width || !height) return;
  if (canvas.width !== Math.round(width * dpr) || canvas.height !== Math.round(height * dpr)) {
    canvas.width = Math.round(width * dpr);
    canvas.height = Math.round(height * dpr);
  }
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  ctx.clearRect(0, 0, width, height);
  ctx.font = "12px system-ui, sans-serif";
  ctx.fillStyle = "#9daab8";
  ctx.strokeStyle = "#2b3644";
  const left = 55, right = 12, top = 10, bottom = 25;
  const plotWidth = width - left - right, plotHeight = height - top - bottom;
  const visible = visibleSamples();
  if (visible.length < 2) {
    ctx.fillText("Waiting for IMU samples…", left, top + 20);
    return;
  }
  const allValues = visible.flatMap(sample => keys.map(key => sample[key]));
  let min = Math.min(...allValues), max = Math.max(...allValues);
  if (min === max) { min -= 1; max += 1; }
  const padding = (max - min) * 0.12;
  min -= padding; max += padding;
  for (let i = 0; i <= 4; i++) {
    const y = top + (plotHeight * i / 4);
    const value = max - ((max - min) * i / 4);
    ctx.beginPath(); ctx.moveTo(left, y); ctx.lineTo(width - right, y); ctx.stroke();
    ctx.fillText(value.toFixed(unit === "°C" ? 1 : 0), 2, y + 4);
  }
  const lastTime = visible[visible.length - 1].time;
  const windowMs = windowDurationMs();
  const firstTime = fixedWindow ? lastTime - windowMs : visible[0].time;
  const timeRange = Math.max(1, lastTime - firstTime);
  const pointStep = Math.max(1, Math.ceil(visible.length / (plotWidth * 2)));
  for (let series = 0; series < keys.length; series++) {
    ctx.beginPath(); ctx.strokeStyle = colors[series]; ctx.lineWidth = 1.6;
    for (let index = 0; index < visible.length; index += pointStep) {
      const sample = visible[index];
      const x = left + (sample.time - firstTime) / timeRange * plotWidth;
      const y = top + (max - sample[keys[series]]) / (max - min) * plotHeight;
      if (index === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
    }
    const last = visible[visible.length - 1];
    if ((visible.length - 1) % pointStep !== 0) {
      ctx.lineTo(
        left + (last.time - firstTime) / timeRange * plotWidth,
        top + (max - last[keys[series]]) / (max - min) * plotHeight
      );
    }
    ctx.stroke();
  }
  ctx.fillStyle = "#9daab8";
  ctx.fillText(`${firstTime} ms`, left, height - 5);
  ctx.textAlign = "right";
  ctx.fillText(`${lastTime} ms`, width - right, height - 5);
  ctx.textAlign = "left";
}

function render() {
  chart("accel", "Acceleration", ["ax", "ay", "az"], "mg");
  chart("gyro", "Angular velocity", ["gx", "gy", "gz"], "mdps");
  chart("temperature", "Temperature", ["temp"], "°C");
}

function scheduleRender() {
  if (renderScheduled) return;
  renderScheduled = true;
  const delay = Math.max(0, MIN_RENDER_INTERVAL_MS - (performance.now() - lastRenderAt));
  window.setTimeout(() => {
    requestAnimationFrame(() => {
      renderScheduled = false;
      lastRenderAt = performance.now();
      render();
    });
  }, delay);
}

const events = new EventSource("/events");
events.addEventListener("reset", () => {
  samples.length = 0;
  latest.textContent = "Waiting for samples";
  scheduleRender();
});
events.addEventListener("status", event => {
  const state = JSON.parse(event.data);
  status.textContent = state.message;
  updateStreaming(state);
});
events.addEventListener("sample", event => {
  const sample = JSON.parse(event.data);
  samples.push(sample);
  if (samples.length > 30000) samples.shift();
  latest.textContent = `t=${sample.time} ms · ${sample.temp.toFixed(2)} °C`;
  scheduleRender();
});
events.onerror = () => { status.textContent = "Connection interrupted; reconnecting…"; };
clearGraph.addEventListener("click", () => {
  samples.length = 0;
  latest.textContent = "Waiting for samples";
  scheduleRender();
});
streamToggle.addEventListener("click", async () => {
  const interval = sampleInterval.valueAsNumber;
  if (!streaming && (!Number.isInteger(interval) || interval < 10 || interval > 60000)) {
    status.textContent = "Sample interval must be an integer from 10 to 60000 ms.";
    return;
  }
  streamToggle.disabled = true;
  try {
    const response = await fetch("/stream", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ action: streaming ? "stop" : "start", interval })
    });
    const state = await response.json();
    if (!response.ok) throw new Error(state.error || "Stream control failed");
    status.textContent = state.message;
    updateStreaming(state);
  } catch (error) {
    status.textContent = error.message;
  } finally {
    streamToggle.disabled = false;
  }
});
modeToggle.addEventListener("click", () => {
  fixedWindow = !fixedWindow;
  updateModeLabel();
  scheduleRender();
});
windowSeconds.addEventListener("input", () => {
  updateModeLabel();
  scheduleRender();
});
windowSeconds.addEventListener("change", () => {
  windowSeconds.value = String(windowDurationMs() / 1000);
  updateModeLabel();
  scheduleRender();
});
window.addEventListener("resize", scheduleRender);
scheduleRender();
updateModeLabel();
</script>
</body>
</html>
"""


class SampleStore:
    def __init__(self, interval: int) -> None:
        self.condition = threading.Condition()
        self.samples: deque[dict[str, int | float]] = deque(maxlen=MAX_HISTORY)
        self.sequence = 0
        self.status = "Waiting for serial data"
        self.streaming = False
        self.interval = interval
        self.generation = 0
        self.last_sample_time: int | None = None

    def start_stream(self, interval: int) -> None:
        with self.condition:
            self.generation += 1
            self.samples.clear()
            self.status = "Starting IMU stream"
            self.streaming = True
            self.interval = interval
            self.condition.notify_all()

    def add(self, sample: dict[str, int | float]) -> None:
        with self.condition:
            sample_time = int(sample["time"])
            if (
                self.streaming
                and self.last_sample_time is not None
                and sample_time < self.last_sample_time
            ):
                self.generation += 1
                self.samples.clear()
                self.status = "Streaming IMU data"
            self.sequence += 1
            sample["sequence"] = self.sequence
            self.samples.append(sample)
            if self.streaming and self.status == "Starting IMU stream":
                self.status = "Streaming IMU data"
            self.last_sample_time = sample_time
            self.condition.notify_all()

    def set_status(
        self,
        message: str,
        streaming: bool | None = None,
        interval: int | None = None,
    ) -> None:
        with self.condition:
            self.status = message
            if streaming is not None:
                self.streaming = streaming
            if interval is not None:
                self.interval = interval
            self.condition.notify_all()


class StreamController:
    def __init__(self, port: Any, store: SampleStore) -> None:
        self.port = port
        self.store = store
        self.lock = threading.Lock()

    def set_running(
        self,
        running: bool,
        interval: int | None = None,
    ) -> dict[str, str | bool | int]:
        with self.lock:
            if self.store.streaming == running:
                return {
                    "message": self.store.status,
                    "streaming": self.store.streaming,
                    "interval": self.store.interval,
                }
            if running:
                if interval is None or not 10 <= interval <= 60000:
                    raise ValueError("Sample interval must be between 10 and 60000 ms")
                self.store.start_stream(interval)
                try:
                    self.port.write(f"imu stream {interval}\r".encode("ascii"))
                except OSError:
                    self.store.set_status("Could not start IMU stream", streaming=False)
                    raise
            else:
                self.port.write(b"\x1b")
                self.store.set_status("IMU stream stopped", streaming=False)
            return {
                "message": self.store.status,
                "streaming": self.store.streaming,
                "interval": self.store.interval,
            }


def parse_sample(line: str) -> dict[str, int | float] | None:
    match = SAMPLE_PATTERN.fullmatch(line.strip())
    if match is None:
        return None
    fields = match.groupdict()
    return {
        "time": int(fields["time"]),
        "ax": int(fields["ax"]),
        "ay": int(fields["ay"]),
        "az": int(fields["az"]),
        "gx": int(fields["gx"]),
        "gy": int(fields["gy"]),
        "gz": int(fields["gz"]),
        "temp": float(fields["temp"]),
    }


def make_handler(
    store: SampleStore,
    stream_controller: StreamController,
    window_seconds: int,
    initial_interval: int,
) -> type[BaseHTTPRequestHandler]:
    class Handler(BaseHTTPRequestHandler):
        def send_json(
            self,
            status_code: int,
            payload: dict[str, str | bool | int],
        ) -> None:
            content = json.dumps(payload).encode("utf-8")
            self.send_response(status_code)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Content-Length", str(len(content)))
            self.end_headers()
            self.wfile.write(content)

        def do_GET(self) -> None:
            if self.path == "/":
                content = (
                    PAGE.replace("__WINDOW_SECONDS__", str(window_seconds))
                    .replace("__INTERVAL__", str(initial_interval))
                    .encode("utf-8")
                )
                self.send_response(200)
                self.send_header("Content-Type", "text/html; charset=utf-8")
                self.send_header("Content-Length", str(len(content)))
                self.end_headers()
                self.wfile.write(content)
                return
            if self.path == "/events":
                self.send_response(200)
                self.send_header("Content-Type", "text/event-stream; charset=utf-8")
                self.send_header("Cache-Control", "no-cache")
                self.send_header("Connection", "keep-alive")
                self.end_headers()
                try:
                    cursor = max(0, int(self.headers.get("Last-Event-ID", "0")))
                except ValueError:
                    cursor = 0
                sent_status = ""
                sent_generation = -1
                try:
                    while True:
                        with store.condition:
                            store.condition.wait_for(
                                lambda: store.sequence > cursor or store.status != sent_status,
                                timeout=15,
                            )
                            pending = [s.copy() for s in store.samples if s["sequence"] > cursor]
                            current_status = store.status
                            current_streaming = store.streaming
                            current_interval = store.interval
                            current_generation = store.generation
                        if current_generation != sent_generation:
                            payload = json.dumps({"generation": current_generation})
                            self.wfile.write(f"event: reset\ndata: {payload}\n\n".encode("utf-8"))
                            sent_generation = current_generation
                        if current_status != sent_status:
                            payload = json.dumps(
                                {
                                    "message": current_status,
                                    "streaming": current_streaming,
                                    "interval": current_interval,
                                }
                            )
                            self.wfile.write(f"event: status\ndata: {payload}\n\n".encode("utf-8"))
                            sent_status = current_status
                        for sample in pending:
                            cursor = int(sample["sequence"])
                            payload = json.dumps(sample)
                            self.wfile.write(
                                f"id: {cursor}\nevent: sample\ndata: {payload}\n\n".encode("utf-8")
                            )
                        if not pending and current_status == sent_status:
                            self.wfile.write(b": keep-alive\n\n")
                        self.wfile.flush()
                except (BrokenPipeError, ConnectionResetError):
                    return
            self.send_error(404, "Not found")

        def do_POST(self) -> None:
            if self.path != "/stream":
                self.send_error(404, "Not found")
                return
            try:
                length = int(self.headers.get("Content-Length", "0"))
                if not 0 < length <= 1024:
                    raise ValueError("Invalid request body size")
                request = json.loads(self.rfile.read(length))
                action = request.get("action") if isinstance(request, dict) else None
                if action not in ("start", "stop"):
                    raise ValueError("Action must be 'start' or 'stop'")
                interval = request.get("interval") if action == "start" else None
                if action == "start" and (
                    type(interval) is not int or not 10 <= interval <= 60000
                ):
                    raise ValueError("Sample interval must be an integer from 10 to 60000 ms")
            except (ValueError, json.JSONDecodeError) as error:
                self.send_json(400, {"error": str(error)})
                return
            try:
                state = stream_controller.set_running(action == "start", interval)
            except ValueError as error:
                self.send_json(400, {"error": str(error)})
                return
            except OSError as error:
                self.send_json(503, {"error": f"Serial communication failed: {error}"})
                return
            self.send_json(200, state)

        def log_message(self, format_string: str, *args: Any) -> None:
            print(f"HTTP: {format_string % args}")

    return Handler


def read_serial(port: Any, store: SampleStore, stop: threading.Event) -> None:
    try:
        while not stop.is_set():
            raw_line = port.readline()
            if not raw_line:
                continue
            line = raw_line.decode("utf-8", errors="replace")
            sample = parse_sample(line)
            if sample is not None:
                store.add(sample)
    except OSError as error:
        store.set_status(f"Serial reader stopped: {error}", streaming=False)
        print(f"ERROR: Serial reader stopped: {error}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serial-port", required=True, help="USB serial port, e.g. COM5")
    parser.add_argument("--baud", type=int, default=115200, help="serial baud rate (default: 115200)")
    parser.add_argument("--interval", type=int, default=20, help="IMU stream interval in ms (10-60000, default: 20)")
    parser.add_argument("--window-seconds", type=int, default=10, help="initial fixed graph window in seconds (default: 10)")
    parser.add_argument("--host", default="127.0.0.1", help="HTTP bind address (default: localhost only)")
    parser.add_argument("--http-port", type=int, default=8000, help="browser UI port (default: 8000)")
    parser.add_argument("--no-browser", action="store_true", help="do not open the browser automatically")
    args = parser.parse_args()
    if not 10 <= args.interval <= 60000:
        parser.error("--interval must be between 10 and 60000 ms")
    if args.baud <= 0:
        parser.error("--baud must be positive")
    if not 1 <= args.http_port <= 65535:
        parser.error("--http-port must be between 1 and 65535")
    if not 1 <= args.window_seconds <= 3600:
        parser.error("--window-seconds must be between 1 and 3600")
    return args


def main() -> int:
    args = parse_args()
    try:
        serial_module = importlib.import_module("serial")
    except ModuleNotFoundError as error:
        if error.name != "serial":
            raise
        print("ERROR: pyserial is required. Install it with: py -m pip install pyserial")
        return 1

    try:
        port = serial_module.Serial(args.serial_port, args.baud, timeout=0.2)
    except serial_module.SerialException as error:
        print(f"ERROR: Could not open {args.serial_port}: {error}")
        return 1

    store = SampleStore(args.interval)
    stream_controller = StreamController(port, store)
    stop = threading.Event()
    server: ThreadingHTTPServer | None = None
    reader = threading.Thread(target=read_serial, args=(port, store, stop), daemon=True)
    try:
        reader.start()
        server = ThreadingHTTPServer(
            (args.host, args.http_port),
            make_handler(store, stream_controller, args.window_seconds, args.interval),
        )
        server.daemon_threads = True
        url_host = "127.0.0.1" if args.host in ("0.0.0.0", "::") else args.host
        url = f"http://{url_host}:{args.http_port}/"
        store.set_status(f"Connected to {args.serial_port}; waiting for IMU samples")
        print(f"Connected to {args.serial_port} at {args.baud} baud; IMU stream starts from the browser.")
        print(f"Open {url} to view the live plots. Press Ctrl+C to stop the server.")
        if not args.no_browser:
            threading.Timer(0.5, webbrowser.open, args=(url,)).start()
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStopping IMU plot server.")
    except serial_module.SerialException as error:
        print(f"ERROR: Serial communication failed: {error}")
        return 1
    except OSError as error:
        print(f"ERROR: Could not start HTTP server: {error}")
        return 1
    finally:
        stop.set()
        if reader.is_alive() and store.streaming:
            try:
                stream_controller.set_running(False)
            except OSError as error:
                print(f"WARNING: Could not send stream-stop key: {error}")
        if reader.is_alive():
            reader.join(timeout=1)
        port.close()
        if server is not None:
            server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
