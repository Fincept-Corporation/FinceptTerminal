// PxWeb Statistics Finland data commands
use crate::python;

/// Execute PxWeb Python script command
#[tauri::command]
pub async fn execute_pxweb_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    let mut cmd_args = vec![command];
    cmd_args.extend(args);

    python::execute(&app, "pxweb_fetcher.py", cmd_args).await
}
