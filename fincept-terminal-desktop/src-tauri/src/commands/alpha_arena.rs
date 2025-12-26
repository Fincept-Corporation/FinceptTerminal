/**
 * Alpha Arena Tauri Commands
 * Rust commands for NOF1-style AI trading competition
 */

use tauri::command;
use serde_json::Value;
use std::process::Command;

/// Execute Python command and return JSON result
/// Uses tokio spawn_blocking to avoid blocking the async runtime
async fn execute_python_alpha_command(args: &[&str]) -> Result<Value, String> {
    let python_script = "agno_trading_service.py";

    // Try multiple possible paths for the script
    let possible_paths = vec![
        std::env::current_dir()
            .ok()
            .and_then(|d| Some(d.join("src-tauri").join("resources").join("scripts").join(python_script))),
        std::env::current_dir()
            .ok()
            .and_then(|d| Some(d.join("resources").join("scripts").join(python_script))),
    ];

    let script_path = possible_paths
        .into_iter()
        .flatten()
        .find(|p| p.exists())
        .ok_or_else(|| format!("Could not find Python script: {}", python_script))?;

    eprintln!("[Alpha Arena] Executing Python script: {:?}", script_path);
    eprintln!("[Alpha Arena] Args: {:?}", args);

    let args_owned: Vec<String> = args.iter().map(|s| s.to_string()).collect();

    // Try PyO3 first, fallback to subprocess
    // Execute in a blocking thread to avoid blocking the async runtime
    let script_path_clone = script_path.clone();
    let args_clone = args_owned.clone();

    let stdout = tokio::task::spawn_blocking(move || {
        // Try PyO3 execution
        match crate::python_runtime::execute_python_script(&script_path_clone, args_clone.clone()) {
            Ok(output) => Ok(output),
            Err(_) => {
                // Fallback to subprocess
                let result = Command::new("python")
                    .env("PYTHONDONTWRITEBYTECODE", "1")
                    .arg(&script_path_clone)
                    .args(&args_clone)
                    .output()
                    .map_err(|e| format!("Failed to execute Python: {}", e))?;

                let stderr = String::from_utf8_lossy(&result.stderr);
                if !stderr.is_empty() {
                    eprintln!("[Alpha Arena] Python stderr: {}", stderr);
                }

                if !result.status.success() {
                    return Err(format!("Python error: {}", stderr));
                }

                Ok(String::from_utf8_lossy(&result.stdout).to_string())
            }
        }
    })
    .await
    .map_err(|e| format!("Task join error: {}", e))??;

    eprintln!("[Alpha Arena] Python stdout length: {} chars", stdout.len());

    // Only log first 1000 chars to avoid flooding logs
    if stdout.len() > 1000 {
        eprintln!("[Alpha Arena] Python stdout (truncated): {}...", &stdout[..1000]);
    } else {
        eprintln!("[Alpha Arena] Python stdout: {}", stdout);
    }

    serde_json::from_str(&stdout)
        .map_err(|e| format!("Failed to parse JSON: {}\nOutput: {}", e, stdout))
}

#[command]
pub async fn create_alpha_competition(
    config_json: String
) -> Result<Value, String> {
    eprintln!("[Alpha Arena] create_alpha_competition called");
    eprintln!("[Alpha Arena] Config JSON length: {}", config_json.len());

    execute_python_alpha_command(&[
        "create_competition",
        &config_json
    ]).await
}

#[command]
pub async fn start_alpha_competition(competition_id: Option<String>) -> Result<Value, String> {
    eprintln!("[Alpha Arena] start_alpha_competition called");
    eprintln!("[Alpha Arena] Competition ID: {:?}", competition_id);

    let id = competition_id.as_deref().unwrap_or("");
    execute_python_alpha_command(&[
        "start_competition",
        id
    ]).await
}

#[command]
pub async fn get_alpha_leaderboard(competition_id: String) -> Result<Value, String> {
    eprintln!("[Alpha Arena] get_alpha_leaderboard called");
    eprintln!("[Alpha Arena] Competition ID: {}", competition_id);

    execute_python_alpha_command(&[
        "get_leaderboard_alpha",
        &competition_id
    ]).await
}

#[command]
pub async fn get_alpha_model_decisions(
    competition_id: String,
    model_name: Option<String>
) -> Result<Value, String> {
    eprintln!("[Alpha Arena] get_alpha_model_decisions called");
    eprintln!("[Alpha Arena] Competition ID: {}", competition_id);
    eprintln!("[Alpha Arena] Model name: {:?}", model_name);

    let model_name_str = model_name.as_deref().unwrap_or("");

    execute_python_alpha_command(&[
        "get_model_decisions_alpha",
        &competition_id,
        model_name_str
    ]).await
}

#[command]
pub async fn stop_alpha_competition(competition_id: String) -> Result<Value, String> {
    eprintln!("[Alpha Arena] stop_alpha_competition called");
    eprintln!("[Alpha Arena] Competition ID: {}", competition_id);

    execute_python_alpha_command(&[
        "stop_competition",
        &competition_id
    ]).await
}

#[command]
pub async fn run_alpha_cycle(competition_id: Option<String>) -> Result<Value, String> {
    eprintln!("[Alpha Arena] run_alpha_cycle called");
    eprintln!("[Alpha Arena] Competition ID: {:?}", competition_id);

    let id = competition_id.as_deref().unwrap_or("");
    execute_python_alpha_command(&[
        "run_cycle",
        id
    ]).await
}
