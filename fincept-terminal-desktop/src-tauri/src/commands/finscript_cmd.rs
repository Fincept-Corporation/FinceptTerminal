use serde::{Deserialize, Serialize};
use std::path::PathBuf;
use tokio::process::Command;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FinScriptResult {
    pub success: bool,
    pub output: String,
    pub signals: Vec<Signal>,
    pub plots: Vec<PlotData>,
    pub errors: Vec<String>,
    pub alerts: Vec<AlertInfo>,
    pub drawings: Vec<DrawingInfo>,
    pub execution_time_ms: u64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Signal {
    pub signal_type: String,
    pub message: String,
    pub timestamp: String,
    pub price: Option<f64>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PlotData {
    pub plot_type: String,
    pub label: String,
    pub data: Vec<PlotPoint>,
    pub color: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PlotPoint {
    pub timestamp: i64,
    pub value: Option<f64>,
    pub open: Option<f64>,
    pub high: Option<f64>,
    pub low: Option<f64>,
    pub close: Option<f64>,
    pub volume: Option<f64>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AlertInfo {
    pub message: String,
    pub alert_type: String,
    pub timestamp: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DrawingInfo {
    pub drawing_type: String,
    pub x1: i64,
    pub y1: f64,
    pub x2: i64,
    pub y2: f64,
    pub color: String,
    pub text: String,
    pub style: String,
    pub width: f64,
}

/// Find the finscript binary path.
/// In development: looks in src-tauri/finscript/target/release/
/// In production: looks next to the main executable.
fn find_finscript_binary() -> Result<PathBuf, String> {
    // Try development path first (relative to src-tauri/)
    let dev_path = PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .join("finscript")
        .join("target")
        .join("release")
        .join(if cfg!(windows) { "finscript.exe" } else { "finscript" });

    if dev_path.exists() {
        return Ok(dev_path);
    }

    // Try debug build path
    let debug_path = PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .join("finscript")
        .join("target")
        .join("debug")
        .join(if cfg!(windows) { "finscript.exe" } else { "finscript" });

    if debug_path.exists() {
        return Ok(debug_path);
    }

    // Try next to the current executable (production bundle)
    if let Ok(exe_path) = std::env::current_exe() {
        if let Some(exe_dir) = exe_path.parent() {
            let bundled_path = exe_dir.join(if cfg!(windows) { "finscript.exe" } else { "finscript" });
            if bundled_path.exists() {
                return Ok(bundled_path);
            }
        }
    }

    Err(format!(
        "FinScript binary not found. Expected at: {}. Run 'cargo build --release' in src-tauri/finscript/ first.",
        dev_path.display()
    ))
}

#[tauri::command]
pub async fn execute_finscript(
    _app: tauri::AppHandle,
    code: String,
) -> Result<FinScriptResult, String> {
    let binary_path = find_finscript_binary()?;

    // Spawn the standalone finscript binary, piping code via stdin
    let mut cmd = Command::new(&binary_path);
    cmd.stdin(std::process::Stdio::piped())
        .stdout(std::process::Stdio::piped())
        .stderr(std::process::Stdio::piped());

    // Hide console window on Windows
    #[cfg(windows)]
    {
        cmd.creation_flags(0x08000000); // CREATE_NO_WINDOW
    }

    let mut child = cmd.spawn()
        .map_err(|e| format!("Failed to start finscript: {}", e))?;

    // Write code to stdin
    if let Some(mut stdin) = child.stdin.take() {
        use tokio::io::AsyncWriteExt;
        stdin.write_all(code.as_bytes()).await
            .map_err(|e| format!("Failed to write to finscript stdin: {}", e))?;
        drop(stdin); // Close stdin to signal EOF
    }

    let output = child.wait_with_output().await
        .map_err(|e| format!("Failed to read finscript output: {}", e))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(format!("FinScript process failed: {}", stderr));
    }

    let stdout = String::from_utf8_lossy(&output.stdout);

    // Parse the JSON output from the binary
    serde_json::from_str::<FinScriptResult>(&stdout)
        .map_err(|e| format!("Failed to parse finscript output: {}. Raw: {}", e, &stdout[..stdout.len().min(200)]))
}
