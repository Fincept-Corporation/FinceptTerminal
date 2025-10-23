// Congress.gov data commands based on OpenBB congress_gov provider
use std::process::Command;
use crate::utils::python::{get_python_path, get_script_path};

/// Execute Congress.gov Python script command
#[tauri::command]
pub async fn execute_congress_gov_command(
    app: tauri::AppHandle, 
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    // Get the Python script path
    let python_path = get_python_path(&app)?;
    let script_path = get_script_path(&app, "congress_gov_data.py")?;

    // Verify script exists
    if !script_path.exists() {
        return Err(format!(
            "Congress.gov script not found at: {}",
            script_path.display()
        ));
    }

    // Build command arguments
    let mut cmd_args = vec![script_path.to_string_lossy().to_string(), command];
    cmd_args.extend(args);

    // Execute Python script
    let output = Command::new(&python_path)
        .args(&cmd_args)
        .output()
        .map_err(|e| format!("Failed to execute Congress.gov command: {}", e))?;

    if output.status.success() {
        let stdout = String::from_utf8_lossy(&output.stdout);
        Ok(stdout.to_string())
    } else {
        let stderr = String::from_utf8_lossy(&output.stderr);
        Err(format!("Congress.gov command failed: {}", stderr))
    }
}

/// Get Congressional bills with various filters
#[tauri::command]
pub async fn get_congress_bills(app: tauri::AppHandle, 
    congress: Option<i32>,
    bill_type: Option<String>,
    start_date: Option<String>,
    end_date: Option<String>,
    limit: Option<i32>,
    offset: Option<i32>,
    sort_by: Option<String>,
    get_all: Option<bool>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(congress) = congress {
        args.push(congress.to_string());
    }
    if let Some(bill_type) = bill_type {
        args.push(bill_type);
    }
    if let Some(start_date) = start_date {
        args.push(start_date);
    }
    if let Some(end_date) = end_date {
        args.push(end_date);
    }
    if let Some(limit) = limit {
        args.push(limit.to_string());
    }
    if let Some(offset) = offset {
        args.push(offset.to_string());
    }
    if let Some(sort_by) = sort_by {
        args.push(sort_by);
    }
    if let Some(get_all) = get_all {
        args.push(get_all.to_string());
    }
    execute_congress_gov_command(app, "congress_bills".to_string(), args).await
}

/// Get detailed information about a specific bill
#[tauri::command]
pub async fn get_bill_info(app: tauri::AppHandle, bill_url: String) -> Result<String, String> {
    execute_congress_gov_command(app, "bill_info".to_string(), vec![bill_url]).await
}

/// Get available text versions for a bill
#[tauri::command]
pub async fn get_bill_text(app: tauri::AppHandle, bill_url: String) -> Result<String, String> {
    execute_congress_gov_command(app, "bill_text".to_string(), vec![bill_url]).await
}

/// Download bill text document
#[tauri::command]
pub async fn download_bill_text(app: tauri::AppHandle, text_url: String) -> Result<String, String> {
    execute_congress_gov_command(app, "download_text".to_string(), vec![text_url]).await
}

/// Get comprehensive data for a bill from multiple endpoints
#[tauri::command]
pub async fn get_comprehensive_bill_data(app: tauri::AppHandle, bill_url: String) -> Result<String, String> {
    execute_congress_gov_command(app, "comprehensive".to_string(), vec![bill_url]).await
}

/// Get a summary of bills by type for a given congress
#[tauri::command]
pub async fn get_bill_summary_by_congress(app: tauri::AppHandle, 
    congress: Option<i32>,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(congress) = congress {
        args.push(congress.to_string());
    }
    if let Some(limit) = limit {
        args.push(limit.to_string());
    }
    execute_congress_gov_command(app, "summary".to_string(), args).await
}