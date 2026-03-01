// BIS Item Query Commands

use super::execute_bis_command;

fn build_item_args(
    agency_id: String,
    resource_id: String,
    version: String,
    item_id: Option<String>,
    references: Option<String>,
    detail: Option<String>,
) -> Vec<String> {
    let mut args = vec![agency_id, resource_id, version];
    args.push(item_id.unwrap_or_else(|| "all".to_string()));
    args.push(references.unwrap_or_else(|| "none".to_string()));
    args.push(detail.unwrap_or_else(|| "full".to_string()));
    args
}

/// Get specific concepts from concept schemes
#[tauri::command]
pub async fn get_bis_concepts(app: tauri::AppHandle,
    agency_id: String, resource_id: String, version: String,
    item_id: Option<String>, references: Option<String>, detail: Option<String>,
) -> Result<String, String> {
    execute_bis_command(app, "get_concepts".to_string(),
        build_item_args(agency_id, resource_id, version, item_id, references, detail)).await
}

/// Get specific codes from codelists
#[tauri::command]
pub async fn get_bis_codes(app: tauri::AppHandle,
    agency_id: String, resource_id: String, version: String,
    item_id: Option<String>, references: Option<String>, detail: Option<String>,
) -> Result<String, String> {
    execute_bis_command(app, "get_codes".to_string(),
        build_item_args(agency_id, resource_id, version, item_id, references, detail)).await
}

/// Get specific categories from category schemes
#[tauri::command]
pub async fn get_bis_categories(app: tauri::AppHandle,
    agency_id: String, resource_id: String, version: String,
    item_id: Option<String>, references: Option<String>, detail: Option<String>,
) -> Result<String, String> {
    execute_bis_command(app, "get_categories".to_string(),
        build_item_args(agency_id, resource_id, version, item_id, references, detail)).await
}

/// Get specific hierarchies from hierarchical codelists
#[tauri::command]
pub async fn get_bis_hierarchies(app: tauri::AppHandle,
    agency_id: String, resource_id: String, version: String,
    item_id: Option<String>, references: Option<String>, detail: Option<String>,
) -> Result<String, String> {
    execute_bis_command(app, "get_hierarchies".to_string(),
        build_item_args(agency_id, resource_id, version, item_id, references, detail)).await
}
