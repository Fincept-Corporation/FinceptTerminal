// BIS Basic Data Query Commands

use super::execute_bis_command;

/// Get statistical data for specific flow and key
#[tauri::command]
pub async fn get_bis_data(app: tauri::AppHandle,
    flow: String,
    key: Option<String>,
    start_period: Option<String>,
    end_period: Option<String>,
    first_n_observations: Option<i32>,
    last_n_observations: Option<i32>,
    detail: Option<String>,
) -> Result<String, String> {
    let mut args = vec![flow];
    if let Some(k) = key { args.push(k); } else { args.push("all".to_string()); }
    if let Some(start) = start_period { args.push(start); }
    if let Some(end) = end_period { args.push(end); }
    if let Some(first_n) = first_n_observations { args.push(first_n.to_string()); }
    if let Some(last_n) = last_n_observations { args.push(last_n.to_string()); }
    if let Some(d) = detail { args.push(d); }
    execute_bis_command(app, "get_data".to_string(), args).await
}

/// Get information about data availability
#[tauri::command]
pub async fn get_bis_available_constraints(app: tauri::AppHandle,
    flow: String,
    key: String,
    component_id: String,
    mode: Option<String>,
    references: Option<String>,
    start_period: Option<String>,
    end_period: Option<String>,
) -> Result<String, String> {
    let mut args = vec![flow, key, component_id];
    if let Some(m) = mode { args.push(m); } else { args.push("exact".to_string()); }
    if let Some(refs) = references { args.push(refs); } else { args.push("none".to_string()); }
    if let Some(start) = start_period { args.push(start); }
    if let Some(end) = end_period { args.push(end); }
    execute_bis_command(app, "get_available_constraints".to_string(), args).await
}
