// Standalone test for Kraken WebSocket connection
// Run with: cargo run --example test_kraken_ws

use tokio_tungstenite::{connect_async, tungstenite::Message};
use futures_util::StreamExt;

const KRAKEN_WS_URL: &str = "wss://ws.kraken.com/v2";

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    println!("===========================================");
    println!("Testing Kraken WebSocket Connection");
    println!("===========================================");

    println!("\n[1] Connecting to: {}", KRAKEN_WS_URL);

    let (ws_stream, response) = connect_async(KRAKEN_WS_URL).await?;

    println!("✓ Connected successfully!");
    println!("   Status: {}", response.status());
    println!("   Headers: {:?}", response.headers());

    let (mut write, mut read) = ws_stream.split();

    println!("\n[2] Subscribing to BTC/USD ticker...");

    let subscribe_msg = serde_json::json!({
        "method": "subscribe",
        "params": {
            "channel": "ticker",
            "symbol": ["BTC/USD"]
        }
    });

    let msg_str = serde_json::to_string(&subscribe_msg)?;
    println!("   Sending: {}", msg_str);

    use futures_util::SinkExt;
    write.send(Message::Text(msg_str)).await?;

    println!("✓ Subscription message sent!");

    println!("\n[3] Waiting for messages (10 seconds)...");
    println!("   Press Ctrl+C to stop\n");

    let mut count = 0;
    let timeout = tokio::time::sleep(tokio::time::Duration::from_secs(10));
    tokio::pin!(timeout);

    loop {
        tokio::select! {
            _ = &mut timeout => {
                println!("\n⏱ 10 seconds elapsed, stopping...");
                break;
            }
            msg = read.next() => {
                match msg {
                    Some(Ok(Message::Text(text))) => {
                        count += 1;
                        println!("\n📨 Message {}: {}", count, &text[..text.len().min(200)]);

                        // Try to parse and show key info
                        if let Ok(json) = serde_json::from_str::<serde_json::Value>(&text) {
                            if let Some(channel) = json.get("channel") {
                                println!("   Channel: {}", channel);
                            }
                            if let Some(data) = json.get("data") {
                                println!("   Data: {}", serde_json::to_string_pretty(data)?);
                            }
                        }
                    }
                    Some(Ok(Message::Close(frame))) => {
                        println!("\n❌ Connection closed: {:?}", frame);
                        break;
                    }
                    Some(Err(e)) => {
                        println!("\n❌ Error: {}", e);
                        break;
                    }
                    None => {
                        println!("\n❌ Stream ended");
                        break;
                    }
                    _ => {}
                }
            }
        }
    }

    println!("\n===========================================");
    println!("Test Complete!");
    println!("Messages received: {}", count);
    println!("===========================================");

    Ok(())
}
