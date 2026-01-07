#!/usr/bin/env python3
"""
Python Worker Process - Persistent process with ThreadPoolExecutor
Communicates with Rust via Unix Domain Sockets using MessagePack
Single process handles multiple requests concurrently using threads
"""

import sys
import os
import json
import traceback
import importlib.util
import socket
import struct
import msgpack
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import threading

class PythonWorker:
    def __init__(self, socket_name, worker_id, num_threads=4):
        self.socket_name = socket_name
        self.worker_id = worker_id
        self.num_threads = num_threads
        self.socket = None
        self.running = True
        self.executor = ThreadPoolExecutor(max_workers=num_threads)

        print(f"[Worker {worker_id}] Initializing with {num_threads} threads", file=sys.stderr, flush=True)
        print(f"[Worker {worker_id}] Python: {sys.version}", file=sys.stderr, flush=True)
        print(f"[Worker {worker_id}] Socket: {socket_name}", file=sys.stderr, flush=True)

    def connect_socket(self):
        """Connect to Unix domain socket or Windows named pipe"""
        if sys.platform == 'win32':
            # Windows named pipe
            import win32pipe, win32file
            pipe_path = f"\\\\.\\pipe\\{self.socket_name}"
            print(f"[Worker {self.worker_id}] Connecting to named pipe: {pipe_path}", file=sys.stderr, flush=True)

            # Wait for Rust to create the pipe
            while self.running:
                try:
                    self.socket = win32file.CreateFile(
                        pipe_path,
                        win32file.GENERIC_READ | win32file.GENERIC_WRITE,
                        0,
                        None,
                        win32file.OPEN_EXISTING,
                        0,
                        None
                    )
                    print(f"[Worker {self.worker_id}] Connected to named pipe", file=sys.stderr, flush=True)
                    break
                except Exception as e:
                    import time
                    time.sleep(0.1)
        else:
            # Unix domain socket
            if self.socket_name.startswith('/'):
                sock_path = self.socket_name
            else:
                sock_path = f"/tmp/{self.socket_name}"

            print(f"[Worker {self.worker_id}] Connecting to socket: {sock_path}", file=sys.stderr, flush=True)

            self.socket = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)

            # Wait for Rust to create the socket
            import time
            for i in range(50):  # 5 seconds total
                try:
                    self.socket.connect(sock_path)
                    print(f"[Worker {self.worker_id}] Connected to socket", file=sys.stderr, flush=True)
                    break
                except Exception as e:
                    if i == 49:
                        raise
                    time.sleep(0.1)

    def read_message(self):
        """Read a MessagePack message from socket"""
        if sys.platform == 'win32':
            import win32file
            # Read length (4 bytes)
            _, len_bytes = win32file.ReadFile(self.socket, 4)
            msg_len = struct.unpack('<I', len_bytes)[0]

            # Read message
            _, msg_bytes = win32file.ReadFile(self.socket, msg_len)
            return msgpack.unpackb(msg_bytes, raw=False)
        else:
            # Read length (4 bytes)
            len_bytes = self.socket.recv(4)
            if not len_bytes:
                return None

            msg_len = struct.unpack('<I', len_bytes)[0]

            # Read message
            msg_bytes = b''
            while len(msg_bytes) < msg_len:
                chunk = self.socket.recv(msg_len - len(msg_bytes))
                if not chunk:
                    break
                msg_bytes += chunk

            return msgpack.unpackb(msg_bytes, raw=False)

    def send_message(self, message):
        """Send a MessagePack message to socket"""
        msg_bytes = msgpack.packb(message, use_bin_type=True)
        len_bytes = struct.pack('<I', len(msg_bytes))

        if sys.platform == 'win32':
            import win32file
            win32file.WriteFile(self.socket, len_bytes + msg_bytes)
        else:
            self.socket.sendall(len_bytes + msg_bytes)

    def execute_script(self, task):
        """Execute a Python script and return the result"""
        try:
            script_path = task['script_path']
            args = task.get('args', [])
            task_id = task['task_id']

            print(f"[Worker {self.worker_id}] Executing: {script_path}", file=sys.stderr, flush=True)
            print(f"[Worker {self.worker_id}] Args: {args}", file=sys.stderr, flush=True)

            # Check if script exists
            if not os.path.exists(script_path):
                return {
                    'task_id': task_id,
                    'status': 'error',
                    'data': f'Script not found: {script_path}'
                }

            # Load script as module
            spec = importlib.util.spec_from_file_location("dynamic_script", script_path)
            module = importlib.util.module_from_spec(spec)

            # Add script directory to sys.path
            script_dir = os.path.dirname(script_path)
            if script_dir not in sys.path:
                sys.path.insert(0, script_dir)

            # Execute script
            spec.loader.exec_module(module)

            # Check for main() function
            if hasattr(module, 'main'):
                result = module.main(args)

                # If result is already JSON string, use it directly
                if isinstance(result, str):
                    result_data = result
                else:
                    result_data = json.dumps(result)

                return {
                    'task_id': task_id,
                    'status': 'success',
                    'data': result_data
                }
            else:
                return {
                    'task_id': task_id,
                    'status': 'error',
                    'data': f'Script {script_path} must have a main() function'
                }

        except Exception as e:
            error_msg = f"Error executing script: {str(e)}\n{traceback.format_exc()}"
            print(f"[Worker {self.worker_id}] ERROR: {error_msg}", file=sys.stderr, flush=True)

            return {
                'task_id': task.get('task_id', 'unknown'),
                'status': 'error',
                'data': error_msg
            }

    def handle_task_completion(self, future):
        """Callback when a task completes - sends response back"""
        try:
            response = future.result()
            self.send_message(response)
            print(f"[Worker {self.worker_id}] Task {response['task_id']} completed and sent", file=sys.stderr, flush=True)
        except Exception as e:
            print(f"[Worker {self.worker_id}] ERROR sending response: {e}", file=sys.stderr, flush=True)
            traceback.print_exc()

    def run(self):
        """Main worker loop - handles multiple concurrent requests via thread pool"""
        try:
            self.connect_socket()

            print(f"[Worker {self.worker_id}] Ready to accept tasks (thread pool with {self.num_threads} threads)", file=sys.stderr, flush=True)

            while self.running:
                try:
                    # Read task from socket (can handle multiple tasks)
                    task = self.read_message()

                    if task is None:
                        print(f"[Worker {self.worker_id}] Connection closed", file=sys.stderr, flush=True)
                        break

                    print(f"[Worker {self.worker_id}] Received task: {task.get('task_id')}", file=sys.stderr, flush=True)

                    # Submit execution to thread pool (runs in background - NO BLOCKING)
                    future = self.executor.submit(self.execute_script, task)

                    # Add callback to send response when done (non-blocking)
                    future.add_done_callback(self.handle_task_completion)

                    print(f"[Worker {self.worker_id}] Task submitted to thread pool", file=sys.stderr, flush=True)

                except KeyboardInterrupt:
                    print(f"[Worker {self.worker_id}] Interrupted", file=sys.stderr, flush=True)
                    break
                except Exception as e:
                    print(f"[Worker {self.worker_id}] ERROR in main loop: {e}", file=sys.stderr, flush=True)
                    traceback.print_exc()

        finally:
            # Shutdown thread pool
            print(f"[Worker {self.worker_id}] Shutting down thread pool...", file=sys.stderr, flush=True)
            self.executor.shutdown(wait=True)

            if self.socket:
                try:
                    if sys.platform == 'win32':
                        import win32file
                        win32file.CloseHandle(self.socket)
                    else:
                        self.socket.close()
                except:
                    pass

            print(f"[Worker {self.worker_id}] Shutdown complete", file=sys.stderr, flush=True)


if __name__ == '__main__':
    if len(sys.argv) < 4:
        print("Usage: worker.py <socket_name> <worker_id> <num_threads>", file=sys.stderr)
        sys.exit(1)

    socket_name = sys.argv[1]
    worker_id = int(sys.argv[2])
    num_threads = int(sys.argv[3])

    worker = PythonWorker(socket_name, worker_id, num_threads)
    worker.run()
