use crate::utils::python::execute_python_script_simple;
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

    let mut args = vec!["instruments", &page_str, &sort_str, &order_str];

    let filter_on_str;
    let filter_value_str;
    if let Some(fo) = filter_on {
        filter_on_str = fo;
        args.push("--filter-on");
        args.push(&filter_on_str);
    }

    if let Some(fv) = filter_value {
        filter_value_str = fv;
        args.push("--filter-value");
        args.push(&filter_value_str);
    }

    execute_python_script_simple(&app, "oscar_data.py", &args)
}

#[command]
pub async fn oscar_get_instrument_by_slug(app: tauri::AppHandle, slug: String) -> Result<String, String> {
    execute_python_script_simple(&app, "oscar_data.py", &["instrument", &slug])
}

#[command]
pub async fn oscar_search_instruments_by_type(
    app: tauri::AppHandle,
    instrument_type: String,
    page: Option<i32>
) -> Result<String, String> {
    let mut args = vec!["search_instruments_type", &instrument_type];

    let page_str;
    if let Some(p) = page {
        page_str = p.to_string();
        args.push(&page_str);
    }

    execute_python_script_simple(&app, "oscar_data.py", &args)
}

#[command]
pub async fn oscar_search_instruments_by_agency(
    app: tauri::AppHandle,
    agency: String,
    page: Option<i32>
) -> Result<String, String> {
    let mut args = vec!["search_instruments_agency", &agency];

    let page_str;
    if let Some(p) = page {
        page_str = p.to_string();
        args.push(&page_str);
    }

    execute_python_script_simple(&app, "oscar_data.py", &args)
}

#[command]
pub async fn oscar_search_instruments_by_year_range(
    app: tauri::AppHandle,
    start_year: i32,
    end_year: i32,
    page: Option<i32>
) -> Result<String, String> {
    let start_year_str = start_year.to_string();
    let end_year_str = end_year.to_string();
    let mut args = vec!["search_instruments_year", &start_year_str, &end_year_str];

    let page_str;
    if let Some(p) = page {
        page_str = p.to_string();
        args.push(&page_str);
    }

    execute_python_script_simple(&app, "oscar_data.py", &args)
}

#[command]
pub async fn oscar_get_instrument_classifications(app: tauri::AppHandle) -> Result<String, String> {
    execute_python_script_simple(&app, "oscar_data.py", &["instrument_classifications"])
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

    let mut args = vec!["satellites", &page_str, &sort_str, &order_str];

    let filter_on_str;
    let filter_value_str;
    if let Some(fo) = filter_on {
        filter_on_str = fo;
        args.push("--filter-on");
        args.push(&filter_on_str);
    }

    if let Some(fv) = filter_value {
        filter_value_str = fv;
        args.push("--filter-value");
        args.push(&filter_value_str);
    }

    execute_python_script_simple(&app, "oscar_data.py", &args)
}

#[command]
pub async fn oscar_get_satellite_by_slug(app: tauri::AppHandle, slug: String) -> Result<String, String> {
    execute_python_script_simple(&app, "oscar_data.py", &["satellite", &slug])
}

#[command]
pub async fn oscar_search_satellites_by_orbit(
    app: tauri::AppHandle,
    orbit_type: String,
    page: Option<i32>
) -> Result<String, String> {
    let mut args = vec!["search_satellites_orbit", &orbit_type];

    let page_str;
    if let Some(p) = page {
        page_str = p.to_string();
        args.push(&page_str);
    }

    execute_python_script_simple(&app, "oscar_data.py", &args)
}

#[command]
pub async fn oscar_search_satellites_by_agency(
    app: tauri::AppHandle,
    agency: String,
    page: Option<i32>
) -> Result<String, String> {
    let mut args = vec!["search_satellites_agency", &agency];

    let page_str;
    if let Some(p) = page {
        page_str = p.to_string();
        args.push(&page_str);
    }

    execute_python_script_simple(&app, "oscar_data.py", &args)
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

    let mut args = vec!["variables", &page_str, &sort_str, &order_str];

    let filter_on_str;
    let filter_value_str;
    if let Some(fo) = filter_on {
        filter_on_str = fo;
        args.push("--filter-on");
        args.push(&filter_on_str);
    }

    if let Some(fv) = filter_value {
        filter_value_str = fv;
        args.push("--filter-value");
        args.push(&filter_value_str);
    }

    execute_python_script_simple(&app, "oscar_data.py", &args)
}

#[command]
pub async fn oscar_get_variable_by_slug(app: tauri::AppHandle, slug: String) -> Result<String, String> {
    execute_python_script_simple(&app, "oscar_data.py", &["variable", &slug])
}

#[command]
pub async fn oscar_search_variables_by_instrument(
    app: tauri::AppHandle,
    instrument: String,
    page: Option<i32>
) -> Result<String, String> {
    let mut args = vec!["search_variables_instrument", &instrument];

    let page_str;
    if let Some(p) = page {
        page_str = p.to_string();
        args.push(&page_str);
    }

    execute_python_script_simple(&app, "oscar_data.py", &args)
}

#[command]
pub async fn oscar_get_ecv_variables(app: tauri::AppHandle, page: Option<i32>) -> Result<String, String> {
    let mut args = vec!["ecv_variables"];

    let page_str;
    if let Some(p) = page {
        page_str = p.to_string();
        args.push(&page_str);
    }

    execute_python_script_simple(&app, "oscar_data.py", &args)
}

// ==================== CONVENIENCE METHODS ====================

#[command]
pub async fn oscar_get_space_agencies(app: tauri::AppHandle) -> Result<String, String> {
    execute_python_script_simple(&app, "oscar_data.py", &["space_agencies"])
}

#[command]
pub async fn oscar_get_instrument_types(app: tauri::AppHandle) -> Result<String, String> {
    execute_python_script_simple(&app, "oscar_data.py", &["instrument_types"])
}

#[command]
pub async fn oscar_search_weather_instruments(app: tauri::AppHandle, page: Option<i32>) -> Result<String, String> {
    let mut args = vec!["weather_instruments"];

    let page_str;
    if let Some(p) = page {
        page_str = p.to_string();
        args.push(&page_str);
    }

    execute_python_script_simple(&app, "oscar_data.py", &args)
}

#[command]
pub async fn oscar_get_climate_monitoring_instruments(app: tauri::AppHandle, page: Option<i32>) -> Result<String, String> {
    let mut args = vec!["climate_instruments"];

    let page_str;
    if let Some(p) = page {
        page_str = p.to_string();
        args.push(&page_str);
    }

    execute_python_script_simple(&app, "oscar_data.py", &args)
}

#[command]
pub async fn oscar_get_overview_statistics(app: tauri::AppHandle) -> Result<String, String> {
    execute_python_script_simple(&app, "oscar_data.py", &["overview_statistics"])
}
