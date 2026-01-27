// Unified Python Runtime Module
// Provides Python script execution via subprocess
// Public API: execute(), execute_sync(), execute_with_stdin(), initialize(), get_bun_path()

use std::io::Write;
use std::path::PathBuf;
use std::process::{Command, Stdio};

use tauri::Manager;

#[cfg(target_os = "windows")]
use std::os::windows::process::CommandExt;

#[cfg(target_os = "windows")]
const CREATE_NO_WINDOW: u32 = 0x08000000;

// Directory names of scripts requiring NumPy 1.x venv.
// Matched against path components (directory/file names), not arbitrary substrings.
const NUMPY1_SCRIPTS: &[&str] = &[
    "vectorbt", "backtesting", "gluonts", "functime", "pyportfolioopt", "financepy",
];

// ============================================================================
// INTERNAL: Path Resolution
// ============================================================================

fn get_install_dir(app: &tauri::AppHandle) -> Result<PathBuf, String> {
    if cfg!(debug_assertions) {
        let base = if cfg!(target_os = "windows") {
            std::env::var("LOCALAPPDATA")
                .map(PathBuf::from)
                .unwrap_or_else(|_| PathBuf::from("C:\\Users\\Default\\AppData\\Local"))
        } else if cfg!(target_os = "macos") {
            std::env::var("HOME")
                .map(|h| PathBuf::from(h).join("Library/Application Support"))
                .unwrap_or_else(|_| PathBuf::from("/tmp"))
        } else {
            std::env::var("HOME")
                .map(|h| PathBuf::from(h).join(".local/share"))
                .unwrap_or_else(|_| PathBuf::from("/tmp"))
        };
        Ok(base.join("fincept-dev"))
    } else {
        app.path().app_data_dir()
            .map_err(|e| format!("Failed to get app data dir: {}", e))
    }
}

fn get_venv(script: &str) -> &'static str {
    // Match against individual path components to avoid false positives
    // e.g. "Analytics/backtesting/vectorbt/vbt_strategies.py" matches "vectorbt"
    // but "my_backtesting_report.py" should NOT match if it's just a filename
    let lower = script.to_lowercase();
    let needs_numpy1 = NUMPY1_SCRIPTS.iter().any(|keyword| {
        // Check path components separated by / or \
        lower.split(&['/', '\\'][..])
            .any(|component| component == *keyword || component.starts_with(&format!("{}.", keyword)) || component.starts_with(&format!("{}_", keyword)))
    });
    if needs_numpy1 { "venv-numpy1" } else { "venv-numpy2" }
}

fn clean_path(path: PathBuf) -> PathBuf {
    let s = path.to_string_lossy();
    if s.len() > 4 && s.starts_with(r"\\?\") {
        PathBuf::from(&s[4..])
    } else {
        path
    }
}

fn resolve_python_path(app: &tauri::AppHandle, script: &str) -> Result<PathBuf, String> {
    let install_dir = get_install_dir(app)?;
    let venv = get_venv(script);

    let python = if cfg!(target_os = "windows") {
        install_dir.join(venv).join("Scripts").join("python.exe")
    } else {
        install_dir.join(venv).join("bin").join("python3")
    };

    if python.exists() {
        return Ok(clean_path(python));
    }

    #[cfg(debug_assertions)]
    {
        let sys = if cfg!(target_os = "windows") { "python" } else { "python3" };
        if Command::new(sys).arg("--version").output().map(|o| o.status.success()).unwrap_or(false) {
            return Ok(PathBuf::from(sys));
        }
    }

    Err(format!("Python not found at: {}", python.display()))
}

fn resolve_script_path(app: &tauri::AppHandle, script: &str) -> Result<PathBuf, String> {
    let mut candidates = Vec::new();

    if let Ok(res) = app.path().resource_dir() {
        candidates.push(res.join("scripts").join(script));
    }
    if let Ok(exe) = std::env::current_exe() {
        if let Some(dir) = exe.parent() {
            candidates.push(dir.join("resources").join("scripts").join(script));
            candidates.push(dir.join("scripts").join(script));
        }
    }
    if let Ok(cwd) = std::env::current_dir() {
        candidates.push(cwd.join("resources").join("scripts").join(script));
        candidates.push(cwd.join("src-tauri").join("resources").join("scripts").join(script));
    }

    for p in &candidates {
        if p.exists() {
            return Ok(clean_path(p.clone()));
        }
    }

    Err(format!(
        "Script '{}' not found. Searched: {}",
        script,
        candidates.iter().map(|p| p.display().to_string()).collect::<Vec<_>>().join(", ")
    ))
}

fn extract_json(output: &str) -> Result<String, String> {
    let lines: Vec<&str> = output.lines().collect();

    // Try last line (compact JSON)
    if let Some(last) = lines.last() {
        let t = last.trim();
        if t.starts_with('{') || t.starts_with('[') {
            return Ok(t.to_string());
        }
    }

    // Try finding JSON from start (multi-line)
    if let Some(idx) = lines.iter().position(|l| {
        let t = l.trim();
        t.starts_with('{') || t.starts_with('[')
    }) {
        return Ok(lines[idx..].join("\n").trim().to_string());
    }

    // No JSON found — return error with diagnostic info
    let preview = if output.len() > 200 { &output[..200] } else { output };
    Err(format!(
        "Script produced non-JSON output: {}",
        preview.trim()
    ))
}

// ============================================================================
// INTERNAL: Subprocess Execution
// ============================================================================

fn run_subprocess(
    python: &PathBuf,
    script: &PathBuf,
    args: &[String],
) -> Result<String, String> {
    let mut cmd = Command::new(python);
    cmd.arg(script).args(args)
        .stdout(Stdio::piped())
        .stderr(Stdio::piped());

    #[cfg(target_os = "windows")]
    cmd.creation_flags(CREATE_NO_WINDOW);

    let output = cmd.output().map_err(|e| format!("Failed to execute: {}", e))?;
    let stderr = String::from_utf8_lossy(&output.stderr);

    if output.status.success() {
        let stdout = String::from_utf8_lossy(&output.stdout);
        extract_json(&stdout).map_err(|e| {
            if stderr.is_empty() { e } else { format!("{}\nStderr: {}", e, stderr) }
        })
    } else {
        let stdout = String::from_utf8_lossy(&output.stdout);
        let mut msg = format!("Script failed (exit {})", output.status.code().unwrap_or(-1));
        if !stderr.is_empty() {
            msg.push_str(&format!(": {}", stderr));
        }
        if !stdout.is_empty() {
            msg.push_str(&format!("\nStdout: {}", stdout));
        }
        Err(msg)
    }
}

fn run_subprocess_with_stdin(
    python: &PathBuf,
    script: &PathBuf,
    args: &[String],
    stdin_data: &str,
) -> Result<String, String> {
    let mut cmd = Command::new(python);
    cmd.arg(script).args(args).arg("--stdin")
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .stderr(Stdio::piped());

    #[cfg(target_os = "windows")]
    cmd.creation_flags(CREATE_NO_WINDOW);

    let mut child = cmd.spawn().map_err(|e| format!("Failed to spawn: {}", e))?;

    if let Some(mut stdin) = child.stdin.take() {
        stdin.write_all(stdin_data.as_bytes())
            .map_err(|e| format!("Failed to write stdin: {}", e))?;
        // stdin is dropped here, closing the pipe and signaling EOF
    }

    let output = child.wait_with_output()
        .map_err(|e| format!("Failed to wait: {}", e))?;
    let stderr = String::from_utf8_lossy(&output.stderr);

    if output.status.success() {
        let stdout = String::from_utf8_lossy(&output.stdout);
        extract_json(&stdout).map_err(|e| {
            if stderr.is_empty() { e } else { format!("{}\nStderr: {}", e, stderr) }
        })
    } else {
        Err(format!("Script failed: {}", stderr))
    }
}

// ============================================================================
// PUBLIC API
// ============================================================================

/// Initialize Python runtime (call once at startup)
/// Validates that the Python executable exists and is runnable.
pub async fn initialize(app: &tauri::AppHandle) -> Result<(), String> {
    let install_dir = get_install_dir(app)?;

    // Verify at least one Python venv exists
    let numpy2_python = if cfg!(target_os = "windows") {
        install_dir.join("venv-numpy2").join("Scripts").join("python.exe")
    } else {
        install_dir.join("venv-numpy2").join("bin").join("python3")
    };

    if !numpy2_python.exists() {
        // In debug mode, check system Python
        #[cfg(debug_assertions)]
        {
            let sys = if cfg!(target_os = "windows") { "python" } else { "python3" };
            if Command::new(sys).arg("--version").output().map(|o| o.status.success()).unwrap_or(false) {
                eprintln!("[Python] Initialized with system Python (debug mode)");
                return Ok(());
            }
        }
        return Err(format!("Python venv not found at: {}", numpy2_python.display()));
    }

    eprintln!("[Python] Initialized — venv found at: {}", numpy2_python.display());
    Ok(())
}

/// Execute Python script (async, uses subprocess execution)
pub async fn execute(
    app: &tauri::AppHandle,
    script: &str,
    args: Vec<String>,
) -> Result<String, String> {
    let script_path = resolve_script_path(app, script)?;
    let python = resolve_python_path(app, script)?;
    run_subprocess(&python, &script_path, &args)
}

/// Execute Python script (sync wrapper)
pub fn execute_sync(
    app: &tauri::AppHandle,
    script: &str,
    args: Vec<String>,
) -> Result<String, String> {
    let script_path = resolve_script_path(app, script)?;
    let python = resolve_python_path(app, script)?;
    run_subprocess(&python, &script_path, &args)
}

/// Execute with stdin data (for large payloads, sync only)
pub fn execute_with_stdin(
    app: &tauri::AppHandle,
    script: &str,
    args: Vec<String>,
    stdin_data: &str,
) -> Result<String, String> {
    let python = resolve_python_path(app, script)?;
    let script_path = resolve_script_path(app, script)?;
    run_subprocess_with_stdin(&python, &script_path, &args, stdin_data)
}

/// Get Bun executable path
pub fn get_bun_path(app: &tauri::AppHandle) -> Result<PathBuf, String> {
    let install_dir = get_install_dir(app)?;
    let bun = if cfg!(target_os = "windows") {
        install_dir.join("bun").join("bun.exe")
    } else {
        install_dir.join("bun").join("bin").join("bun")
    };

    #[cfg(debug_assertions)]
    {
        let sys = if cfg!(target_os = "windows") { "bun.exe" } else { "bun" };
        if Command::new(sys).arg("--version").output().map(|o| o.status.success()).unwrap_or(false) {
            return Ok(PathBuf::from(sys));
        }
    }

    if bun.exists() {
        Ok(bun)
    } else {
        Err(format!("Bun not found at: {}", bun.display()))
    }
}

/// Get Python executable path (for jupyter.rs and other direct Python usage)
/// If library_hint is provided, selects the appropriate venv for that library.
pub fn get_python_path(app: &tauri::AppHandle, library_hint: Option<&str>) -> Result<PathBuf, String> {
    let venv_script = library_hint.unwrap_or("numpy2");
    resolve_python_path(app, venv_script)
}

/// Get script path (for direct Command usage in data_sources)
pub fn get_script_path(app: &tauri::AppHandle, script: &str) -> Result<PathBuf, String> {
    resolve_script_path(app, script)
}
