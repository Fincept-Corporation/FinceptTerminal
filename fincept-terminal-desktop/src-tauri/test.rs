use std::process::Command;

fn main() {
    println!("🧪 Quick testing yfinance Python backend...");
    
    // Test the Python script directly first
    let output = Command::new("python")
        .arg("../python_backend/yfinance_backend.py")
        .arg("AAPL")
        .arg("1mo")
        .output()
        .expect("Failed to execute Python script");

    if output.status.success() {
        let result = String::from_utf8_lossy(&output.stdout);
        println!("✅ Python script works!");
        println!("Output: {}", result);
    } else {
        let error = String::from_utf8_lossy(&output.stderr);
        println!("❌ Python script failed: {}", error);
    }
}