use crate::barter_integration::types::*;
use chrono::{DateTime, Utc, TimeZone};
use rust_decimal::Decimal;
use std::collections::HashMap;
use serde::{Deserialize, Serialize};

/// Column mapping configuration for user datasets
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DatasetMapping {
    pub timestamp_column: String,
    pub timestamp_format: TimestampFormat,
    pub open_column: Option<String>,
    pub high_column: Option<String>,
    pub low_column: Option<String>,
    pub close_column: Option<String>,
    pub volume_column: Option<String>,
    pub price_column: Option<String>, // For tick data
    pub size_column: Option<String>,  // For tick data
    pub symbol_column: Option<String>,
    pub aggregation_interval: Option<String>, // "1m", "5m", "1h", etc.
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum TimestampFormat {
    UnixSeconds,
    UnixMilliseconds,
    UnixMicroseconds,
    UnixNanoseconds,
    Rfc3339,
    Custom(String),
}

/// Generic data row that can be mapped
#[derive(Debug, Clone)]
pub struct DataRow {
    pub fields: HashMap<String, String>,
}

/// Trade tick for aggregation
#[derive(Debug, Clone)]
struct TradeTick {
    timestamp: DateTime<Utc>,
    price: Decimal,
    size: Decimal,
}

/// Data mapper for converting user datasets to candle format
pub struct DataMapper {
    mapping: DatasetMapping,
}

impl DataMapper {
    pub fn new(mapping: DatasetMapping) -> Self {
        Self { mapping }
    }

    /// Parse timestamp based on format
    fn parse_timestamp(&self, value: &str) -> BarterResult<DateTime<Utc>> {
        match &self.mapping.timestamp_format {
            TimestampFormat::UnixSeconds => {
                let secs: i64 = value.parse()
                    .map_err(|_| BarterError::Config("Invalid timestamp".into()))?;
                Ok(Utc.timestamp_opt(secs, 0).unwrap())
            }
            TimestampFormat::UnixMilliseconds => {
                let millis: i64 = value.parse()
                    .map_err(|_| BarterError::Config("Invalid timestamp".into()))?;
                Ok(Utc.timestamp_millis_opt(millis).unwrap())
            }
            TimestampFormat::UnixMicroseconds => {
                let micros: i64 = value.parse()
                    .map_err(|_| BarterError::Config("Invalid timestamp".into()))?;
                Ok(Utc.timestamp_micros(micros).unwrap())
            }
            TimestampFormat::UnixNanoseconds => {
                let nanos: i64 = value.parse()
                    .map_err(|_| BarterError::Config("Invalid timestamp".into()))?;
                Ok(Utc.timestamp_nanos(nanos))
            }
            TimestampFormat::Rfc3339 => {
                DateTime::parse_from_rfc3339(value)
                    .map(|dt| dt.with_timezone(&Utc))
                    .map_err(|_| BarterError::Config("Invalid RFC3339 timestamp".into()))
            }
            TimestampFormat::Custom(format) => {
                // Use chrono format string
                chrono::NaiveDateTime::parse_from_str(value, format)
                    .map(|dt| Utc.from_utc_datetime(&dt))
                    .map_err(|_| BarterError::Config("Invalid custom timestamp".into()))
            }
        }
    }

    /// Parse decimal value
    fn parse_decimal(&self, value: &str) -> BarterResult<Decimal> {
        value.parse::<Decimal>()
            .map_err(|_| BarterError::Config(format!("Invalid decimal: {}", value)))
    }

    /// Convert OHLCV data rows to candles
    pub fn map_ohlcv_data(&self, rows: Vec<DataRow>, _symbol: &str) -> BarterResult<Vec<Candle>> {
        let mut candles = Vec::new();

        for row in rows {
            let timestamp = self.parse_timestamp(
                row.fields.get(&self.mapping.timestamp_column)
                    .ok_or_else(|| BarterError::Config("Timestamp column not found".into()))?
            )?;

            let open = if let Some(col) = &self.mapping.open_column {
                self.parse_decimal(row.fields.get(col)
                    .ok_or_else(|| BarterError::Config("Open column not found".into()))?)?
            } else {
                Decimal::ZERO
            };

            let high = if let Some(col) = &self.mapping.high_column {
                self.parse_decimal(row.fields.get(col)
                    .ok_or_else(|| BarterError::Config("High column not found".into()))?)?
            } else {
                open
            };

            let low = if let Some(col) = &self.mapping.low_column {
                self.parse_decimal(row.fields.get(col)
                    .ok_or_else(|| BarterError::Config("Low column not found".into()))?)?
            } else {
                open
            };

            let close = if let Some(col) = &self.mapping.close_column {
                self.parse_decimal(row.fields.get(col)
                    .ok_or_else(|| BarterError::Config("Close column not found".into()))?)?
            } else {
                open
            };

            let volume = if let Some(col) = &self.mapping.volume_column {
                self.parse_decimal(row.fields.get(col)
                    .ok_or_else(|| BarterError::Config("Volume column not found".into()))?)?
            } else {
                Decimal::ZERO
            };

            candles.push(Candle {
                timestamp,
                open,
                high,
                low,
                close,
                volume,
            });
        }

        Ok(candles)
    }

    /// Convert tick data to candles with aggregation
    pub fn map_tick_data(&self, rows: Vec<DataRow>) -> BarterResult<HashMap<String, Vec<Candle>>> {
        // Parse all ticks
        let mut ticks_by_symbol: HashMap<String, Vec<TradeTick>> = HashMap::new();

        for row in rows {
            let timestamp = self.parse_timestamp(
                row.fields.get(&self.mapping.timestamp_column)
                    .ok_or_else(|| BarterError::Config("Timestamp column not found".into()))?
            )?;

            let price = if let Some(col) = &self.mapping.price_column {
                self.parse_decimal(row.fields.get(col)
                    .ok_or_else(|| BarterError::Config("Price column not found".into()))?)?
            } else {
                return Err(BarterError::Config("Price column required for tick data".into()));
            };

            let size = if let Some(col) = &self.mapping.size_column {
                self.parse_decimal(row.fields.get(col)
                    .ok_or_else(|| BarterError::Config("Size column not found".into()))?)?
            } else {
                Decimal::ONE
            };

            let symbol = if let Some(col) = &self.mapping.symbol_column {
                row.fields.get(col)
                    .ok_or_else(|| BarterError::Config("Symbol column not found".into()))?
                    .clone()
            } else {
                "UNKNOWN".to_string()
            };

            ticks_by_symbol.entry(symbol).or_insert_with(Vec::new).push(TradeTick {
                timestamp,
                price,
                size,
            });
        }

        // Aggregate ticks into candles
        let mut result = HashMap::new();
        let interval_secs = self.parse_interval(
            self.mapping.aggregation_interval.as_deref().unwrap_or("1m")
        )?;

        for (symbol, mut ticks) in ticks_by_symbol {
            ticks.sort_by_key(|t| t.timestamp);
            let candles = self.aggregate_ticks(ticks, interval_secs)?;
            result.insert(symbol, candles);
        }

        Ok(result)
    }

    /// Parse interval string to seconds
    fn parse_interval(&self, interval: &str) -> BarterResult<i64> {
        let num: i64 = interval[..interval.len()-1].parse()
            .map_err(|_| BarterError::Config("Invalid interval".into()))?;

        match interval.chars().last().unwrap() {
            's' => Ok(num),
            'm' => Ok(num * 60),
            'h' => Ok(num * 3600),
            'd' => Ok(num * 86400),
            _ => Err(BarterError::Config("Invalid interval suffix".into()))
        }
    }

    /// Aggregate ticks into candles
    fn aggregate_ticks(&self, ticks: Vec<TradeTick>, interval_secs: i64) -> BarterResult<Vec<Candle>> {
        if ticks.is_empty() {
            return Ok(Vec::new());
        }

        let mut candles = Vec::new();
        let start_time = ticks[0].timestamp;
        let mut current_bucket_start = start_time.timestamp() - (start_time.timestamp() % interval_secs);

        let mut bucket_ticks: Vec<TradeTick> = Vec::new();

        for tick in ticks {
            let tick_bucket = tick.timestamp.timestamp() - (tick.timestamp.timestamp() % interval_secs);

            if tick_bucket > current_bucket_start {
                // Finalize current bucket
                if !bucket_ticks.is_empty() {
                    candles.push(self.create_candle_from_ticks(&bucket_ticks, current_bucket_start)?);
                    bucket_ticks.clear();
                }
                current_bucket_start = tick_bucket;
            }

            bucket_ticks.push(tick);
        }

        // Finalize last bucket
        if !bucket_ticks.is_empty() {
            candles.push(self.create_candle_from_ticks(&bucket_ticks, current_bucket_start)?);
        }

        Ok(candles)
    }

    /// Create candle from ticks
    fn create_candle_from_ticks(&self, ticks: &[TradeTick], bucket_timestamp: i64) -> BarterResult<Candle> {
        let open = ticks.first().unwrap().price;
        let close = ticks.last().unwrap().price;
        let high = ticks.iter().map(|t| t.price).max().unwrap();
        let low = ticks.iter().map(|t| t.price).min().unwrap();
        let volume: Decimal = ticks.iter().map(|t| t.size).sum();

        Ok(Candle {
            timestamp: Utc.timestamp_opt(bucket_timestamp, 0).unwrap(),
            open,
            high,
            low,
            close,
            volume,
        })
    }
}

/// Parse Databento TradeMsg format specifically (optimized)
pub fn parse_databento_file(file_path: &str) -> BarterResult<HashMap<String, Vec<Candle>>> {
    use std::fs;
    use rayon::prelude::*;

    let content = fs::read_to_string(file_path)
        .map_err(|e| BarterError::Config(format!("Failed to read file: {}", e)))?;

    // Parse in parallel
    let ticks_by_instrument: HashMap<u32, Vec<TradeTick>> = content
        .par_lines()
        .filter_map(|line| {
            // Fast inline parsing without regex
            parse_trade_line(line)
        })
        .fold(
            || HashMap::new(),
            |mut acc: HashMap<u32, Vec<TradeTick>>, tick| {
                acc.entry(tick.0).or_insert_with(Vec::new).push(tick.1);
                acc
            }
        )
        .reduce(
            || HashMap::new(),
            |mut acc, map| {
                for (id, mut ticks) in map {
                    acc.entry(id).or_insert_with(Vec::new).append(&mut ticks);
                }
                acc
            }
        );

    // Aggregate to 1-minute candles
    let mapper = DataMapper::new(DatasetMapping {
        timestamp_column: String::new(),
        timestamp_format: TimestampFormat::UnixNanoseconds,
        open_column: None,
        high_column: None,
        low_column: None,
        close_column: None,
        volume_column: None,
        price_column: None,
        size_column: None,
        symbol_column: None,
        aggregation_interval: Some("1m".to_string()),
    });

    let mut result = HashMap::new();
    for (instrument_id, mut ticks) in ticks_by_instrument {
        ticks.sort_by_key(|t| t.timestamp);
        let candles = mapper.aggregate_ticks(ticks, 60)?;
        let symbol = map_instrument_id_to_symbol(instrument_id);
        result.insert(symbol, candles);
    }

    Ok(result)
}

/// Map Databento instrument IDs to symbols
fn map_instrument_id_to_symbol(id: u32) -> String {
    match id {
        271726 => "BTC".to_string(),
        3403 | 13615 => "ES".to_string(),
        2895 | 13505 => "NQ".to_string(),
        _ => format!("INST_{}", id),
    }
}

/// Fast inline parser without regex
#[inline]
fn parse_trade_line(line: &str) -> Option<(u32, TradeTick)> {
    // TradeMsg { hd: RecordHeader { ... ts_event: 1654179600001940663 }, price: 32619.000000000, size: 1, ...

    let ts_start = line.find("ts_event: ")? + 10;
    let ts_end = line[ts_start..].find(' ')?;
    let ts_nanos: i64 = line[ts_start..ts_start + ts_end].parse().ok()?;

    let inst_start = line.find("instrument_id: ")? + 15;
    let inst_end = line[inst_start..].find(',')?;
    let instrument_id: u32 = line[inst_start..inst_start + inst_end].parse().ok()?;

    let price_start = line.find("price: ")? + 7;
    let price_end = line[price_start..].find(',')?;
    let price: f64 = line[price_start..price_start + price_end].parse().ok()?;

    let size_start = line.find("size: ")? + 6;
    let size_end = line[size_start..].find(',')?;
    let size: i64 = line[size_start..size_start + size_end].parse().ok()?;

    let timestamp = Utc.timestamp_nanos(ts_nanos);
    let tick = TradeTick {
        timestamp,
        price: Decimal::try_from(price).unwrap_or(Decimal::ZERO),
        size: Decimal::new(size, 0),
    };

    Some((instrument_id, tick))
}
