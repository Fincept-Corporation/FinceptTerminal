use crate::python;
use tauri::command;

// ==================== DATA ENDPOINTS ====================

#[command]
pub async fn unesco_get_indicator_data(
    app: tauri::AppHandle,
    indicators: Option<Vec<String>>,
    geo_units: Option<Vec<String>>,
    geo_unit_type: Option<String>,
    start_year: Option<i32>,
    end_year: Option<i32>,
    footnotes: Option<bool>,
    indicator_metadata: Option<bool>,
    version: Option<String>
) -> Result<String, String> {
    let mut args = vec!["indicator_data".to_string()];

    if let Some(indicators) = indicators {
        for indicator in indicators {
            args.push(indicator);
        }
    }

    if let Some(geo_units) = geo_units {
        for geo_unit in geo_units {
            args.push(geo_unit);
        }
    }

    if let Some(gut) = geo_unit_type {
        args.push("--geo-unit-type".to_string());
        args.push(gut);
    }

    if let Some(sy) = start_year {
        args.push("--start-year".to_string());
        args.push(sy.to_string());
    }

    if let Some(ey) = end_year {
        args.push("--end-year".to_string());
        args.push(ey.to_string());
    }

    if let Some(footnotes) = footnotes {
        if footnotes {
            args.push("--footnotes".to_string());
        }
    }

    if let Some(indicator_metadata) = indicator_metadata {
        if indicator_metadata {
            args.push("--metadata".to_string());
        }
    }

    if let Some(v) = version {
        args.push("--version".to_string());
        args.push(v);
    }

    python::execute_sync(&app, "unesco_data.py", args)
}

#[command]
pub async fn unesco_export_indicator_data(
    app: tauri::AppHandle,
    format_type: String,
    indicators: Option<Vec<String>>,
    geo_units: Option<Vec<String>>,
    geo_unit_type: Option<String>,
    start_year: Option<i32>,
    end_year: Option<i32>,
    footnotes: Option<bool>,
    indicator_metadata: Option<bool>,
    version: Option<String>
) -> Result<String, String> {
    let mut args = vec!["export_indicator_data".to_string(), format_type];

    if let Some(indicators) = indicators {
        for indicator in indicators {
            args.push(indicator);
        }
    }

    if let Some(geo_units) = geo_units {
        for geo_unit in geo_units {
            args.push(geo_unit);
        }
    }

    if let Some(gut) = geo_unit_type {
        args.push("--geo-unit-type".to_string());
        args.push(gut);
    }

    if let Some(sy) = start_year {
        args.push("--start-year".to_string());
        args.push(sy.to_string());
    }

    if let Some(ey) = end_year {
        args.push("--end-year".to_string());
        args.push(ey.to_string());
    }

    if let Some(footnotes) = footnotes {
        if footnotes {
            args.push("--footnotes".to_string());
        }
    }

    if let Some(indicator_metadata) = indicator_metadata {
        if indicator_metadata {
            args.push("--metadata".to_string());
        }
    }

    if let Some(v) = version {
        args.push("--version".to_string());
        args.push(v);
    }

    python::execute_sync(&app, "unesco_data.py", args)
}

// ==================== DEFINITIONS ENDPOINTS ====================

#[command]
pub async fn unesco_list_geo_units(app: tauri::AppHandle, version: Option<String>) -> Result<String, String> {
    let mut args = vec!["list_geo_units".to_string()];

    if let Some(v) = version {
        args.push(v);
    }

    python::execute_sync(&app, "unesco_data.py", args)
}

#[command]
pub async fn unesco_export_geo_units(app: tauri::AppHandle, format_type: String, version: Option<String>) -> Result<String, String> {
    let mut args = vec!["export_geo_units".to_string(), format_type];

    if let Some(v) = version {
        args.push(v);
    }

    python::execute_sync(&app, "unesco_data.py", args)
}

#[command]
pub async fn unesco_list_indicators(
    app: tauri::AppHandle,
    glossary_terms: Option<bool>,
    disaggregations: Option<bool>,
    version: Option<String>
) -> Result<String, String> {
    let mut args = vec!["list_indicators".to_string()];

    if let Some(glossary_terms) = glossary_terms {
        if glossary_terms {
            args.push("--glossary".to_string());
        }
    }

    if let Some(disaggregations) = disaggregations {
        if disaggregations {
            args.push("--disaggregations".to_string());
        }
    }

    if let Some(v) = version {
        args.push("--version".to_string());
        args.push(v);
    }

    python::execute_sync(&app, "unesco_data.py", args)
}

#[command]
pub async fn unesco_export_indicators(
    app: tauri::AppHandle,
    format_type: String,
    glossary_terms: Option<bool>,
    disaggregations: Option<bool>,
    version: Option<String>
) -> Result<String, String> {
    let mut args = vec!["export_indicators".to_string(), format_type];

    if let Some(glossary_terms) = glossary_terms {
        if glossary_terms {
            args.push("--glossary".to_string());
        }
    }

    if let Some(disaggregations) = disaggregations {
        if disaggregations {
            args.push("--disaggregations".to_string());
        }
    }

    if let Some(v) = version {
        args.push("--version".to_string());
        args.push(v);
    }

    python::execute_sync(&app, "unesco_data.py", args)
}

// ==================== VERSION ENDPOINTS ====================

#[command]
pub async fn unesco_get_default_version(app: tauri::AppHandle) -> Result<String, String> {
    python::execute_sync(&app, "unesco_data.py", vec!["get_default_version".to_string()])
}

#[command]
pub async fn unesco_list_versions(app: tauri::AppHandle) -> Result<String, String> {
    python::execute_sync(&app, "unesco_data.py", vec!["list_versions".to_string()])
}

// ==================== CONVENIENCE METHODS ====================

#[command]
pub async fn unesco_get_education_overview(
    app: tauri::AppHandle,
    country_codes: Option<Vec<String>>,
    start_year: Option<i32>,
    end_year: Option<i32>
) -> Result<String, String> {
    let mut args = vec!["education_overview".to_string()];

    if let Some(country_codes) = country_codes {
        for country in country_codes {
            args.push(country);
        }
    }

    if let Some(sy) = start_year {
        args.push("--start-year".to_string());
        args.push(sy.to_string());
    }

    if let Some(ey) = end_year {
        args.push("--end-year".to_string());
        args.push(ey.to_string());
    }

    python::execute_sync(&app, "unesco_data.py", args)
}

#[command]
pub async fn unesco_get_global_education_trends(
    app: tauri::AppHandle,
    indicator_code: Option<String>,
    start_year: Option<i32>,
    end_year: Option<i32>
) -> Result<String, String> {
    let mut args = vec!["global_trends".to_string()];

    if let Some(ic) = indicator_code {
        args.push(ic);
    }

    if let Some(sy) = start_year {
        args.push("--start-year".to_string());
        args.push(sy.to_string());
    }

    if let Some(ey) = end_year {
        args.push("--end-year".to_string());
        args.push(ey.to_string());
    }

    python::execute_sync(&app, "unesco_data.py", args)
}

#[command]
pub async fn unesco_get_country_comparison(
    app: tauri::AppHandle,
    indicator_code: String,
    country_codes: Vec<String>,
    start_year: Option<i32>,
    end_year: Option<i32>
) -> Result<String, String> {
    let mut args = vec!["country_comparison".to_string(), indicator_code];

    for country in country_codes {
        args.push(country);
    }

    if let Some(sy) = start_year {
        args.push("--start-year".to_string());
        args.push(sy.to_string());
    }

    if let Some(ey) = end_year {
        args.push("--end-year".to_string());
        args.push(ey.to_string());
    }

    python::execute_sync(&app, "unesco_data.py", args)
}

#[command]
pub async fn unesco_search_indicators_by_theme(
    app: tauri::AppHandle,
    theme: String,
    limit: Option<i32>
) -> Result<String, String> {
    let mut args = vec!["search_by_theme".to_string(), theme];

    if let Some(lim) = limit {
        args.push(lim.to_string());
    }

    python::execute_sync(&app, "unesco_data.py", args)
}

#[command]
pub async fn unesco_get_regional_education_data(
    app: tauri::AppHandle,
    region_id: String,
    indicator_codes: Option<Vec<String>>,
    start_year: Option<i32>,
    end_year: Option<i32>
) -> Result<String, String> {
    let mut args = vec!["regional_data".to_string(), region_id];

    if let Some(indicator_codes) = indicator_codes {
        for indicator in indicator_codes {
            args.push(indicator);
        }
    }

    if let Some(sy) = start_year {
        args.push("--start-year".to_string());
        args.push(sy.to_string());
    }

    if let Some(ey) = end_year {
        args.push("--end-year".to_string());
        args.push(ey.to_string());
    }

    python::execute_sync(&app, "unesco_data.py", args)
}

#[command]
pub async fn unesco_get_science_technology_data(
    app: tauri::AppHandle,
    country_codes: Option<Vec<String>>,
    start_year: Option<i32>,
    end_year: Option<i32>
) -> Result<String, String> {
    let mut args = vec!["sti_data".to_string()];

    if let Some(country_codes) = country_codes {
        for country in country_codes {
            args.push(country);
        }
    }

    if let Some(sy) = start_year {
        args.push("--start-year".to_string());
        args.push(sy.to_string());
    }

    if let Some(ey) = end_year {
        args.push("--end-year".to_string());
        args.push(ey.to_string());
    }

    python::execute_sync(&app, "unesco_data.py", args)
}

#[command]
pub async fn unesco_get_culture_data(
    app: tauri::AppHandle,
    country_codes: Option<Vec<String>>,
    start_year: Option<i32>,
    end_year: Option<i32>
) -> Result<String, String> {
    let mut args = vec!["culture_data".to_string()];

    if let Some(country_codes) = country_codes {
        for country in country_codes {
            args.push(country);
        }
    }

    if let Some(sy) = start_year {
        args.push("--start-year".to_string());
        args.push(sy.to_string());
    }

    if let Some(ey) = end_year {
        args.push("--end-year".to_string());
        args.push(ey.to_string());
    }

    python::execute_sync(&app, "unesco_data.py", args)
}

#[command]
pub async fn unesco_export_country_dataset(
    app: tauri::AppHandle,
    country_code: String,
    format_type: String,
    include_metadata: Option<bool>
) -> Result<String, String> {
    let mut args = vec!["export_country_dataset".to_string(), country_code, format_type];

    if let Some(include_metadata) = include_metadata {
        if include_metadata {
            args.push("--metadata".to_string());
        }
    }

    python::execute_sync(&app, "unesco_data.py", args)
}