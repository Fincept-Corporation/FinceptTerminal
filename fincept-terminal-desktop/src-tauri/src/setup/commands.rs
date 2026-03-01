// setup/commands.rs - Public Tauri commands for setup management

use tauri::AppHandle;
use super::{SetupStatus, SETUP_RUNNING, get_install_dir, emit_progress};
use super::checks::{check_python, check_bun, check_packages};
use super::install::{install_python, install_bun, install_uv, create_venv, install_packages_in_venv};
use super::requirements::{check_requirements_sync, save_requirements_hash, compute_requirements_hash};

/// Check setup status
#[tauri::command]
pub fn check_setup_status(app: AppHandle) -> Result<SetupStatus, String> {
    let install_dir = get_install_dir(&app)?;

    let (python_installed, _) = check_python(&install_dir);
    let (bun_installed, _) = check_bun(&install_dir);
    let packages_installed = check_packages(&install_dir);

    let needs_setup = !python_installed || !bun_installed || !packages_installed;

    // Check if requirements files have changed (only relevant if base setup is complete)
    let (needs_sync, sync_message) = if !needs_setup {
        check_requirements_sync(&app)
    } else {
        (false, None)
    };

    // Initialize Python runtime in background if setup is complete and no sync needed
    if !needs_setup && !needs_sync && python_installed {
        let app_clone = app.clone();
        tauri::async_runtime::spawn(async move {
            if let Err(e) = crate::python::initialize(&app_clone).await {
                eprintln!("[Setup] Python initialization failed: {}", e);
            }
        });
    }

    Ok(SetupStatus {
        python_installed,
        bun_installed,
        packages_installed,
        needs_setup,
        needs_sync,
        sync_message,
    })
}

/// Run complete setup
#[tauri::command]
pub async fn run_setup(app: AppHandle) -> Result<String, String> {
    // Check if setup is already running
    {
        let mut lock = SETUP_RUNNING.lock().unwrap();
        if *lock {
            return Err("Setup already running".to_string());
        }
        *lock = true;
    } // Lock dropped here

    let result = async {
        let install_dir = get_install_dir(&app)?;
        std::fs::create_dir_all(&install_dir)
            .map_err(|e| format!("Failed to create install dir: {}", e))?;

        eprintln!("[SETUP] Install directory: {}", install_dir.display());
        emit_progress(&app, "init", 0, "Starting setup...", false);

        // Python
        let (python_installed, python_version) = check_python(&install_dir);
        if !python_installed {
            install_python(&app, &install_dir).await?;
        } else {
            eprintln!("[SETUP] Python already installed: {}", python_version.unwrap_or_default());
            emit_progress(&app, "python", 100, "Python already installed", false);
        }

        // Bun
        let (bun_installed, bun_version) = check_bun(&install_dir);
        if !bun_installed {
            install_bun(&app, &install_dir).await?;
        } else {
            eprintln!("[SETUP] Bun already installed: {}", bun_version.unwrap_or_default());
            emit_progress(&app, "bun", 100, "Bun already installed", false);
        }

        // UV (fast package manager)
        install_uv(&app, &install_dir).await?;

        // Create venvs and install packages
        if !check_packages(&install_dir) {
            // Create venv-numpy1 for NumPy 1.x libraries
            emit_progress(&app, "venv", 0, "Creating NumPy 1.x environment...", false);
            create_venv(&app, &install_dir, "venv-numpy1").await?;

            // Install numpy1 packages
            install_packages_in_venv(
                &app,
                &install_dir,
                "venv-numpy1",
                "requirements-numpy1.txt",
                "NumPy 1.x"
            ).await?;

            // Create venv-numpy2 for NumPy 2.x libraries
            emit_progress(&app, "venv", 50, "Creating NumPy 2.x environment...", false);
            create_venv(&app, &install_dir, "venv-numpy2").await?;

            // Install numpy2 packages
            install_packages_in_venv(
                &app,
                &install_dir,
                "venv-numpy2",
                "requirements-numpy2.txt",
                "NumPy 2.x"
            ).await?;
        } else {
            eprintln!("[SETUP] Packages already installed in both venvs");
            emit_progress(&app, "packages", 100, "Packages already installed", false);
        }

        // Save requirements hashes so future updates can detect changes
        if let Err(e) = save_requirements_hash(&app, "requirements-numpy1.txt", "requirements_numpy1_hash") {
            eprintln!("[SETUP] Warning: failed to save numpy1 requirements hash: {}", e);
        }
        if let Err(e) = save_requirements_hash(&app, "requirements-numpy2.txt", "requirements_numpy2_hash") {
            eprintln!("[SETUP] Warning: failed to save numpy2 requirements hash: {}", e);
        }

        emit_progress(&app, "complete", 100, "Setup complete!", false);

        // Initialize Python runtime
        if let Err(e) = crate::python::initialize(&app).await {
            eprintln!("[Setup] Python initialization warning: {}", e);
        }

        Ok("Setup complete".to_string())
    }.await;

    let mut lock = SETUP_RUNNING.lock().unwrap();
    *lock = false;

    result
}

/// Sync requirements: run UV install for any venv whose requirements changed.
/// Called automatically on app update when requirements files differ from stored hashes.
/// This is lightweight — UV only installs what's missing/changed.
#[tauri::command]
pub async fn sync_requirements(app: AppHandle) -> Result<String, String> {
    let install_dir = get_install_dir(&app)?;

    let files_and_venvs = [
        ("requirements-numpy1.txt", "requirements_numpy1_hash", "venv-numpy1", "NumPy 1.x"),
        ("requirements-numpy2.txt", "requirements_numpy2_hash", "venv-numpy2", "NumPy 2.x"),
    ];

    let mut synced_count = 0;

    for (req_file, setting_key, venv_name, label) in &files_and_venvs {
        let current_hash = match compute_requirements_hash(&app, req_file) {
            Ok(h) => h,
            Err(e) => {
                eprintln!("[SYNC] Could not compute hash for {}: {}", req_file, e);
                continue;
            }
        };

        let stored_hash = crate::database::operations::get_setting(setting_key)
            .ok()
            .flatten();

        let needs_update = match &stored_hash {
            Some(stored) => *stored != current_hash,
            None => true,
        };

        if needs_update {
            eprintln!("[SYNC] Syncing {} packages...", label);
            emit_progress(&app, "sync", synced_count * 50, &format!("Syncing {} packages...", label), false);

            // Ensure UV is available
            if let Err(e) = install_uv(&app, &install_dir).await {
                eprintln!("[SYNC] UV install warning: {}", e);
            }

            // Ensure venv exists; if not, create it
            let venv_python = if cfg!(target_os = "windows") {
                install_dir.join(format!("{}/Scripts/python.exe", venv_name))
            } else {
                install_dir.join(format!("{}/bin/python3", venv_name))
            };

            if !venv_python.exists() {
                eprintln!("[SYNC] Venv {} missing, creating...", venv_name);
                create_venv(&app, &install_dir, venv_name).await?;
            }

            // Run UV pip install (incremental — only installs what's missing/changed)
            install_packages_in_venv(&app, &install_dir, venv_name, req_file, label).await?;

            // Save the new hash on success
            save_requirements_hash(&app, req_file, setting_key)?;
            synced_count += 1;

            eprintln!("[SYNC] Successfully synced {} packages", label);
        }
    }

    emit_progress(&app, "sync", 100, "Package sync complete", false);

    // Initialize Python runtime after sync
    if let Err(e) = crate::python::initialize(&app).await {
        eprintln!("[SYNC] Python initialization warning after sync: {}", e);
    }

    Ok(format!("Synced {} requirement files", synced_count))
}
