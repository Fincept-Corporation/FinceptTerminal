// RSS News Feed Fetcher - High-performance Rust implementation
// Fetches real-time news from 20+ financial RSS feeds without CORS restrictions
// Now supports user-configurable RSS feeds stored in SQLite

use reqwest;
use serde::{Deserialize, Serialize};
use std::time::{Duration, SystemTime, UNIX_EPOCH};
use tokio::time::timeout;
use crate::database::pool::get_db;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct NewsArticle {
    pub id: String,
    pub time: String,
    pub priority: String, // FLASH, URGENT, BREAKING, ROUTINE
    pub category: String,
    pub headline: String,
    pub summary: String,
    pub source: String,
    pub region: String,
    pub sentiment: String, // BULLISH, BEARISH, NEUTRAL
    pub impact: String,    // HIGH, MEDIUM, LOW
    pub tickers: Vec<String>,
    pub classification: String,
    pub link: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub pub_date: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RSSFeed {
    pub id: Option<String>,
    pub name: String,
    pub url: String,
    pub category: String,
    pub region: String,
    pub source: String,
    pub enabled: bool,
    pub is_default: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct UserRSSFeed {
    pub id: String,
    pub name: String,
    pub url: String,
    pub category: String,
    pub region: String,
    pub source_name: String,
    pub enabled: bool,
    pub is_default: bool,
    pub created_at: String,
    pub updated_at: String,
}

// Default feed preference info
#[derive(Debug, Clone)]
struct DefaultFeedPref {
    enabled: bool,
    deleted: bool,
}

// Get default feed preferences from database
fn get_default_feed_preferences() -> std::collections::HashMap<String, DefaultFeedPref> {
    let mut prefs = std::collections::HashMap::new();

    let conn = match get_db() {
        Ok(c) => c,
        Err(_) => return prefs,
    };

    let mut stmt = match conn.prepare(
        "SELECT feed_id, enabled, deleted FROM default_rss_feed_preferences"
    ) {
        Ok(s) => s,
        Err(_) => return prefs,
    };

    if let Ok(rows) = stmt.query_map([], |row| {
        Ok((
            row.get::<_, String>(0)?,
            DefaultFeedPref {
                enabled: row.get::<_, i32>(1)? == 1,
                deleted: row.get::<_, i32>(2).unwrap_or(0) == 1,
            }
        ))
    }) {
        for row in rows.flatten() {
            prefs.insert(row.0, row.1);
        }
    }

    prefs
}

// Default RSS feeds (built-in, always available)
fn get_default_rss_feeds() -> Vec<RSSFeed> {
    let prefs = get_default_feed_preferences();

    let default_feeds = vec![
        ("default-yahoo", "Yahoo Finance", "https://finance.yahoo.com/news/rssindex", "MARKETS", "US", "YAHOO"),
        ("default-investing", "Investing.com", "https://www.investing.com/rss/news.rss", "MARKETS", "GLOBAL", "INVESTING.COM"),
        ("default-coindesk", "CoinDesk", "https://www.coindesk.com/arc/outboundfeeds/rss/", "CRYPTO", "GLOBAL", "COINDESK"),
        ("default-cointelegraph", "CoinTelegraph", "https://cointelegraph.com/rss", "CRYPTO", "GLOBAL", "COINTELEGRAPH"),
        ("default-decrypt", "Decrypt", "https://decrypt.co/feed", "CRYPTO", "GLOBAL", "DECRYPT"),
        ("default-techcrunch", "TechCrunch", "https://techcrunch.com/feed/", "TECH", "US", "TECHCRUNCH"),
        ("default-verge", "The Verge", "https://www.theverge.com/rss/index.xml", "TECH", "US", "THE VERGE"),
        ("default-arstechnica", "Ars Technica", "https://feeds.arstechnica.com/arstechnica/index", "TECH", "US", "ARS TECHNICA"),
        ("default-oilprice", "Oil Price", "https://oilprice.com/rss/main", "ENERGY", "GLOBAL", "OILPRICE"),
        ("default-sec", "SEC Press Releases", "https://www.sec.gov/news/pressreleases.rss", "REGULATORY", "US", "SEC"),
    ];

    default_feeds.into_iter()
        .filter_map(|(id, name, url, category, region, source)| {
            let pref = prefs.get(id);
            // If deleted, don't include in the list
            if pref.map(|p| p.deleted).unwrap_or(false) {
                return None;
            }
            let enabled = pref.map(|p| p.enabled).unwrap_or(true);
            Some(RSSFeed {
                id: Some(id.to_string()),
                name: name.to_string(),
                url: url.to_string(),
                category: category.to_string(),
                region: region.to_string(),
                source: source.to_string(),
                enabled,
                is_default: true,
            })
        }).collect()
}

// Get user RSS feeds from database
fn get_user_rss_feeds_from_db() -> Vec<RSSFeed> {
    let conn = match get_db() {
        Ok(c) => c,
        Err(e) => {
            eprintln!("[News] Failed to get database connection: {}", e);
            return vec![];
        }
    };

    let mut stmt = match conn.prepare(
        "SELECT id, name, url, category, region, source_name, enabled, is_default
         FROM user_rss_feeds WHERE enabled = 1"
    ) {
        Ok(s) => s,
        Err(e) => {
            eprintln!("[News] Failed to prepare statement: {}", e);
            return vec![];
        }
    };

    let feeds_result = stmt.query_map([], |row| {
        Ok(RSSFeed {
            id: Some(row.get::<_, String>(0)?),
            name: row.get::<_, String>(1)?,
            url: row.get::<_, String>(2)?,
            category: row.get::<_, String>(3)?,
            region: row.get::<_, String>(4)?,
            source: row.get::<_, String>(5)?,
            enabled: row.get::<_, i32>(6)? == 1,
            is_default: row.get::<_, i32>(7)? == 1,
        })
    });

    match feeds_result {
        Ok(rows) => rows.filter_map(|r| r.ok()).collect(),
        Err(e) => {
            eprintln!("[News] Query failed: {}", e);
            vec![]
        }
    }
}

// Get all RSS feeds (default + user-configured)
// Get all RSS feeds (default + user-configured) - only enabled ones for fetching
fn get_rss_feeds() -> Vec<RSSFeed> {
    let mut all_feeds: Vec<RSSFeed> = get_default_rss_feeds()
        .into_iter()
        .filter(|f| f.enabled)
        .collect();
    let user_feeds: Vec<RSSFeed> = get_user_rss_feeds_from_db()
        .into_iter()
        .filter(|f| f.enabled)
        .collect();
    all_feeds.extend(user_feeds);
    all_feeds
}

// Parse RSS XML to articles
fn parse_rss_feed(xml_text: &str, feed: &RSSFeed) -> Vec<NewsArticle> {
    let mut articles = Vec::new();

    // Use quick-xml for fast parsing
    use quick_xml::events::Event;
    use quick_xml::Reader;

    let mut reader = Reader::from_str(xml_text);
    reader.config_mut().trim_text(true);

    let mut current_item: Option<NewsArticle> = None;
    let mut current_tag = String::new();
    let mut buf = Vec::new();

    loop {
        match reader.read_event_into(&mut buf) {
            Ok(Event::Start(e)) => {
                let tag_name = String::from_utf8_lossy(e.name().as_ref()).to_string();
                current_tag = tag_name.clone();

                // Start of new item/entry
                if tag_name == "item" || tag_name == "entry" {
                    let timestamp = SystemTime::now()
                        .duration_since(UNIX_EPOCH)
                        .unwrap()
                        .as_millis();
                    current_item = Some(NewsArticle {
                        id: format!("{}-{}", feed.source, timestamp),
                        time: String::new(),
                        priority: "ROUTINE".to_string(),
                        category: feed.category.clone(),
                        headline: String::new(),
                        summary: String::new(),
                        source: feed.source.clone(),
                        region: feed.region.clone(),
                        sentiment: "NEUTRAL".to_string(),
                        impact: "LOW".to_string(),
                        tickers: Vec::new(),
                        classification: "PUBLIC".to_string(),
                        link: None,
                        pub_date: None,
                    });
                }
            }
            Ok(Event::Text(e)) => {
                if let Some(ref mut item) = current_item {
                    let text = e.unescape().unwrap_or_default().to_string();

                    match current_tag.as_str() {
                        "title" => {
                            if item.headline.is_empty() {
                                item.headline = text.chars().take(200).collect();
                            }
                        }
                        "description" | "summary" | "content:encoded" => {
                            if item.summary.is_empty() {
                                // Strip HTML tags
                                let stripped = strip_html_tags(&text);
                                item.summary = stripped.chars().take(300).collect();
                            }
                        }
                        "link" => {
                            if item.link.is_none() {
                                item.link = Some(text);
                            }
                        }
                        "pubDate" | "published" | "updated" => {
                            if item.pub_date.is_none() {
                                item.pub_date = Some(text.clone());
                                // Extract time
                                if let Ok(dt) = chrono::DateTime::parse_from_rfc2822(&text) {
                                    item.time = dt.format("%H:%M:%S").to_string();
                                } else if let Ok(dt) = chrono::DateTime::parse_from_rfc3339(&text) {
                                    item.time = dt.format("%H:%M:%S").to_string();
                                }
                            }
                        }
                        _ => {}
                    }
                }
            }
            Ok(Event::End(e)) => {
                let tag_name = String::from_utf8_lossy(e.name().as_ref()).to_string();

                // End of item/entry - process and add to articles
                if (tag_name == "item" || tag_name == "entry") && current_item.is_some() {
                    let mut item = current_item.take().unwrap();

                    // Skip items without headline
                    if item.headline.is_empty() {
                        continue;
                    }

                    // Set default time if not parsed
                    if item.time.is_empty() {
                        item.time = chrono::Local::now().format("%H:%M:%S").to_string();
                    }

                    // Analyze and enrich the article
                    enrich_article(&mut item);

                    articles.push(item);
                }
            }
            Ok(Event::Eof) => break,
            Err(_) => break,
            _ => {}
        }
        buf.clear();
    }

    articles
}

// Strip HTML tags from text
fn strip_html_tags(html: &str) -> String {
    let re = regex::Regex::new(r"<[^>]*>").unwrap();
    re.replace_all(html, "").to_string()
}

// Enrich article with sentiment, priority, tickers, etc.
fn enrich_article(article: &mut NewsArticle) {
    let text_lower = format!("{} {}", article.headline, article.summary).to_lowercase();

    // Determine priority based on keywords
    if text_lower.contains("breaking") || text_lower.contains("alert") {
        article.priority = "FLASH".to_string();
    } else if text_lower.contains("urgent") || text_lower.contains("emergency") {
        article.priority = "URGENT".to_string();
    } else if text_lower.contains("announce") || text_lower.contains("report") {
        article.priority = "BREAKING".to_string();
    }

    // Comprehensive sentiment analysis
    let bullish_words = vec![
        "surge", "rally", "gain", "rise", "jump", "soar", "climb", "spike", "advance", "rebound",
        "boost", "beat", "exceed", "outpace", "tops", "strong", "stellar", "robust", "record",
        "growth", "expand", "upgrade", "buy", "profit", "optimistic", "positive", "breakthrough",
        "success", "win", "approval", "deal", "partnership", "dividend increase", "confidence",
    ];

    let bearish_words = vec![
        "fall", "drop", "decline", "plunge", "crash", "tumble", "slide", "sink", "slump", "dip",
        "tank", "collapse", "miss", "disappoint", "fail", "worst", "lowest", "poor", "weak",
        "loss", "deficit", "trouble", "concern", "worry", "fear", "risk", "threat", "warning",
        "layoff", "cut", "downgrade", "sell", "bankruptcy", "debt", "lawsuit", "scandal",
        "recession", "inflation", "slowdown", "unemployment", "bearish", "negative",
    ];

    let bullish_count = bullish_words.iter().filter(|w| text_lower.contains(*w)).count();
    let bearish_count = bearish_words.iter().filter(|w| text_lower.contains(*w)).count();

    if bullish_count > bearish_count && bullish_count > 0 {
        article.sentiment = "BULLISH".to_string();
    } else if bearish_count > bullish_count && bearish_count > 0 {
        article.sentiment = "BEARISH".to_string();
    }

    // Determine impact
    if article.priority == "FLASH" || article.priority == "URGENT" {
        article.impact = "HIGH".to_string();
    } else if article.priority == "BREAKING" {
        article.impact = "MEDIUM".to_string();
    }

    // Category refinement
    if text_lower.contains("earnings") || text_lower.contains("profit") || text_lower.contains("revenue") {
        article.category = "EARNINGS".to_string();
    } else if text_lower.contains("fed") || text_lower.contains("inflation") || text_lower.contains("gdp") || text_lower.contains("jobs") {
        article.category = "ECONOMIC".to_string();
    } else if text_lower.contains("tech") || text_lower.contains("ai") || text_lower.contains("software") {
        article.category = "TECH".to_string();
    } else if text_lower.contains("energy") || text_lower.contains("oil") || text_lower.contains("gas") {
        article.category = "ENERGY".to_string();
    } else if text_lower.contains("bank") || text_lower.contains("financial") {
        article.category = "BANKING".to_string();
    } else if text_lower.contains("crypto") || text_lower.contains("bitcoin") || text_lower.contains("ethereum") {
        article.category = "CRYPTO".to_string();
    } else if text_lower.contains("china") || text_lower.contains("russia") || text_lower.contains("ukraine") || text_lower.contains("war") {
        article.category = "GEOPOLITICS".to_string();
    }

    // Extract tickers (simple regex for uppercase 1-5 letter words)
    let ticker_re = regex::Regex::new(r"\b[A-Z]{1,5}\b").unwrap();
    let combined_text = format!("{} {}", article.headline, article.summary);
    let tickers: Vec<String> = ticker_re
        .find_iter(&combined_text)
        .map(|m| m.as_str().to_string())
        .collect::<std::collections::HashSet<_>>()
        .into_iter()
        .take(5)
        .collect();
    article.tickers = tickers;
}

// Fetch single RSS feed with timeout
async fn fetch_rss_feed(feed: RSSFeed, client: &reqwest::Client) -> Vec<NewsArticle> {
    // 10 second timeout per feed
    let fetch_future = client
        .get(&feed.url)
        .header("Accept", "application/rss+xml, application/xml, text/xml, */*")
        .header("User-Agent", "FinceptTerminal/3.0")
        .send();

    match timeout(Duration::from_secs(10), fetch_future).await {
        Ok(Ok(response)) => {
            if response.status().is_success() {
                match response.text().await {
                    Ok(xml_text) => {
                        if xml_text.trim().starts_with('<') {
                            return parse_rss_feed(&xml_text, &feed);
                        }
                    }
                    Err(_) => {}
                }
            }
        }
        _ => {}
    }

    Vec::new()
}

// Fetch all RSS feeds in parallel
#[tauri::command]
pub async fn fetch_all_rss_news() -> Result<Vec<NewsArticle>, String> {
    let feeds = get_rss_feeds();
    let client = reqwest::Client::builder()
        .timeout(Duration::from_secs(10))
        .build()
        .map_err(|e| format!("Failed to create HTTP client: {}", e))?;

    // Fetch all feeds in parallel
    let mut tasks = Vec::new();
    for feed in feeds {
        let client_clone = client.clone();
        tasks.push(tokio::spawn(async move {
            fetch_rss_feed(feed, &client_clone).await
        }));
    }

    let mut all_articles = Vec::new();
    for task in tasks {
        if let Ok(articles) = task.await {
            all_articles.extend(articles);
        }
    }

    // Sort by publication date (newest first)
    all_articles.sort_by(|a, b| {
        let date_a = a.pub_date.as_ref().and_then(|d| {
            chrono::DateTime::parse_from_rfc2822(d)
                .or_else(|_| chrono::DateTime::parse_from_rfc3339(d))
                .ok()
        });
        let date_b = b.pub_date.as_ref().and_then(|d| {
            chrono::DateTime::parse_from_rfc2822(d)
                .or_else(|_| chrono::DateTime::parse_from_rfc3339(d))
                .ok()
        });

        match (date_b, date_a) {
            (Some(db), Some(da)) => db.cmp(&da),
            (Some(_), None) => std::cmp::Ordering::Less,
            (None, Some(_)) => std::cmp::Ordering::Greater,
            (None, None) => std::cmp::Ordering::Equal,
        }
    });

    Ok(all_articles)
}

// Get RSS feed count
#[tauri::command]
pub fn get_rss_feed_count() -> usize {
    get_rss_feeds().len()
}

// Get active source names
#[tauri::command]
pub fn get_active_sources() -> Vec<String> {
    get_rss_feeds().into_iter().map(|f| f.source).collect()
}

// ============================================================================
// User RSS Feed CRUD Operations
// ============================================================================

/// Get all RSS feeds (default + user-configured)
#[tauri::command]
pub fn get_all_rss_feeds() -> Result<Vec<RSSFeed>, String> {
    Ok(get_rss_feeds())
}

/// Get only default RSS feeds
#[tauri::command]
pub fn get_default_feeds() -> Result<Vec<RSSFeed>, String> {
    Ok(get_default_rss_feeds())
}

/// Get only user-configured RSS feeds from database
#[tauri::command]
pub fn get_user_rss_feeds() -> Result<Vec<UserRSSFeed>, String> {
    let conn = get_db().map_err(|e| format!("Database error: {}", e))?;

    let mut stmt = conn.prepare(
        "SELECT id, name, url, category, region, source_name, enabled, is_default, created_at, updated_at
         FROM user_rss_feeds ORDER BY created_at DESC"
    ).map_err(|e| format!("Failed to prepare statement: {}", e))?;

    let feeds: Vec<UserRSSFeed> = stmt
        .query_map([], |row| {
            Ok(UserRSSFeed {
                id: row.get(0)?,
                name: row.get(1)?,
                url: row.get(2)?,
                category: row.get(3)?,
                region: row.get(4)?,
                source_name: row.get(5)?,
                enabled: row.get::<_, i32>(6)? == 1,
                is_default: row.get::<_, i32>(7)? == 1,
                created_at: row.get(8)?,
                updated_at: row.get(9)?,
            })
        })
        .map_err(|e| format!("Query failed: {}", e))?
        .filter_map(|r| r.ok())
        .collect();

    Ok(feeds)
}

/// Add a new user RSS feed
#[tauri::command]
pub fn add_user_rss_feed(
    name: String,
    url: String,
    category: String,
    region: String,
    source_name: String,
) -> Result<UserRSSFeed, String> {
    let conn = get_db().map_err(|e| format!("Database error: {}", e))?;

    // Generate unique ID
    let id = format!("user-{}", uuid::Uuid::new_v4().to_string());
    let now = chrono::Utc::now().to_rfc3339();

    conn.execute(
        "INSERT INTO user_rss_feeds (id, name, url, category, region, source_name, enabled, is_default, created_at, updated_at)
         VALUES (?1, ?2, ?3, ?4, ?5, ?6, 1, 0, ?7, ?7)",
        rusqlite::params![id, name, url, category, region, source_name, now],
    ).map_err(|e| format!("Failed to insert feed: {}", e))?;

    Ok(UserRSSFeed {
        id,
        name,
        url,
        category,
        region,
        source_name,
        enabled: true,
        is_default: false,
        created_at: now.clone(),
        updated_at: now,
    })
}

/// Update an existing user RSS feed
#[tauri::command]
pub fn update_user_rss_feed(
    id: String,
    name: String,
    url: String,
    category: String,
    region: String,
    source_name: String,
    enabled: bool,
) -> Result<(), String> {
    let conn = get_db().map_err(|e| format!("Database error: {}", e))?;
    let now = chrono::Utc::now().to_rfc3339();

    let rows_updated = conn.execute(
        "UPDATE user_rss_feeds SET name = ?1, url = ?2, category = ?3, region = ?4, source_name = ?5, enabled = ?6, updated_at = ?7
         WHERE id = ?8",
        rusqlite::params![name, url, category, region, source_name, if enabled { 1 } else { 0 }, now, id],
    ).map_err(|e| format!("Failed to update feed: {}", e))?;

    if rows_updated == 0 {
        return Err("Feed not found".to_string());
    }

    Ok(())
}

/// Delete a user RSS feed
#[tauri::command]
pub fn delete_user_rss_feed(id: String) -> Result<(), String> {
    let conn = get_db().map_err(|e| format!("Database error: {}", e))?;

    let rows_deleted = conn.execute(
        "DELETE FROM user_rss_feeds WHERE id = ?1",
        rusqlite::params![id],
    ).map_err(|e| format!("Failed to delete feed: {}", e))?;

    if rows_deleted == 0 {
        return Err("Feed not found".to_string());
    }

    Ok(())
}

/// Toggle enabled status of a user RSS feed
#[tauri::command]
pub fn toggle_user_rss_feed(id: String, enabled: bool) -> Result<(), String> {
    let conn = get_db().map_err(|e| format!("Database error: {}", e))?;
    let now = chrono::Utc::now().to_rfc3339();

    let rows_updated = conn.execute(
        "UPDATE user_rss_feeds SET enabled = ?1, updated_at = ?2 WHERE id = ?3",
        rusqlite::params![if enabled { 1 } else { 0 }, now, id],
    ).map_err(|e| format!("Failed to toggle feed: {}", e))?;

    if rows_updated == 0 {
        return Err("Feed not found".to_string());
    }

    Ok(())
}

/// Test if an RSS feed URL is valid and accessible
#[tauri::command]
pub async fn test_rss_feed_url(url: String) -> Result<bool, String> {
    let client = reqwest::Client::builder()
        .timeout(Duration::from_secs(10))
        .build()
        .map_err(|e| format!("Failed to create HTTP client: {}", e))?;

    let response = client
        .get(&url)
        .header("Accept", "application/rss+xml, application/xml, text/xml, */*")
        .header("User-Agent", "FinceptTerminal/3.0")
        .send()
        .await
        .map_err(|e| format!("Failed to fetch URL: {}", e))?;

    if !response.status().is_success() {
        return Ok(false);
    }

    let text = response.text().await.map_err(|e| format!("Failed to read response: {}", e))?;

    // Check if it looks like valid RSS/XML
    let is_valid = text.trim().starts_with('<') &&
        (text.contains("<rss") || text.contains("<feed") || text.contains("<channel"));

    Ok(is_valid)
}

/// Toggle enabled status of a default RSS feed
#[tauri::command]
pub fn toggle_default_rss_feed(feed_id: String, enabled: bool) -> Result<(), String> {
    let conn = get_db().map_err(|e| format!("Database error: {}", e))?;
    let now = chrono::Utc::now().to_rfc3339();

    // Use INSERT OR REPLACE to handle both new and existing preferences
    conn.execute(
        "INSERT OR REPLACE INTO default_rss_feed_preferences (feed_id, enabled, updated_at)
         VALUES (?1, ?2, ?3)",
        rusqlite::params![feed_id, if enabled { 1 } else { 0 }, now],
    ).map_err(|e| format!("Failed to toggle default feed: {}", e))?;

    Ok(())
}

/// Get all default feed preferences
#[tauri::command]
pub fn get_default_feed_prefs() -> Result<std::collections::HashMap<String, bool>, String> {
    let prefs = get_default_feed_preferences();
    Ok(prefs.into_iter().map(|(k, v)| (k, v.enabled)).collect())
}

/// Delete (hide) a default RSS feed
#[tauri::command]
pub fn delete_default_rss_feed(feed_id: String) -> Result<(), String> {
    let conn = get_db().map_err(|e| format!("Database error: {}", e))?;
    let now = chrono::Utc::now().to_rfc3339();

    conn.execute(
        "INSERT OR REPLACE INTO default_rss_feed_preferences (feed_id, enabled, deleted, updated_at)
         VALUES (?1, 0, 1, ?2)",
        rusqlite::params![feed_id, now],
    ).map_err(|e| format!("Failed to delete default feed: {}", e))?;

    Ok(())
}

/// Restore a deleted default RSS feed
#[tauri::command]
pub fn restore_default_rss_feed(feed_id: String) -> Result<(), String> {
    let conn = get_db().map_err(|e| format!("Database error: {}", e))?;
    let now = chrono::Utc::now().to_rfc3339();

    conn.execute(
        "INSERT OR REPLACE INTO default_rss_feed_preferences (feed_id, enabled, deleted, updated_at)
         VALUES (?1, 1, 0, ?2)",
        rusqlite::params![feed_id, now],
    ).map_err(|e| format!("Failed to restore default feed: {}", e))?;

    Ok(())
}

/// Get list of deleted default feed IDs
#[tauri::command]
pub fn get_deleted_default_feeds() -> Result<Vec<String>, String> {
    let conn = get_db().map_err(|e| format!("Database error: {}", e))?;

    let mut stmt = conn.prepare(
        "SELECT feed_id FROM default_rss_feed_preferences WHERE deleted = 1"
    ).map_err(|e| format!("Failed to prepare statement: {}", e))?;

    let feeds: Vec<String> = stmt
        .query_map([], |row| row.get(0))
        .map_err(|e| format!("Query failed: {}", e))?
        .filter_map(|r| r.ok())
        .collect();

    Ok(feeds)
}

/// Restore all deleted default feeds
#[tauri::command]
pub fn restore_all_default_feeds() -> Result<(), String> {
    let conn = get_db().map_err(|e| format!("Database error: {}", e))?;

    conn.execute(
        "DELETE FROM default_rss_feed_preferences",
        [],
    ).map_err(|e| format!("Failed to restore all feeds: {}", e))?;

    Ok(())
}
