# Adding a New Data Source

Quick guide to add a new data source from Python to Frontend.

## Step 1: Create Python Script

Location: `src-tauri/resources/scripts/your_source_data.py`

```python
import sys

def main():
    if len(sys.argv) < 2:
        print("Error: No command provided")
        return

    command = sys.argv[1]

    if command == "get_data":
        # Your data fetching logic
        result = fetch_some_data()
        print(result)  # Output to stdout

    elif command == "search":
        query = sys.argv[2] if len(sys.argv) > 2 else ""
        # Your search logic
        print(search_results)

if __name__ == "__main__":
    main()
```

**Key Points:**
- Accept commands via `sys.argv`
- Print results to stdout (JSON recommended)
- Handle errors gracefully

## Step 2: Create Rust Bridge

Location: `src-tauri/src/commands/your_source.rs`

```rust
use std::process::Command;
use crate::utils::python::{get_python_path, get_script_path};

/// Execute your data source Python script
#[tauri::command]
pub async fn execute_your_source_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    let python_path = get_python_path(&app)?;
    let script_path = get_script_path(&app, "your_source_data.py")?;

    if !script_path.exists() {
        return Err(format!("Script not found at: {}", script_path.display()));
    }

    let mut cmd_args = vec![script_path.to_string_lossy().to_string(), command];
    cmd_args.extend(args);

    let output = Command::new(&python_path)
        .args(&cmd_args)
        .output()
        .map_err(|e| format!("Failed to execute command: {}", e))?;

    if output.status.success() {
        Ok(String::from_utf8_lossy(&output.stdout).to_string())
    } else {
        Err(String::from_utf8_lossy(&output.stderr).to_string())
    }
}

/// Get specific data (wrapper for convenience)
#[tauri::command]
pub async fn get_your_source_data(
    app: tauri::AppHandle,
    param: String,
) -> Result<String, String> {
    execute_your_source_command(app, "get_data".to_string(), vec![param]).await
}
```

## Step 3: Register Module

**File: `src-tauri/src/commands/mod.rs`**

```rust
pub mod your_source;
```

## Step 4: Register Commands

**File: `src-tauri/src/lib.rs`**

Add to the `invoke_handler!` macro:

```rust
.invoke_handler(tauri::generate_handler![
    // ... other commands
    commands::your_source::execute_your_source_command,
    commands::your_source::get_your_source_data,
])
```

## Step 5: Call from Frontend

**React/TypeScript:**

```typescript
import { invoke } from '@tauri-apps/api/core';

// Generic command
const result = await invoke('execute_your_source_command', {
  command: 'get_data',
  args: ['param1', 'param2']
});

// Specific wrapper function
const data = await invoke('get_your_source_data', {
  param: 'some_value'
});

console.log(JSON.parse(result));
```

## Flow Diagram

```
Frontend (React)
    ↓ invoke('command_name', {args})
Rust Bridge (Tauri Command)
    ↓ Execute Python with args
Python Script (Data Source)
    ↓ Fetch data from API/DB
Python Script
    ↓ Print to stdout
Rust Bridge
    ↓ Capture output
Frontend
    ↓ Receive JSON result
```

## Tips

- **Return JSON** from Python for easy parsing
- **Error handling**: Python errors go to stderr, Rust catches them
- **Testing**: Test Python script standalone first
- **Compile**: Run `cargo check` after adding Rust files
- **Hot reload**: Frontend changes hot-reload, backend needs restart

## Example Full Flow

1. Python script fetches stock data from API
2. Rust command calls Python with symbol parameter
3. Python returns JSON: `{"price": 150.25, "volume": 1000000}`
4. Rust receives stdout and returns to frontend
5. React component displays the data

---

**All 42 data sources follow this exact pattern!**
