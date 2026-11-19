#!/usr/bin/env python3
"""
SortSense - Standalone Local AI WebSocket Vision Test Server (Phase 2)
Accepts binary JPEG frames from ESP32-S3 over WebSocket at /ws/vision, saves the
most recent frame to latest.jpg, logs received FPS and payload size, and simulates
AI classification replies cycling through bins 0-3 every 3 seconds.

Built with standard Python asyncio and the robust `websockets` library to eliminate
FastAPI/Starlette version mismatches.

Requirements:
    pip install websockets

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
    import websockets
except ImportError:
    print("\n[ERROR] The 'websockets' library is required.")
    print("Please install it using: pip install websockets\n")
    sys.exit(1)

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    datefmt="%H:%M:%S",
)
logger = logging.getLogger("SortSense-Server")

# Simulated classification cycle matching CONTEXT.md contract
CLASSIFICATION_CYCLES = [
    {"category": "plastic", "bin_index": 0, "confidence": 0.96},
    {"category": "paper", "bin_index": 1, "confidence": 0.92},
    {"category": "metal", "bin_index": 2, "confidence": 0.89},
    {"category": "general", "bin_index": 3, "confidence": 0.84},
]

OUTPUT_IMAGE_PATH = os.path.join(os.getcwd(), "latest.jpg")


def get_connection_path(websocket) -> str:
    """Extract requested URL path across different websockets library versions."""
    if hasattr(websocket, "request") and hasattr(websocket.request, "path"):
        return websocket.request.path
    if hasattr(websocket, "path"):
        return websocket.path
    return "/"


def get_remote_address(websocket) -> str:
    """Extract client IP/port for clean logging."""
    if hasattr(websocket, "remote_address") and websocket.remote_address:
        return f"{websocket.remote_address[0]}:{websocket.remote_address[1]}"
    return "ESP32-S3"


async def handle_client(*args):
    """
    WebSocket client connection handler.
    Supports both websockets < 14 (ws, path) and >= 14 (ws) callback signatures.
    """
    websocket = args[0]
    path = args[1] if len(args) > 1 else get_connection_path(websocket)
    client_addr = get_remote_address(websocket)

    logger.info(f"Client connected from {client_addr} on path: {path}")
    if path != "/ws/vision":
        logger.warning(
            f"Client connected to '{path}', expected '/ws/vision'. Continuing anyway..."
        )

    # Background task to send simulated AI classifications every 3 seconds
    async def classification_sender():
        cycle_idx = 0
        try:
            while True:
                await asyncio.sleep(3.0)
                msg = CLASSIFICATION_CYCLES[cycle_idx % len(CLASSIFICATION_CYCLES)]
                cycle_idx += 1
                payload_str = json.dumps(msg)
                await websocket.send(payload_str)
                logger.info(
                    f"[SIMULATED AI -> ESP32] Bin {msg['bin_index']} "
                    f"({msg['category'].upper()}) | Conf: {msg['confidence']:.2f}"
                )
        except (asyncio.CancelledError, websockets.exceptions.ConnectionClosed):
            pass
        except Exception as err:
            logger.debug(f"Classification sender stopped: {err}")

    sender_task = asyncio.create_task(classification_sender())

    frame_count = 0
    total_bytes = 0
    last_fps_time = time.time()
    last_logged_time = time.time()

    try:
        async for message in websocket:
            now = time.time()

            if isinstance(message, bytes):
                frame_len = len(message)
                frame_count += 1
                total_bytes += frame_len

                # Save the latest received JPEG to disk for real-time inspection
                try:
                    with open(OUTPUT_IMAGE_PATH, "wb") as f:
                        f.write(message)
                except Exception as io_err:
                    logger.warning(f"Failed to write latest.jpg: {io_err}")

                # Log FPS and throughput metrics approximately once per second
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

            elif isinstance(message, str):
                logger.info(f"[TEXT FROM ESP32] {message}")

    except websockets.exceptions.ConnectionClosed as cc:
        logger.warning(f"Connection closed by {client_addr} (code: {cc.code})")
    except Exception as exc:
        logger.error(f"Unexpected error handling client {client_addr}: {exc}")
    finally:
        sender_task.cancel()
        try:
            await sender_task
        except asyncio.CancelledError:
            pass
        logger.info(f"Client session cleaned up for {client_addr}")


async def run_server(host: str, port: int):
    # Allow frames up to 10 MB and unbounded bursts
    async with websockets.serve(
        handle_client,
        host=host,
        port=port,
        max_size=10 * 1024 * 1024,
        ping_interval=20,
        ping_timeout=20,
    ):
        logger.info("==================================================================")
        logger.info("SortSense Standalone AI WebSocket Vision Server")
        logger.info(f"Listening on: ws://{host}:{port}/ws/vision")
        logger.info(f"Latest frame output: {OUTPUT_IMAGE_PATH}")
        logger.info("Simulated AI: Cycling bins 0-3 every 3s")
        logger.info("Press Ctrl+C to stop")
        logger.info("==================================================================")
        # Keep server running until cancelled
        await asyncio.Future()


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

    loop = asyncio.new_event_loop()
    asyncio.set_event_loop(loop)

    server_task = loop.create_task(run_server(args.host, args.port))

    def shutdown():
        server_task.cancel()

    for sig in (signal.SIGINT, signal.SIGTERM):
        try:
            loop.add_signal_handler(sig, shutdown)
        except (NotImplementedError, RuntimeError):
            pass

    try:
        loop.run_until_complete(server_task)
    except (asyncio.CancelledError, KeyboardInterrupt):
        logger.info("Server stopped cleanly by user.")
    finally:
        loop.close()


if __name__ == "__main__":
    main()
