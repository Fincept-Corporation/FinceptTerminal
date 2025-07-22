#!/usr/bin/env python3
"""
Simple WebSocket client to connect to Fincept vessel updates
"""

import asyncio
import websockets
import json


async def connect_and_listen():
    """Connect to WebSocket and listen for vessel updates"""
    uri = "wss://finceptbackend.share.zrok.io/ws/vessel-updates/fk_user_R9RnHoBchXbcPDrvgKi2m8MmB3GTcqKGTw9HGFDcjdc"

    try:
        async with websockets.connect(uri) as websocket:
            print(f"✅ Connected to {uri}")

            # Listen for messages
            async for message in websocket:
                data = json.loads(message)
                print(f"📨 Received: {data}")

                # Handle vessel updates specifically
                if data.get('type') == 'vessel_update':
                    print(f"🚢 Vessel Update: {data.get('operation')} - {data.get('data')}")

    except Exception as e:
        print(f"❌ Error: {e}")


# Run the client
if __name__ == "__main__":
    asyncio.run(connect_and_listen())