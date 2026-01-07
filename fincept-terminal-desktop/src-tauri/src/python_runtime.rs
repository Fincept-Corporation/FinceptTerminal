// Python Runtime Module - Worker Pool Integration
// Provides backward-compatible API using worker pool instead of PyO3

use std::path::PathBuf;

/// Execute a Python script using the worker pool (async version)
/// This is the primary execution method - fast, persistent workers, no subprocess spawning
pub async fn execute_python_script_async(
    script_path: &PathBuf,
    args: Vec<String>,
) -> Result<String, String> {
    // Determine venv based on script path or library requirements
    let venv = determine_venv_for_script(script_path);

    // Use worker pool
    crate::worker_pool::execute_python_script(
        script_path.clone(),
        args,
        venv,
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
 feature/peer-comparison-and-technicals-fix

        Python::with_gil(|py| {
            // Preload common heavy libraries
            let _ = py.import("pandas")?;
            let _ = py.import("numpy")?;

            println!("Python runtime: Heavy dependencies preloaded");
            self.preloaded = true;
            Ok(())
        })
    }

    /// Execute a Python script with arguments
    pub fn execute_script(
        &mut self,
        script_path: &PathBuf,
        args: Vec<String>,
    ) -> PyResult<String> {
        Python::with_gil(|py| {
            // Set environment variables for Python script access
            // Check if FMP_API_KEY is already set, if not, try to get it from Rust environment
            let os_module = py.import("os")?;
            let environ = os_module.getattr("environ")?;

            // Get FMP_API_KEY from Rust environment if available
            if let Ok(api_key) = std::env::var("FMP_API_KEY") {
                environ.set_item("FMP_API_KEY", api_key)?;
            } else {
                // Try alternative environment variable names
                if let Ok(api_key) = std::env::var("FINANCIAL_MODELING_PREP_API_KEY") {
                    environ.set_item("FMP_API_KEY", api_key)?;
                }
            }

            // Read the Python script
            let script_code = std::fs::read_to_string(script_path)
                .map_err(|e| PyErr::new::<pyo3::exceptions::PyIOError, _>(
                    format!("Failed to read script: {}", e)
                ))?;

            // Convert paths to CString for PyO3 0.23+
            let file_path_cstr = std::ffi::CString::new(script_path.to_string_lossy().as_ref())
                .unwrap_or_else(|_| std::ffi::CString::new("script.py").unwrap());
            let module_name_cstr = std::ffi::CString::new(
                script_path.file_stem().unwrap().to_string_lossy().as_ref()
            ).unwrap_or_else(|_| std::ffi::CString::new("module").unwrap());
            let code_cstr = std::ffi::CString::new(script_code.as_str())
                .map_err(|e| PyErr::new::<pyo3::exceptions::PyValueError, _>(
                    format!("Invalid Python code: {}", e)
                ))?;

            // Create a new module for this script execution
            let module = PyModule::from_code(
                py,
                &code_cstr,
                &file_path_cstr,
                &module_name_cstr,
            )?;

            // Check if module has a 'main' function
            if module.hasattr("main")? {
                // Call main function with args
                let main_fn = module.getattr("main")?;
                let result = main_fn.call1((args,))?;

                // Extract result as string (assuming JSON output)
                let output: String = result.extract()?;
                Ok(output)
            } else if module.hasattr("calculate_all_indicators")? {
                // For technical_indicators.py, call the calculation function
                // This is a temporary bridge - will be improved in next iteration
                Err(PyErr::new::<pyo3::exceptions::PyAttributeError, _>(
                    "Script needs a 'main' function for PyO3 execution"
                ))
            } else {
                Err(PyErr::new::<pyo3::exceptions::PyAttributeError, _>(
                    "Script must have a 'main' function"
                ))
            }
        })
    }

    /// Execute Python code directly (for simple calculations)
    pub fn execute_code(&self, code: &str) -> PyResult<String> {
        Python::with_gil(|py| {
            let locals = PyDict::new(py);

            // PyO3 0.23+ requires CStr for run
            let code_cstr = std::ffi::CString::new(code)
                .map_err(|e| PyErr::new::<pyo3::exceptions::PyValueError, _>(
                    format!("Invalid code string: {}", e)
                ))?;

            py.run(&code_cstr, None, Some(&locals))?;

            // Get result from locals if available
            if let Ok(result) = locals.get_item("result") {
                if let Some(result) = result {
                    return result.extract();
                }
            }

            Ok(String::new())
        })
      main
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
fn determine_venv_for_script(script_path: &PathBuf) -> &'static str {
    let path_str = script_path.to_string_lossy().to_lowercase();

    // NumPy 1.x libraries
    if path_str.contains("vectorbt")
        || path_str.contains("backtesting")
        || path_str.contains("gluonts")
        || path_str.contains("functime")
        || path_str.contains("pyportfolioopt")
        || path_str.contains("pyqlib")
        || path_str.contains("rdagent")
        || path_str.contains("gs-quant")
    {
        return "venv-numpy1";
    }

    // Default to NumPy 2.x
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
