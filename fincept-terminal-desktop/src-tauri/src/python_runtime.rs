// Python Runtime Module - Worker Pool Integration
// Provides async/sync execution of Python scripts via persistent worker processes
// Primary method: Worker pool with Unix Domain Sockets (fast, persistent workers)
// Fallback method: Direct subprocess execution (slower but reliable)

use std::path::PathBuf;

/// Execute a Python script using the worker pool (async version)
/// This is the primary execution method - fast, persistent workers, no subprocess spawning
pub async fn execute_python_script_async(
    script_path: &PathBuf,
    args: Vec<String>,
) -> Result<String, String> {
    execute_python_script_async_with_priority(script_path, args, 1).await  // Default to NORMAL
}

/// Execute a Python script with custom priority (async version)
pub async fn execute_python_script_async_with_priority(
    script_path: &PathBuf,
    args: Vec<String>,
    priority: u8,
) -> Result<String, String> {
    // Determine venv based on script path or library requirements
    let venv = determine_venv_for_script(script_path);

    // Use worker pool with priority
    crate::worker_pool::execute_python_script_with_priority(
        script_path.clone(),
        args,
        venv,
        priority,
    ).await
}

/// Execute a Python script using the worker pool (blocking sync wrapper)
/// Backward-compatible sync API that internally uses async runtime
pub fn execute_python_script(
    script_path: &PathBuf,
    args: Vec<String>,
) -> Result<String, String> {
    // Get or create a tokio runtime handle
    let handle = tokio::runtime::Handle::try_current()
        .ok();

    match handle {
        Some(h) => {
            // We're already in a tokio runtime, use block_in_place
            tokio::task::block_in_place(|| {
                h.block_on(execute_python_script_async(script_path, args))
            })
        }
        None => {
            // No runtime, create one temporarily
            let rt = tokio::runtime::Runtime::new()
                .map_err(|e| format!("Failed to create runtime: {}", e))?;
            rt.block_on(execute_python_script_async(script_path, args))
        }
    }
}

/// Execute Python code directly (for simple calculations)
/// Note: This spawns a temporary Python process since workers are script-based
pub async fn execute_python_code(code: &str) -> Result<String, String> {
    // For inline code execution, we need to write to a temp file
    // or use a dedicated eval worker (future enhancement)
    use std::io::Write;

    let temp_dir = std::env::temp_dir();
    let script_path = temp_dir.join(format!("temp_code_{}.py", uuid::Uuid::new_v4()));

    // Write code to temp file with main() wrapper
    let wrapped_code = format!(
        r#"
import json

def main(args):
    {}
    # Return result if available
    if 'result' in locals():
        return json.dumps({{"result": result}})
    return json.dumps({{"result": None}})

if __name__ == '__main__':
    import sys
    print(main(sys.argv[1:]))
"#,
        code
    );

    let mut file = std::fs::File::create(&script_path)
        .map_err(|e| format!("Failed to create temp script: {}", e))?;
    file.write_all(wrapped_code.as_bytes())
        .map_err(|e| format!("Failed to write temp script: {}", e))?;
    drop(file);

    // Execute using worker pool
    let result = execute_python_script_async(&script_path, vec![]).await;

    // Clean up temp file
    let _ = std::fs::remove_file(&script_path);

    result
}

/// Determine which venv to use based on script path
///
/// venv-numpy1 (NumPy <2.0): vectorbt, backtesting, gluonts, functime, PyPortfolioOpt, financepy
/// venv-numpy2 (NumPy 2.x): Everything else including ta, vnpy, edgartools, pyqlib, rdagent, etc.
fn determine_venv_for_script(script_path: &PathBuf) -> &'static str {
    let path_str = script_path.to_string_lossy().to_lowercase();

    // NumPy 1.x libraries (from requirements-numpy1.txt)
    // ONLY libraries that are INCOMPATIBLE with NumPy 2.x go here
    if path_str.contains("vectorbt")
        || path_str.contains("backtesting")
        || path_str.contains("gluonts")
        || path_str.contains("functime")
        || path_str.contains("pyportfolioopt")
        || path_str.contains("financepy")
    {
        return "venv-numpy1";
    }

    // NumPy 2.x libraries (DEFAULT - most libraries including modern quant tools)
    // Includes: ta, vnpy, edgartools, pyqlib, rdagent, gs-quant, ffn, fortitudo, finquant, etc.
    "venv-numpy2"
}

/// Initialize worker pool (async version)
pub async fn initialize_global_async(python_base_path: PathBuf) -> Result<(), String> {
    crate::worker_pool::initialize_worker_pool(python_base_path).await
}

/// Initialize worker pool (blocking sync wrapper)
pub fn initialize_global(python_base_path: PathBuf) -> Result<(), String> {
    let handle = tokio::runtime::Handle::try_current().ok();

    match handle {
        Some(h) => {
            tokio::task::block_in_place(|| {
                h.block_on(initialize_global_async(python_base_path))
            })
        }
        None => {
            let rt = tokio::runtime::Runtime::new()
                .map_err(|e| format!("Failed to create runtime: {}", e))?;
            rt.block_on(initialize_global_async(python_base_path))
        }
    }
}
