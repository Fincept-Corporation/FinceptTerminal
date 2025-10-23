// Economic calendar events commands
use std::process::Command;
use crate::utils::python::{get_python_path, get_script_path};

/// Execute Economic Calendar Python script command
#[tauri::command]
pub async fn execute_economic_calendar_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    let python_path = get_python_path(&app)?;
    let script_path = get_script_path(&app, "economic_calendar.py")?;

    if !script_path.exists() {
        return Err(format!(
            "Economic Calendar script not found at: {}",
            script_path.display()
        ));
    }

    let mut cmd_args = vec![script_path.to_string_lossy().to_string(), command];
    cmd_args.extend(args);

    let output = Command::new(&python_path)
        .args(&cmd_args)
        .output()
        .map_err(|e| format!("Failed to execute Economic Calendar command: {}", e))?;

    if output.status.success() {
        let stdout = String::from_utf8_lossy(&output.stdout);
        Ok(stdout.to_string())
    } else {
        let stderr = String::from_utf8_lossy(&output.stderr);
        Err(format!("Economic Calendar command failed: {}", stderr))
    }
}

/// Get today's economic events
#[tauri::command]
pub async fn get_economic_calendar_today(
    app: tauri::AppHandle,
) -> Result<String, String> {
    execute_economic_calendar_command(app, "today".to_string(), vec![]).await
}

/// Get upcoming economic events
#[tauri::command]
pub async fn get_economic_calendar_upcoming(
    app: tauri::AppHandle,
    days: Option<i32>,
) -> Result<String, String> {
    let args = if let Some(d) = days {
        vec![d.to_string()]
    } else {
        vec![]
    };
    execute_economic_calendar_command(app, "upcoming".to_string(), args).await
}

/// Get events by country
#[tauri::command]
pub async fn get_economic_calendar_by_country(
    app: tauri::AppHandle,
    country: String,
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let mut args = vec![country];
    if let Some(start) = start_date {
        args.push(start);
    }
    if let Some(end) = end_date {
        args.push(end);
    }
    execute_economic_calendar_command(app, "country".to_string(), args).await
}

/// Get high impact events
#[tauri::command]
pub async fn get_economic_calendar_high_impact(
    app: tauri::AppHandle,
) -> Result<String, String> {
    execute_economic_calendar_command(app, "high_impact".to_string(), vec![]).await
}
