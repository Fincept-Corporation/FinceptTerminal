use tauri::command;

// ==================== DATA ENDPOINTS ====================

#[command]
pub async fn unesco_get_indicator_data(
    indicators: Option<Vec<String>>,
    geo_units: Option<Vec<String>>,
    geo_unit_type: Option<String>,
    start_year: Option<i32>,
    end_year: Option<i32>,
    footnotes: Option<bool>,
    indicator_metadata: Option<bool>,
    version: Option<String>
) -> Result<String, String> {
    let mut cmd = crate::utils::python::python_command();
    cmd.arg("resources/scripts/unesco_data.py")
        .arg("indicator_data");

    if let Some(indicators) = indicators {
        for indicator in indicators {
            cmd.arg(&indicator);
        }
    }

    if let Some(geo_units) = geo_units {
        for geo_unit in geo_units {
            cmd.arg(&geo_unit);
        }
    }

    if let Some(geo_unit_type) = geo_unit_type {
        cmd.arg("--geo-unit-type");
        cmd.arg(&geo_unit_type);
    }

    if let Some(start_year) = start_year {
        cmd.arg("--start-year");
        cmd.arg(start_year.to_string());
    }

    if let Some(end_year) = end_year {
        cmd.arg("--end-year");
        cmd.arg(end_year.to_string());
    }

    if let Some(footnotes) = footnotes {
        if footnotes {
            cmd.arg("--footnotes");
        }
    }

    if let Some(indicator_metadata) = indicator_metadata {
        if indicator_metadata {
            cmd.arg("--metadata");
        }
    }

    if let Some(version) = version {
        cmd.arg("--version");
        cmd.arg(&version);
    }

    let output = cmd.output()
        .map_err(|e| format!("Failed to execute Python command: {}", e))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(format!("Python script failed: {}", stderr));
    }

    let result = String::from_utf8_lossy(&output.stdout).to_string();
    Ok(result)
}

#[command]
pub async fn unesco_export_indicator_data(
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
    let mut cmd = crate::utils::python::python_command();
    cmd.arg("resources/scripts/unesco_data.py")
        .arg("export_indicator_data")
        .arg(&format_type);

    if let Some(indicators) = indicators {
        for indicator in indicators {
            cmd.arg(&indicator);
        }
    }

    if let Some(geo_units) = geo_units {
        for geo_unit in geo_units {
            cmd.arg(&geo_unit);
        }
    }

    if let Some(geo_unit_type) = geo_unit_type {
        cmd.arg("--geo-unit-type");
        cmd.arg(&geo_unit_type);
    }

    if let Some(start_year) = start_year {
        cmd.arg("--start-year");
        cmd.arg(start_year.to_string());
    }

    if let Some(end_year) = end_year {
        cmd.arg("--end-year");
        cmd.arg(end_year.to_string());
    }

    if let Some(footnotes) = footnotes {
        if footnotes {
            cmd.arg("--footnotes");
        }
    }

    if let Some(indicator_metadata) = indicator_metadata {
        if indicator_metadata {
            cmd.arg("--metadata");
        }
    }

    if let Some(version) = version {
        cmd.arg("--version");
        cmd.arg(&version);
    }

    let output = cmd.output()
        .map_err(|e| format!("Failed to execute Python command: {}", e))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(format!("Python script failed: {}", stderr));
    }

    let result = String::from_utf8_lossy(&output.stdout).to_string();
    Ok(result)
}

// ==================== DEFINITIONS ENDPOINTS ====================

#[command]
pub async fn unesco_list_geo_units(version: Option<String>) -> Result<String, String> {
    let mut cmd = crate::utils::python::python_command();
    cmd.arg("resources/scripts/unesco_data.py")
        .arg("list_geo_units");

    if let Some(version) = version {
        cmd.arg(&version);
    }

    let output = cmd.output()
        .map_err(|e| format!("Failed to execute Python command: {}", e))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(format!("Python script failed: {}", stderr));
    }

    let result = String::from_utf8_lossy(&output.stdout).to_string();
    Ok(result)
}

#[command]
pub async fn unesco_export_geo_units(format_type: String, version: Option<String>) -> Result<String, String> {
    let mut cmd = crate::utils::python::python_command();
    cmd.arg("resources/scripts/unesco_data.py")
        .arg("export_geo_units")
        .arg(&format_type);

    if let Some(version) = version {
        cmd.arg(&version);
    }

    let output = cmd.output()
        .map_err(|e| format!("Failed to execute Python command: {}", e))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(format!("Python script failed: {}", stderr));
    }

    let result = String::from_utf8_lossy(&output.stdout).to_string();
    Ok(result)
}

#[command]
pub async fn unesco_list_indicators(
    glossary_terms: Option<bool>,
    disaggregations: Option<bool>,
    version: Option<String>
) -> Result<String, String> {
    let mut cmd = crate::utils::python::python_command();
    cmd.arg("resources/scripts/unesco_data.py")
        .arg("list_indicators");

    if let Some(glossary_terms) = glossary_terms {
        if glossary_terms {
            cmd.arg("--glossary");
        }
    }

    if let Some(disaggregations) = disaggregations {
        if disaggregations {
            cmd.arg("--disaggregations");
        }
    }

    if let Some(version) = version {
        cmd.arg("--version");
        cmd.arg(&version);
    }

    let output = cmd.output()
        .map_err(|e| format!("Failed to execute Python command: {}", e))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(format!("Python script failed: {}", stderr));
    }

    let result = String::from_utf8_lossy(&output.stdout).to_string();
    Ok(result)
}

#[command]
pub async fn unesco_export_indicators(
    format_type: String,
    glossary_terms: Option<bool>,
    disaggregations: Option<bool>,
    version: Option<String>
) -> Result<String, String> {
    let mut cmd = crate::utils::python::python_command();
    cmd.arg("resources/scripts/unesco_data.py")
        .arg("export_indicators")
        .arg(&format_type);

    if let Some(glossary_terms) = glossary_terms {
        if glossary_terms {
            cmd.arg("--glossary");
        }
    }

    if let Some(disaggregations) = disaggregations {
        if disaggregations {
            cmd.arg("--disaggregations");
        }
    }

    if let Some(version) = version {
        cmd.arg("--version");
        cmd.arg(&version);
    }

    let output = cmd.output()
        .map_err(|e| format!("Failed to execute Python command: {}", e))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(format!("Python script failed: {}", stderr));
    }

    let result = String::from_utf8_lossy(&output.stdout).to_string();
    Ok(result)
}

// ==================== VERSION ENDPOINTS ====================

#[command]
pub async fn unesco_get_default_version() -> Result<String, String> {
    let output = crate::utils::python::python_command()
        .arg("resources/scripts/unesco_data.py")
        .arg("get_default_version")
        .output()
        .map_err(|e| format!("Failed to execute Python command: {}", e))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(format!("Python script failed: {}", stderr));
    }

    let result = String::from_utf8_lossy(&output.stdout).to_string();
    Ok(result)
}

#[command]
pub async fn unesco_list_versions() -> Result<String, String> {
    let output = crate::utils::python::python_command()
        .arg("resources/scripts/unesco_data.py")
        .arg("list_versions")
        .output()
        .map_err(|e| format!("Failed to execute Python command: {}", e))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(format!("Python script failed: {}", stderr));
    }

    let result = String::from_utf8_lossy(&output.stdout).to_string();
    Ok(result)
}

// ==================== CONVENIENCE METHODS ====================

#[command]
pub async fn unesco_get_education_overview(
    country_codes: Option<Vec<String>>,
    start_year: Option<i32>,
    end_year: Option<i32>
) -> Result<String, String> {
    let mut cmd = crate::utils::python::python_command();
    cmd.arg("resources/scripts/unesco_data.py")
        .arg("education_overview");

    if let Some(country_codes) = country_codes {
        for country in country_codes {
            cmd.arg(&country);
        }
    }

    if let Some(start_year) = start_year {
        cmd.arg("--start-year");
        cmd.arg(start_year.to_string());
    }

    if let Some(end_year) = end_year {
        cmd.arg("--end-year");
        cmd.arg(end_year.to_string());
    }

    let output = cmd.output()
        .map_err(|e| format!("Failed to execute Python command: {}", e))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(format!("Python script failed: {}", stderr));
    }

    let result = String::from_utf8_lossy(&output.stdout).to_string();
    Ok(result)
}

#[command]
pub async fn unesco_get_global_education_trends(
    indicator_code: Option<String>,
    start_year: Option<i32>,
    end_year: Option<i32>
) -> Result<String, String> {
    let mut cmd = crate::utils::python::python_command();
    cmd.arg("resources/scripts/unesco_data.py")
        .arg("global_trends");

    if let Some(indicator_code) = indicator_code {
        cmd.arg(&indicator_code);
    }

    if let Some(start_year) = start_year {
        cmd.arg("--start-year");
        cmd.arg(start_year.to_string());
    }

    if let Some(end_year) = end_year {
        cmd.arg("--end-year");
        cmd.arg(end_year.to_string());
    }

    let output = cmd.output()
        .map_err(|e| format!("Failed to execute Python command: {}", e))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(format!("Python script failed: {}", stderr));
    }

    let result = String::from_utf8_lossy(&output.stdout).to_string();
    Ok(result)
}

#[command]
pub async fn unesco_get_country_comparison(
    indicator_code: String,
    country_codes: Vec<String>,
    start_year: Option<i32>,
    end_year: Option<i32>
) -> Result<String, String> {
    let mut cmd = crate::utils::python::python_command();
    cmd.arg("resources/scripts/unesco_data.py")
        .arg("country_comparison")
        .arg(&indicator_code);

    for country in country_codes {
        cmd.arg(&country);
    }

    if let Some(start_year) = start_year {
        cmd.arg("--start-year");
        cmd.arg(start_year.to_string());
    }

    if let Some(end_year) = end_year {
        cmd.arg("--end-year");
        cmd.arg(end_year.to_string());
    }

    let output = cmd.output()
        .map_err(|e| format!("Failed to execute Python command: {}", e))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(format!("Python script failed: {}", stderr));
    }

    let result = String::from_utf8_lossy(&output.stdout).to_string();
    Ok(result)
}

#[command]
pub async fn unesco_search_indicators_by_theme(
    theme: String,
    limit: Option<i32>
) -> Result<String, String> {
    let mut cmd = crate::utils::python::python_command();
    cmd.arg("resources/scripts/unesco_data.py")
        .arg("search_by_theme")
        .arg(&theme);

    if let Some(limit) = limit {
        cmd.arg(limit.to_string());
    }

    let output = cmd.output()
        .map_err(|e| format!("Failed to execute Python command: {}", e))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(format!("Python script failed: {}", stderr));
    }

    let result = String::from_utf8_lossy(&output.stdout).to_string();
    Ok(result)
}

#[command]
pub async fn unesco_get_regional_education_data(
    region_id: String,
    indicator_codes: Option<Vec<String>>,
    start_year: Option<i32>,
    end_year: Option<i32>
) -> Result<String, String> {
    let mut cmd = crate::utils::python::python_command();
    cmd.arg("resources/scripts/unesco_data.py")
        .arg("regional_data")
        .arg(&region_id);

    if let Some(indicator_codes) = indicator_codes {
        for indicator in indicator_codes {
            cmd.arg(&indicator);
        }
    }

    if let Some(start_year) = start_year {
        cmd.arg("--start-year");
        cmd.arg(start_year.to_string());
    }

    if let Some(end_year) = end_year {
        cmd.arg("--end-year");
        cmd.arg(end_year.to_string());
    }

    let output = cmd.output()
        .map_err(|e| format!("Failed to execute Python command: {}", e))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(format!("Python script failed: {}", stderr));
    }

    let result = String::from_utf8_lossy(&output.stdout).to_string();
    Ok(result)
}

#[command]
pub async fn unesco_get_science_technology_data(
    country_codes: Option<Vec<String>>,
    start_year: Option<i32>,
    end_year: Option<i32>
) -> Result<String, String> {
    let mut cmd = crate::utils::python::python_command();
    cmd.arg("resources/scripts/unesco_data.py")
        .arg("sti_data");

    if let Some(country_codes) = country_codes {
        for country in country_codes {
            cmd.arg(&country);
        }
    }

    if let Some(start_year) = start_year {
        cmd.arg("--start-year");
        cmd.arg(start_year.to_string());
    }

    if let Some(end_year) = end_year {
        cmd.arg("--end-year");
        cmd.arg(end_year.to_string());
    }

    let output = cmd.output()
        .map_err(|e| format!("Failed to execute Python command: {}", e))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(format!("Python script failed: {}", stderr));
    }

    let result = String::from_utf8_lossy(&output.stdout).to_string();
    Ok(result)
}

#[command]
pub async fn unesco_get_culture_data(
    country_codes: Option<Vec<String>>,
    start_year: Option<i32>,
    end_year: Option<i32>
) -> Result<String, String> {
    let mut cmd = crate::utils::python::python_command();
    cmd.arg("resources/scripts/unesco_data.py")
        .arg("culture_data");

    if let Some(country_codes) = country_codes {
        for country in country_codes {
            cmd.arg(&country);
        }
    }

    if let Some(start_year) = start_year {
        cmd.arg("--start-year");
        cmd.arg(start_year.to_string());
    }

    if let Some(end_year) = end_year {
        cmd.arg("--end-year");
        cmd.arg(end_year.to_string());
    }

    let output = cmd.output()
        .map_err(|e| format!("Failed to execute Python command: {}", e))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(format!("Python script failed: {}", stderr));
    }

    let result = String::from_utf8_lossy(&output.stdout).to_string();
    Ok(result)
}

#[command]
pub async fn unesco_export_country_dataset(
    country_code: String,
    format_type: String,
    include_metadata: Option<bool>
) -> Result<String, String> {
    let mut cmd = crate::utils::python::python_command();
    cmd.arg("resources/scripts/unesco_data.py")
        .arg("export_country_dataset")
        .arg(&country_code)
        .arg(&format_type);

    if let Some(include_metadata) = include_metadata {
        if include_metadata {
            cmd.arg("--metadata");
        }
    }

    let output = cmd.output()
        .map_err(|e| format!("Failed to execute Python command: {}", e))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(format!("Python script failed: {}", stderr));
    }

    let result = String::from_utf8_lossy(&output.stdout).to_string();
    Ok(result)
}