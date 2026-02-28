// Tauri commands module

// Core infrastructure commands (extracted from lib.rs)
pub mod websocket;
pub mod monitoring;
pub mod mcp;

pub mod news;
pub mod market_data;
pub mod alpha_arena_bridge;
pub mod yfinance;
pub mod alphavantage;
pub mod pmdarima;
pub mod government_us;
pub mod congress_gov;
pub mod oecd;
pub mod imf;
pub mod fmp;
pub mod federal_reserve;
pub mod sec;
pub mod edgar;
pub mod edgar_cache;
pub mod cftc;
pub mod cboe;
pub mod bls;
pub mod nasdaq;
pub mod bis;
pub mod multpl;
pub mod eia;
pub mod bea;
pub mod ecb;
pub mod wto;
pub mod wits;
pub mod unesco;
pub mod oscar;

// Financial data sources
pub mod fred;
pub mod coingecko;
pub mod worldbank;
pub mod trading_economics;
pub mod econdb;

// Government data sources
pub mod canada_gov;
pub mod datagovuk;
pub mod datagov_au;
pub mod estat_japan;
pub mod govdata_de;

// Regional data sources
pub mod datagovsg;
pub mod data_gov_hk;
pub mod french_gov;
pub mod swiss_gov;
pub mod spain_data;
pub mod adb;

// Nordic / PxWeb data sources
pub mod pxweb;

// Specialized data sources
pub mod openafrica;
pub mod hdx;
pub mod economic_calendar;
pub mod databento;
pub mod sentinelhub;

// Code editor and Jupyter notebook
pub mod jupyter;
pub mod finscript_cmd;

// Python Agents
pub mod agents;
pub mod agent_streaming;

// Portfolio Analytics
pub mod portfolio;

// Portfolio Management (CRUD operations)
pub mod portfolio_management;

// skfolio - Advanced Portfolio Optimization
pub mod skfolio;

pub mod pyportfolioopt;

// Technical Analysis
pub mod technical_analysis;

// Additional data sources
pub mod fiscaldata;
pub mod nasa_gibs;
pub mod ckan;

// Analytics modules
pub mod analytics;

// FinancePy - Derivatives Pricing
pub mod financepy;

// py_vollib - Black, Black-Scholes, Black-Scholes-Merton pricing
pub mod vollib;

// AI Agents

// Report Generator
pub mod report_generator;

// Backtesting
pub mod backtesting;

// Company News
pub mod company_news;

// Agno Trading Agents
pub mod agno_trading;
pub mod alpha_arena;

// High-performance Rust SQLite Database
pub mod database;

// AI Quant Lab - Qlib and RD-Agent
pub mod ai_quant_lab;

// FFN Analytics - Portfolio Performance Analysis
pub mod ffn_analytics;

// QuantStats - Portfolio Performance & Risk Analytics
pub mod quantstats;

// Fortitudo.tech Analytics - Portfolio Risk Analytics
pub mod fortitudo_analytics;

// Functime Analytics - Time Series Forecasting
pub mod functime_analytics;

// GluonTS - Deep Learning Time Series Forecasting
pub mod gluonts;

// High-Performance OrderBook Processing
pub mod orderbook;
pub mod financial_analysis;

// Peer Comparison & Benchmarking - GICS-based peer analysis
pub mod peer_commands;

// Stock Screener - Advanced equity screening
pub mod screener_commands;

// Geocoding - Location search and reverse geocoding
pub mod geocoding;

// Indian Stock Brokers - Modular broker integrations
pub mod brokers;

// Paper Trading moved to src-tauri/src/paper_trading.rs (universal module)

// Broker Credentials - Secure encrypted storage for broker API credentials
pub mod broker_credentials;

// Master Contract Cache - REMOVED (use symbol_master instead)
// pub mod master_contract;

// Unified Symbol Master - Cross-broker symbol database
pub mod symbol_master;

// Broker Master Contract Downloads - Unified symbol download for all brokers
pub mod broker_downloads;

// Generic Storage - SQLite key-value storage
pub mod storage;

// Unified Trading - Live/Paper mode switching for all brokers
pub mod unified_trading;

// AKShare - Free Chinese & Global market data
pub mod akshare;

// LLM Models - LiteLLM provider and model listing
pub mod llm_models;

// Custom Index - Aggregate Index Feature
pub mod custom_index;

// M&A Analytics - Complete M&A and Financial Advisory System
pub mod ma_analytics;

// Strategy Engine - 400+ algorithmic trading strategies
pub mod strategies;

// Algo Trading - Visual condition-based strategy builder, deployment, scanner
pub mod algo_trading;

// TALIpp - Incremental Technical Analysis Indicators
pub mod talipp;

// PyPME - Public Market Equivalent calculations
pub mod pypme;

// GS-Quant - Goldman Sachs Quant Analytics
pub mod gs_quant;

// Polymarket - Prediction markets data
pub mod polymarket;

// VisionQuant - AI K-line Pattern Intelligence
pub mod vision_quant;
