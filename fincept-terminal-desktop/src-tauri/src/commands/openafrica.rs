// OpenAFRICA data portal commands
use std::process::Command;
use crate::utils::python::{get_python_path, get_script_path};

/// Execute OpenAFRICA Python script command
#[tauri::command]
pub async fn execute_openafrica_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    let python_path = get_python_path(&app)?;
    let script_path = get_script_path(&app, "openafrica_api.py")?;

    if !script_path.exists() {
        return Err(format!(
            "OpenAFRICA script not found at: {}",
            script_path.display()
        ));
    }

    let mut cmd_args = vec![script_path.to_string_lossy().to_string(), command];
    cmd_args.extend(args);

    let output = Command::new(&python_path)
        .args(&cmd_args)
        .output()
        .map_err(|e| format!("Failed to execute OpenAFRICA command: {}", e))?;

    if output.status.success() {
        let stdout = String::from_utf8_lossy(&output.stdout);
        Ok(stdout.to_string())
    } else {
        let stderr = String::from_utf8_lossy(&output.stderr);
        Err(format!("OpenAFRICA command failed: {}", stderr))
    }
}

/// Search African datasets
#[tauri::command]
pub async fn search_openafrica_datasets(
    app: tauri::AppHandle,
    query: String,
) -> Result<String, String> {
    execute_openafrica_command(app, "search".to_string(), vec![query]).await
}

/// Get dataset by ID
#[tauri::command]
pub async fn get_openafrica_dataset(
    app: tauri::AppHandle,
    dataset_id: String,
) -> Result<String, String> {
    execute_openafrica_command(app, "dataset".to_string(), vec![dataset_id]).await
}

/// Get datasets by country
#[tauri::command]
pub async fn get_openafrica_country_datasets(
    app: tauri::AppHandle,
    country: String,
) -> Result<String, String> {
    execute_openafrica_command(app, "country".to_string(), vec![country]).await
}
