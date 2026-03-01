// BIS Structure Query Commands

use super::execute_bis_command;

fn build_structure_args(
    agency_id: Option<String>,
    resource_id: Option<String>,
    version: Option<String>,
    references: Option<String>,
    detail: Option<String>,
) -> Vec<String> {
    let mut args = Vec::new();
    args.push(agency_id.unwrap_or_else(|| "all".to_string()));
    args.push(resource_id.unwrap_or_else(|| "all".to_string()));
    args.push(version.unwrap_or_else(|| "all".to_string()));
    args.push(references.unwrap_or_else(|| "none".to_string()));
    args.push(detail.unwrap_or_else(|| "full".to_string()));
    args
}

/// Get data structure definitions
#[tauri::command]
pub async fn get_bis_data_structures(app: tauri::AppHandle,
    agency_id: Option<String>, resource_id: Option<String>, version: Option<String>,
    references: Option<String>, detail: Option<String>,
) -> Result<String, String> {
    execute_bis_command(app, "get_data_structures".to_string(),
        build_structure_args(agency_id, resource_id, version, references, detail)).await
}

/// Get dataflow definitions (available datasets)
#[tauri::command]
pub async fn get_bis_dataflows(app: tauri::AppHandle,
    agency_id: Option<String>, resource_id: Option<String>, version: Option<String>,
    references: Option<String>, detail: Option<String>,
) -> Result<String, String> {
    execute_bis_command(app, "get_dataflows".to_string(),
        build_structure_args(agency_id, resource_id, version, references, detail)).await
}

/// Get categorisation definitions
#[tauri::command]
pub async fn get_bis_categorisations(app: tauri::AppHandle,
    agency_id: Option<String>, resource_id: Option<String>, version: Option<String>,
    references: Option<String>, detail: Option<String>,
) -> Result<String, String> {
    execute_bis_command(app, "get_categorisations".to_string(),
        build_structure_args(agency_id, resource_id, version, references, detail)).await
}

/// Get content constraint definitions
#[tauri::command]
pub async fn get_bis_content_constraints(app: tauri::AppHandle,
    agency_id: Option<String>, resource_id: Option<String>, version: Option<String>,
    references: Option<String>, detail: Option<String>,
) -> Result<String, String> {
    execute_bis_command(app, "get_content_constraints".to_string(),
        build_structure_args(agency_id, resource_id, version, references, detail)).await
}

/// Get actual constraint definitions
#[tauri::command]
pub async fn get_bis_actual_constraints(app: tauri::AppHandle,
    agency_id: Option<String>, resource_id: Option<String>, version: Option<String>,
    references: Option<String>, detail: Option<String>,
) -> Result<String, String> {
    execute_bis_command(app, "get_actual_constraints".to_string(),
        build_structure_args(agency_id, resource_id, version, references, detail)).await
}

/// Get allowed constraint definitions
#[tauri::command]
pub async fn get_bis_allowed_constraints(app: tauri::AppHandle,
    agency_id: Option<String>, resource_id: Option<String>, version: Option<String>,
    references: Option<String>, detail: Option<String>,
) -> Result<String, String> {
    execute_bis_command(app, "get_allowed_constraints".to_string(),
        build_structure_args(agency_id, resource_id, version, references, detail)).await
}

/// Get general structure definitions
#[tauri::command]
pub async fn get_bis_structures(app: tauri::AppHandle,
    agency_id: Option<String>, resource_id: Option<String>, version: Option<String>,
    references: Option<String>, detail: Option<String>,
) -> Result<String, String> {
    execute_bis_command(app, "get_structures".to_string(),
        build_structure_args(agency_id, resource_id, version, references, detail)).await
}

/// Get concept scheme definitions
#[tauri::command]
pub async fn get_bis_concept_schemes(app: tauri::AppHandle,
    agency_id: Option<String>, resource_id: Option<String>, version: Option<String>,
    references: Option<String>, detail: Option<String>,
) -> Result<String, String> {
    execute_bis_command(app, "get_concept_schemes".to_string(),
        build_structure_args(agency_id, resource_id, version, references, detail)).await
}

/// Get codelist definitions
#[tauri::command]
pub async fn get_bis_codelists(app: tauri::AppHandle,
    agency_id: Option<String>, resource_id: Option<String>, version: Option<String>,
    references: Option<String>, detail: Option<String>,
) -> Result<String, String> {
    execute_bis_command(app, "get_codelists".to_string(),
        build_structure_args(agency_id, resource_id, version, references, detail)).await
}

/// Get category scheme definitions
#[tauri::command]
pub async fn get_bis_category_schemes(app: tauri::AppHandle,
    agency_id: Option<String>, resource_id: Option<String>, version: Option<String>,
    references: Option<String>, detail: Option<String>,
) -> Result<String, String> {
    execute_bis_command(app, "get_category_schemes".to_string(),
        build_structure_args(agency_id, resource_id, version, references, detail)).await
}

/// Get hierarchical codelist definitions
#[tauri::command]
pub async fn get_bis_hierarchical_codelists(app: tauri::AppHandle,
    agency_id: Option<String>, resource_id: Option<String>, version: Option<String>,
    references: Option<String>, detail: Option<String>,
) -> Result<String, String> {
    execute_bis_command(app, "get_hierarchical_codelists".to_string(),
        build_structure_args(agency_id, resource_id, version, references, detail)).await
}

/// Get agency scheme definitions
#[tauri::command]
pub async fn get_bis_agency_schemes(app: tauri::AppHandle,
    agency_id: Option<String>, resource_id: Option<String>, version: Option<String>,
    references: Option<String>, detail: Option<String>,
) -> Result<String, String> {
    execute_bis_command(app, "get_agency_schemes".to_string(),
        build_structure_args(agency_id, resource_id, version, references, detail)).await
}
