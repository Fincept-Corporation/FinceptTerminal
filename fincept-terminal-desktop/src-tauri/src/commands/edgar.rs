// Edgar (SEC filings) data commands using edgartools library
use crate::python;


#[tauri::command]
pub async fn execute_edgar_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    // Build command arguments
    let mut cmd_args = vec![command];
    cmd_args.extend(args);

    
    python::execute(&app, "edgar_tools.py", cmd_args).await
}
