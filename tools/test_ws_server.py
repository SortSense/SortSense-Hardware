#!/usr/bin/env python3
"""
SortSense - Standalone Local AI Server & Telemetry Monitor (Phases 2 & 3)
Provides:
  1. WebSocket Vision Endpoint: ws://<HOST>:<PORT>/ws/vision
     - Accepts binary JPEG frames from ESP32-S3
     - Saves most recent frame to latest.jpg for live inspection
     - Logs frame size and FPS
     - Dispatches simulated classification JSON every 3s cycling bins 0-3
  2. Drop Verification HTTP POST Endpoint: http://<HOST>:<PORT>/internal/verify-drop
     - Receives {"station_id": "ST-001", "bin_index": 0}
     - Logs real-time trash drop verification from LM393 IR break-beam sensors
  3. Fill-Levels Telemetry HTTP POST Endpoint: http://<HOST>:<PORT>/api/telemetry/fill-levels
     - Receives compartment fill percentages from 4x VL53L0X ToF laser sensors

Requirements:
    pip install aiohttp

Usage:
    python3 tools/test_ws_server.py --host 0.0.0.0 --port 8000
"""

import argparse
import asyncio
import json
import logging
import os
import signal
import sys
import time

try:
    from aiohttp import web, WSMsgType
except ImportError:
    print("\n[ERROR] The 'aiohttp' library is required.")
    print("Please install it using: pip install aiohttp\n")
    sys.exit(1)

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    datefmt="%H:%M:%S",
)
logger = logging.getLogger("SortSense-Server")

# Standardized bin categories matching CONTEXT.md
BIN_NAMES = ["PLASTIC", "PAPER", "METAL", "GENERAL"]

CLASSIFICATION_CYCLES = [
    {"category": "plastic", "bin_index": 0, "confidence": 0.96},
    {"category": "paper", "bin_index": 1, "confidence": 0.92},
    {"category": "metal", "bin_index": 2, "confidence": 0.89},
    {"category": "general", "bin_index": 3, "confidence": 0.84},
]

OUTPUT_IMAGE_PATH = os.path.join(os.getcwd(), "latest.jpg")


async def root_handler(request):
    """Health check endpoint."""
    return web.json_response({
        "status": "online",
        "service": "SortSense Local AI & Telemetry Server",
        "endpoints": {
            "websocket_vision": "/ws/vision",
            "verify_drop": "/internal/verify-drop",
            "fill_levels": "/api/telemetry/fill-levels",
        },
        "latest_image": OUTPUT_IMAGE_PATH,
    })


async def verify_drop_handler(request):
    """
    Phase 3B: LM393 IR Break-Beam drop verification receiver.
    Contract: {"station_id": "ST-001", "bin_index": <0-3>}
    """
    try:
        data = await request.json()
        station_id = data.get("station_id", "UNKNOWN")
        bin_index = data.get("bin_index", -1)
        bin_label = BIN_NAMES[bin_index] if 0 <= bin_index < len(BIN_NAMES) else "UNKNOWN"

        logger.info(
            f"🎯 [DROP VERIFIED] Station: {station_id} -> "
            f"Bin {bin_index} ({bin_label}) Drop confirmed via IR interrupt!"
        )
        return web.json_response({
            "status": "ok",
            "verified": True,
            "station_id": station_id,
            "bin_index": bin_index,
        })
    except Exception as e:
        logger.warning(f"Failed to process verify-drop: {e}")
        return web.json_response({"status": "error", "message": str(e)}, status=400)


async def fill_levels_handler(request):
    """
    Phase 3C: 4x VL53L0X Time-of-Flight compartment fill telemetry receiver.
    Contract: {"station_id": "ST-001", "fill_levels": {"plastic": 45, ...}}
    """
    try:
        data = await request.json()
        station_id = data.get("station_id", "UNKNOWN")
        levels = data.get("fill_levels", {})
        logger.info(f"📊 [TELEMETRY] Station: {station_id} Fill Levels: {levels}")
        return web.json_response({"status": "ok", "received": True})
    except Exception as e:
        logger.warning(f"Failed to process fill-levels: {e}")
        return web.json_response({"status": "error", "message": str(e)}, status=400)


async def websocket_vision_handler(request):
    """
    Phase 2: WebSocket camera stream endpoint.
    Receives binary JPEG frames, saves latest.jpg, and streams classification inferences.
    """
    ws = web.WebSocketResponse(max_msg_size=10 * 1024 * 1024)
    await ws.prepare(request)

    client_ip = request.remote or "ESP32-S3"
    logger.info(f"ESP32-S3 connected on /ws/vision from {client_ip}")

    # Background task to send simulated AI classifications every 3 seconds
    async def classification_sender():
        cycle_idx = 0
        try:
            while not ws.closed:
                await asyncio.sleep(3.0)
                msg = CLASSIFICATION_CYCLES[cycle_idx % len(CLASSIFICATION_CYCLES)]
                cycle_idx += 1
                await ws.send_str(json.dumps(msg))
                logger.info(
                    f"[SIMULATED AI -> ESP32] Bin {msg['bin_index']} "
                    f"({msg['category'].upper()}) | Conf: {msg['confidence']:.2f}"
                )
        except asyncio.CancelledError:
            pass
        except Exception as err:
            logger.debug(f"Classification sender stopped: {err}")

    sender_task = asyncio.create_task(classification_sender())

    frame_count = 0
    total_bytes = 0
    last_fps_time = time.time()
    last_logged_time = time.time()

    try:
        async for msg in ws:
            now = time.time()

            if msg.type == WSMsgType.BINARY:
                frame_bytes = msg.data
                frame_len = len(frame_bytes)
                frame_count += 1
                total_bytes += frame_len

                # Save the latest received JPEG to disk for real-time inspection
                try:
                    with open(OUTPUT_IMAGE_PATH, "wb") as f:
                        f.write(frame_bytes)
                except Exception as io_err:
                    logger.warning(f"Failed to write latest.jpg: {io_err}")

                # Calculate stream FPS
                dt = now - last_fps_time
                if dt >= 1.0:
                    fps = frame_count / dt
                    logger.info(
                        f"[FRAME] Size: {frame_len:,} bytes ({frame_len / 1024:.1f} KB) | "
                        f"Stream Rate: {fps:.1f} FPS | Saved -> latest.jpg"
                    )
                    frame_count = 0
                    last_fps_time = now
                    last_logged_time = now
                elif now - last_logged_time >= 3.0:
                    logger.info(
                        f"[FRAME] Size: {frame_len:,} bytes ({frame_len / 1024:.1f} KB) | Saved -> latest.jpg"
                    )
                    last_logged_time = now

            elif msg.type == WSMsgType.TEXT:
                logger.info(f"[TEXT FROM ESP32] {msg.data}")

            elif msg.type in (WSMsgType.CLOSED, WSMsgType.ERROR):
                break

    finally:
        sender_task.cancel()
        try:
            await sender_task
        except asyncio.CancelledError:
            pass
        logger.info(f"Client disconnected from {client_ip}")

    return ws


def make_app():
    app = web.Application()
    app.router.add_get("/", root_handler)
    app.router.add_get("/ws/vision", websocket_vision_handler)
    app.router.add_post("/internal/verify-drop", verify_drop_handler)
    app.router.add_post("/api/telemetry/fill-levels", fill_levels_handler)
    return app


def main():
    parser = argparse.ArgumentParser(
        description="SortSense Standalone AI Server & Telemetry Monitor"
    )
    parser.add_argument(
        "--host", default="0.0.0.0", help="Binding host address (default: 0.0.0.0)"
    )
    parser.add_argument(
        "--port", type=int, default=8000, help="Port to listen on (default: 8000)"
    )
    args = parser.parse_args()

    app = make_app()

    print("==================================================================")
    print("SortSense Local AI Vision Server & Drop Monitor (Phases 2 & 3)")
    print(f"WebSocket Vision Stream: ws://{args.host}:{args.port}/ws/vision")
    print(f"IR Drop Verification:    http://{args.host}:{args.port}/internal/verify-drop")
    print(f"ToF Fill Telemetry:      http://{args.host}:{args.port}/api/telemetry/fill-levels")
    print(f"Latest frame saved to:   {OUTPUT_IMAGE_PATH}")
    print("Simulated AI: Cycling bins 0-3 every 3s")
    print("Press Ctrl+C to stop")
    print("==================================================================")

    web.run_app(app, host=args.host, port=args.port, print=None)


if __name__ == "__main__":
    main()
