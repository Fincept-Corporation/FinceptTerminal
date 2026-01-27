use crate::python;
use tauri::command;

// ==================== INSTRUMENTS ENDPOINTS ====================

#[command]
pub async fn oscar_get_instruments(
    app: tauri::AppHandle,
    page: Option<i32>,
    sort: Option<String>,
    order: Option<String>,
    filter_on: Option<String>,
    filter_value: Option<String>
) -> Result<String, String> {
    let page_str = page.unwrap_or(1).to_string();
    let sort_str = sort.unwrap_or_else(|| "id".to_string());
    let order_str = order.unwrap_or_else(|| "asc".to_string());

    let mut args = vec!["instruments".to_string(), page_str, sort_str, order_str];

    if let Some(fo) = filter_on {
        args.push("--filter-on".to_string());
        args.push(fo);
    }

    if let Some(fv) = filter_value {
        args.push("--filter-value".to_string());
        args.push(fv);
    }

    python::execute_sync(&app, "oscar_data.py", args)
}

#[command]
pub async fn oscar_get_instrument_by_slug(app: tauri::AppHandle, slug: String) -> Result<String, String> {
    python::execute_sync(&app, "oscar_data.py", vec!["instrument".to_string(), slug])
}

#[command]
pub async fn oscar_search_instruments_by_type(
    app: tauri::AppHandle,
    instrument_type: String,
    page: Option<i32>
) -> Result<String, String> {
    let mut args = vec!["search_instruments_type".to_string(), instrument_type];

    if let Some(p) = page {
        args.push(p.to_string());
    }

    python::execute_sync(&app, "oscar_data.py", args)
}

#[command]
pub async fn oscar_search_instruments_by_agency(
    app: tauri::AppHandle,
    agency: String,
    page: Option<i32>
) -> Result<String, String> {
    let mut args = vec!["search_instruments_agency".to_string(), agency];

    if let Some(p) = page {
        args.push(p.to_string());
    }

    python::execute_sync(&app, "oscar_data.py", args)
}

#[command]
pub async fn oscar_search_instruments_by_year_range(
    app: tauri::AppHandle,
    start_year: i32,
    end_year: i32,
    page: Option<i32>
) -> Result<String, String> {
    let mut args = vec!["search_instruments_year".to_string(), start_year.to_string(), end_year.to_string()];

    if let Some(p) = page {
        args.push(p.to_string());
    }

    python::execute_sync(&app, "oscar_data.py", args)
}

#[command]
pub async fn oscar_get_instrument_classifications(app: tauri::AppHandle) -> Result<String, String> {
    python::execute_sync(&app, "oscar_data.py", vec!["instrument_classifications".to_string()])
}

// ==================== SATELLITES ENDPOINTS ====================

#[command]
pub async fn oscar_get_satellites(
    app: tauri::AppHandle,
    page: Option<i32>,
    sort: Option<String>,
    order: Option<String>,
    filter_on: Option<String>,
    filter_value: Option<String>
) -> Result<String, String> {
    let page_str = page.unwrap_or(1).to_string();
    let sort_str = sort.unwrap_or_else(|| "id".to_string());
    let order_str = order.unwrap_or_else(|| "asc".to_string());

    let mut args = vec!["satellites".to_string(), page_str, sort_str, order_str];

    if let Some(fo) = filter_on {
        args.push("--filter-on".to_string());
        args.push(fo);
    }

    if let Some(fv) = filter_value {
        args.push("--filter-value".to_string());
        args.push(fv);
    }

    python::execute_sync(&app, "oscar_data.py", args)
}

#[command]
pub async fn oscar_get_satellite_by_slug(app: tauri::AppHandle, slug: String) -> Result<String, String> {
    python::execute_sync(&app, "oscar_data.py", vec!["satellite".to_string(), slug])
}

#[command]
pub async fn oscar_search_satellites_by_orbit(
    app: tauri::AppHandle,
    orbit_type: String,
    page: Option<i32>
) -> Result<String, String> {
    let mut args = vec!["search_satellites_orbit".to_string(), orbit_type];

    if let Some(p) = page {
        args.push(p.to_string());
    }

    python::execute_sync(&app, "oscar_data.py", args)
}

#[command]
pub async fn oscar_search_satellites_by_agency(
    app: tauri::AppHandle,
    agency: String,
    page: Option<i32>
) -> Result<String, String> {
    let mut args = vec!["search_satellites_agency".to_string(), agency];

    if let Some(p) = page {
        args.push(p.to_string());
    }

    python::execute_sync(&app, "oscar_data.py", args)
}

// ==================== VARIABLES ENDPOINTS ====================

#[command]
pub async fn oscar_get_variables(
    app: tauri::AppHandle,
    page: Option<i32>,
    sort: Option<String>,
    order: Option<String>,
    filter_on: Option<String>,
    filter_value: Option<String>
) -> Result<String, String> {
    let page_str = page.unwrap_or(1).to_string();
    let sort_str = sort.unwrap_or_else(|| "id".to_string());
    let order_str = order.unwrap_or_else(|| "asc".to_string());

    let mut args = vec!["variables".to_string(), page_str, sort_str, order_str];

    if let Some(fo) = filter_on {
        args.push("--filter-on".to_string());
        args.push(fo);
    }

    if let Some(fv) = filter_value {
        args.push("--filter-value".to_string());
        args.push(fv);
    }

    python::execute_sync(&app, "oscar_data.py", args)
}

#[command]
pub async fn oscar_get_variable_by_slug(app: tauri::AppHandle, slug: String) -> Result<String, String> {
    python::execute_sync(&app, "oscar_data.py", vec!["variable".to_string(), slug])
}

#[command]
pub async fn oscar_search_variables_by_instrument(
    app: tauri::AppHandle,
    instrument: String,
    page: Option<i32>
) -> Result<String, String> {
    let mut args = vec!["search_variables_instrument".to_string(), instrument];

    if let Some(p) = page {
        args.push(p.to_string());
    }

    python::execute_sync(&app, "oscar_data.py", args)
}

#[command]
pub async fn oscar_get_ecv_variables(app: tauri::AppHandle, page: Option<i32>) -> Result<String, String> {
    let mut args = vec!["ecv_variables".to_string()];

    if let Some(p) = page {
        args.push(p.to_string());
    }

    python::execute_sync(&app, "oscar_data.py", args)
}

// ==================== CONVENIENCE METHODS ====================

#[command]
pub async fn oscar_get_space_agencies(app: tauri::AppHandle) -> Result<String, String> {
    python::execute_sync(&app, "oscar_data.py", vec!["space_agencies".to_string()])
}

#[command]
pub async fn oscar_get_instrument_types(app: tauri::AppHandle) -> Result<String, String> {
    python::execute_sync(&app, "oscar_data.py", vec!["instrument_types".to_string()])
}

#[command]
pub async fn oscar_search_weather_instruments(app: tauri::AppHandle, page: Option<i32>) -> Result<String, String> {
    let mut args = vec!["weather_instruments".to_string()];

    if let Some(p) = page {
        args.push(p.to_string());
    }

    python::execute_sync(&app, "oscar_data.py", args)
}

#[command]
pub async fn oscar_get_climate_monitoring_instruments(app: tauri::AppHandle, page: Option<i32>) -> Result<String, String> {
    let mut args = vec!["climate_instruments".to_string()];

    if let Some(p) = page {
        args.push(p.to_string());
    }

    python::execute_sync(&app, "oscar_data.py", args)
}

#[command]
pub async fn oscar_get_overview_statistics(app: tauri::AppHandle) -> Result<String, String> {
    python::execute_sync(&app, "oscar_data.py", vec!["overview_statistics".to_string()])
}
