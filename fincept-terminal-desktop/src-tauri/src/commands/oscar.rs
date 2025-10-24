use tauri::command;

// ==================== INSTRUMENTS ENDPOINTS ====================

#[command]
pub async fn oscar_get_instruments(
    page: Option<i32>,
    sort: Option<String>,
    order: Option<String>,
    filter_on: Option<String>,
    filter_value: Option<String>
) -> Result<String, String> {
    let mut cmd = crate::utils::python::python_command();
    cmd.arg("resources/scripts/oscar_data.py")
        .arg("instruments");

    if let Some(page) = page {
        cmd.arg(page.to_string());
    } else {
        cmd.arg("1");
    }

    if let Some(sort) = sort {
        cmd.arg(&sort);
    } else {
        cmd.arg("id");
    }

    if let Some(order) = order {
        cmd.arg(&order);
    } else {
        cmd.arg("asc");
    }

    if let Some(filter_on) = filter_on {
        cmd.arg("--filter-on");
        cmd.arg(&filter_on);
    }

    if let Some(filter_value) = filter_value {
        cmd.arg("--filter-value");
        cmd.arg(&filter_value);
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
pub async fn oscar_get_instrument_by_slug(slug: String) -> Result<String, String> {
    let output = crate::utils::python::python_command()
        .arg("resources/scripts/oscar_data.py")
        .arg("instrument")
        .arg(&slug)
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
pub async fn oscar_search_instruments_by_type(
    instrument_type: String,
    page: Option<i32>
) -> Result<String, String> {
    let mut cmd = crate::utils::python::python_command();
    cmd.arg("resources/scripts/oscar_data.py")
        .arg("search_instruments_type")
        .arg(&instrument_type);

    if let Some(page) = page {
        cmd.arg(page.to_string());
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
pub async fn oscar_search_instruments_by_agency(
    agency: String,
    page: Option<i32>
) -> Result<String, String> {
    let mut cmd = crate::utils::python::python_command();
    cmd.arg("resources/scripts/oscar_data.py")
        .arg("search_instruments_agency")
        .arg(&agency);

    if let Some(page) = page {
        cmd.arg(page.to_string());
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
pub async fn oscar_search_instruments_by_year_range(
    start_year: i32,
    end_year: i32,
    page: Option<i32>
) -> Result<String, String> {
    let mut cmd = crate::utils::python::python_command();
    cmd.arg("resources/scripts/oscar_data.py")
        .arg("search_instruments_year")
        .arg(start_year.to_string())
        .arg(end_year.to_string());

    if let Some(page) = page {
        cmd.arg(page.to_string());
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
pub async fn oscar_get_instrument_classifications() -> Result<String, String> {
    let output = crate::utils::python::python_command()
        .arg("resources/scripts/oscar_data.py")
        .arg("instrument_classifications")
        .output()
        .map_err(|e| format!("Failed to execute Python command: {}", e))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(format!("Python script failed: {}", stderr));
    }

    let result = String::from_utf8_lossy(&output.stdout).to_string();
    Ok(result)
}

// ==================== SATELLITES ENDPOINTS ====================

#[command]
pub async fn oscar_get_satellites(
    page: Option<i32>,
    sort: Option<String>,
    order: Option<String>,
    filter_on: Option<String>,
    filter_value: Option<String>
) -> Result<String, String> {
    let mut cmd = crate::utils::python::python_command();
    cmd.arg("resources/scripts/oscar_data.py")
        .arg("satellites");

    if let Some(page) = page {
        cmd.arg(page.to_string());
    } else {
        cmd.arg("1");
    }

    if let Some(sort) = sort {
        cmd.arg(&sort);
    } else {
        cmd.arg("id");
    }

    if let Some(order) = order {
        cmd.arg(&order);
    } else {
        cmd.arg("asc");
    }

    if let Some(filter_on) = filter_on {
        cmd.arg("--filter-on");
        cmd.arg(&filter_on);
    }

    if let Some(filter_value) = filter_value {
        cmd.arg("--filter-value");
        cmd.arg(&filter_value);
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
pub async fn oscar_get_satellite_by_slug(slug: String) -> Result<String, String> {
    let output = crate::utils::python::python_command()
        .arg("resources/scripts/oscar_data.py")
        .arg("satellite")
        .arg(&slug)
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
pub async fn oscar_search_satellites_by_orbit(
    orbit_type: String,
    page: Option<i32>
) -> Result<String, String> {
    let mut cmd = crate::utils::python::python_command();
    cmd.arg("resources/scripts/oscar_data.py")
        .arg("search_satellites_orbit")
        .arg(&orbit_type);

    if let Some(page) = page {
        cmd.arg(page.to_string());
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
pub async fn oscar_search_satellites_by_agency(
    agency: String,
    page: Option<i32>
) -> Result<String, String> {
    let mut cmd = crate::utils::python::python_command();
    cmd.arg("resources/scripts/oscar_data.py")
        .arg("search_satellites_agency")
        .arg(&agency);

    if let Some(page) = page {
        cmd.arg(page.to_string());
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

// ==================== VARIABLES ENDPOINTS ====================

#[command]
pub async fn oscar_get_variables(
    page: Option<i32>,
    sort: Option<String>,
    order: Option<String>,
    filter_on: Option<String>,
    filter_value: Option<String>
) -> Result<String, String> {
    let mut cmd = crate::utils::python::python_command();
    cmd.arg("resources/scripts/oscar_data.py")
        .arg("variables");

    if let Some(page) = page {
        cmd.arg(page.to_string());
    } else {
        cmd.arg("1");
    }

    if let Some(sort) = sort {
        cmd.arg(&sort);
    } else {
        cmd.arg("id");
    }

    if let Some(order) = order {
        cmd.arg(&order);
    } else {
        cmd.arg("asc");
    }

    if let Some(filter_on) = filter_on {
        cmd.arg("--filter-on");
        cmd.arg(&filter_on);
    }

    if let Some(filter_value) = filter_value {
        cmd.arg("--filter-value");
        cmd.arg(&filter_value);
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
pub async fn oscar_get_variable_by_slug(slug: String) -> Result<String, String> {
    let output = crate::utils::python::python_command()
        .arg("resources/scripts/oscar_data.py")
        .arg("variable")
        .arg(&slug)
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
pub async fn oscar_search_variables_by_instrument(
    instrument: String,
    page: Option<i32>
) -> Result<String, String> {
    let mut cmd = crate::utils::python::python_command();
    cmd.arg("resources/scripts/oscar_data.py")
        .arg("search_variables_instrument")
        .arg(&instrument);

    if let Some(page) = page {
        cmd.arg(page.to_string());
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
pub async fn oscar_get_ecv_variables(page: Option<i32>) -> Result<String, String> {
    let mut cmd = crate::utils::python::python_command();
    cmd.arg("resources/scripts/oscar_data.py")
        .arg("ecv_variables");

    if let Some(page) = page {
        cmd.arg(page.to_string());
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

// ==================== CONVENIENCE METHODS ====================

#[command]
pub async fn oscar_get_space_agencies() -> Result<String, String> {
    let output = crate::utils::python::python_command()
        .arg("resources/scripts/oscar_data.py")
        .arg("space_agencies")
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
pub async fn oscar_get_instrument_types() -> Result<String, String> {
    let output = crate::utils::python::python_command()
        .arg("resources/scripts/oscar_data.py")
        .arg("instrument_types")
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
pub async fn oscar_search_weather_instruments(page: Option<i32>) -> Result<String, String> {
    let mut cmd = crate::utils::python::python_command();
    cmd.arg("resources/scripts/oscar_data.py")
        .arg("weather_instruments");

    if let Some(page) = page {
        cmd.arg(page.to_string());
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
pub async fn oscar_get_climate_monitoring_instruments(page: Option<i32>) -> Result<String, String> {
    let mut cmd = crate::utils::python::python_command();
    cmd.arg("resources/scripts/oscar_data.py")
        .arg("climate_instruments");

    if let Some(page) = page {
        cmd.arg(page.to_string());
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
pub async fn oscar_get_overview_statistics() -> Result<String, String> {
    let output = crate::utils::python::python_command()
        .arg("resources/scripts/oscar_data.py")
        .arg("overview_statistics")
        .output()
        .map_err(|e| format!("Failed to execute Python command: {}", e))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(format!("Python script failed: {}", stderr));
    }

    let result = String::from_utf8_lossy(&output.stdout).to_string();
    Ok(result)
}