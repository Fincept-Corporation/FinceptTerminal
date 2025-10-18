// Yahoo Finance data commands
use std::process::Command;

/// Execute Yahoo Finance Python script command
#[tauri::command]
pub async fn execute_yfinance_command(
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    // Get the Python script path
    let manifest_dir = std::path::Path::new(env!("CARGO_MANIFEST_DIR"));
    let script_path = manifest_dir
        .join("resources")
        .join("scripts")
        .join("yfinance_data.py");

    // Verify script exists
    if !script_path.exists() {
        return Err(format!(
            "YFinance script not found at: {}",
            script_path.display()
        ));
    }

    // Build command arguments
    let mut cmd_args = vec![script_path.to_string_lossy().to_string(), command];
    cmd_args.extend(args);

    // Execute Python script
    let output = Command::new("python")
        .args(&cmd_args)
        .output()
        .map_err(|e| format!("Failed to execute command: {}", e))?;

    if output.status.success() {
        let stdout = String::from_utf8_lossy(&output.stdout);
        Ok(stdout.to_string())
    } else {
        let stderr = String::from_utf8_lossy(&output.stderr);
        Err(format!("Command failed: {}", stderr))
    }
}
