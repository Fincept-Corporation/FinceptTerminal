// BIS (Bank for International Settlements) data commands based on SDMX API

pub mod data;
pub mod structure;
pub mod items;
pub mod market_data;
pub mod analysis;

pub use data::*;
pub use structure::*;
pub use items::*;
pub use market_data::*;
pub use analysis::*;

use crate::python;

/// Execute a BIS command by delegating to the Python script
#[tauri::command]
pub async fn execute_bis_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    let mut cmd_args = vec![command];
    cmd_args.extend(args);
    python::execute(&app, "bis_data.py", cmd_args).await
}
