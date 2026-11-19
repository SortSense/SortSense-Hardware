#!/usr/bin/env python3
"""
SortSense - Standalone Local AI Server, Interactive REPL & Telemetry Monitor
=============================================================================
Provides:
  1. WebSocket Vision Endpoint: ws://<HOST>:<PORT>/ws/vision
     - Accepts binary JPEG frames from ESP32-S3
     - Saves most recent frame to latest.jpg for live inspection
     - Logs frame size and FPS
     - Broadcasts AI classification inferences to set ESP32 onboard RGB LED:
         - Bin 0 (Plastic) -> Blue
         - Bin 1 (Paper)   -> Green
         - Bin 2 (Metal)   -> Yellow
         - Bin 3 (General) -> White
         - Bin -1 (None)   -> Soft Cyan Breathing
  2. Drop Verification HTTP POST Endpoint: http://<HOST>:<PORT>/internal/verify-drop
     - Receives {"station_id": "ST-001", "bin_index": 0}
     - Logs real-time trash drop verification from LM393 IR break-beam sensors
  3. Fill-Levels Telemetry HTTP POST Endpoint: http://<HOST>:<PORT>/api/telemetry/fill-levels
     - Receives compartment fill percentages from 4x VL53L0X ToF laser sensors
  4. Interactive REPL Terminal:
     - Allows live control over ESP32 LED by typing 0, 1, 2, 3, none, or auto.

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

# Standardized bin categories
BIN_NAMES = ["PLASTIC", "PAPER", "METAL", "GENERAL"]
BIN_COLORS = {
    0: "BLUE (Plastic)",
    1: "GREEN (Paper)",
    2: "YELLOW (Metal)",
    3: "WHITE (General)",
    -1: "CYAN (Idle / Breathing)",
}

CLASSIFICATION_PRESETS = {
    "0": {"category": "plastic", "bin_index": 0, "confidence": 0.98},
    "plastic": {"category": "plastic", "bin_index": 0, "confidence": 0.98},
    "p": {"category": "plastic", "bin_index": 0, "confidence": 0.98},
    "blue": {"category": "plastic", "bin_index": 0, "confidence": 0.98},

    "1": {"category": "paper", "bin_index": 1, "confidence": 0.95},
    "paper": {"category": "paper", "bin_index": 1, "confidence": 0.95},
    "g": {"category": "paper", "bin_index": 1, "confidence": 0.95},
    "green": {"category": "paper", "bin_index": 1, "confidence": 0.95},

    "2": {"category": "metal", "bin_index": 2, "confidence": 0.93},
    "metal": {"category": "metal", "bin_index": 2, "confidence": 0.93},
    "y": {"category": "metal", "bin_index": 2, "confidence": 0.93},
    "yellow": {"category": "metal", "bin_index": 2, "confidence": 0.93},

    "3": {"category": "general", "bin_index": 3, "confidence": 0.91},
    "general": {"category": "general", "bin_index": 3, "confidence": 0.91},
    "w": {"category": "general", "bin_index": 3, "confidence": 0.91},
    "white": {"category": "general", "bin_index": 3, "confidence": 0.91},

    "-1": {"category": "none", "bin_index": -1, "confidence": 0.0},
    "none": {"category": "none", "bin_index": -1, "confidence": 0.0},
    "idle": {"category": "none", "bin_index": -1, "confidence": 0.0},
    "c": {"category": "none", "bin_index": -1, "confidence": 0.0},
    "cyan": {"category": "none", "bin_index": -1, "confidence": 0.0},
}

AUTO_CYCLES = [
    {"category": "plastic", "bin_index": 0, "confidence": 0.96},
    {"category": "paper", "bin_index": 1, "confidence": 0.92},
    {"category": "metal", "bin_index": 2, "confidence": 0.89},
    {"category": "general", "bin_index": 3, "confidence": 0.84},
]

OUTPUT_IMAGE_PATH = os.path.join(os.getcwd(), "latest.jpg")

# Global server state
active_ws_clients = set()
auto_cycle_enabled = False
current_prediction = {"category": "none", "bin_index": -1, "confidence": 0.0}
latest_frame_info = {"size": 0, "time": 0}


async def broadcast_classification(msg: dict):
    """Broadcast JSON prediction message to all connected ESP32 vision clients."""
    global current_prediction
    current_prediction = msg
    payload = json.dumps(msg)

    stale_clients = []
    for ws in list(active_ws_clients):
        if ws.closed:
            stale_clients.append(ws)
            continue
        try:
            await ws.send_str(payload)
        except Exception as e:
            logger.warning(f"Error broadcasting to client: {e}")
            stale_clients.append(ws)

    for ws in stale_clients:
        active_ws_clients.discard(ws)

    color_desc = BIN_COLORS.get(msg["bin_index"], "UNKNOWN")
    logger.info(
        f"[AI -> ESP32] Sent Bin {msg['bin_index']} ({msg['category'].upper()}) | "
        f"LED Target: {color_desc} (Active Clients: {len(active_ws_clients)})"
    )


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
        "active_clients": len(active_ws_clients),
        "auto_cycle": auto_cycle_enabled,
        "current_prediction": current_prediction,
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
            f"[DROP VERIFIED] Station: {station_id} -> "
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

        lines = [f"[TELEMETRY] Station: {station_id} Compartment Fill Levels:"]
        for bin_name in ["plastic", "paper", "metal", "general"]:
            pct = int(levels.get(bin_name, 0))
            filled = min(10, max(0, int(round(pct / 10.0))))
            bar = "#" * filled + "-" * (10 - filled)
            lines.append(f"    - {bin_name.capitalize():<8}: [{bar}] {pct:3d}%")
        logger.info("\n" + "\n".join(lines))
        return web.json_response({"status": "ok", "received": True})
    except Exception as e:
        logger.warning(f"Failed to process fill-levels: {e}")
        return web.json_response({"status": "error", "message": str(e)}, status=400)


async def websocket_vision_handler(request):
    """
    Phase 2: WebSocket camera stream endpoint.
    Receives binary JPEG frames and registers client for live inference dispatches.
    """
    ws = web.WebSocketResponse(max_msg_size=10 * 1024 * 1024)
    await ws.prepare(request)

    client_ip = request.remote or "ESP32-S3"
    active_ws_clients.add(ws)
    logger.info(f"ESP32-S3 connected on /ws/vision from {client_ip} (Total Clients: {len(active_ws_clients)})")

    # Send current prediction to newly connected ESP32 immediately
    if current_prediction.get("bin_index", -1) != -1:
        await ws.send_str(json.dumps(current_prediction))

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
                    latest_frame_info["size"] = frame_len
                    latest_frame_info["time"] = now
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
        active_ws_clients.discard(ws)
        logger.info(f"Client disconnected from {client_ip} (Remaining: {len(active_ws_clients)})")

    return ws


async def auto_cycle_worker():
    """Background task cycling classifications every 3 seconds when auto mode is enabled."""
    cycle_idx = 0
    while True:
        await asyncio.sleep(3.0)
        if auto_cycle_enabled and active_ws_clients:
            msg = AUTO_CYCLES[cycle_idx % len(AUTO_CYCLES)]
            cycle_idx += 1
            await broadcast_classification(msg)


async def repl_worker():
    """Interactive command-line REPL reader running asynchronously."""
    global auto_cycle_enabled
    loop = asyncio.get_running_loop()

    print("\n" + "=" * 65)
    print("SortSense Interactive REPL Active:")
    print("  Type '0' (or 'plastic') -> Bin 0: Blue LED")
    print("  Type '1' (or 'paper')   -> Bin 1: Green LED")
    print("  Type '2' (or 'metal')   -> Bin 2: Yellow LED")
    print("  Type '3' (or 'general') -> Bin 3: White LED")
    print("  Type 'none' (or '-1')   -> Bin -1: Idle Cyan Breathing LED")
    print("  Type 'auto'             -> Toggle 3-second auto cycling")
    print("  Type 'status'           -> Show active connections and stream info")
    print("  Type 'help'             -> Show this help menu")
    print("=" * 65 + "\n")

    while True:
        try:
            line = await loop.run_in_executor(None, sys.stdin.readline)
            if not line:
                break
            cmd = line.strip().lower()
            if not cmd:
                continue

            if cmd in CLASSIFICATION_PRESETS:
                msg = CLASSIFICATION_PRESETS[cmd]
                await broadcast_classification(msg)
                print(f">>> [REPL] Set to Bin {msg['bin_index']} ({msg['category'].upper()}) -> {BIN_COLORS.get(msg['bin_index'])}")

            elif cmd == "auto":
                auto_cycle_enabled = not auto_cycle_enabled
                print(f">>> [REPL] Auto-cycle mode: {'ENABLED (cycling every 3s)' if auto_cycle_enabled else 'DISABLED (manual REPL mode)'}")

            elif cmd in ("status", "stat", "info"):
                img_age = time.time() - latest_frame_info["time"] if latest_frame_info["time"] > 0 else -1
                age_str = f"{img_age:.1f}s ago" if img_age >= 0 else "None received yet"
                print("\n--- SERVER STATUS ---")
                print(f"Connected ESP32 Clients: {len(active_ws_clients)}")
                print(f"Auto-Cycle Mode:         {'ON' if auto_cycle_enabled else 'OFF'}")
                print(f"Current Active Pred:     Bin {current_prediction.get('bin_index')} ({current_prediction.get('category')})")
                print(f"Latest Frame Saved:      {OUTPUT_IMAGE_PATH}")
                print(f"Last Frame Size:         {latest_frame_info['size']:,} bytes ({latest_frame_info['size'] / 1024:.1f} KB), {age_str}")
                print("---------------------\n")

            elif cmd in ("help", "?"):
                print("\nCommands:")
                print("  0: Plastic (Blue)")
                print("  1: Paper (Green)")
                print("  2: Metal (Yellow)")
                print("  3: General (White)")
                print("  none / -1: Idle Cyan")
                print("  auto: Toggle auto cycle")
                print("  status: Inspect connections & camera stream")
                print("  exit / quit: Shutdown server\n")

            elif cmd in ("exit", "quit", "q"):
                print(">>> Shutting down test server...")
                os.kill(os.getpid(), 15)
                break

            else:
                print(f"Unknown command: '{cmd}'. Type 'help' for options.")

        except Exception as err:
            logger.debug(f"REPL reader exception: {err}")
            await asyncio.sleep(0.5)


async def start_background_tasks(app):
    app["auto_cycle_task"] = asyncio.create_task(auto_cycle_worker())
    app["repl_task"] = asyncio.create_task(repl_worker())


async def cleanup_background_tasks(app):
    app["auto_cycle_task"].cancel()
    app["repl_task"].cancel()
    await asyncio.gather(app["auto_cycle_task"], app["repl_task"], return_exceptions=True)


def make_app():
    app = web.Application()
    app.router.add_get("/", root_handler)
    app.router.add_get("/ws/vision", websocket_vision_handler)
    app.router.add_post("/internal/verify-drop", verify_drop_handler)
    app.router.add_post("/api/telemetry/fill-levels", fill_levels_handler)
    app.on_startup.append(start_background_tasks)
    app.on_cleanup.append(cleanup_background_tasks)
    return app


def main():
    parser = argparse.ArgumentParser(
        description="SortSense Standalone AI Server & Interactive REPL Monitor"
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
    print("==================================================================")

    web.run_app(app, host=args.host, port=args.port, print=None)


if __name__ == "__main__":
    main()
