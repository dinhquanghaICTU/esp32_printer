#!/usr/bin/env python3
"""
Simple MQTT web uploader for esp32_printer.

Install:
    pip install flask paho-mqtt

Run:
    python tools/mqtt_web_uploader.py --host 192.168.1.175 --port 1883

Open:
    http://127.0.0.1:8080
"""

from __future__ import annotations

import argparse
import binascii
import os
import struct
import time
from dataclasses import dataclass
from typing import Iterable

from flask import Flask, request, render_template_string, redirect, url_for
import paho.mqtt.client as mqtt

MQTT_OTA_BEGIN = 1
MQTT_OTA_DATA = 2
MQTT_OTA_END = 3

DEFAULT_GCODE_TOPIC = "printer/gcode"
DEFAULT_OTA_TOPIC = "printer/ota"
DEFAULT_CHUNK_SIZE = 256

HTML = r"""
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <title>ESP32 Printer MQTT Uploader</title>
  <style>
    body { font-family: Arial, sans-serif; max-width: 860px; margin: 32px auto; padding: 0 16px; }
    h1 { margin-bottom: 6px; }
    .muted { color: #666; margin-top: 0; }
    form { border: 1px solid #ddd; padding: 16px; margin: 18px 0; border-radius: 8px; }
    label { display: block; margin-top: 10px; font-weight: 600; }
    input, select { margin-top: 6px; padding: 8px; width: 100%; box-sizing: border-box; }
    button { margin-top: 14px; padding: 10px 16px; cursor: pointer; }
    pre { background: #111; color: #eee; padding: 12px; border-radius: 8px; overflow: auto; }
  </style>
</head>
<body>
  <h1>ESP32 Printer MQTT Uploader</h1>
  <p class="muted">Broker: {{ broker_host }}:{{ broker_port }}</p>

  <form action="/upload" method="post" enctype="multipart/form-data">
    <label>File mode</label>
    <select name="mode">
      <option value="gcode">G-code: publish line by line</option>
      <option value="ota">STM32 OTA .bin: publish binary packets</option>
    </select>

    <label>File</label>
    <input type="file" name="file" required>

    <label>G-code topic</label>
    <input name="gcode_topic" value="{{ gcode_topic }}">

    <label>OTA topic</label>
    <input name="ota_topic" value="{{ ota_topic }}">

    <label>OTA chunk size</label>
    <input name="chunk_size" value="{{ chunk_size }}">

    <button type="submit">Upload and publish</button>
  </form>

  {% if message %}
  <h2>Result</h2>
  <pre>{{ message }}</pre>
  {% endif %}

  <h2>Quick publish</h2>
  <form action="/publish" method="post">
    <label>Topic</label>
    <input name="topic" value="test/topic">
    <label>Message</label>
    <input name="message" value="hello esp32">
    <button type="submit">Publish</button>
  </form>
</body>
</html>
"""


@dataclass
class Settings:
    broker_host: str
    broker_port: int
    gcode_topic: str
    ota_topic: str
    chunk_size: int


def mqtt_client(settings: Settings) -> mqtt.Client:
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    client.connect(settings.broker_host, settings.broker_port, keepalive=60)
    client.loop_start()
    return client


def publish_wait(client: mqtt.Client, topic: str, payload: bytes, qos: int = 1, retain: bool = False) -> None:
    info = client.publish(topic, payload=payload, qos=qos, retain=retain)
    info.wait_for_publish(timeout=10)
    if not info.is_published():
        raise RuntimeError(f"publish timeout topic={topic}")


def iter_gcode_lines(data: bytes) -> Iterable[bytes]:
    text = data.decode("utf-8", errors="ignore")
    for raw_line in text.splitlines():
        line = raw_line.strip()
        if not line:
            continue
        if line.startswith(";"):
            continue
        if ";" in line:
            line = line.split(";", 1)[0].strip()
        if line:
            yield (line + "\n").encode("utf-8")


def ota_packet(packet_type: int, seq: int, size: int, crc32: int, payload: bytes = b"") -> bytes:
    # Must match ESP32 ota_mqtt_header_t: uint8_t type; uint32_t seq; uint32_t size; uint32_t crc32;
    header = struct.pack("<BIII", packet_type, seq, size, crc32)
    return header + payload


def publish_gcode(settings: Settings, data: bytes, topic: str) -> str:
    client = mqtt_client(settings)
    count = 0
    try:
        for line in iter_gcode_lines(data):
            publish_wait(client, topic, line, qos=1, retain=False)
            count += 1
            time.sleep(0.005)
    finally:
        client.loop_stop()
        client.disconnect()
    return f"Published G-code lines: {count}\nTopic: {topic}"


def publish_ota(settings: Settings, data: bytes, topic: str, chunk_size: int) -> str:
    client = mqtt_client(settings)
    fw_size = len(data)
    fw_crc32 = binascii.crc32(data) & 0xFFFFFFFF
    seq = 0
    chunks = 0

    try:
        publish_wait(client, topic, ota_packet(MQTT_OTA_BEGIN, seq, fw_size, fw_crc32), qos=1, retain=False)
        seq += 1
        time.sleep(0.05)

        for offset in range(0, fw_size, chunk_size):
            chunk = data[offset:offset + chunk_size]
            chunk_crc = binascii.crc32(chunk) & 0xFFFFFFFF
            publish_wait(client, topic, ota_packet(MQTT_OTA_DATA, seq, len(chunk), chunk_crc, chunk), qos=1, retain=False)
            seq += 1
            chunks += 1
            time.sleep(0.01)

        publish_wait(client, topic, ota_packet(MQTT_OTA_END, seq, fw_size, fw_crc32), qos=1, retain=False)
    finally:
        client.loop_stop()
        client.disconnect()

    return (
        f"Published OTA firmware\n"
        f"Topic: {topic}\n"
        f"Size: {fw_size} bytes\n"
        f"CRC32: 0x{fw_crc32:08X}\n"
        f"Chunks: {chunks}\n"
        f"Chunk size: {chunk_size}"
    )


def create_app(settings: Settings) -> Flask:
    app = Flask(__name__)
    last_message = {"text": ""}

    @app.get("/")
    def index():
        return render_template_string(
            HTML,
            broker_host=settings.broker_host,
            broker_port=settings.broker_port,
            gcode_topic=settings.gcode_topic,
            ota_topic=settings.ota_topic,
            chunk_size=settings.chunk_size,
            message=last_message["text"],
        )

    @app.post("/publish")
    def publish_text():
        topic = request.form.get("topic", "test/topic")
        message = request.form.get("message", "")
        client = mqtt_client(settings)
        try:
            publish_wait(client, topic, message.encode("utf-8"), qos=1, retain=False)
            last_message["text"] = f"Published text\nTopic: {topic}\nMessage: {message}"
        finally:
            client.loop_stop()
            client.disconnect()
        return redirect(url_for("index"))

    @app.post("/upload")
    def upload():
        mode = request.form.get("mode", "gcode")
        file = request.files.get("file")
        if file is None:
            last_message["text"] = "No file selected"
            return redirect(url_for("index"))

        data = file.read()
        if not data:
            last_message["text"] = "Empty file"
            return redirect(url_for("index"))

        gcode_topic = request.form.get("gcode_topic", settings.gcode_topic)
        ota_topic = request.form.get("ota_topic", settings.ota_topic)
        chunk_size = int(request.form.get("chunk_size", settings.chunk_size))

        if mode == "ota":
            last_message["text"] = publish_ota(settings, data, ota_topic, chunk_size)
        else:
            last_message["text"] = publish_gcode(settings, data, gcode_topic)

        last_message["text"] = f"File: {file.filename}\n" + last_message["text"]
        return redirect(url_for("index"))

    return app


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default=os.environ.get("MQTT_HOST", "192.168.1.175"))
    parser.add_argument("--port", type=int, default=int(os.environ.get("MQTT_PORT", "1883")))
    parser.add_argument("--web-port", type=int, default=8080)
    parser.add_argument("--gcode-topic", default=DEFAULT_GCODE_TOPIC)
    parser.add_argument("--ota-topic", default=DEFAULT_OTA_TOPIC)
    parser.add_argument("--chunk-size", type=int, default=DEFAULT_CHUNK_SIZE)
    args = parser.parse_args()

    settings = Settings(
        broker_host=args.host,
        broker_port=args.port,
        gcode_topic=args.gcode_topic,
        ota_topic=args.ota_topic,
        chunk_size=args.chunk_size,
    )

    app = create_app(settings)
    app.run(host="127.0.0.1", port=args.web_port, debug=False)


if __name__ == "__main__":
    main()
