// Worker Pool Manager - Persistent Python processes with Unix Domain Sockets
use std::path::PathBuf;
use std::process::{Child, Command, Stdio};
use std::sync::Arc;
use std::sync::atomic::{AtomicU64, Ordering};
use tokio::sync::{Mutex, Semaphore, mpsc};
use interprocess::local_socket::prelude::*;
use interprocess::local_socket::GenericNamespaced;
use serde::{Deserialize, Serialize};
use std::collections::BinaryHeap;
use std::io::{Read, Write};

#[cfg(target_os = "windows")]
use std::os::windows::process::CommandExt;

#[cfg(target_os = "windows")]
const CREATE_NO_WINDOW: u32 = 0x08000000;

const NUM_WORKERS: usize = 2;  // Two Python processes
const NUM_THREADS: usize = 4;  // Number of concurrent scripts per process

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WorkerTask {
    pub task_id: String,
    pub script_path: String,
    pub args: Vec<String>,
    pub venv: String,  // "venv-numpy1" or "venv-numpy2"
    pub priority: u8,  // 0=HIGH, 1=NORMAL, 2=LOW
}

// Priority wrapper for BinaryHeap (min-heap based on priority)
#[derive(Debug, Clone)]
struct PrioritizedTask {
    task: WorkerTask,
    sequence: u64,  // For FIFO within same priority
}

impl PartialEq for PrioritizedTask {
    fn eq(&self, other: &Self) -> bool {
        self.task.priority == other.task.priority && self.sequence == other.sequence
    }
}

impl Eq for PrioritizedTask {}

impl PartialOrd for PrioritizedTask {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for PrioritizedTask {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        // Reverse order for min-heap (lower priority value = higher priority)
        other.task.priority.cmp(&self.task.priority)
            .then_with(|| self.sequence.cmp(&other.sequence))
    }
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
    task_sender: mpsc::UnboundedSender<PrioritizedTask>,
    response_senders: Arc<Mutex<std::collections::HashMap<String, tokio::sync::oneshot::Sender<WorkerResponse>>>>,
    sequence_counter: Arc<AtomicU64>,
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
        eprintln!("[WorkerPool] Initializing with {} workers, {} threads each", NUM_WORKERS, NUM_THREADS);

        // Create socket name base
        let socket_name = format!("fincept-worker-{}", std::process::id());
        eprintln!("[WorkerPool] Socket name base: {}", socket_name);

        // Start multiple worker processes
        let mut workers_vec = Vec::new();
        for i in 0..NUM_WORKERS {
            let worker_socket = format!("{}-{}", socket_name, i);
            eprintln!("[WorkerPool] Starting worker {} with {} threads", i, NUM_THREADS);

            // Use venv-numpy2 as default (has most modern libraries)
            let worker = Self::spawn_worker(i, &python_base_path, "venv-numpy2", &worker_socket).await?;
            workers_vec.push(worker);
        }

        let workers = Arc::new(Mutex::new(workers_vec));

        // Create priority queue channel
        let (task_sender, task_receiver) = mpsc::unbounded_channel::<PrioritizedTask>();
        let response_senders = Arc::new(Mutex::new(std::collections::HashMap::new()));
        let sequence_counter = Arc::new(AtomicU64::new(0));

        eprintln!("[WorkerPool] All {} workers started successfully", NUM_WORKERS);

        // Spawn queue processor task
        let workers_clone = workers.clone();
        let response_senders_clone = response_senders.clone();
        tokio::spawn(Self::process_queue(task_receiver, workers_clone, response_senders_clone));

        Ok(WorkerPool {
            workers,
            task_sender,
            response_senders,
            sequence_counter,
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

        // Wait for worker to start listening with retry logic
        // Python worker creates server socket, so we need to wait for it
        let mut socket: Option<LocalSocketStream> = None;
        let max_retries = 50;  // 5 seconds total (50 * 100ms)

        for attempt in 0..max_retries {
            tokio::time::sleep(tokio::time::Duration::from_millis(100)).await;

            match LocalSocketStream::connect(
                socket_name.to_ns_name::<GenericNamespaced>()
                    .map_err(|e| format!("Failed to create socket name: {}", e))?
            ) {
                Ok(s) => {
                    socket = Some(s);
                    eprintln!("[Worker {}] Connected to socket after {} attempts", worker_id, attempt + 1);
                    break;
                }
                Err(e) => {
                    if attempt == max_retries - 1 {
                        return Err(format!("Failed to connect to worker {} after {} attempts: {}", worker_id, max_retries, e));
                    }
                    // Continue retrying silently
                }
            }
        }

        let socket = socket.ok_or_else(|| format!("Failed to connect to worker {}", worker_id))?;

        Ok(Worker {
            process,
            socket,
        })
    }

    /// Process the priority queue - distributes tasks to available workers
    async fn process_queue(
        mut task_receiver: mpsc::UnboundedReceiver<PrioritizedTask>,
        workers: Arc<Mutex<Vec<Worker>>>,
        response_senders: Arc<Mutex<std::collections::HashMap<String, tokio::sync::oneshot::Sender<WorkerResponse>>>>,
    ) {
        let mut priority_queue: BinaryHeap<PrioritizedTask> = BinaryHeap::new();
        let mut worker_index = 0;  // Round-robin worker selection

        eprintln!("[WorkerPool] Queue processor started with {} workers", NUM_WORKERS);

        // Spawn response reader tasks for each worker
        for i in 0..NUM_WORKERS {
            let workers_clone = workers.clone();
            let response_senders_clone = response_senders.clone();

            tokio::spawn(async move {
                loop {
                    tokio::time::sleep(tokio::time::Duration::from_millis(10)).await;

                    let mut workers_lock = workers_clone.lock().await;
                    if i >= workers_lock.len() {
                        break;
                    }

                    let worker = &mut workers_lock[i];

                    // Try to read response (non-blocking check)
                    let mut len_buf = [0u8; 4];
                    if worker.socket.read_exact(&mut len_buf).is_ok() {
                        let response_len = u32::from_le_bytes(len_buf) as usize;
                        let mut response_buf = vec![0u8; response_len];

                        if worker.socket.read_exact(&mut response_buf).is_ok() {
                            if let Ok(response) = rmp_serde::from_slice::<WorkerResponse>(&response_buf) {
                                eprintln!("[WorkerPool] Worker {} completed task {}: {}", i, response.task_id, response.status);

                                // Send response back to caller
                                let mut senders = response_senders_clone.lock().await;
                                if let Some(sender) = senders.remove(&response.task_id) {
                                    let _ = sender.send(response);
                                }
                            }
                        }
                    }

                    drop(workers_lock);
                }
            });
        }

        loop {
            tokio::select! {
                Some(new_task) = task_receiver.recv() => {
                    // Add new task to priority queue
                    priority_queue.push(new_task);
                }
                _ = tokio::time::sleep(tokio::time::Duration::from_millis(1)) => {
                    // Try to dispatch tasks to workers
                    if let Some(prioritized_task) = priority_queue.pop() {
                        let task = prioritized_task.task;

                        eprintln!("[WorkerPool] Dispatching task {} (priority {}) to worker {}", task.task_id, task.priority, worker_index);

                        // Send task to next worker (round-robin)
                        let mut workers_lock = workers.lock().await;
                        if worker_index < workers_lock.len() {
                            let worker = &mut workers_lock[worker_index];

                            // Serialize and send task
                            if let Ok(task_bytes) = rmp_serde::to_vec(&task) {
                                let len_bytes = (task_bytes.len() as u32).to_le_bytes();

                                if worker.socket.write_all(&len_bytes).is_ok()
                                    && worker.socket.write_all(&task_bytes).is_ok()
                                    && worker.socket.flush().is_ok() {
                                    eprintln!("[WorkerPool] Task {} sent to worker {}", task.task_id, worker_index);

                                    // Round-robin to next worker
                                    worker_index = (worker_index + 1) % NUM_WORKERS;
                                } else {
                                    eprintln!("[WorkerPool] Failed to send task {} to worker {}", task.task_id, worker_index);
                                }
                            }
                        }
                        drop(workers_lock);
                    }
                }
            }
        }
    }

    /// Execute a task using the worker pool
    pub async fn execute_task(&self, task: WorkerTask) -> Result<WorkerResponse, String> {
        let task_id = task.task_id.clone();

        eprintln!("[WorkerPool] Queuing task {} with priority {}", task_id, task.priority);

        // Create response channel
        let (response_tx, response_rx) = tokio::sync::oneshot::channel();

        {
            let mut senders = self.response_senders.lock().await;
            senders.insert(task_id.clone(), response_tx);
        }

        // Add task to priority queue
        let sequence = self.sequence_counter.fetch_add(1, Ordering::Relaxed);
        let prioritized_task = PrioritizedTask {
            task,
            sequence,
        };

        self.task_sender.send(prioritized_task)
            .map_err(|e| format!("Failed to queue task: {}", e))?;

        // Wait for response
        let response = response_rx.await
            .map_err(|e| format!("Failed to receive response: {}", e))?;

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
    execute_python_script_with_priority(script_path, args, venv, 1).await  // Default to NORMAL priority
}

/// Execute a Python script using the worker pool with custom priority
pub async fn execute_python_script_with_priority(
    script_path: PathBuf,
    args: Vec<String>,
    venv: &str,
    priority: u8,
) -> Result<String, String> {
    let task = WorkerTask {
        task_id: uuid::Uuid::new_v4().to_string(),
        script_path: script_path.to_string_lossy().to_string(),
        args,
        venv: venv.to_string(),
        priority,
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
