// PyO3-based Python Runtime Module
// Replaces subprocess execution with embedded Python interpreter

use pyo3::prelude::*;
use pyo3::types::{PyDict, PyModule};
use std::path::PathBuf;
use std::sync::Mutex;
use once_cell::sync::Lazy;

// Global Python runtime instance (singleton)
static PYTHON_RUNTIME: Lazy<Mutex<PythonRuntime>> = Lazy::new(|| {
    Mutex::new(PythonRuntime::new().expect("Failed to initialize Python runtime"))
});

pub struct PythonRuntime {
    // Flag to track if heavy libraries are preloaded
    preloaded: bool,
}

impl PythonRuntime {
    /// Initialize the Python runtime
    pub fn new() -> PyResult<Self> {
        // Initialize Python interpreter (only happens once)
        pyo3::prepare_freethreaded_python();

        Ok(Self {
            preloaded: false,
        })
    }

    /// Preload heavy dependencies (pandas, numpy) to cache them
    pub fn preload_dependencies(&mut self) -> PyResult<()> {
        if self.preloaded {
            return Ok(());
        }

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
    }
}

/// Get the global Python runtime instance
pub fn get_runtime() -> &'static Mutex<PythonRuntime> {
    &PYTHON_RUNTIME
}

/// Execute a Python script (public API)
pub fn execute_python_script(
    script_path: &PathBuf,
    args: Vec<String>,
) -> Result<String, String> {
    let mut runtime = get_runtime()
        .lock()
        .map_err(|e| format!("Failed to lock Python runtime: {}", e))?;

    // Preload dependencies on first use
    if !runtime.preloaded {
        runtime.preload_dependencies()
            .map_err(|e| format!("Failed to preload dependencies: {}", e))?;
    }

    // Execute script
    runtime.execute_script(script_path, args)
        .map_err(|e| format!("Python execution failed: {}", e))
}

/// Execute Python code directly (public API)
pub fn execute_python_code(code: &str) -> Result<String, String> {
    let runtime = get_runtime()
        .lock()
        .map_err(|e| format!("Failed to lock Python runtime: {}", e))?;

    runtime.execute_code(code)
        .map_err(|e| format!("Python execution failed: {}", e))
}
