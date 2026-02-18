// RSS News Feed Fetcher - High-performance Rust implementation
// Fetches real-time news from 20+ financial RSS feeds without CORS restrictions
// Now supports user-configurable RSS feeds stored in SQLite

use reqwest;
use serde::{Deserialize, Serialize};
use std::time::{Duration, SystemTime, UNIX_EPOCH};
use tokio::time::timeout;
use chrono::{DateTime, FixedOffset, NaiveDateTime, Utc, TimeZone};
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
    /// Unix timestamp (seconds) for reliable cross-feed sorting
    #[serde(default)]
    pub sort_ts: i64,
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
        ("default-nytimes-world", "NYT World News", "https://rss.nytimes.com/services/xml/rss/nyt/World.xml", "GEOPOLITICS", "GLOBAL", "NYT"),
        ("default-cnbc-finance", "CNBC Finance", "https://search.cnbc.com/rs/search/combinedcms/view.xml?partnerId=wrss01&id=100003114", "MARKETS", "US", "CNBC"),
        ("default-cnbc-world", "CNBC World", "https://search.cnbc.com/rs/search/combinedcms/view.xml?partnerId=wrss01&id=100727362", "MARKETS", "GLOBAL", "CNBC"),
        ("default-diplomat", "The Diplomat", "https://thediplomat.com/feed/", "GEOPOLITICS", "ASIA", "THE DIPLOMAT"),
        ("default-spglobal-oil", "S&P Global Oil & Crude", "https://www.spglobal.com/content/spglobal/energy/us/en/rss/oil-crude.xml", "ENERGY", "GLOBAL", "S&P GLOBAL"),
        // Regulatory feeds
        ("default-sec-press", "SEC Press Releases", "https://www.sec.gov/news/pressreleases.rss", "REGULATORY", "US", "SEC"),
        ("default-fed-press", "Federal Reserve Press", "https://www.federalreserve.gov/feeds/press_all.xml", "REGULATORY", "US", "FEDERAL RESERVE"),
        // Crypto feeds
        ("default-coindesk", "CoinDesk", "https://www.coindesk.com/arc/outboundfeeds/rss/", "CRYPTO", "GLOBAL", "COINDESK"),
        ("default-cointelegraph", "Cointelegraph", "https://cointelegraph.com/rss", "CRYPTO", "GLOBAL", "COINTELEGRAPH"),
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

/// Try to parse a date string into a DateTime using multiple common RSS date formats.
fn parse_date_flexible(date_str: &str) -> Option<DateTime<FixedOffset>> {
    let s = date_str.trim();

    // RFC 2822  (most RSS feeds)
    if let Ok(dt) = DateTime::parse_from_rfc2822(s) {
        return Some(dt);
    }
    // RFC 3339 / ISO 8601 with offset  (Atom feeds)
    if let Ok(dt) = DateTime::parse_from_rfc3339(s) {
        return Some(dt);
    }

    // Common variants that chrono's strict parsers miss
    let extra_formats = [
        "%Y-%m-%dT%H:%M:%S%.f%:z",   // 2026-01-27T16:45:00.000+00:00
        "%Y-%m-%dT%H:%M:%S%:z",       // 2026-01-27T16:45:00+00:00
        "%Y-%m-%dT%H:%M:%SZ",         // 2026-01-27T16:45:00Z  (no offset)
        "%Y-%m-%dT%H:%M:%S%.fZ",      // 2026-01-27T16:45:00.000Z
        "%a, %d %b %Y %H:%M:%S %Z",   // Tue, 27 Jan 2026 16:45:00 GMT
        "%d %b %Y %H:%M:%S %z",       // 27 Jan 2026 16:45:00 +0000
        "%Y-%m-%d %H:%M:%S",          // 2026-01-27 16:45:00 (naive)
        "%B %d, %Y %H:%M:%S",         // January 27, 2026 16:45:00
        "%b %d, %Y %H:%M:%S",         // Jan 27, 2026 16:45:00
    ];

    for fmt in &extra_formats {
        // Try with offset
        if let Ok(dt) = DateTime::parse_from_str(s, fmt) {
            return Some(dt);
        }
        // Try as naive (assume UTC)
        if let Ok(naive) = NaiveDateTime::parse_from_str(s, fmt) {
            return Some(Utc.from_utc_datetime(&naive).fixed_offset());
        }
    }

    None
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
    let mut item_index: u32 = 0;

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
                    item_index += 1;
                    let feed_id = feed.id.as_deref().unwrap_or(&feed.source);
                    current_item = Some(NewsArticle {
                        id: format!("{}-{}-{}", feed_id, timestamp, item_index),
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
                        sort_ts: 0,
                    });
                }
            }
            Ok(Event::Text(e)) => {
                if let Some(ref mut item) = current_item {
                    let text = e.unescape().unwrap_or_default().trim().to_string();
                    if text.is_empty() {
                        // skip empty text nodes
                    } else {
                        // Strip namespace prefix for tag matching (e.g. "dc:date" -> "dc:date", "content:encoded" stays)
                        let tag = current_tag.as_str();
                        match tag {
                            "title" => {
                                if item.headline.is_empty() {
                                    item.headline = text.chars().take(200).collect();
                                }
                            }
                            "description" | "summary" | "content:encoded" => {
                                if item.summary.is_empty() {
                                    let stripped = strip_html_tags(&text);
                                    item.summary = stripped.chars().take(300).collect();
                                }
                            }
                            "link" => {
                                if item.link.is_none() {
                                    item.link = Some(text);
                                }
                            }
                            "pubDate" | "published" | "updated" | "dc:date" | "date" => {
                                if item.pub_date.is_none() {
                                    // Trim aggressively — some feeds have \n or extra spaces
                                    let clean = text.replace('\n', " ").trim().to_string();
                                    item.pub_date = Some(clean.clone());
                                    if let Some(dt) = parse_date_flexible(&clean) {
                                        let ts = dt.timestamp();
                                        item.sort_ts = ts;
                                        item.time = dt.format("%b %d, %H:%M").to_string();
                                    } else {
                                        // Fallback: show raw date string truncated
                                        item.time = clean.chars().take(22).collect();
                                    }
                                }
                            }
                            _ => {}
                        }
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

                    // Set default time if no date tag was found at all
                    if item.time.is_empty() {
                        item.time = chrono::Local::now().format("%b %d, %H:%M").to_string();
                    }
                    // Assign current timestamp if no pub_date was parsed (so it still sorts)
                    if item.sort_ts == 0 {
                        item.sort_ts = Utc::now().timestamp();
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

    // Weighted sentiment analysis — covers finance, geopolitics, and general news
    // (word, weight): higher weight = stronger signal
    let positive_words: Vec<(&str, i32)> = vec![
        // Strong positive (weight 3)
        ("surge", 3), ("soar", 3), ("skyrocket", 3), ("breakthrough", 3), ("boom", 3),
        ("landslide victory", 3), ("historic deal", 3), ("record high", 3), ("blockbuster", 3),
        // Medium positive (weight 2)
        ("rally", 2), ("gain", 2), ("rise", 2), ("jump", 2), ("climb", 2), ("spike", 2),
        ("advance", 2), ("rebound", 2), ("boost", 2), ("beat", 2), ("exceed", 2),
        ("outpace", 2), ("tops", 2), ("upgrade", 2), ("profit", 2), ("growth", 2),
        ("expand", 2), ("recover", 2), ("victory", 2), ("triumph", 2), ("liberate", 2),
        ("peace deal", 2), ("ceasefire", 2), ("treaty", 2), ("alliance", 2), ("reform", 2),
        ("innovate", 2), ("milestone", 2), ("optimism", 2),
        // Mild positive (weight 1)
        ("strong", 1), ("robust", 1), ("stellar", 1), ("record", 1), ("buy", 1),
        ("optimistic", 1), ("positive", 1), ("success", 1), ("win", 1), ("approval", 1),
        ("deal", 1), ("partnership", 1), ("confidence", 1), ("dividend", 1), ("agree", 1),
        ("support", 1), ("progress", 1), ("improve", 1), ("hope", 1), ("promise", 1),
        ("cooperat", 1), ("stabil", 1), ("prosper", 1), ("thrive", 1), ("upbeat", 1),
        ("welcome", 1), ("resolve", 1), ("aid", 1), ("relief", 1), ("ease", 1),
        ("secure", 1), ("bolster", 1), ("strengthen", 1), ("uplift", 1), ("endorse", 1),
        ("acclaim", 1), ("celebrate", 1), ("achieve", 1), ("launch", 1), ("unveil", 1),
        ("recommend", 1), ("embrace", 1), ("momentum", 1), ("recover", 1), ("upturn", 1),
        ("outperform", 1), ("bullish", 1), ("upside", 1), ("favorable", 1),
    ];

    let negative_words: Vec<(&str, i32)> = vec![
        // Strong negative (weight 3)
        ("crash", 3), ("plunge", 3), ("collapse", 3), ("devastat", 3), ("catastroph", 3),
        ("massacre", 3), ("genocide", 3), ("invasion", 3), ("war crime", 3), ("nuclear", 3),
        ("pandemic", 3), ("bankruptcy", 3), ("meltdown", 3), ("freefall", 3),
        // Medium negative (weight 2)
        ("fall", 2), ("drop", 2), ("decline", 2), ("tumble", 2), ("slide", 2), ("sink", 2),
        ("slump", 2), ("tank", 2), ("miss", 2), ("disappoint", 2), ("fail", 2),
        ("recession", 2), ("crisis", 2), ("conflict", 2), ("attack", 2), ("strike", 2),
        ("bomb", 2), ("kill", 2), ("dead", 2), ("death", 2), ("casualt", 2),
        ("sanction", 2), ("tariff", 2), ("embargo", 2), ("escalat", 2), ("tension", 2),
        ("layoff", 2), ("downgrade", 2), ("default", 2), ("fraud", 2), ("indict", 2),
        ("scandal", 2), ("resign", 2), ("impeach", 2), ("coup", 2), ("revolt", 2),
        ("protest", 2), ("riot", 2), ("flee", 2), ("refugee", 2), ("disaster", 2),
        // Mild negative (weight 1)
        ("dip", 1), ("worst", 1), ("lowest", 1), ("poor", 1), ("weak", 1), ("loss", 1),
        ("deficit", 1), ("trouble", 1), ("concern", 1), ("worry", 1), ("fear", 1),
        ("risk", 1), ("threat", 1), ("warning", 1), ("cut", 1), ("sell", 1),
        ("debt", 1), ("lawsuit", 1), ("inflation", 1), ("slowdown", 1), ("unemployment", 1),
        ("bearish", 1), ("negative", 1), ("volatile", 1), ("uncertain", 1), ("stall", 1),
        ("delay", 1), ("setback", 1), ("obstacle", 1), ("challenge", 1), ("oppose", 1),
        ("reject", 1), ("ban", 1), ("block", 1), ("suspend", 1), ("revoke", 1),
        ("accuse", 1), ("allege", 1), ("investigat", 1), ("probe", 1), ("penalt", 1),
        ("fine", 1), ("violat", 1), ("breach", 1), ("hack", 1), ("leak", 1),
        ("contamin", 1), ("pollut", 1), ("drought", 1), ("flood", 1), ("earthquake", 1),
        ("storm", 1), ("shortage", 1), ("disrupt", 1), ("strain", 1), ("divide", 1),
        ("downside", 1), ("headwind", 1), ("erode", 1), ("shrink", 1), ("retreat", 1),
    ];

    let pos_score: i32 = positive_words.iter()
        .filter(|(w, _)| text_lower.contains(w))
        .map(|(_, weight)| weight)
        .sum();
    let neg_score: i32 = negative_words.iter()
        .filter(|(w, _)| text_lower.contains(w))
        .map(|(_, weight)| weight)
        .sum();

    let net = pos_score - neg_score;
    if net >= 2 {
        article.sentiment = "BULLISH".to_string();
    } else if net <= -2 {
        article.sentiment = "BEARISH".to_string();
    } else if net == 1 {
        // Mild positive — still classify if there's any signal
        article.sentiment = "BULLISH".to_string();
    } else if net == -1 {
        article.sentiment = "BEARISH".to_string();
    }
    // net == 0 stays NEUTRAL

    // Determine impact — based on priority + sentiment strength
    let sentiment_strength = net.unsigned_abs();
    if article.priority == "FLASH" || article.priority == "URGENT" || sentiment_strength >= 6 {
        article.impact = "HIGH".to_string();
    } else if article.priority == "BREAKING" || sentiment_strength >= 3 {
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

    // Sort by sort_ts descending (newest first) — interleaves all feeds by actual publish time
    all_articles.sort_by(|a, b| b.sort_ts.cmp(&a.sort_ts));

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
pub async fn test_rss_feed_url(url: String) -> Result<serde_json::Value, String> {
    let client = match reqwest::Client::builder()
        .timeout(Duration::from_secs(15))
        .redirect(reqwest::redirect::Policy::limited(5))
        .danger_accept_invalid_certs(false)
        .build()
    {
        Ok(c) => c,
        Err(e) => return Ok(serde_json::json!({
            "valid": false,
            "error": format!("Failed to create HTTP client: {}", e)
        })),
    };

    let response = match client
        .get(&url)
        .header("Accept", "application/rss+xml, application/xml, text/xml, */*")
        .header("User-Agent", "Mozilla/5.0 (compatible; FinceptTerminal/3.0)")
        .send()
        .await
    {
        Ok(r) => r,
        Err(e) => return Ok(serde_json::json!({
            "valid": false,
            "error": format!("Network error: {}", e)
        })),
    };

    let status = response.status().as_u16();

    if !response.status().is_success() {
        let preview = response.text().await.unwrap_or_default();
        let snippet: String = preview.chars().take(300).collect();
        return Ok(serde_json::json!({
            "valid": false,
            "status": status,
            "error": format!("HTTP {} - server rejected the request", status),
            "response_preview": snippet
        }));
    }

    let text = match response.text().await {
        Ok(t) => t,
        Err(e) => return Ok(serde_json::json!({
            "valid": false,
            "status": status,
            "error": format!("Failed to read response body: {}", e)
        })),
    };

    // Check if it looks like valid RSS/XML
    let trimmed = text.trim();
    let starts_with_tag = trimmed.starts_with('<');
    let has_rss = trimmed.contains("<rss");
    let has_feed = trimmed.contains("<feed");
    let has_channel = trimmed.contains("<channel");
    let is_valid = starts_with_tag && (has_rss || has_feed || has_channel);

    let snippet: String = trimmed.chars().take(300).collect();

    Ok(serde_json::json!({
        "valid": is_valid,
        "status": status,
        "starts_with_tag": starts_with_tag,
        "has_rss_tag": has_rss,
        "has_feed_tag": has_feed,
        "has_channel_tag": has_channel,
        "response_preview": snippet
    }))
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
