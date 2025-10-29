// Jupyter notebook command module
use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::path::PathBuf;
use std::process::Command;

#[cfg(target_os = "windows")]
use std::os::windows::process::CommandExt;

#[cfg(target_os = "windows")]
const CREATE_NO_WINDOW: u32 = 0x08000000;

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct NotebookCell {
    pub id: String,
    pub cell_type: String, // "code" or "markdown"
    pub source: Vec<String>,
    pub outputs: Vec<CellOutput>,
    pub execution_count: Option<i32>,
    pub metadata: HashMap<String, serde_json::Value>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct CellOutput {
    pub output_type: String, // "execute_result", "display_data", "stream", "error"
    pub data: Option<HashMap<String, Vec<String>>>,
    pub text: Option<Vec<String>>,
    pub name: Option<String>, // for stream outputs (stdout/stderr)
    pub ename: Option<String>, // for error outputs
    pub evalue: Option<String>,
    pub traceback: Option<Vec<String>>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct NotebookMetadata {
    pub kernelspec: KernelSpec,
    pub language_info: LanguageInfo,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct KernelSpec {
    pub display_name: String,
    pub language: String,
    pub name: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LanguageInfo {
    pub name: String,
    pub version: String,
    pub mimetype: String,
    pub file_extension: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct JupyterNotebook {
    pub cells: Vec<NotebookCell>,
    pub metadata: NotebookMetadata,
    pub nbformat: i32,
    pub nbformat_minor: i32,
}

#[derive(Debug, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct ExecutionResult {
    pub success: bool,
    pub outputs: Vec<CellOutput>,
    pub execution_count: i32,
    pub error: Option<String>,
}

/// Execute Python code and return structured output
#[tauri::command]
pub async fn execute_python_code(
    app: tauri::AppHandle,
    code: String,
    execution_count: i32,
) -> Result<ExecutionResult, String> {
    // Get bundled Python path
    let python_path = crate::utils::python::get_python_path(&app)?;

    // Verify Python exists
    if !python_path.exists() {
        return Err(format!(
            "Bundled Python not found at: {}",
            python_path.display()
        ));
    }

    // Create temporary Python script
    let temp_dir = std::env::temp_dir();
    let script_path = temp_dir.join(format!("jupyter_cell_{}.py", execution_count));

    // Write code to temporary file
    std::fs::write(&script_path, &code)
        .map_err(|e| format!("Failed to write temp script: {}", e))?;

    // Execute Python code
    let mut cmd = Command::new(&python_path);
    cmd.arg(&script_path);

    // Hide console window on Windows
    #[cfg(target_os = "windows")]
    cmd.creation_flags(CREATE_NO_WINDOW);

    // Set PYTHONIOENCODING to UTF-8 for proper encoding
    cmd.env("PYTHONIOENCODING", "utf-8");

    // Execute and capture output
    let output = cmd
        .output()
        .map_err(|e| format!("Failed to execute Python: {}", e))?;

    // Clean up temp file
    let _ = std::fs::remove_file(&script_path);

    let mut outputs = Vec::new();

    if output.status.success() {
        let stdout = String::from_utf8_lossy(&output.stdout);
        if !stdout.is_empty() {
            outputs.push(CellOutput {
                output_type: "stream".to_string(),
                data: None,
                text: Some(stdout.lines().map(|s| s.to_string()).collect()),
                name: Some("stdout".to_string()),
                ename: None,
                evalue: None,
                traceback: None,
            });
        }

        Ok(ExecutionResult {
            success: true,
            outputs,
            execution_count,
            error: None,
        })
    } else {
        let stderr = String::from_utf8_lossy(&output.stderr);
        outputs.push(CellOutput {
            output_type: "error".to_string(),
            data: None,
            text: None,
            name: None,
            ename: Some("ExecutionError".to_string()),
            evalue: Some(stderr.lines().last().unwrap_or("Unknown error").to_string()),
            traceback: Some(stderr.lines().map(|s| s.to_string()).collect()),
        });

        Ok(ExecutionResult {
            success: false,
            outputs,
            execution_count,
            error: Some(stderr.to_string()),
        })
    }
}

/// Open and parse a .ipynb file
#[tauri::command]
pub async fn open_notebook(file_path: String) -> Result<JupyterNotebook, String> {
    let path = PathBuf::from(&file_path);

    if !path.exists() {
        return Err(format!("File not found: {}", file_path));
    }

    let content = std::fs::read_to_string(&path)
        .map_err(|e| format!("Failed to read file: {}", e))?;

    let notebook: JupyterNotebook = serde_json::from_str(&content)
        .map_err(|e| format!("Failed to parse notebook: {}", e))?;

    Ok(notebook)
}

/// Save notebook to .ipynb file
#[tauri::command]
pub async fn save_notebook(
    file_path: String,
    notebook: JupyterNotebook,
) -> Result<String, String> {
    let path = PathBuf::from(&file_path);

    let json = serde_json::to_string_pretty(&notebook)
        .map_err(|e| format!("Failed to serialize notebook: {}", e))?;

    std::fs::write(&path, json).map_err(|e| format!("Failed to write file: {}", e))?;

    Ok(format!("Notebook saved to: {}", file_path))
}

/// Create a new empty notebook
#[tauri::command]
pub async fn create_new_notebook() -> Result<JupyterNotebook, String> {
    Ok(JupyterNotebook {
        cells: vec![NotebookCell {
            id: uuid::Uuid::new_v4().to_string(),
            cell_type: "code".to_string(),
            source: vec![],
            outputs: vec![],
            execution_count: None,
            metadata: HashMap::new(),
        }],
        metadata: NotebookMetadata {
            kernelspec: KernelSpec {
                display_name: "Python 3".to_string(),
                language: "python".to_string(),
                name: "python3".to_string(),
            },
            language_info: LanguageInfo {
                name: "python".to_string(),
                version: "3.12.0".to_string(),
                mimetype: "text/x-python".to_string(),
                file_extension: ".py".to_string(),
            },
        },
        nbformat: 4,
        nbformat_minor: 5,
    })
}

/// Get Python version from bundled interpreter
#[tauri::command]
pub async fn get_python_version(app: tauri::AppHandle) -> Result<String, String> {
    let python_path = crate::utils::python::get_python_path(&app)?;

    let mut cmd = Command::new(&python_path);
    cmd.arg("--version");

    #[cfg(target_os = "windows")]
    cmd.creation_flags(CREATE_NO_WINDOW);

    let output = cmd
        .output()
        .map_err(|e| format!("Failed to get Python version: {}", e))?;

    let version = String::from_utf8_lossy(&output.stdout);
    let version_stderr = String::from_utf8_lossy(&output.stderr);

    // Python sometimes outputs version to stderr
    let result = if !version.is_empty() {
        version.trim().to_string()
    } else {
        version_stderr.trim().to_string()
    };

    Ok(result)
}

/// Install a Python package using pip
#[tauri::command]
pub async fn install_python_package(
    app: tauri::AppHandle,
    package_name: String,
) -> Result<String, String> {
    let python_path = crate::utils::python::get_python_path(&app)?;

    let mut cmd = Command::new(&python_path);
    cmd.args(&["-m", "pip", "install", &package_name]);

    #[cfg(target_os = "windows")]
    cmd.creation_flags(CREATE_NO_WINDOW);

    let output = cmd
        .output()
        .map_err(|e| format!("Failed to install package: {}", e))?;

    if output.status.success() {
        Ok(format!(
            "Successfully installed {}",
            String::from_utf8_lossy(&output.stdout)
        ))
    } else {
        Err(format!(
            "Failed to install package: {}",
            String::from_utf8_lossy(&output.stderr)
        ))
    }
}

/// List installed Python packages
#[tauri::command]
pub async fn list_python_packages(app: tauri::AppHandle) -> Result<Vec<String>, String> {
    let python_path = crate::utils::python::get_python_path(&app)?;

    let mut cmd = Command::new(&python_path);
    cmd.args(&["-m", "pip", "list", "--format=freeze"]);

    #[cfg(target_os = "windows")]
    cmd.creation_flags(CREATE_NO_WINDOW);

    let output = cmd
        .output()
        .map_err(|e| format!("Failed to list packages: {}", e))?;

    if output.status.success() {
        let stdout = String::from_utf8_lossy(&output.stdout);
        let packages: Vec<String> = stdout.lines().map(|s| s.to_string()).collect();
        Ok(packages)
    } else {
        Err(format!(
            "Failed to list packages: {}",
            String::from_utf8_lossy(&output.stderr)
        ))
    }
}
