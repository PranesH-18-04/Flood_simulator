import asyncio
import websockets
import serial
import serial.tools.list_ports
import json
import sys

# ======================================================================
# PYTHON SERIAL-TO-WEBSOCKET BRIDGE
# Use this if your browser (e.g. Firefox, Safari) does not support the 
# Web Serial API. 
# Requires: pip install pyserial websockets
# ======================================================================

BAUD_RATE = 115200
WS_PORT = 8765

def find_esp32_port():
    ports = serial.tools.list_ports.comports()
    for port in ports:
        # Common ESP32 USB-to-UART chips
        if "CP210" in port.description or "CH340" in port.description or "UART" in port.description:
            return port.device
    
    # Fallback if specific chip not identified
    if ports:
        return ports[0].device
    return None

async def serial_reader(websocket, path):
    port_name = find_esp32_port()
    if not port_name:
        print("No serial ports found. Is the ESP32 plugged in?")
        return

    print(f"Connecting to ESP32 on {port_name} at {BAUD_RATE} baud...")
    
    try:
        ser = serial.Serial(port_name, BAUD_RATE, timeout=1)
        print(f"Client connected! Forwarding data to ws://localhost:{WS_PORT}")
        
        while True:
            if ser.in_waiting > 0:
                line = ser.readline().decode('utf-8').strip()
                if line:
                    # Validate JSON before sending
                    try:
                        json.loads(line)
                        await websocket.send(line)
                    except json.JSONDecodeError:
                        pass # Ignore corrupted serial lines
            
            await asyncio.sleep(0.01) # Yield to event loop
            
    except serial.SerialException as e:
        print(f"Serial error: {e}. Is Arduino IDE Serial Monitor open?")
    except websockets.exceptions.ConnectionClosed:
        print("Web browser disconnected.")
    finally:
        if 'ser' in locals() and ser.is_open:
            ser.close()

async def main():
    print(f"Starting WebSocket server on ws://localhost:{WS_PORT}")
    print("If using this script, you must modify the HTML file to connect to WebSockets instead of navigator.serial")
    
    async with websockets.serve(serial_reader, "localhost", WS_PORT):
        await asyncio.Future()  # run forever

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nExiting...")
        sys.exit(0)
