#!/usr/bin/env python3
"""
Python Worker Process - Persistent process with ThreadPoolExecutor
Communicates with Rust via Named Pipes (Windows) or Unix Domain Sockets
Single process handles multiple requests concurrently using threads

IMPORTANT: This worker LISTENS (server mode), Rust CONNECTS (client mode)
"""

import sys
import os
import json
import traceback
import importlib.util
import socket
import struct
import threading
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor

# Try to import msgpack, fall back to json if not available
try:
    import msgpack
    USE_MSGPACK = True
except ImportError:
    USE_MSGPACK = False
    print("[Worker] msgpack not available, using JSON fallback", file=sys.stderr, flush=True)


class PythonWorker:
    def __init__(self, socket_name, worker_id, num_threads=4):
        self.socket_name = socket_name
        self.worker_id = worker_id
        self.num_threads = num_threads
        self.server_socket = None
        self.client_socket = None
        self.running = True
        self.executor = ThreadPoolExecutor(max_workers=num_threads)
        self.send_lock = threading.Lock()

        print(f"[Worker {worker_id}] Initializing with {num_threads} threads", file=sys.stderr, flush=True)
        print(f"[Worker {worker_id}] Python: {sys.version}", file=sys.stderr, flush=True)
        print(f"[Worker {worker_id}] Socket: {socket_name}", file=sys.stderr, flush=True)

    def start_server(self):
        """Create socket/pipe and listen for Rust connection"""
        if sys.platform == 'win32':
            self._start_windows_pipe_server()
        else:
            self._start_unix_socket_server()

    def _start_windows_pipe_server(self):
        """Create Windows named pipe server"""
        try:
            import win32pipe
            import win32file
            import pywintypes
        except ImportError:
            # Fallback to TCP socket on localhost for Windows without pywin32
            print(f"[Worker {self.worker_id}] pywin32 not available, using TCP fallback", file=sys.stderr, flush=True)
            self._start_tcp_server()
            return

        pipe_name = f"\\\\.\\pipe\\{self.socket_name}"
        print(f"[Worker {self.worker_id}] Creating named pipe: {pipe_name}", file=sys.stderr, flush=True)

        try:
            # Create named pipe (server mode)
            self.server_socket = win32pipe.CreateNamedPipe(
                pipe_name,
                win32pipe.PIPE_ACCESS_DUPLEX,
                win32pipe.PIPE_TYPE_BYTE | win32pipe.PIPE_READMODE_BYTE | win32pipe.PIPE_WAIT,
                1,  # Max instances
                65536,  # Out buffer size
                65536,  # In buffer size
                0,  # Default timeout
                None  # Security attributes
            )

            print(f"[Worker {self.worker_id}] Named pipe created, waiting for connection...", file=sys.stderr, flush=True)

            # Wait for client (Rust) to connect
            win32pipe.ConnectNamedPipe(self.server_socket, None)
            self.client_socket = self.server_socket  # Use same handle for read/write

            print(f"[Worker {self.worker_id}] Client connected to named pipe", file=sys.stderr, flush=True)

        except pywintypes.error as e:
            print(f"[Worker {self.worker_id}] Named pipe error: {e}, falling back to TCP", file=sys.stderr, flush=True)
            self._start_tcp_server()

    def _start_unix_socket_server(self):
        """Create Unix domain socket server"""
        # Determine socket path
        if self.socket_name.startswith('/'):
            sock_path = self.socket_name
        else:
            sock_path = f"/tmp/{self.socket_name}"

        # Remove existing socket file if present
        try:
            os.unlink(sock_path)
        except FileNotFoundError:
            pass

        print(f"[Worker {self.worker_id}] Creating Unix socket: {sock_path}", file=sys.stderr, flush=True)

        self.server_socket = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.server_socket.bind(sock_path)
        self.server_socket.listen(1)

        print(f"[Worker {self.worker_id}] Listening for connection...", file=sys.stderr, flush=True)

        # Accept one connection from Rust
        self.client_socket, addr = self.server_socket.accept()
        print(f"[Worker {self.worker_id}] Client connected", file=sys.stderr, flush=True)

    def _start_tcp_server(self):
        """Fallback TCP server on localhost (cross-platform)"""
        # Use port based on worker ID to avoid conflicts
        base_port = 19876
        port = base_port + int(self.socket_name.split('-')[-1]) if '-' in self.socket_name else base_port

        print(f"[Worker {self.worker_id}] Creating TCP socket on port {port}", file=sys.stderr, flush=True)

        self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.server_socket.bind(('127.0.0.1', port))
        self.server_socket.listen(1)

        # Write port to a temp file so Rust can find it
        port_file = Path(os.environ.get('TEMP', '/tmp')) / f"{self.socket_name}.port"
        port_file.write_text(str(port))
        print(f"[Worker {self.worker_id}] Port file: {port_file}", file=sys.stderr, flush=True)

        print(f"[Worker {self.worker_id}] Listening on TCP port {port}...", file=sys.stderr, flush=True)

        # Accept one connection from Rust
        self.client_socket, addr = self.server_socket.accept()
        print(f"[Worker {self.worker_id}] Client connected from {addr}", file=sys.stderr, flush=True)

    def read_message(self):
        """Read a MessagePack/JSON message from socket"""
        try:
            if sys.platform == 'win32' and hasattr(self, 'using_pipe') and self.using_pipe:
                import win32file
                # Read length (4 bytes)
                _, len_bytes = win32file.ReadFile(self.client_socket, 4)
                if len(len_bytes) < 4:
                    return None
                msg_len = struct.unpack('<I', len_bytes)[0]

                # Read message
                _, msg_bytes = win32file.ReadFile(self.client_socket, msg_len)
            else:
                # TCP or Unix socket
                len_bytes = self._recv_exact(4)
                if not len_bytes or len(len_bytes) < 4:
                    return None

                msg_len = struct.unpack('<I', len_bytes)[0]
                msg_bytes = self._recv_exact(msg_len)
                if not msg_bytes:
                    return None

            if USE_MSGPACK:
                return msgpack.unpackb(msg_bytes, raw=False)
            else:
                return json.loads(msg_bytes.decode('utf-8'))

        except Exception as e:
            print(f"[Worker {self.worker_id}] Error reading message: {e}", file=sys.stderr, flush=True)
            return None

    def _recv_exact(self, n):
        """Receive exactly n bytes from socket"""
        data = b''
        while len(data) < n:
            chunk = self.client_socket.recv(n - len(data))
            if not chunk:
                return None
            data += chunk
        return data

    def send_message(self, message):
        """Send a MessagePack/JSON message to socket"""
        with self.send_lock:
            try:
                if USE_MSGPACK:
                    msg_bytes = msgpack.packb(message, use_bin_type=True)
                else:
                    msg_bytes = json.dumps(message).encode('utf-8')

                len_bytes = struct.pack('<I', len(msg_bytes))

                if sys.platform == 'win32' and hasattr(self, 'using_pipe') and self.using_pipe:
                    import win32file
                    win32file.WriteFile(self.client_socket, len_bytes + msg_bytes)
                else:
                    self.client_socket.sendall(len_bytes + msg_bytes)

            except Exception as e:
                print(f"[Worker {self.worker_id}] Error sending message: {e}", file=sys.stderr, flush=True)

    def execute_script(self, task):
        """Execute a Python script and return the result"""
        try:
            script_path = task['script_path']
            args = task.get('args', [])
            task_id = task['task_id']

            print(f"[Worker {self.worker_id}] Executing: {script_path}", file=sys.stderr, flush=True)

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
            print(f"[Worker {self.worker_id}] Task {response['task_id']} completed", file=sys.stderr, flush=True)
        except Exception as e:
            print(f"[Worker {self.worker_id}] ERROR sending response: {e}", file=sys.stderr, flush=True)
            traceback.print_exc()

    def run(self):
        """Main worker loop - handles multiple concurrent requests via thread pool"""
        try:
            # Start server and wait for Rust to connect
            self.start_server()

            print(f"[Worker {self.worker_id}] Ready to accept tasks (thread pool with {self.num_threads} threads)", file=sys.stderr, flush=True)

            while self.running:
                # Read next task from socket
                task = self.read_message()
                if task is None:
                    print(f"[Worker {self.worker_id}] Connection closed", file=sys.stderr, flush=True)
                    break

                print(f"[Worker {self.worker_id}] Received task: {task.get('task_id', 'unknown')}", file=sys.stderr, flush=True)

                # Submit task to thread pool
                future = self.executor.submit(self.execute_script, task)
                future.add_done_callback(self.handle_task_completion)

        except KeyboardInterrupt:
            print(f"[Worker {self.worker_id}] Shutting down...", file=sys.stderr, flush=True)
        except Exception as e:
            print(f"[Worker {self.worker_id}] FATAL ERROR: {e}", file=sys.stderr, flush=True)
            traceback.print_exc()
        finally:
            self.shutdown()

    def shutdown(self):
        """Clean up resources"""
        self.running = False
        self.executor.shutdown(wait=False)

        if self.client_socket:
            try:
                if sys.platform == 'win32' and hasattr(self, 'using_pipe') and self.using_pipe:
                    import win32file
                    win32file.CloseHandle(self.client_socket)
                else:
                    self.client_socket.close()
            except:
                pass

        if self.server_socket and self.server_socket != self.client_socket:
            try:
                self.server_socket.close()
            except:
                pass

        print(f"[Worker {self.worker_id}] Shutdown complete", file=sys.stderr, flush=True)


def main():
    if len(sys.argv) < 3:
        print("Usage: worker.py <socket_name> <worker_id> [num_threads]", file=sys.stderr)
        sys.exit(1)

    socket_name = sys.argv[1]
    worker_id = int(sys.argv[2])
    num_threads = int(sys.argv[3]) if len(sys.argv) > 3 else 4

    worker = PythonWorker(socket_name, worker_id, num_threads)
    worker.run()


if __name__ == "__main__":
    main()
