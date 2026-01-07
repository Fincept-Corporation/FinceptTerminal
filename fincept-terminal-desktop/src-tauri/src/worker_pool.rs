// Worker Pool Manager - Persistent Python processes with Unix Domain Sockets
use std::path::PathBuf;
use std::process::{Child, Command, Stdio};
use std::sync::Arc;
use tokio::sync::{Mutex, Semaphore};
use interprocess::local_socket::prelude::*;
use interprocess::local_socket::GenericNamespaced;
use serde::{Deserialize, Serialize};
use std::collections::VecDeque;
use std::io::{Read, Write};

#[cfg(target_os = "windows")]
use std::os::windows::process::CommandExt;

#[cfg(target_os = "windows")]
const CREATE_NO_WINDOW: u32 = 0x08000000;

const NUM_WORKERS: usize = 1;  // Single process with multi-threading inside Python
const NUM_THREADS: usize = 4;  // Number of Python threads for parallel execution

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WorkerTask {
    pub task_id: String,
    pub script_path: String,
    pub args: Vec<String>,
    pub venv: String,  // "venv-numpy1" or "venv-numpy2"
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WorkerResponse {
    pub task_id: String,
    pub status: String,  // "success" or "error"
    pub data: String,    // JSON result or error message
}

struct Worker {
    process: Child,
    socket: LocalSocketStream,
}

pub struct WorkerPool {
    workers: Arc<Mutex<Vec<Worker>>>,
    task_queue: Arc<Mutex<VecDeque<WorkerTask>>>,
    semaphore: Arc<Semaphore>,
    socket_name: String,
}

impl WorkerPool {
    /// Find worker script in multiple possible locations (dev/prod compatible)
    fn find_worker_script(python_base_path: &PathBuf) -> Result<PathBuf, String> {
        let mut candidate_paths = Vec::new();

        // Strategy: Try multiple paths like get_script_path does

        // 1. Production: relative to python_base_path parent (install_dir)
        if let Some(install_dir) = python_base_path.parent() {
            candidate_paths.push(install_dir.join("resources").join("scripts").join("worker.py"));
        }

        // 2. Dev mode: relative to current working directory
        if let Ok(cwd) = std::env::current_dir() {
            candidate_paths.push(cwd.join("resources").join("scripts").join("worker.py"));
            candidate_paths.push(cwd.join("src-tauri").join("resources").join("scripts").join("worker.py"));
        }

        // 3. Production: relative to executable
        if let Ok(exe_path) = std::env::current_exe() {
            if let Some(exe_dir) = exe_path.parent() {
                candidate_paths.push(exe_dir.join("resources").join("scripts").join("worker.py"));
                candidate_paths.push(exe_dir.join("scripts").join("worker.py"));
            }
        }

        // Try each candidate path
        for path in candidate_paths.iter() {
            eprintln!("[WorkerPool] Checking: {:?}", path);
            if path.exists() {
                // Strip the \\?\ prefix that can cause issues on Windows
                let path_str = path.to_string_lossy().to_string();
                let clean_path = if path_str.starts_with(r"\\?\") {
                    PathBuf::from(&path_str[4..])
                } else {
                    path.clone()
                };
                eprintln!("[WorkerPool] Found worker script at: {:?}", clean_path);
                return Ok(clean_path);
            }
        }

        Err(format!(
            "Worker script 'worker.py' not found in any of {} candidate locations",
            candidate_paths.len()
        ))
    }

    /// Initialize the worker pool
    pub async fn new(python_base_path: PathBuf) -> Result<Self, String> {
        eprintln!("[WorkerPool] Initializing with {} workers", NUM_WORKERS);

        // Create socket name
        let socket_name = format!("fincept-worker-{}", std::process::id());
        eprintln!("[WorkerPool] Socket name: {}", socket_name);

        // Start single worker process with multi-threading
        eprintln!("[WorkerPool] Starting single worker process with {} threads", NUM_THREADS);

        // Use venv-numpy2 as default (has most modern libraries)
        let worker = Self::spawn_worker(0, &python_base_path, "venv-numpy2", &socket_name).await?;
        let workers = vec![worker];

        eprintln!("[WorkerPool] All {} workers started successfully", NUM_WORKERS);

        Ok(WorkerPool {
            workers: Arc::new(Mutex::new(workers)),
            task_queue: Arc::new(Mutex::new(VecDeque::new())),
            semaphore: Arc::new(Semaphore::new(NUM_WORKERS)),
            socket_name,
        })
    }

    /// Spawn a single worker process
    async fn spawn_worker(
        worker_id: usize,
        python_base_path: &PathBuf,
        venv_type: &str,
        socket_name: &str,
    ) -> Result<Worker, String> {
        // Get Python executable path
        let python_exe = if cfg!(target_os = "windows") {
            python_base_path.join(format!("{}/Scripts/python.exe", venv_type))
        } else {
            python_base_path.join(format!("{}/bin/python3", venv_type))
        };

        if !python_exe.exists() {
            return Err(format!("Python not found at: {:?}", python_exe));
        }

        // Get worker script path - try multiple locations for dev/prod compatibility
        let worker_script = Self::find_worker_script(python_base_path)?;

        if !worker_script.exists() {
            return Err(format!("Worker script not found at: {:?}", worker_script));
        }

        eprintln!("[Worker {}] Python: {:?}", worker_id, python_exe);
        eprintln!("[Worker {}] Script: {:?}", worker_id, worker_script);
        eprintln!("[Worker {}] Socket: {}", worker_id, socket_name);

        // Spawn Python process
        let mut cmd = Command::new(&python_exe);
        cmd.arg(worker_script.to_str().unwrap())
            .arg(socket_name)
            .arg(worker_id.to_string())
            .arg(NUM_THREADS.to_string())  // Pass number of threads
            .stdin(Stdio::null())
            .stdout(Stdio::piped())
            .stderr(Stdio::piped());

        #[cfg(target_os = "windows")]
        cmd.creation_flags(CREATE_NO_WINDOW);

        let process = cmd.spawn()
            .map_err(|e| format!("Failed to spawn worker {}: {}", worker_id, e))?;

        eprintln!("[Worker {}] Process spawned, PID: {:?}", worker_id, process.id());

        // Wait a bit for worker to start listening (minimal delay)
        tokio::time::sleep(tokio::time::Duration::from_millis(50)).await;

        // Connect to worker's socket
        let socket = LocalSocketStream::connect(
            socket_name.to_ns_name::<GenericNamespaced>()
                .map_err(|e| format!("Failed to create socket name: {}", e))?
        ).map_err(|e| format!("Failed to connect to worker {}: {}", worker_id, e))?;

        eprintln!("[Worker {}] Connected to socket", worker_id);

        Ok(Worker {
            process,
            socket,
        })
    }

    /// Execute a task using the worker pool
    pub async fn execute_task(&self, task: WorkerTask) -> Result<WorkerResponse, String> {
        // Acquire semaphore permit (blocks if all workers busy)
        let permit = self.semaphore.acquire().await
            .map_err(|e| format!("Failed to acquire worker: {}", e))?;

        // Get the single worker
        let mut workers = self.workers.lock().await;
        let worker = &mut workers[0];

        eprintln!("[WorkerPool] Executing task {}", task.task_id);

        // Serialize task to MessagePack
        let task_bytes = rmp_serde::to_vec(&task)
            .map_err(|e| format!("Failed to serialize task: {}", e))?;

        // Send task length + task data
        let len_bytes = (task_bytes.len() as u32).to_le_bytes();

        worker.socket.write_all(&len_bytes)
            .map_err(|e| format!("Failed to send task length: {}", e))?;
        worker.socket.write_all(&task_bytes)
            .map_err(|e| format!("Failed to send task: {}", e))?;
        worker.socket.flush()
            .map_err(|e| format!("Failed to flush socket: {}", e))?;

        eprintln!("[WorkerPool] Task sent, waiting for response...");

        // Read response length
        let mut len_buf = [0u8; 4];
        worker.socket.read_exact(&mut len_buf)
            .map_err(|e| format!("Failed to read response length: {}", e))?;
        let response_len = u32::from_le_bytes(len_buf) as usize;

        eprintln!("[WorkerPool] Response length: {} bytes", response_len);

        // Read response data
        let mut response_buf = vec![0u8; response_len];
        worker.socket.read_exact(&mut response_buf)
            .map_err(|e| format!("Failed to read response: {}", e))?;

        // Deserialize response
        let response: WorkerResponse = rmp_serde::from_slice(&response_buf)
            .map_err(|e| format!("Failed to deserialize response: {}", e))?;

        eprintln!("[WorkerPool] Task {} completed with status: {}", task.task_id, response.status);

        // Release permit
        drop(permit);
        drop(workers);

        Ok(response)
    }

    /// Shutdown all workers
    pub async fn shutdown(&self) {
        eprintln!("[WorkerPool] Shutting down all workers");

        let mut workers = self.workers.lock().await;

        for (i, worker) in workers.iter_mut().enumerate() {
            eprintln!("[WorkerPool] Killing worker {}", i);
            let _ = worker.process.kill();
        }

        eprintln!("[WorkerPool] Shutdown complete");
    }
}

impl Drop for WorkerPool {
    fn drop(&mut self) {
        eprintln!("[WorkerPool] Dropping worker pool");
    }
}

// Global worker pool instance
use once_cell::sync::OnceCell;
static WORKER_POOL: OnceCell<Arc<Mutex<Option<WorkerPool>>>> = OnceCell::new();

/// Initialize the global worker pool
pub async fn initialize_worker_pool(python_base_path: PathBuf) -> Result<(), String> {
    eprintln!("[WorkerPool] Initializing global worker pool");

    let pool = WorkerPool::new(python_base_path).await?;

    WORKER_POOL.get_or_init(|| Arc::new(Mutex::new(Some(pool))));

    eprintln!("[WorkerPool] Global worker pool initialized");
    Ok(())
}

/// Get the global worker pool
pub async fn get_worker_pool() -> Result<Arc<Mutex<Option<WorkerPool>>>, String> {
    WORKER_POOL.get()
        .ok_or_else(|| "Worker pool not initialized. Please run setup first.".to_string())
        .map(|pool| pool.clone())
}

/// Execute a Python script using the worker pool
pub async fn execute_python_script(
    script_path: PathBuf,
    args: Vec<String>,
    venv: &str,
) -> Result<String, String> {
    let task = WorkerTask {
        task_id: uuid::Uuid::new_v4().to_string(),
        script_path: script_path.to_string_lossy().to_string(),
        args,
        venv: venv.to_string(),
    };

    let pool_mutex = get_worker_pool().await?;
    let pool_guard = pool_mutex.lock().await;
    let pool = pool_guard.as_ref()
        .ok_or("Worker pool not initialized")?;

    let response = pool.execute_task(task).await?;

    if response.status == "success" {
        Ok(response.data)
    } else {
        Err(response.data)
    }
}
