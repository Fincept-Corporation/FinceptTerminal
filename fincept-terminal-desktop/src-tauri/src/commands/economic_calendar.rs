// Economic calendar events commands
use crate::utils::python::execute_python_command;

/// Execute Economic Calendar Python script command
#[tauri::command]
pub async fn execute_economic_calendar_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    // Build command arguments
    let mut cmd_args = vec![command];
    cmd_args.extend(args);

    // Execute Python script with console window hidden on Windows
    execute_python_command(&app, "economic_calendar.py", &cmd_args)
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
