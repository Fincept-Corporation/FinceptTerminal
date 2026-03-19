// database.cpp — split into focused files.
// All implementations live in:
//   db_core.cpp           — Utility, Statement, Database class (WAL, PRAGMAs, schema)
//   db_cache.cpp          — CacheDatabase class
//   db_ops_settings.cpp   — Settings, Key-Value storage, Credentials, LLM Config
//   db_ops_chat.cpp       — Chat session operations
//   db_ops_watchlist.cpp  — Watchlist and M&A Deals operations
//   db_ops_cache.cpp      — Cache operations (TTL-based)
//   db_ops_sessions.cpp   — Tab Sessions and Agent Configs
//   db_ops_notes_algo.cpp — Notes and Algo Trading operations
