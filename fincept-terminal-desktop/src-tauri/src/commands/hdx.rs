use crate::utils::python::execute_python_script_simple;
use tauri::command;

#[command]
pub async fn hdx_search_datasets(
    app: tauri::AppHandle,
    query: String,
    limit: Option<i32>,
) -> Result<String, String> {
    let limit_str = limit.map(|l| l.to_string());
    let args: Vec<&str> = if let Some(ref l) = limit_str {
        vec!["search_datasets", &query, l]
    } else {
        vec!["search_datasets", &query]
    };

    execute_python_script_simple(&app, "hdx_data.py", &args)
}

#[command]
pub async fn hdx_get_dataset(
    app: tauri::AppHandle,
    dataset_id: String,
) -> Result<String, String> {
    execute_python_script_simple(&app, "hdx_data.py", &["get_dataset", &dataset_id])
}

#[command]
pub async fn hdx_search_conflict(
    app: tauri::AppHandle,
    country: String,
    limit: Option<i32>,
) -> Result<String, String> {
    let limit_str = limit.map(|l| l.to_string());
    let args: Vec<&str> = if let Some(ref l) = limit_str {
        vec!["search_conflict", &country, l]
    } else {
        vec!["search_conflict", &country]
    };

    execute_python_script_simple(&app, "hdx_data.py", &args)
}

#[command]
pub async fn hdx_search_humanitarian(
    app: tauri::AppHandle,
    country: String,
    limit: Option<i32>,
) -> Result<String, String> {
    let limit_str = limit.map(|l| l.to_string());
    let args: Vec<&str> = if let Some(ref l) = limit_str {
        vec!["search_humanitarian", &country, l]
    } else {
        vec!["search_humanitarian", &country]
    };

    execute_python_script_simple(&app, "hdx_data.py", &args)
}

#[command]
pub async fn hdx_search_by_country(
    app: tauri::AppHandle,
    country_code: String,
    limit: Option<i32>,
) -> Result<String, String> {
    let limit_str = limit.map(|l| l.to_string());
    let args: Vec<&str> = if let Some(ref l) = limit_str {
        vec!["search_by_country", &country_code, l]
    } else {
        vec!["search_by_country", &country_code]
    };

    execute_python_script_simple(&app, "hdx_data.py", &args)
}

#[command]
pub async fn hdx_search_by_organization(
    app: tauri::AppHandle,
    org_slug: String,
    limit: Option<i32>,
) -> Result<String, String> {
    let limit_str = limit.map(|l| l.to_string());
    let args: Vec<&str> = if let Some(ref l) = limit_str {
        vec!["search_by_organization", &org_slug, l]
    } else {
        vec!["search_by_organization", &org_slug]
    };

    execute_python_script_simple(&app, "hdx_data.py", &args)
}

#[command]
pub async fn hdx_search_by_topic(
    app: tauri::AppHandle,
    topic: String,
    limit: Option<i32>,
) -> Result<String, String> {
    let limit_str = limit.map(|l| l.to_string());
    let args: Vec<&str> = if let Some(ref l) = limit_str {
        vec!["search_by_topic", &topic, l]
    } else {
        vec!["search_by_topic", &topic]
    };

    execute_python_script_simple(&app, "hdx_data.py", &args)
}

#[command]
pub async fn hdx_search_by_dataseries(
    app: tauri::AppHandle,
    series_name: String,
    limit: Option<i32>,
) -> Result<String, String> {
    let limit_str = limit.map(|l| l.to_string());
    let args: Vec<&str> = if let Some(ref l) = limit_str {
        vec!["search_by_dataseries", &series_name, l]
    } else {
        vec!["search_by_dataseries", &series_name]
    };

    execute_python_script_simple(&app, "hdx_data.py", &args)
}

#[command]
pub async fn hdx_list_countries(
    app: tauri::AppHandle,
    limit: Option<i32>,
) -> Result<String, String> {
    let limit_str = limit.map(|l| l.to_string());
    let args: Vec<&str> = if let Some(ref l) = limit_str {
        vec!["list_countries", l]
    } else {
        vec!["list_countries"]
    };

    execute_python_script_simple(&app, "hdx_data.py", &args)
}

#[command]
pub async fn hdx_list_organizations(
    app: tauri::AppHandle,
    limit: Option<i32>,
) -> Result<String, String> {
    let limit_str = limit.map(|l| l.to_string());
    let args: Vec<&str> = if let Some(ref l) = limit_str {
        vec!["list_organizations", l]
    } else {
        vec!["list_organizations"]
    };

    execute_python_script_simple(&app, "hdx_data.py", &args)
}

#[command]
pub async fn hdx_list_topics(
    app: tauri::AppHandle,
    limit: Option<i32>,
) -> Result<String, String> {
    let limit_str = limit.map(|l| l.to_string());
    let args: Vec<&str> = if let Some(ref l) = limit_str {
        vec!["list_topics", l]
    } else {
        vec!["list_topics"]
    };

    execute_python_script_simple(&app, "hdx_data.py", &args)
}

#[command]
pub async fn hdx_get_resource_url(
    app: tauri::AppHandle,
    dataset_id: String,
    resource_index: Option<i32>,
) -> Result<String, String> {
    let index_str = resource_index.map(|i| i.to_string());
    let args: Vec<&str> = if let Some(ref idx) = index_str {
        vec!["get_resource_download_url", &dataset_id, idx]
    } else {
        vec!["get_resource_download_url", &dataset_id]
    };

    execute_python_script_simple(&app, "hdx_data.py", &args)
}

#[command]
pub async fn hdx_advanced_search(
    app: tauri::AppHandle,
    filters: String,
) -> Result<String, String> {
    // Build args vector with owned strings
    let mut args_owned = vec!["advanced_search".to_string()];

    // Parse filters string and add as arguments
    // Expected format: "country:ukr organization:ocha limit:20"
    for filter in filters.split_whitespace() {
        args_owned.push(filter.to_string());
    }

    // Convert to borrowed references
    let args: Vec<&str> = args_owned.iter().map(|s| s.as_str()).collect();

    execute_python_script_simple(&app, "hdx_data.py", &args)
}

#[command]
pub async fn hdx_test_connection(app: tauri::AppHandle) -> Result<String, String> {
    execute_python_script_simple(&app, "hdx_data.py", &["test_connection"])
}
