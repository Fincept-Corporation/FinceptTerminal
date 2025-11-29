// Financial Report Generator Commands using Jinja2
#![allow(dead_code)]
use crate::utils::python::execute_python_command;

/// Generate HTML from report template
#[tauri::command]
pub async fn generate_report_html(
    app: tauri::AppHandle,
    template_json: String,
) -> Result<String, String> {
    let args = vec!["generate_html".to_string(), template_json];
    execute_python_command(&app, "financial_report_generator.py", &args)
}

/// Generate PDF from report template
#[tauri::command]
pub async fn generate_report_pdf(
    app: tauri::AppHandle,
    template_json: String,
    output_path: String,
) -> Result<String, String> {
    let args = vec!["generate_pdf".to_string(), template_json, output_path];
    execute_python_command(&app, "financial_report_generator.py", &args)
}

/// Create default Jinja2 template
#[tauri::command]
pub async fn create_default_report_template(
    app: tauri::AppHandle,
) -> Result<String, String> {
    let args = vec!["create_template".to_string()];
    execute_python_command(&app, "financial_report_generator.py", &args)
}
