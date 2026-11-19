#!/usr/bin/env python3
"""
SortSense - Standalone Local AI WebSocket Vision Test Server (Phase 2)
Accepts binary JPEG frames from ESP32-S3 over WebSocket at /ws/vision, saves the
most recent frame to latest.jpg, logs received FPS and payload size, and simulates
AI classification replies cycling through bins 0-3 every 3 seconds.

Requirements:
    pip install fastapi uvicorn websockets

Usage:
    python3 tools/test_ws_server.py --host 0.0.0.0 --port 8000
"""

import argparse
import asyncio
import json
import logging
import os
import time
from typing import Dict

import uvicorn
from fastapi import FastAPI, WebSocket, WebSocketDisconnect

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    datefmt="%H:%M:%S",
)
logger = logging.getLogger("SortSense-TestServer")

app = FastAPI(title="SortSense Local AI Vision Test Server")

# Simulated classification cycle
CLASSIFICATION_CYCLES = [
    {"category": "plastic", "bin_index": 0, "confidence": 0.96},
    {"category": "paper", "bin_index": 1, "confidence": 0.92},
    {"category": "metal", "bin_index": 2, "confidence": 0.89},
    {"category": "general", "bin_index": 3, "confidence": 0.84},
]

OUTPUT_IMAGE_PATH = os.path.join(os.getcwd(), "latest.jpg")


@app.get("/")
async def root():
    return {
        "status": "online",
        "service": "SortSense Local AI Vision Server",
        "endpoints": ["/ws/vision"],
        "latest_image": OUTPUT_IMAGE_PATH,
    }


@app.websocket("/ws/vision")
async def websocket_vision_endpoint(websocket: WebSocket):
    await websocket.accept()
    client_host = websocket.client.host if websocket.client else "unknown"
    logger.info(f"ESP32-S3 connected from {client_host} on /ws/vision")

    frame_count = 0
    total_bytes = 0
    last_fps_time = time.time()
    fps = 0.0

    async def classification_sender():
        cycle_idx = 0
        try:
            while True:
                await asyncio.sleep(3.0)
                msg = CLASSIFICATION_CYCLES[cycle_idx % len(CLASSIFICATION_CYCLES)]
                cycle_idx += 1
                payload_str = json.dumps(msg)
                await websocket.send_text(payload_str)
                logger.info(
                    f"[SIMULATED AI] Dispatched -> Bin {msg['bin_index']} "
                    f"({msg['category'].upper()}) Conf: {msg['confidence']:.2f}"
                )
        except asyncio.CancelledError:
            pass
        except Exception as e:
            logger.debug(f"Classification sender stopped: {e}")

    sender_task = asyncio.create_task(classification_sender())

    try:
        while True:
            # Receive binary frame from ESP32-S3
            data = await websocket.receive_bytes()
            frame_len = len(data)
            frame_count += 1
            total_bytes += frame_len

            # Save latest frame for live inspection
            try:
                with open(OUTPUT_IMAGE_PATH, "wb") as f:
                    f.write(data)
            except Exception as io_err:
                logger.warning(f"Failed to write latest.jpg: {io_err}")

            # Compute running FPS over 1-second rolling windows
            now = time.time()
            dt = now - last_fps_time
            if dt >= 1.0:
                fps = frame_count / dt
                frame_count = 0
                last_fps_time = now
                logger.info(
                    f"[FRAME] Size: {frame_len:,} bytes ({frame_len / 1024:.1f} KB) | "
                    f"Rate: {fps:.1f} FPS | Saved -> {OUTPUT_IMAGE_PATH}"
                )

    except WebSocketDisconnect:
        logger.warning(f"ESP32-S3 client disconnected from {client_host}")
    except Exception as exc:
        logger.error(f"WebSocket error: {exc}")
    finally:
        sender_task.cancel()
        try:
            await sender_task
        except asyncio.CancelledError:
            pass
        logger.info("Session cleanup completed.")


def main():
    parser = argparse.ArgumentParser(
        description="SortSense Phase 2 Local AI WebSocket Test Server"
    )
    parser.add_argument(
        "--host", default="0.0.0.0", help="Binding host address (default: 0.0.0.0)"
    )
    parser.add_argument(
        "--port", type=int, default=8000, help="Port to listen on (default: 8000)"
    )
    args = parser.parse_args()

    print("==================================================================")
    print("SortSense AI Vision Test Server (Phase 2)")
    print(f"Listening on: ws://{args.host}:{args.port}/ws/vision")
    print(f"Latest frame saved to: {OUTPUT_IMAGE_PATH}")
    print("Simulated classifications: cycling bins 0-3 every 3s")
    print("==================================================================")

    uvicorn.run(app, host=args.host, port=args.port, log_level="warning")


if __name__ == "__main__":
    main()

