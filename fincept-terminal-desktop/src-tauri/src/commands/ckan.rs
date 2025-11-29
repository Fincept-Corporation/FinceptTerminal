// Universal CKAN Data Portals API commands
#![allow(dead_code)]
use crate::utils::python::execute_python_command;

/// Execute Universal CKAN Python script command
#[tauri::command]
pub async fn execute_ckan_command(
    app: tauri::AppHandle,
    country: String,
    action: String,
    args: Vec<String>,
) -> Result<String, String> {
    let mut cmd_args = vec!["--country".to_string(), country, "--action".to_string(), action];
    cmd_args.extend(args);
    execute_python_command(&app, "universal_ckan_api.py", &cmd_args)
}

/// Get list of supported countries and their portal information
#[tauri::command]
pub async fn get_ckan_supported_countries(
    app: tauri::AppHandle,
) -> Result<String, String> {
    execute_ckan_command(app, "us".to_string(), "supported_countries".to_string(), vec![]).await
}

/// Test connection to a specific CKAN portal
#[tauri::command]
pub async fn test_ckan_portal_connection(
    app: tauri::AppHandle,
    country: String,
) -> Result<String, String> {
    execute_ckan_command(app, country, "test_connection".to_string(), vec![]).await
}

/// List all organizations (catalogues) in the specified portal
#[tauri::command]
pub async fn list_ckan_organizations(
    app: tauri::AppHandle,
    country: String,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(l) = limit {
        args.push("--limit".to_string());
        args.push(l.to_string());
    }
    execute_ckan_command(app, country, "list_organizations".to_string(), args).await
}

/// Get detailed information about a specific organization
#[tauri::command]
pub async fn get_ckan_organization_details(
    app: tauri::AppHandle,
    country: String,
    organization_id: String,
) -> Result<String, String> {
    let args = vec!["--org".to_string(), organization_id];
    execute_ckan_command(app, country, "get_organization_details".to_string(), args).await
}

/// List all datasets in the specified portal
#[tauri::command]
pub async fn list_ckan_datasets(
    app: tauri::AppHandle,
    country: String,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(l) = limit {
        args.push("--limit".to_string());
        args.push(l.to_string());
    }
    execute_ckan_command(app, country, "list_datasets".to_string(), args).await
}

/// Search for datasets in the specified portal
#[tauri::command]
pub async fn search_ckan_datasets(
    app: tauri::AppHandle,
    country: String,
    query: Option<String>,
    limit: Option<i32>,
    organization: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();

    if let Some(q) = query {
        args.push("--query".to_string());
        args.push(q);
    }

    if let Some(l) = limit {
        args.push("--limit".to_string());
        args.push(l.to_string());
    }

    if let Some(org) = organization {
        args.push("--org".to_string());
        args.push(org);
    }

    execute_ckan_command(app, country, "search_datasets".to_string(), args).await
}

/// Get detailed information about a specific dataset
#[tauri::command]
pub async fn get_ckan_dataset_details(
    app: tauri::AppHandle,
    country: String,
    dataset_id: String,
) -> Result<String, String> {
    let args = vec!["--id".to_string(), dataset_id];
    execute_ckan_command(app, country, "get_dataset_details".to_string(), args).await
}

/// Get all datasets belonging to a specific organization
#[tauri::command]
pub async fn get_ckan_datasets_by_organization(
    app: tauri::AppHandle,
    country: String,
    organization_id: String,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = vec!["--org".to_string(), organization_id];

    if let Some(l) = limit {
        args.push("--limit".to_string());
        args.push(l.to_string());
    }

    execute_ckan_command(app, country, "get_datasets_by_organization".to_string(), args).await
}

/// Get all resources (data files) for a specific dataset
#[tauri::command]
pub async fn get_ckan_dataset_resources(
    app: tauri::AppHandle,
    country: String,
    dataset_id: String,
) -> Result<String, String> {
    let args = vec!["--id".to_string(), dataset_id];
    execute_ckan_command(app, country, "get_dataset_resources".to_string(), args).await
}

/// Get detailed information about a specific resource
#[tauri::command]
pub async fn get_ckan_resource_details(
    app: tauri::AppHandle,
    country: String,
    resource_id: String,
) -> Result<String, String> {
    let args = vec!["--id".to_string(), resource_id];
    execute_ckan_command(app, country, "get_resource_details".to_string(), args).await
}
