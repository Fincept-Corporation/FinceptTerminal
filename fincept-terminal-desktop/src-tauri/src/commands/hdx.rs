use crate::python;
use tauri::command;

#[command]
pub async fn hdx_search_datasets(
    app: tauri::AppHandle,
    query: String,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = vec!["search_datasets".to_string(), query];
    if let Some(l) = limit {
        args.push(l.to_string());
    }

    python::execute_sync(&app, "hdx_data.py", args)
}

#[command]
pub async fn hdx_get_dataset(
    app: tauri::AppHandle,
    dataset_id: String,
) -> Result<String, String> {
    python::execute_sync(&app, "hdx_data.py", vec!["get_dataset".to_string(), dataset_id])
}

#[command]
pub async fn hdx_search_conflict(
    app: tauri::AppHandle,
    country: String,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = vec!["search_conflict".to_string(), country];
    if let Some(l) = limit {
        args.push(l.to_string());
    }

    python::execute_sync(&app, "hdx_data.py", args)
}

#[command]
pub async fn hdx_search_humanitarian(
    app: tauri::AppHandle,
    country: String,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = vec!["search_humanitarian".to_string(), country];
    if let Some(l) = limit {
        args.push(l.to_string());
    }

    python::execute_sync(&app, "hdx_data.py", args)
}

#[command]
pub async fn hdx_search_by_country(
    app: tauri::AppHandle,
    country_code: String,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = vec!["search_by_country".to_string(), country_code];
    if let Some(l) = limit {
        args.push(l.to_string());
    }

    python::execute_sync(&app, "hdx_data.py", args)
}

#[command]
pub async fn hdx_search_by_organization(
    app: tauri::AppHandle,
    org_slug: String,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = vec!["search_by_organization".to_string(), org_slug];
    if let Some(l) = limit {
        args.push(l.to_string());
    }

    python::execute_sync(&app, "hdx_data.py", args)
}

#[command]
pub async fn hdx_search_by_topic(
    app: tauri::AppHandle,
    topic: String,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = vec!["search_by_topic".to_string(), topic];
    if let Some(l) = limit {
        args.push(l.to_string());
    }

    python::execute_sync(&app, "hdx_data.py", args)
}

#[command]
pub async fn hdx_search_by_dataseries(
    app: tauri::AppHandle,
    series_name: String,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = vec!["search_by_dataseries".to_string(), series_name];
    if let Some(l) = limit {
        args.push(l.to_string());
    }

    python::execute_sync(&app, "hdx_data.py", args)
}

#[command]
pub async fn hdx_list_countries(
    app: tauri::AppHandle,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = vec!["list_countries".to_string()];
    if let Some(l) = limit {
        args.push(l.to_string());
    }

    python::execute_sync(&app, "hdx_data.py", args)
}

#[command]
pub async fn hdx_list_organizations(
    app: tauri::AppHandle,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = vec!["list_organizations".to_string()];
    if let Some(l) = limit {
        args.push(l.to_string());
    }

    python::execute_sync(&app, "hdx_data.py", args)
}

#[command]
pub async fn hdx_list_topics(
    app: tauri::AppHandle,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = vec!["list_topics".to_string()];
    if let Some(l) = limit {
        args.push(l.to_string());
    }

    python::execute_sync(&app, "hdx_data.py", args)
}

#[command]
pub async fn hdx_get_resource_url(
    app: tauri::AppHandle,
    dataset_id: String,
    resource_index: Option<i32>,
) -> Result<String, String> {
    let mut args = vec!["get_resource_download_url".to_string(), dataset_id];
    if let Some(idx) = resource_index {
        args.push(idx.to_string());
    }

    python::execute_sync(&app, "hdx_data.py", args)
}

#[command]
pub async fn hdx_advanced_search(
    app: tauri::AppHandle,
    filters: String,
) -> Result<String, String> {
    let mut args = vec!["advanced_search".to_string()];

    // Parse filters string and add as arguments
    // Expected format: "country:ukr organization:ocha limit:20"
    for filter in filters.split_whitespace() {
        args.push(filter.to_string());
    }

    python::execute_sync(&app, "hdx_data.py", args)
}

#[command]
pub async fn hdx_test_connection(app: tauri::AppHandle) -> Result<String, String> {
    python::execute_sync(&app, "hdx_data.py", vec!["test_connection".to_string()])
}
