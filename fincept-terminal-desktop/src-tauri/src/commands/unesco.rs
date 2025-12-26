use crate::utils::python::execute_python_script_simple;
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
    let mut args = vec!["indicator_data"];

    let indicator_strs: Vec<String>;
    if let Some(indicators) = indicators {
        indicator_strs = indicators;
        for indicator in &indicator_strs {
            args.push(indicator.as_str());
        }
    }

    let geo_unit_strs: Vec<String>;
    if let Some(geo_units) = geo_units {
        geo_unit_strs = geo_units;
        for geo_unit in &geo_unit_strs {
            args.push(geo_unit.as_str());
        }
    }

    let geo_unit_type_str;
    if let Some(gut) = geo_unit_type {
        geo_unit_type_str = gut;
        args.push("--geo-unit-type");
        args.push(&geo_unit_type_str);
    }

    let start_year_str;
    if let Some(sy) = start_year {
        start_year_str = sy.to_string();
        args.push("--start-year");
        args.push(&start_year_str);
    }

    let end_year_str;
    if let Some(ey) = end_year {
        end_year_str = ey.to_string();
        args.push("--end-year");
        args.push(&end_year_str);
    }

    if let Some(footnotes) = footnotes {
        if footnotes {
            args.push("--footnotes");
        }
    }

    if let Some(indicator_metadata) = indicator_metadata {
        if indicator_metadata {
            args.push("--metadata");
        }
    }

    let version_str;
    if let Some(v) = version {
        version_str = v;
        args.push("--version");
        args.push(&version_str);
    }

    execute_python_script_simple(&app, "unesco_data.py", &args)
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
    let mut args = vec!["export_indicator_data", &format_type];

    let indicator_strs: Vec<String>;
    if let Some(indicators) = indicators {
        indicator_strs = indicators;
        for indicator in &indicator_strs {
            args.push(indicator.as_str());
        }
    }

    let geo_unit_strs: Vec<String>;
    if let Some(geo_units) = geo_units {
        geo_unit_strs = geo_units;
        for geo_unit in &geo_unit_strs {
            args.push(geo_unit.as_str());
        }
    }

    let geo_unit_type_str;
    if let Some(gut) = geo_unit_type {
        geo_unit_type_str = gut;
        args.push("--geo-unit-type");
        args.push(&geo_unit_type_str);
    }

    let start_year_str;
    if let Some(sy) = start_year {
        start_year_str = sy.to_string();
        args.push("--start-year");
        args.push(&start_year_str);
    }

    let end_year_str;
    if let Some(ey) = end_year {
        end_year_str = ey.to_string();
        args.push("--end-year");
        args.push(&end_year_str);
    }

    if let Some(footnotes) = footnotes {
        if footnotes {
            args.push("--footnotes");
        }
    }

    if let Some(indicator_metadata) = indicator_metadata {
        if indicator_metadata {
            args.push("--metadata");
        }
    }

    let version_str;
    if let Some(v) = version {
        version_str = v;
        args.push("--version");
        args.push(&version_str);
    }

    execute_python_script_simple(&app, "unesco_data.py", &args)
}

// ==================== DEFINITIONS ENDPOINTS ====================

#[command]
pub async fn unesco_list_geo_units(app: tauri::AppHandle, version: Option<String>) -> Result<String, String> {
    let mut args = vec!["list_geo_units"];

    let version_str;
    if let Some(v) = version {
        version_str = v;
        args.push(&version_str);
    }

    execute_python_script_simple(&app, "unesco_data.py", &args)
}

#[command]
pub async fn unesco_export_geo_units(app: tauri::AppHandle, format_type: String, version: Option<String>) -> Result<String, String> {
    let mut args = vec!["export_geo_units", &format_type];

    let version_str;
    if let Some(v) = version {
        version_str = v;
        args.push(&version_str);
    }

    execute_python_script_simple(&app, "unesco_data.py", &args)
}

#[command]
pub async fn unesco_list_indicators(
    app: tauri::AppHandle,
    glossary_terms: Option<bool>,
    disaggregations: Option<bool>,
    version: Option<String>
) -> Result<String, String> {
    let mut args = vec!["list_indicators"];

    if let Some(glossary_terms) = glossary_terms {
        if glossary_terms {
            args.push("--glossary");
        }
    }

    if let Some(disaggregations) = disaggregations {
        if disaggregations {
            args.push("--disaggregations");
        }
    }

    let version_str;
    if let Some(v) = version {
        version_str = v;
        args.push("--version");
        args.push(&version_str);
    }

    execute_python_script_simple(&app, "unesco_data.py", &args)
}

#[command]
pub async fn unesco_export_indicators(
    app: tauri::AppHandle,
    format_type: String,
    glossary_terms: Option<bool>,
    disaggregations: Option<bool>,
    version: Option<String>
) -> Result<String, String> {
    let mut args = vec!["export_indicators", &format_type];

    if let Some(glossary_terms) = glossary_terms {
        if glossary_terms {
            args.push("--glossary");
        }
    }

    if let Some(disaggregations) = disaggregations {
        if disaggregations {
            args.push("--disaggregations");
        }
    }

    let version_str;
    if let Some(v) = version {
        version_str = v;
        args.push("--version");
        args.push(&version_str);
    }

    execute_python_script_simple(&app, "unesco_data.py", &args)
}

// ==================== VERSION ENDPOINTS ====================

#[command]
pub async fn unesco_get_default_version(app: tauri::AppHandle) -> Result<String, String> {
    execute_python_script_simple(&app, "unesco_data.py", &["get_default_version"])
}

#[command]
pub async fn unesco_list_versions(app: tauri::AppHandle) -> Result<String, String> {
    execute_python_script_simple(&app, "unesco_data.py", &["list_versions"])
}

// ==================== CONVENIENCE METHODS ====================

#[command]
pub async fn unesco_get_education_overview(
    app: tauri::AppHandle,
    country_codes: Option<Vec<String>>,
    start_year: Option<i32>,
    end_year: Option<i32>
) -> Result<String, String> {
    let mut args = vec!["education_overview"];

    let country_code_strs: Vec<String>;
    if let Some(country_codes) = country_codes {
        country_code_strs = country_codes;
        for country in &country_code_strs {
            args.push(country.as_str());
        }
    }

    let start_year_str;
    if let Some(sy) = start_year {
        start_year_str = sy.to_string();
        args.push("--start-year");
        args.push(&start_year_str);
    }

    let end_year_str;
    if let Some(ey) = end_year {
        end_year_str = ey.to_string();
        args.push("--end-year");
        args.push(&end_year_str);
    }

    execute_python_script_simple(&app, "unesco_data.py", &args)
}

#[command]
pub async fn unesco_get_global_education_trends(
    app: tauri::AppHandle,
    indicator_code: Option<String>,
    start_year: Option<i32>,
    end_year: Option<i32>
) -> Result<String, String> {
    let mut args = vec!["global_trends"];

    let indicator_code_str;
    if let Some(ic) = indicator_code {
        indicator_code_str = ic;
        args.push(&indicator_code_str);
    }

    let start_year_str;
    if let Some(sy) = start_year {
        start_year_str = sy.to_string();
        args.push("--start-year");
        args.push(&start_year_str);
    }

    let end_year_str;
    if let Some(ey) = end_year {
        end_year_str = ey.to_string();
        args.push("--end-year");
        args.push(&end_year_str);
    }

    execute_python_script_simple(&app, "unesco_data.py", &args)
}

#[command]
pub async fn unesco_get_country_comparison(
    app: tauri::AppHandle,
    indicator_code: String,
    country_codes: Vec<String>,
    start_year: Option<i32>,
    end_year: Option<i32>
) -> Result<String, String> {
    let mut args = vec!["country_comparison", &indicator_code];

    for country in &country_codes {
        args.push(country.as_str());
    }

    let start_year_str;
    if let Some(sy) = start_year {
        start_year_str = sy.to_string();
        args.push("--start-year");
        args.push(&start_year_str);
    }

    let end_year_str;
    if let Some(ey) = end_year {
        end_year_str = ey.to_string();
        args.push("--end-year");
        args.push(&end_year_str);
    }

    execute_python_script_simple(&app, "unesco_data.py", &args)
}

#[command]
pub async fn unesco_search_indicators_by_theme(
    app: tauri::AppHandle,
    theme: String,
    limit: Option<i32>
) -> Result<String, String> {
    let mut args = vec!["search_by_theme", &theme];

    let limit_str;
    if let Some(lim) = limit {
        limit_str = lim.to_string();
        args.push(&limit_str);
    }

    execute_python_script_simple(&app, "unesco_data.py", &args)
}

#[command]
pub async fn unesco_get_regional_education_data(
    app: tauri::AppHandle,
    region_id: String,
    indicator_codes: Option<Vec<String>>,
    start_year: Option<i32>,
    end_year: Option<i32>
) -> Result<String, String> {
    let mut args = vec!["regional_data", &region_id];

    let indicator_code_strs: Vec<String>;
    if let Some(indicator_codes) = indicator_codes {
        indicator_code_strs = indicator_codes;
        for indicator in &indicator_code_strs {
            args.push(indicator.as_str());
        }
    }

    let start_year_str;
    if let Some(sy) = start_year {
        start_year_str = sy.to_string();
        args.push("--start-year");
        args.push(&start_year_str);
    }

    let end_year_str;
    if let Some(ey) = end_year {
        end_year_str = ey.to_string();
        args.push("--end-year");
        args.push(&end_year_str);
    }

    execute_python_script_simple(&app, "unesco_data.py", &args)
}

#[command]
pub async fn unesco_get_science_technology_data(
    app: tauri::AppHandle,
    country_codes: Option<Vec<String>>,
    start_year: Option<i32>,
    end_year: Option<i32>
) -> Result<String, String> {
    let mut args = vec!["sti_data"];

    let country_code_strs: Vec<String>;
    if let Some(country_codes) = country_codes {
        country_code_strs = country_codes;
        for country in &country_code_strs {
            args.push(country.as_str());
        }
    }

    let start_year_str;
    if let Some(sy) = start_year {
        start_year_str = sy.to_string();
        args.push("--start-year");
        args.push(&start_year_str);
    }

    let end_year_str;
    if let Some(ey) = end_year {
        end_year_str = ey.to_string();
        args.push("--end-year");
        args.push(&end_year_str);
    }

    execute_python_script_simple(&app, "unesco_data.py", &args)
}

#[command]
pub async fn unesco_get_culture_data(
    app: tauri::AppHandle,
    country_codes: Option<Vec<String>>,
    start_year: Option<i32>,
    end_year: Option<i32>
) -> Result<String, String> {
    let mut args = vec!["culture_data"];

    let country_code_strs: Vec<String>;
    if let Some(country_codes) = country_codes {
        country_code_strs = country_codes;
        for country in &country_code_strs {
            args.push(country.as_str());
        }
    }

    let start_year_str;
    if let Some(sy) = start_year {
        start_year_str = sy.to_string();
        args.push("--start-year");
        args.push(&start_year_str);
    }

    let end_year_str;
    if let Some(ey) = end_year {
        end_year_str = ey.to_string();
        args.push("--end-year");
        args.push(&end_year_str);
    }

    execute_python_script_simple(&app, "unesco_data.py", &args)
}

#[command]
pub async fn unesco_export_country_dataset(
    app: tauri::AppHandle,
    country_code: String,
    format_type: String,
    include_metadata: Option<bool>
) -> Result<String, String> {
    let mut args = vec!["export_country_dataset", &country_code, &format_type];

    if let Some(include_metadata) = include_metadata {
        if include_metadata {
            args.push("--metadata");
        }
    }

    execute_python_script_simple(&app, "unesco_data.py", &args)
}