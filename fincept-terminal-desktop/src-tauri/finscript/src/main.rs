use std::io::Read;

fn main() {
    let code = if let Some(file_path) = std::env::args().nth(1) {
        // Read from file
        match std::fs::read_to_string(&file_path) {
            Ok(content) => content,
            Err(e) => {
                let err = finscript::FinScriptResult {
                    success: false,
                    output: String::new(),
                    signals: vec![],
                    plots: vec![],
                    errors: vec![format!("Failed to read file '{}': {}", file_path, e)],
                    alerts: vec![],
                    drawings: vec![],
                    execution_time_ms: 0,
                };
                println!("{}", serde_json::to_string(&err).unwrap());
                return;
            }
        }
    } else {
        // Read from stdin
        let mut input = String::new();
        std::io::stdin().read_to_string(&mut input).unwrap_or_default();
        input
    };

    let result = finscript::execute(&code);
    println!("{}", serde_json::to_string(&result).unwrap());
}
