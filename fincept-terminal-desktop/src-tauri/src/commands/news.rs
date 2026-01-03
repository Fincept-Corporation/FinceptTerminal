// RSS News Feed Fetcher - High-performance Rust implementation
// Fetches real-time news from 20+ financial RSS feeds without CORS restrictions

use reqwest;
use serde::{Deserialize, Serialize};
use std::time::{Duration, SystemTime, UNIX_EPOCH};
use tokio::time::timeout;

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

#[derive(Debug, Clone)]
struct RSSFeed {
    name: String,
    url: String,
    category: String,
    region: String,
    source: String,
}

// Verified working RSS feeds (2025)
fn get_rss_feeds() -> Vec<RSSFeed> {
    vec![
        // Major Financial News
        RSSFeed {
            name: "Yahoo Finance".to_string(),
            url: "https://finance.yahoo.com/news/rssindex".to_string(),
            category: "MARKETS".to_string(),
            region: "US".to_string(),
            source: "YAHOO".to_string(),
        },
        RSSFeed {
            name: "Investing.com".to_string(),
            url: "https://www.investing.com/rss/news.rss".to_string(),
            category: "MARKETS".to_string(),
            region: "GLOBAL".to_string(),
            source: "INVESTING.COM".to_string(),
        },
        // Cryptocurrency
        RSSFeed {
            name: "CoinDesk".to_string(),
            url: "https://www.coindesk.com/arc/outboundfeeds/rss/".to_string(),
            category: "CRYPTO".to_string(),
            region: "GLOBAL".to_string(),
            source: "COINDESK".to_string(),
        },
        RSSFeed {
            name: "CoinTelegraph".to_string(),
            url: "https://cointelegraph.com/rss".to_string(),
            category: "CRYPTO".to_string(),
            region: "GLOBAL".to_string(),
            source: "COINTELEGRAPH".to_string(),
        },
        RSSFeed {
            name: "Decrypt".to_string(),
            url: "https://decrypt.co/feed".to_string(),
            category: "CRYPTO".to_string(),
            region: "GLOBAL".to_string(),
            source: "DECRYPT".to_string(),
        },
        // Technology
        RSSFeed {
            name: "TechCrunch".to_string(),
            url: "https://techcrunch.com/feed/".to_string(),
            category: "TECH".to_string(),
            region: "US".to_string(),
            source: "TECHCRUNCH".to_string(),
        },
        RSSFeed {
            name: "The Verge".to_string(),
            url: "https://www.theverge.com/rss/index.xml".to_string(),
            category: "TECH".to_string(),
            region: "US".to_string(),
            source: "THE VERGE".to_string(),
        },
        RSSFeed {
            name: "Ars Technica".to_string(),
            url: "https://feeds.arstechnica.com/arstechnica/index".to_string(),
            category: "TECH".to_string(),
            region: "US".to_string(),
            source: "ARS TECHNICA".to_string(),
        },
        // Energy & Commodities
        RSSFeed {
            name: "Oil Price".to_string(),
            url: "https://oilprice.com/rss/main".to_string(),
            category: "ENERGY".to_string(),
            region: "GLOBAL".to_string(),
            source: "OILPRICE".to_string(),
        },
        // SEC Press Releases
        RSSFeed {
            name: "SEC Press Releases".to_string(),
            url: "https://www.sec.gov/news/pressreleases.rss".to_string(),
            category: "REGULATORY".to_string(),
            region: "US".to_string(),
            source: "SEC".to_string(),
        },
    ]
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
