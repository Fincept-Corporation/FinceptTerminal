// setup/checks.rs - Check installation status

use std::path::PathBuf;
use std::process::Command;

#[cfg(target_os = "windows")]
use super::CommandExt;
#[cfg(target_os = "windows")]
use super::CREATE_NO_WINDOW;

/// Check Python installation
pub(super) fn check_python(install_dir: &PathBuf) -> (bool, Option<String>) {
    let python_exe = if cfg!(target_os = "windows") {
        install_dir.join("python/python.exe")
    } else {
        // Linux and macOS both use python-build-standalone now
        install_dir.join("python/bin/python3")
    };

    if !python_exe.exists() {
        return (false, None);
    }

    let mut cmd = Command::new(&python_exe);
    cmd.arg("--version");

    #[cfg(target_os = "windows")]
    cmd.creation_flags(CREATE_NO_WINDOW);

    if let Ok(output) = cmd.output() {
        if output.status.success() {
            let version = String::from_utf8_lossy(&output.stdout).trim().to_string();
            return (true, Some(version));
        }
    }
    (false, None)
}

/// Check Bun installation
pub(super) fn check_bun(install_dir: &PathBuf) -> (bool, Option<String>) {
    let bun_exe = if cfg!(target_os = "windows") {
        install_dir.join("bun/bun.exe")
    } else {
        install_dir.join("bun/bin/bun")
    };

    if !bun_exe.exists() {
        return (false, None);
    }

    let mut cmd = Command::new(&bun_exe);
    cmd.arg("--version");
    #[cfg(target_os = "windows")]
    cmd.creation_flags(CREATE_NO_WINDOW);

    if let Ok(output) = cmd.output() {
        if output.status.success() {
            let version = String::from_utf8_lossy(&output.stdout).trim().to_string();
            return (true, Some(version));
        }
    }
    (false, None)
}

/// Check if Python packages are installed in both venvs
pub(super) fn check_packages(install_dir: &PathBuf) -> bool {
    let venv1_python = if cfg!(target_os = "windows") {
        install_dir.join("venv-numpy1/Scripts/python.exe")
    } else {
        install_dir.join("venv-numpy1/bin/python3")
    };

    let venv2_python = if cfg!(target_os = "windows") {
        install_dir.join("venv-numpy2/Scripts/python.exe")
    } else {
        install_dir.join("venv-numpy2/bin/python3")
    };

    if !venv1_python.exists() || !venv2_python.exists() {
        eprintln!("[SETUP] Venvs don't exist - need to install packages");
        return false;
    }

    // Check numpy1 venv for key packages
    let mut cmd1 = Command::new(&venv1_python);
    cmd1.args(&["-c", "import numpy, pandas, vectorbt, backtesting, gluonts"]);

    #[cfg(target_os = "windows")]
    cmd1.creation_flags(CREATE_NO_WINDOW);
    let check1 = cmd1.output().map(|o| o.status.success()).unwrap_or(false);

    if !check1 {
        eprintln!("[SETUP] NumPy 1.x venv packages check failed - need to reinstall");
    }

    // Check numpy2 venv for key packages
    let mut cmd2 = Command::new(&venv2_python);
    cmd2.args(&["-c", "import numpy, pandas, yfinance, vnpy"]);

    #[cfg(target_os = "windows")]
    cmd2.creation_flags(CREATE_NO_WINDOW);
    let check2 = cmd2.output().map(|o| o.status.success()).unwrap_or(false);

    if !check2 {
        eprintln!("[SETUP] NumPy 2.x venv packages check failed - need to reinstall");
    }

    check1 && check2
}
