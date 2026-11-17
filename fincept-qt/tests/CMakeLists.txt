# ── Fincept Terminal unit tests (Qt Test) ────────────────────────────────────
#
# Reached ONLY from the top-level CMakeLists.txt under `if(FINCEPT_BUILD_TESTS)`,
# which is OFF by default. Nothing in this file affects a normal build.
#
#   cmake -B build -DFINCEPT_BUILD_TESTS=ON ...
#   cmake --build build
#   ctest --test-dir build --output-on-failure          # add -C Release on MSVC
#
# ─────────────────────────────────────────────────────────────────────────────
# HARD RULE — read this before adding a target.
#
#   A test target lists the SPECIFIC sources it needs, explicitly, by path.
#
# Do NOT "solve" a missing symbol by refactoring the app into an OBJECT library
# and linking it here. The release/CI presets build with CMAKE_UNITY_BUILD=ON
# (batch size 20) while `win-dev` builds unity OFF, so any change to the main
# source list re-batches every file after it and can produce file-scope symbol
# redefinitions that appear ONLY on CI and never reproduce locally. See the
# "unity build" notes in ../CLAUDE.md.
#
# If a unit cannot be tested without dragging in half the application, that is a
# signal about the unit, not about this file: extract the pure logic into a small
# header (or a leaf .cpp) and test that. Do not grow the link line.
#
# Current cost of the whole suite: 6 executables, 9 translation units.
# ─────────────────────────────────────────────────────────────────────────────

# Qt Test ships with every Qt installation, so this adds no new dependency.
# _FINCEPT_QT_VER_ARGS is inherited from the parent scope so the version pin
# (6.8.3 EXACT by default) stays identical to the one the app is built against.
find_package(Qt6 ${_FINCEPT_QT_VER_ARGS} QUIET COMPONENTS Test)
if(NOT Qt6Test_FOUND)
    message(FATAL_ERROR
        "FINCEPT_BUILD_TESTS=ON but the Qt6 Test module was not found.\n"
        "Qt Test ships with the standard Qt installer — re-run the Qt maintenance "
        "tool and make sure 'Qt Test' is selected for your kit, or configure with "
        "-DFINCEPT_BUILD_TESTS=OFF.")
endif()

# Windows has no rpath: a freshly built tst_*.exe cannot find Qt6Core.dll unless
# the Qt bin dir is on PATH. Resolve it once and inject it per-test below, so
# `ctest` works straight out of a fresh configure without anyone having to run
# windeployqt against the test binaries.
#
# Best-effort by design: if the Qt layout is unusual (distro split packages, no
# qmake shipped) we simply skip the injection rather than fail the configure —
# on those layouts Qt is on the loader path already.
set(_fincept_qt_bindir "")
set(_fincept_qt_libdir "")
if(TARGET Qt6::qmake)
    get_target_property(_fincept_qt_qmake Qt6::qmake IMPORTED_LOCATION)
    if(_fincept_qt_qmake AND EXISTS "${_fincept_qt_qmake}")
        get_filename_component(_fincept_qt_bindir "${_fincept_qt_qmake}" DIRECTORY)
        get_filename_component(_fincept_qt_prefix "${_fincept_qt_bindir}" DIRECTORY)
        set(_fincept_qt_libdir "${_fincept_qt_prefix}/lib")
    endif()
endif()
if(NOT _fincept_qt_bindir AND Qt6_DIR)
    # Qt6_DIR is <prefix>/lib/cmake/Qt6 for a standard installer layout.
    get_filename_component(_fincept_qt_prefix "${Qt6_DIR}/../../.." ABSOLUTE)
    if(EXISTS "${_fincept_qt_prefix}/bin")
        set(_fincept_qt_bindir "${_fincept_qt_prefix}/bin")
        set(_fincept_qt_libdir "${_fincept_qt_prefix}/lib")
    endif()
endif()

# fincept_add_test(<name> <source> [<extra sources>...])
#
# <name> must be tst_<unit>. Extra sources are the app .cpp files this unit
# genuinely needs — keep that list as short as it can possibly be.
function(fincept_add_test _name)
    add_executable(${_name} ${ARGN})

    # Qt6::Test pulls in Qt6::Core. Deliberately no Widgets/Network/Sql here:
    # a unit test that needs those is testing the wrong seam.
    target_link_libraries(${_name} PRIVATE Qt6::Test Qt6::Core)

    # Sources use project-root-relative includes ("trading/OrderValidator.h"),
    # matching target_include_directories(FinceptTerminal PRIVATE src).
    target_include_directories(${_name} PRIVATE "${PROJECT_SOURCE_DIR}/src")

    set_target_properties(${_name} PROPERTIES
        AUTOMOC ON            # Q_OBJECT test classes live in the .cpp (+ #include "<name>.moc")
        UNITY_BUILD OFF       # never batch test TUs, whatever the parent preset says
        FOLDER "tests")

    # NOTE: no install() rule, by design. Test executables must never ship in a
    # release artifact. They also land in ${CMAKE_CURRENT_BINARY_DIR} (build/tests/),
    # not next to FinceptTerminal.exe, so the installer's
    # install(DIRECTORY "$<TARGET_FILE_DIR:FinceptTerminal>/" ... FILES_MATCHING "*.dll")
    # rule cannot pick them up either.

    add_test(NAME ${_name} COMMAND ${_name})
    set_property(GLOBAL APPEND PROPERTY FINCEPT_TEST_TARGETS ${_name})

    if(WIN32 AND _fincept_qt_bindir)
        set_tests_properties(${_name} PROPERTIES
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_fincept_qt_bindir}")
    elseif(UNIX AND NOT APPLE AND _fincept_qt_libdir)
        set_tests_properties(${_name} PROPERTIES
            ENVIRONMENT_MODIFICATION "LD_LIBRARY_PATH=path_list_prepend:${_fincept_qt_libdir}")
    endif()
endfunction()

# ── Suites ───────────────────────────────────────────────────────────────────

# src/trading/brokers/BrokerModifyFields.h — header-only, zero app sources.
fincept_add_test(tst_broker_modify_fields
    tst_broker_modify_fields.cpp)

# src/core/result/Result.h — header-only, zero app sources.
fincept_add_test(tst_result
    tst_result.cpp)

# src/trading/OrderValidator.{h,cpp} — the .cpp is the PURE half of the class
# (validation + risk rules) and includes only its own header, which includes only
# trading/TradingTypes.h (Qt Core types + plain structs, no .cpp of its own). The
# persistence / logging / account-snapshot shell lives in OrderRiskGate.cpp and is
# deliberately NOT listed here: it pulls in SettingsRepository -> QtSql and the
# Logger. (It once lived in OrderValidator.cpp, which made this suite unbuildable.)
fincept_add_test(tst_order_validator
    tst_order_validator.cpp
    "${PROJECT_SOURCE_DIR}/src/trading/OrderValidator.cpp")

# src/trading/OrderApprovalRule.h — header-only, pure (Qt Core + TradingTypes.h): the
# approval decision UnifiedTrading makes from UnifiedOrder::source (source kind x
# approval mode x reduce-only verified x came-from-approval -> place / queue / refuse).
# No extra TU.
fincept_add_test(tst_order_approval_rule
    tst_order_approval_rule.cpp)

# src/algo_engine/IndicatorEngine.{h,cpp}: the .cpp includes only its own
# header, which includes only algo_engine/AlgoEngineTypes.h (Qt Core types +
# plain structs, no .cpp of its own). One extra TU, same shape as the validator
# suite above.
fincept_add_test(tst_indicator_engine
    tst_indicator_engine.cpp
    "${PROJECT_SOURCE_DIR}/src/algo_engine/IndicatorEngine.cpp")

# src/trading/MarketDataRequests.h — header-only, zero app sources. The
# latest-request-wins gate, failure-reason text and symbol matching behind
# AccountDataStream's chart / depth / tape fetches.
fincept_add_test(tst_market_data_requests
    tst_market_data_requests.cpp)

# src/screens/equity_trading/EquitySymbolMap.h — header-only, zero app sources
# (Qt Core only, despite living under screens/). Broker symbol -> Yahoo ticker and
# chart timeframe -> Yahoo history request for the Equity Trading public feed.
fincept_add_test(tst_equity_symbol_map
    tst_equity_symbol_map.cpp)

# src/screens/equity_trading/EquityLayoutPlan.h — header-only, zero app sources (Qt
# Core only): the Equity Trading screen's width classes / breakpoints (font-scaled),
# the per-class zone plan, splitter fitting and per-class restore.
fincept_add_test(tst_equity_layout_plan
    tst_equity_layout_plan.cpp)

# src/screens/equity_trading/EquitySingleInstance.h — header-only, zero app sources.
# Reproduces the shell re-entering a screen factory while the first build is being
# shown (two stacked Equity Trading screens) and checks that the second build is an
# inert twin. Offscreen widgets.
fincept_add_test(tst_equity_single_instance
    tst_equity_single_instance.cpp)
target_link_libraries(tst_equity_single_instance PRIVATE Qt6::Widgets)
set_tests_properties(tst_equity_single_instance PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# src/ui/formatting — central number / currency / date formatter. Qt Core only.
# FormatSettings.h is listed so AUTOMOC sees its Q_OBJECT.
fincept_add_test(tst_number_format
    tst_number_format.cpp
    "${PROJECT_SOURCE_DIR}/src/ui/formatting/FormatSettings.h"
    "${PROJECT_SOURCE_DIR}/src/ui/formatting/FormatSettings.cpp"
    "${PROJECT_SOURCE_DIR}/src/ui/formatting/CurrencyTable.cpp"
    "${PROJECT_SOURCE_DIR}/src/ui/formatting/NumberFormat.cpp"
    "${PROJECT_SOURCE_DIR}/src/ui/formatting/DateFormat.cpp")

# src/ui/formatting/ParseAmount.h — parse_amount(), the inverse of money() (implemented in
# NumberFormat.cpp): typed / printed amounts under every number and currency setting,
# and the round trip parse_amount(money(v, ccy), ccy) for every setting. Qt Core only.
fincept_add_test(tst_amount_parse
    tst_amount_parse.cpp
    "${PROJECT_SOURCE_DIR}/src/ui/formatting/FormatSettings.h"
    "${PROJECT_SOURCE_DIR}/src/ui/formatting/FormatSettings.cpp"
    "${PROJECT_SOURCE_DIR}/src/ui/formatting/CurrencyTable.cpp"
    "${PROJECT_SOURCE_DIR}/src/ui/formatting/NumberFormat.cpp"
    "${PROJECT_SOURCE_DIR}/src/ui/formatting/DateFormat.cpp")

# src/ui/formatting/AmountInput.h — the typed-field rules on parse_amount() used by the M&A deal
# dialog, the data grids' money columns and the option pricer's dividends: another currency refused
# (never converted), plain numbers, stored values written back so they read back under every locale.
fincept_add_test(tst_amount_input
    tst_amount_input.cpp
    "${PROJECT_SOURCE_DIR}/src/ui/formatting/FormatSettings.h"
    "${PROJECT_SOURCE_DIR}/src/ui/formatting/FormatSettings.cpp"
    "${PROJECT_SOURCE_DIR}/src/ui/formatting/CurrencyTable.cpp"
    "${PROJECT_SOURCE_DIR}/src/ui/formatting/NumberFormat.cpp"
    "${PROJECT_SOURCE_DIR}/src/ui/formatting/DateFormat.cpp")

# src/services/markets/FxRateMath — FX legs (fiat pairs, crypto / stablecoin
# "-USD"), crosses, minor units, staleness, injected rates (no hub). The
# currency table decides which codes are crypto.
fincept_add_test(tst_fx_rate_math
    tst_fx_rate_math.cpp
    "${PROJECT_SOURCE_DIR}/src/services/markets/FxRateMath.cpp"
    "${PROJECT_SOURCE_DIR}/src/ui/formatting/FormatSettings.h"
    "${PROJECT_SOURCE_DIR}/src/ui/formatting/FormatSettings.cpp"
    "${PROJECT_SOURCE_DIR}/src/ui/formatting/CurrencyTable.cpp")

# src/ui/formatting/FormatWidgets + the core/currency facade — repaint on a
# format change (models, cached-text widgets, bound labels / spin boxes). A
# widget test: links Qt Widgets, runs on the offscreen platform.
fincept_add_test(tst_format_refresh
    tst_format_refresh.cpp
    "${PROJECT_SOURCE_DIR}/src/ui/formatting/FormatSettings.h"
    "${PROJECT_SOURCE_DIR}/src/ui/formatting/FormatSettings.cpp"
    "${PROJECT_SOURCE_DIR}/src/ui/formatting/CurrencyTable.cpp"
    "${PROJECT_SOURCE_DIR}/src/ui/formatting/NumberFormat.cpp"
    "${PROJECT_SOURCE_DIR}/src/ui/formatting/DateFormat.cpp"
    "${PROJECT_SOURCE_DIR}/src/ui/formatting/FormatWidgets.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/currency/CurrencyManager.h"
    "${PROJECT_SOURCE_DIR}/src/core/currency/CurrencyManager.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/logging/Logger.cpp")
target_link_libraries(tst_format_refresh PRIVATE Qt6::Widgets)
set_tests_properties(tst_format_refresh PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# src/services/markets — quote/info reply parsing and the multi-horizon
# performance helper. Both are leaf TUs (Qt Core only).
fincept_add_test(tst_quote_parsing
    tst_quote_parsing.cpp
    "${PROJECT_SOURCE_DIR}/src/services/markets/MarketDataParsing.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/markets/MarketPerformance.cpp")

# src/core/market — exchange sessions, calendars, instrument currency. The
# currency table is linked to cross-check core's minor-unit list against it.
fincept_add_test(tst_market_sessions
    tst_market_sessions.cpp
    "${PROJECT_SOURCE_DIR}/src/core/market/MarketSessions.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/market/InstrumentCurrency.cpp"
    "${PROJECT_SOURCE_DIR}/src/ui/formatting/FormatSettings.h"
    "${PROJECT_SOURCE_DIR}/src/ui/formatting/FormatSettings.cpp"
    "${PROJECT_SOURCE_DIR}/src/ui/formatting/CurrencyTable.cpp")

# src/services/portfolio — ledger replay / tax lots and TWR / XIRR. Leaf TUs only.
fincept_add_test(tst_portfolio_lots
    tst_portfolio_lots.cpp
    "${PROJECT_SOURCE_DIR}/src/services/portfolio/PortfolioLots.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/portfolio/PortfolioPerformance.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/market/InstrumentCurrency.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/market/MarketSessions.cpp")

# src/algo_engine — execution/cost model, performance metrics, session VWAP,
# position risk limits, scan signal direction. Leaf TUs only.
fincept_add_test(tst_algo_execution_model
    tst_algo_execution_model.cpp
    "${PROJECT_SOURCE_DIR}/src/algo_engine/ExecutionModel.cpp")
fincept_add_test(tst_algo_performance_metrics
    tst_algo_performance_metrics.cpp
    "${PROJECT_SOURCE_DIR}/src/algo_engine/PerformanceMetrics.cpp")
fincept_add_test(tst_algo_session_vwap
    tst_algo_session_vwap.cpp
    "${PROJECT_SOURCE_DIR}/src/algo_engine/IndicatorEngine.cpp")
fincept_add_test(tst_algo_position_risk
    tst_algo_position_risk.cpp
    "${PROJECT_SOURCE_DIR}/src/algo_engine/PositionManager.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/logging/Logger.cpp")
fincept_add_test(tst_algo_scan_signal
    tst_algo_scan_signal.cpp)

# src/services/algo_trading/AlgoRuleLogic — the Algo Builder's rule sentences, inline rule
# validation (the blank form's "Close > 0" placeholder never runs), blank-draft detection,
# rule-derived warm-up and the built-in templates. AlgoTradingTypes.h includes the theme
# header (a colour helper): Qt Gui headers only, no Gui code is called.
fincept_add_test(tst_algo_rule_logic
    tst_algo_rule_logic.cpp
    "${PROJECT_SOURCE_DIR}/src/services/algo_trading/AlgoRuleLogic.cpp")
target_link_libraries(tst_algo_rule_logic PRIVATE Qt6::Gui)

# src/algo_engine/BacktestEngine against an independent reference implementation of its fill
# model on deterministic synthetic bars (trades, equity, costs, Sharpe: daily bars x sqrt(252),
# intraday on trading-day closes). Engine leaf TUs + the logger; Qt Gui headers as above.
fincept_add_test(tst_algo_backtest_reference
    tst_algo_backtest_reference.cpp
    "${PROJECT_SOURCE_DIR}/src/algo_engine/BacktestEngine.cpp"
    "${PROJECT_SOURCE_DIR}/src/algo_engine/ConditionEvaluator.cpp"
    "${PROJECT_SOURCE_DIR}/src/algo_engine/IndicatorEngine.cpp"
    "${PROJECT_SOURCE_DIR}/src/algo_engine/ExecutionModel.cpp"
    "${PROJECT_SOURCE_DIR}/src/algo_engine/PerformanceMetrics.cpp"
    "${PROJECT_SOURCE_DIR}/src/algo_engine/AlgoEngineTypes.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/logging/Logger.cpp")
target_link_libraries(tst_algo_backtest_reference PRIVATE Qt6::Gui)

# src/services/news — language detection, ticker tagging, analytics maths.
fincept_add_test(tst_news_nlp
    tst_news_nlp.cpp
    "${PROJECT_SOURCE_DIR}/src/services/news/NewsLanguage.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/news/NewsTickerTagger.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/news/NewsAnalytics.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/instrument/SymbolInput.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/instrument/VenueSymbols.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/market/MarketSessions.cpp")

# Relationship Map money / unit labels (quote currency, minor units, major-unit totals).
fincept_add_test(tst_relmap_format
    tst_relmap_format.cpp
    "${PROJECT_SOURCE_DIR}/src/ui/formatting/FormatSettings.h"
    "${PROJECT_SOURCE_DIR}/src/ui/formatting/FormatSettings.cpp"
    "${PROJECT_SOURCE_DIR}/src/ui/formatting/CurrencyTable.cpp"
    "${PROJECT_SOURCE_DIR}/src/ui/formatting/NumberFormat.cpp"
    "${PROJECT_SOURCE_DIR}/src/ui/formatting/DateFormat.cpp")

# Alpha Arena: why a harness model is or is not ready (reasons shown in the wizard).
fincept_add_test(tst_arena_model_readiness
    tst_arena_model_readiness.cpp
    "${PROJECT_SOURCE_DIR}/src/services/harness/HarnessEligibility.cpp")

# src/services/news/NewsFeedParse.h — header-only, zero app sources. Feed
# parsing (RSS / RDF / Atom), publication times, the time policy and ordering.
fincept_add_test(tst_news_feed_parse
    tst_news_feed_parse.cpp)

# src/services/news/NewsClassify.h — header-only, zero app sources. Priority,
# market impact, event type and threat rules for one headline.
fincept_add_test(tst_news_classify
    tst_news_classify.cpp)

# src/services/workflow/adapters/WorkflowDbPolicy.h (+ the SQL console guard it
# applies) — header-only, zero app sources. What the workflow Database node may run.
fincept_add_test(tst_workflow_db_policy
    tst_workflow_db_policy.cpp)

# src/trading/instruments/InstrumentSearchSql.h — header-only, zero app sources.
# LIKE escaping and the indexed prefix range for the SQLite instrument search.
fincept_add_test(tst_instrument_search_sql
    tst_instrument_search_sql.cpp)

# src/services/agent_tasks — Chat Mode's run model (states, budgets, ids, JSON)
# and the cloud agent's SSE vocabulary mapped to run events. Leaf TUs (Qt Core only).
fincept_add_test(tst_agent_task_runs
    tst_agent_task_runs.cpp
    "${PROJECT_SOURCE_DIR}/src/services/agent_tasks/TaskRunTypes.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/agent_tasks/CloudSseMapping.cpp")

# src/services/agent_tasks — the multi-step executors (CloudPlanExecutor, plan
# parsing / prompts in RunPlanPrompts.h, LocalAgenticExecutor) driven by fake
# turns / a fake local engine through their seams (CloudTurnPort.h,
# LocalAgentPort.h). Both executor .cpp files are Qt Core only; their default
# (network / AgentService) constructors live in CloudTurnDriver.cpp and
# LocalAgentEngine.cpp, which are deliberately NOT listed. The Q_OBJECT headers
# are listed so AUTOMOC sees them.
fincept_add_test(tst_agent_task_executors
    tst_agent_task_executors.cpp
    "${PROJECT_SOURCE_DIR}/src/services/agent_tasks/IRunExecutor.h"
    "${PROJECT_SOURCE_DIR}/src/services/agent_tasks/CloudPlanExecutor.h"
    "${PROJECT_SOURCE_DIR}/src/services/agent_tasks/LocalAgenticExecutor.h"
    "${PROJECT_SOURCE_DIR}/src/services/agent_tasks/TaskRunTypes.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/agent_tasks/CloudPlanExecutor.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/agent_tasks/LocalAgenticExecutor.cpp")

# src/screens/chat_mode/RunViewLogic.h — header-only logic of Chat Mode's run views
# (tones, RUNS filter / order, run actions, activity pairing, composer prompt).
fincept_add_test(tst_chat_run_views
    tst_chat_run_views.cpp
    "${PROJECT_SOURCE_DIR}/src/services/agent_tasks/TaskRunTypes.cpp")

# src/services/agent_tasks/ArtefactLogic.{h,cpp} (+ ReportHandoff.h's pure half) —
# run artefacts: markdown tables, CSV / Markdown / JSON forms, atomic writes,
# extraction from recorded runs, retention, tool-result shapes. Qt Core only.
fincept_add_test(tst_run_artefacts
    tst_run_artefacts.cpp
    "${PROJECT_SOURCE_DIR}/src/services/agent_tasks/TaskRunTypes.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/agent_tasks/ArtefactLogic.cpp")

# src/services/agent_tasks/ScheduleLogic + RunTemplates — schedule occurrences
# across time zones / DST, missed runs, overlap, daily cap, relative dates, task
# templates, cost estimate and the working clock. Qt Core only.
fincept_add_test(tst_run_scheduling
    tst_run_scheduling.cpp
    "${PROJECT_SOURCE_DIR}/src/services/agent_tasks/TaskRunTypes.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/agent_tasks/ScheduleLogic.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/agent_tasks/RunTemplates.cpp")

# src/screens/notes/NotesLogic.h — header-only, zero app sources. Tag / ticker
# lists, $CASHTAG extraction, search / views / sorting, templates and the
# Markdown export / import round trip behind the Notes screen.
fincept_add_test(tst_notes_logic
    tst_notes_logic.cpp)

# src/screens/notes/NotesBoardLogic.h (+ storage/repositories/NoteBoardTypes.h) —
# header-only, zero app sources. Board lanes / grouping / ordering, filters,
# manual order (ranks), drag-move rules, board JSON round trip, the v066
# migration's defaults for existing notes, command-field parsing.
fincept_add_test(tst_notes_board_logic
    tst_notes_board_logic.cpp)

# src/core/session/ScreenStatePolicy.h — header-only, zero app sources. Which
# stored copy of a panel's UI state a restore uses (newest wins) and the gate
# that keeps an older async write from landing on top of a newer one.
fincept_add_test(tst_screen_state_policy
    tst_screen_state_policy.cpp)

# src/ui/navigation/CommandMatch.h — header-only, zero app sources. How a
# command-bar query matches a screen (full titles with spaces, word prefixes).
fincept_add_test(tst_command_match
    tst_command_match.cpp)

# src/core/instrument — the symbol system's normaliser, venue / provider table and
# ranking (Qt Core only). All three read tests/data/symbol_vectors.json (located with
# QFINDTESTDATA next to the test sources; override with FINCEPT_SYMBOL_VECTORS), the
# vector file the backend's Python port of the normaliser runs too.
fincept_add_test(tst_symbol_input
    tst_symbol_input.cpp
    "${PROJECT_SOURCE_DIR}/src/core/instrument/SymbolInput.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/instrument/VenueSymbols.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/market/MarketSessions.cpp")
fincept_add_test(tst_venue_symbols
    tst_venue_symbols.cpp
    "${PROJECT_SOURCE_DIR}/src/core/instrument/SymbolInput.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/instrument/VenueSymbols.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/market/MarketSessions.cpp")
# src/screens/markets — the Markets screen's pure rules (MarketsLogic.h, header-only) and
# its column registry (MarketColumns.cpp): Qt Core + core::instrument only.
fincept_add_test(tst_markets_logic
    tst_markets_logic.cpp
    "${PROJECT_SOURCE_DIR}/src/screens/markets/MarketColumns.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/instrument/SymbolInput.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/instrument/VenueSymbols.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/market/MarketSessions.cpp")
# src/screens/code_editor — the notebook model (.ipynb read/write/migration, exports, outputs): Qt Core only.
fincept_add_test(tst_notebook_document
    tst_notebook_document.cpp
    "${PROJECT_SOURCE_DIR}/src/screens/code_editor/NotebookDocument.cpp")
fincept_add_test(tst_instrument_rank
    tst_instrument_rank.cpp
    "${PROJECT_SOURCE_DIR}/src/core/instrument/InstrumentRank.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/instrument/SymbolInput.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/instrument/VenueSymbols.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/market/MarketSessions.cpp")

# The symbol system's local instrument index (WP2): migration v060, SymbolMasterRepository,
# the bundled seed and the async InstrumentIndex service, against temp-file SQLite
# databases (so this one suite links Qt Sql). Database / MigrationRunner / SqliteSupport /
# Logger are the storage layer the index runs on; v013 only provides ensure_news_fts(),
# which Database::open() calls (a no-op without the news tables). InstrumentIndex.h is
# listed so AUTOMOC sees its Q_OBJECT.
fincept_add_test(tst_symbol_master_repo
    tst_symbol_master_repo.cpp
    "${PROJECT_SOURCE_DIR}/src/services/instruments/InstrumentIndex.h"
    "${PROJECT_SOURCE_DIR}/src/services/instruments/InstrumentIndex.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/instruments/SymbolSeed.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/repositories/SymbolMasterRepository.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/Database.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/SqliteSupport.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/MigrationRunner.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v013_news_articles.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v060_symbol_master.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/logging/Logger.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/instrument/InstrumentRank.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/instrument/SymbolInput.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/instrument/VenueSymbols.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/market/MarketSessions.cpp")
target_link_libraries(tst_symbol_master_repo PRIVATE Qt6::Sql)

# CustomIndexRepository::get_values: the most recent N levels, oldest first (it
# returned the oldest N), on a temp-file database through Database + migration v011.
fincept_add_test(tst_custom_index_repo
    tst_custom_index_repo.cpp
    "${PROJECT_SOURCE_DIR}/src/storage/repositories/CustomIndexRepository.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/Database.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/SqliteSupport.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/MigrationRunner.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v013_news_articles.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v011_custom_indices.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/logging/Logger.cpp")
target_link_libraries(tst_custom_index_repo PRIVATE Qt6::Sql)

# The symbol system's resolver service and backend-master sync (WP3): the REAL local index
# on a temp-file database (the storage list of tst_symbol_master_repo) plus fake backend /
# Yahoo sources, so no network and no Python. InstrumentResolverSources.cpp (HttpClient,
# PythonRunner) is deliberately NOT listed. Q_OBJECT headers listed for AUTOMOC.
fincept_add_test(tst_instrument_resolver
    tst_instrument_resolver.cpp
    "${PROJECT_SOURCE_DIR}/src/services/instruments/InstrumentResolver.h"
    "${PROJECT_SOURCE_DIR}/src/services/instruments/InstrumentResolver.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/instruments/InstrumentResolverIndex.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/instruments/InstrumentSync.h"
    "${PROJECT_SOURCE_DIR}/src/services/instruments/InstrumentSync.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/instruments/InstrumentIndex.h"
    "${PROJECT_SOURCE_DIR}/src/services/instruments/InstrumentIndex.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/instruments/SymbolSeed.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/repositories/SymbolMasterRepository.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/Database.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/SqliteSupport.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/MigrationRunner.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v013_news_articles.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v060_symbol_master.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/logging/Logger.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/instrument/InstrumentRank.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/instrument/SymbolInput.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/instrument/VenueSymbols.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/market/MarketSessions.cpp")
target_link_libraries(tst_instrument_resolver PRIVATE Qt6::Sql)

# The resolver's backend source: GET /symbols/search first, /market/search when it is not
# deployed (404, decided for the session) or failed (that request only), over a fake
# asynchronous HTTP function. InstrumentResolver.h listed for AUTOMOC.
fincept_add_test(tst_backend_symbol_search
    tst_backend_symbol_search.cpp
    "${PROJECT_SOURCE_DIR}/src/services/instruments/BackendSymbolSearch.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/instruments/InstrumentResolver.h"
    "${PROJECT_SOURCE_DIR}/src/services/instruments/InstrumentResolver.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/instrument/InstrumentRank.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/instrument/SymbolInput.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/instrument/VenueSymbols.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/market/MarketSessions.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/logging/Logger.cpp")

# The shared symbol picker (WP4): its keyboard model, row states, recents and sizes, as QTest
# key / mouse events against the real widget, with a private resolver over the in-memory seed.
# A widget test, so it links Qt Widgets and runs on the offscreen platform. The theme it draws
# with persists one setting through SettingsRepository; tests/stubs shadows that header with
# an in-memory map for THIS target only (the real repository needs the whole storage layer).
fincept_add_test(tst_symbol_search_box
    tst_symbol_search_box.cpp
    "${PROJECT_SOURCE_DIR}/src/ui/widgets/symbol/SymbolSearchBox.h"
    "${PROJECT_SOURCE_DIR}/src/ui/widgets/symbol/SymbolSearchBox.cpp"
    "${PROJECT_SOURCE_DIR}/src/ui/widgets/symbol/SymbolPickerPopup.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/instruments/InstrumentResolver.h"
    "${PROJECT_SOURCE_DIR}/src/services/instruments/InstrumentResolver.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/instruments/SymbolSeed.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/instrument/InstrumentRank.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/instrument/SymbolInput.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/instrument/VenueSymbols.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/market/MarketSessions.cpp"
    "${PROJECT_SOURCE_DIR}/src/ui/theme/ThemeManager.h"
    "${PROJECT_SOURCE_DIR}/src/ui/theme/ThemeManager.cpp"
    "${PROJECT_SOURCE_DIR}/src/ui/theme/ThemePalettes.cpp"
    "${PROJECT_SOURCE_DIR}/src/ui/theme/ThemeRestyle.cpp"
    "${PROJECT_SOURCE_DIR}/src/ui/theme/Theme.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/logging/Logger.cpp")
target_link_libraries(tst_symbol_search_box PRIVATE Qt6::Widgets Qt6::Concurrent)
target_include_directories(tst_symbol_search_box BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/stubs")
set_tests_properties(tst_symbol_search_box PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# src/services/harness — agent-harness detection over fake file systems
# (HarnessDetect.cpp), the adapters' command lines, launch spec, child
# environment and MCP registration forms, the stream parsers against the
# fixtures in data/harness/<id>/ (each with a .meta.json naming its source), and
# sign-in / model-list parsing. The adapters, parsers, detection, launch helpers
# and the coding_agents parsers / types / prompts they wrap are all Qt Core
# only; HarnessRegistry (settings, logger, processes) is NOT listed.
set(_fincept_harness_core_sources
    "${PROJECT_SOURCE_DIR}/src/services/harness/adapters/AdapterSupport.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/harness/adapters/ClaudeAdapter.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/harness/adapters/CodexAdapter.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/harness/adapters/GrokAdapter.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/harness/adapters/GeminiAdapter.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/harness/adapters/CursorAdapter.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/harness/adapters/CopilotAdapter.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/harness/adapters/QwenAdapter.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/harness/parsers/StreamParserSupport.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/harness/parsers/AcpUpdateParser.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/harness/parsers/PlainTextParser.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/harness/parsers/GeminiStreamParser.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/harness/parsers/CursorStreamParser.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/coding_agents/AgentStreamParsers.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/coding_agents/CodingAgentTypes.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/coding_agents/CodingAgentPrompts.cpp")
fincept_add_test(tst_harness_detect
    tst_harness_detect.cpp
    "${PROJECT_SOURCE_DIR}/src/services/harness/HarnessDetect.cpp"
    ${_fincept_harness_core_sources})
fincept_add_test(tst_harness_commands
    tst_harness_commands.cpp
    "${PROJECT_SOURCE_DIR}/src/services/harness/HarnessLaunch.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/process/ProcessTree.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/process/HostExec.cpp"
    ${_fincept_harness_core_sources})
fincept_add_test(tst_harness_parsers
    tst_harness_parsers.cpp
    ${_fincept_harness_core_sources})
fincept_add_test(tst_harness_auth_models
    tst_harness_auth_models.cpp
    ${_fincept_harness_core_sources})

# Terminal AI through a harness (WP6–WP8). All Qt Core only; nothing here runs a
# real CLI or model:
#   tst_model_router         — ModelRouter's rule + fallback decision and the
#                              eligibility rule (API-key auth only, consent,
#                              per-feature data switch), over hand-built states.
#   tst_harness_chat         — the per-call process runner against a FAKE CLI
#                              (the test re-runs itself with FINCEPT_FAKE_HARNESS
#                              and replays data/harness fixtures): stream mapping,
#                              cancel, timeouts, fallback before / after output,
#                              subscription session refused, process pool, chat argv.
#   tst_mcp_run_tokens       — run-scoped MCP tokens (header-only table); the
#                              live-server checks are in McpServerSelfTest.
#   tst_harness_arena_client — Alpha Arena harness client: schema, JSON extraction,
#                              result / timeout mapping (header-only logic).
#   tst_harness_run_executor — Chat Mode harness engine: event / plan mapping
#                              (header-only logic).
fincept_add_test(tst_model_router
    tst_model_router.cpp
    "${PROJECT_SOURCE_DIR}/src/services/llm/ModelRouter.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/harness/HarnessEligibility.cpp")
fincept_add_test(tst_harness_chat
    tst_harness_chat.cpp
    "${PROJECT_SOURCE_DIR}/src/services/harness/HarnessChatRun.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/harness/HarnessProcessPool.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/harness/HarnessLaunch.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/process/ProcessTree.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/process/HostExec.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/harness/HarnessEligibility.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/llm/ModelRouter.cpp"
    ${_fincept_harness_core_sources})
fincept_add_test(tst_mcp_run_tokens
    tst_mcp_run_tokens.cpp)
# The external MCP server's protocol engine over a real loopback socket with a
# fake backend (handshake, errors, sessions, cancellation, deadlines, SSE,
# limits, shutdown, client profiles) and the stdio bridge script when Python is
# found (FINCEPT_TEST_PYTHON or PATH). Needs Qt Network (the engine is sockets).
fincept_add_test(tst_mcp_server_engine
    tst_mcp_server_engine.cpp
    "${PROJECT_SOURCE_DIR}/src/mcp/server/McpWire.cpp"
    "${PROJECT_SOURCE_DIR}/src/mcp/server/McpClientProfile.cpp"
    "${PROJECT_SOURCE_DIR}/src/mcp/server/McpServerEngine.h"
    "${PROJECT_SOURCE_DIR}/src/mcp/server/McpServerEngine.cpp"
    "${PROJECT_SOURCE_DIR}/src/mcp/server/McpServerSelfCheck.cpp"
    "${PROJECT_SOURCE_DIR}/src/mcp/server/ApprovalPolicy.cpp"
    "${PROJECT_SOURCE_DIR}/src/mcp/GeminiSchema.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/logging/Logger.cpp")
find_package(Qt6 ${_FINCEPT_QT_VER_ARGS} QUIET COMPONENTS Network)
target_link_libraries(tst_mcp_server_engine PRIVATE Qt6::Network)
target_compile_definitions(tst_mcp_server_engine PRIVATE FINCEPT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
fincept_add_test(tst_harness_arena_client
    tst_harness_arena_client.cpp
    "${PROJECT_SOURCE_DIR}/src/services/alpha_arena/ArenaDecisionParser.cpp")
fincept_add_test(tst_harness_run_executor
    tst_harness_run_executor.cpp)

# Migration v061 (harness runs) and CodingAgentRepository on top of it: a v052-era
# file migrated through the real Database::open() path, old rows still load,
# an unknown harness id is kept verbatim, the new fields round-trip, v061 is
# idempotent and inspect() calls the result up to date. Temp-file + in-memory
# SQLite, so this one suite links Qt Sql (storage list as tst_symbol_master_repo;
# v013 only provides ensure_news_fts(), which Database::open() calls).
fincept_add_test(tst_harness_runs_repo
    tst_harness_runs_repo.cpp
    "${PROJECT_SOURCE_DIR}/src/services/coding_agents/CodingAgentRepository.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/coding_agents/CodingAgentTypes.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/Database.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/SqliteSupport.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/MigrationRunner.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v013_news_articles.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v052_coding_agents.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v061_harness_runs.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/logging/Logger.cpp")
target_link_libraries(tst_harness_runs_repo PRIVATE Qt6::Sql)

# src/services/updater/UpdateManifest.cpp — the in-app updater's trust root:
# Ed25519 signature check of updates.json against the pinned key, strict
# version parsing / comparison and manifest parsing. Qt Core only; the
# signature check calls the orlp/ed25519 C library directly (the static
# fincept_ed25519 target the app already builds), not the wallet's
# Ed25519Verifier, which pulls in the Logger.
fincept_add_test(tst_update_manifest
    tst_update_manifest.cpp
    "${PROJECT_SOURCE_DIR}/src/services/updater/UpdateManifest.cpp")
target_link_libraries(tst_update_manifest PRIVATE fincept_ed25519)

# src/ui/widgets/AutoLoadPolicy + Requirements: the pure rules of the shared
# auto-load panes (staleness, input signatures, when a view auto-runs, the
# app-wide queue) and the requirement check over a fake environment. Both
# .cpp files are QtCore-only by design (the live environment and the widgets
# are in RequirementsLive.cpp / AutoLoadPane.cpp / AutoLoadGate.cpp).
fincept_add_test(tst_autoload_logic
    tst_autoload_logic.cpp
    "${PROJECT_SOURCE_DIR}/src/ui/widgets/AutoLoadPolicy.cpp"
    "${PROJECT_SOURCE_DIR}/src/ui/widgets/Requirements.cpp")

# src/ui/widgets/EmptyState.h: Spec equality (apply() of the spec already on screen is a no-op)
# and the remembered-spec rule it uses. Header-only — no EmptyState is built, no app sources;
# Qt6::Widgets only for the headers EmptyState.h includes.
fincept_add_test(tst_empty_state_spec
    tst_empty_state_spec.cpp)
target_link_libraries(tst_empty_state_spec PRIVATE Qt6::Widgets)

# ui::EmptyState keyboard order, offscreen: built unparented (before it joins a layout) with DETAILS,
# clickable detail rows, both actions and suggestion rows, then parented / shown / moved between
# windows — no "setTabOrder ... same window" warning, and the reading order holds. Real widget:
# the theme sources + SettingsRepository stub as tst_symbol_search_box.
fincept_add_test(tst_empty_state_tab_chain
    tst_empty_state_tab_chain.cpp
    "${PROJECT_SOURCE_DIR}/src/ui/widgets/EmptyState.h"
    "${PROJECT_SOURCE_DIR}/src/ui/widgets/EmptyState.cpp"
    "${PROJECT_SOURCE_DIR}/src/ui/widgets/LoadingSkeleton.h"
    "${PROJECT_SOURCE_DIR}/src/ui/widgets/LoadingSkeleton.cpp"
    "${PROJECT_SOURCE_DIR}/src/ui/widgets/AutoLoadPolicy.cpp"
    "${PROJECT_SOURCE_DIR}/src/ui/widgets/Requirements.cpp"
    "${PROJECT_SOURCE_DIR}/src/ui/theme/ThemeManager.h"
    "${PROJECT_SOURCE_DIR}/src/ui/theme/ThemeManager.cpp"
    "${PROJECT_SOURCE_DIR}/src/ui/theme/ThemePalettes.cpp"
    "${PROJECT_SOURCE_DIR}/src/ui/theme/ThemeRestyle.cpp"
    "${PROJECT_SOURCE_DIR}/src/ui/theme/Theme.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/logging/Logger.cpp")
target_link_libraries(tst_empty_state_tab_chain PRIVATE Qt6::Widgets Qt6::Concurrent)
target_include_directories(tst_empty_state_tab_chain BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/stubs")
set_tests_properties(tst_empty_state_tab_chain PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# src/ui/widgets/InputMemory.h: the pure rules of the shared input memory
# (input_memory_detail, header-inline, QtCore only); the signature comes from
# AutoLoadPolicy.cpp.
fincept_add_test(tst_input_memory
    tst_input_memory.cpp
    "${PROJECT_SOURCE_DIR}/src/ui/widgets/AutoLoadPolicy.cpp")

# Economics results view, pure half (QtCore only; EconPanelBase runs it on a worker thread): column
# order and newest-first sort, stat cards (projections excluded, interleaved series without cards),
# sort by any column (blanks last), CSV text, and the min/max thinning of chart lines to the pixel
# width (first / last / every bucket extreme kept, zoom window at full resolution).
fincept_add_test(tst_econ_table_analysis
    tst_econ_table_analysis.cpp
    "${PROJECT_SOURCE_DIR}/src/screens/economics/panels/EconTableAnalysis.cpp"
    "${PROJECT_SOURCE_DIR}/src/ui/widgets/AutoLoadPolicy.cpp")

# Backtesting tab (QtCore only): the provider protocol (stderr progress lines, run outcome — a killed,
# cancelled or envelope-less process is never an empty success), the request the screen sends (exact provider
# keys, cost presets, fill timing, risk-free source, Monte Carlo / optimise / walk-forward settings, signature),
# and contract-v2 parsing with v1 fallbacks + the CSV / JSON export text, on the backend's real samples in
# tests/data/backtesting/ (copied from scripts/Analytics/backtesting/tests/data).
fincept_add_test(tst_backtest_protocol
    tst_backtest_protocol.cpp
    "${PROJECT_SOURCE_DIR}/src/services/backtesting/BacktestRunProtocol.cpp")
fincept_add_test(tst_backtest_request
    tst_backtest_request.cpp
    "${PROJECT_SOURCE_DIR}/src/services/backtesting/BacktestRequest.cpp")
fincept_add_test(tst_backtest_result_model
    tst_backtest_result_model.cpp
    "${PROJECT_SOURCE_DIR}/src/services/backtesting/BacktestResultModel.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/backtesting/BacktestExportText.cpp")

# src/ui/widgets/SensitivityGrid.h: the pure rules of the shared sensitivity grid (X2) — colour
# scale position (diverging around the base cell, sequential, direction), export text, axes.
fincept_add_test(tst_sensitivity_grid
    tst_sensitivity_grid.cpp)

# src/ui/charts/ChartRanges.h: the pure range rules of the painted charts (header-only, QtCore) —
# auto-range extent (FanChart fan + extra lines), time → bars for CandleChart::set_visible_range /
# add_band, the centred window for short spans and the candle view clamp.
fincept_add_test(tst_chart_ranges
    tst_chart_ranges.cpp)

# src/ui/charts/TsChartPanes.h: the pure pane rules of ui::TimeSeriesChart (header-only, QtCore) —
# slots of the shown panes, hide / remove refusal of the last shown pane, pane fallback, line width.
fincept_add_test(tst_ts_chart_panes
    tst_ts_chart_panes.cpp)

# src/ui/widgets/FlowLayoutGeometry.h: the pure geometry of ui::FlowLayout (header-only, QtCore) —
# wrapping, exact fit, clamping, skipped items, centred / right-to-left lines, vertical alignment.
fincept_add_test(tst_flow_layout
    tst_flow_layout.cpp)

# src/ui/widgets/FormGridMetrics.h (header-only, std only): compact-form field / span / form widths
# and the responsive inputs-beside-results breakpoint (Derivatives pricer).
fincept_add_test(tst_form_grid_metrics
    tst_form_grid_metrics.cpp)

# src/ui/widgets/KeySortProxy.cpp (QtCore only): typed sort keys (numbers, dates, collated strings),
# nulls and pinned rows last in both orders, text filter across chosen columns, host predicates.
fincept_add_test(tst_key_sort_proxy
    tst_key_sort_proxy.cpp
    "${PROJECT_SOURCE_DIR}/src/ui/widgets/KeySortProxy.cpp")

# Alternative Investments pure maths (header-only, QtCore): live-data prefill (Treasury yield at a
# maturity, nearest futures contract), private credit (yield, duration, expected loss, shocks) and
# allocation & liquidity (ladder, unfunded cover, denominator-effect stress).
fincept_add_test(tst_alt_live_math
    tst_alt_live_math.cpp)
fincept_add_test(tst_alt_credit_math
    tst_alt_credit_math.cpp)
fincept_add_test(tst_alt_allocation_math
    tst_alt_allocation_math.cpp)
# Due-diligence checklist templates: item ids unique and stable (saved statuses are keyed by id).
fincept_add_test(tst_alt_dd_templates
    tst_alt_dd_templates.cpp)
# M&A football field (src/screens/ma_analytics/MaFootballFieldMath.h, header-only): bridge from EV to
# per-share, LBO value band by interpolation, DCF grid range, rows that cannot be converted.
fincept_add_test(tst_ma_football_field
    tst_ma_football_field.cpp)

# Derivatives tab maths (pure C++, std only): BSM Greeks to third order, Black-76, Bachelier,
# CRR binomial with discrete dividends, Barone-Adesi-Whaley, IV solvers, digitals, barriers,
# Asians, Kirk / Margrabe. Published reference values (Haug 2007, Hull) are cited in the test.
fincept_add_test(tst_deriv_pricing
    tst_deriv_pricing.cpp
    "${PROJECT_SOURCE_DIR}/src/services/options/OptionModels.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/options/ExoticPricing.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/options/OptionPricing.cpp")

# Derivatives analytics: SVI fit + arbitrage checks, strategy / scenario maths, par-curve
# bootstrap, and the chain / curve parsers on the recorded fixtures in tests/data/derivatives/.
fincept_add_test(tst_deriv_analytics
    tst_deriv_analytics.cpp
    "${PROJECT_SOURCE_DIR}/src/services/options/VolSurface.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/options/StrategyMath.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/options/ScenarioMath.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/options/RatesMath.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/options/DerivMarketData.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/options/OptionModels.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/options/OptionPricing.cpp")

# Derivatives credit (D8): piecewise-constant hazard bootstrap of a CDS spread curve, risky
# annuity, upfront <-> running spread, CS01 / recovery01 / jump-to-default; Hull and O'Kane
# reference values recomputed independently (cited in the test).
fincept_add_test(tst_deriv_credit
    tst_deriv_credit.cpp
    "${PROJECT_SOURCE_DIR}/src/services/options/CreditMath.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/options/RatesMath.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/options/OptionModels.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/options/OptionPricing.cpp")

# Derivatives position book (D7): migration v065 (applied twice; inspect() up to date on a
# temp file), DerivBookRepository on an in-memory database, the legs JSON codec and the book
# valuation / aggregation (BookMath) on the recorded chain and curve fixtures. Links Qt Sql
# (storage list as tst_harness_runs_repo minus Database.cpp: the repository takes a connection).
fincept_add_test(tst_deriv_book
    tst_deriv_book.cpp
    "${PROJECT_SOURCE_DIR}/src/services/options/BookMath.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/options/DerivMarketData.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/options/VolSurface.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/options/RatesMath.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/options/OptionModels.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/options/OptionPricing.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/repositories/DerivBookRepository.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v065_deriv_book.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/MigrationRunner.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/SqliteSupport.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/logging/Logger.cpp")
target_link_libraries(tst_deriv_book PRIVATE Qt6::Sql)

# Report Builder core (all header-only, Qt Core): block builders from a send-to-report
# payload, data bindings and their JSON round trip (old .fincept files / `reports` rows
# still load), the incremental layout planner, the export chunking rules and the
# library's document operations (rename, duplicate, the closed-report append queue).
fincept_add_test(tst_report_builder_core
    tst_report_builder_core.cpp)

# Report Builder renderer: in-place replace / insert / remove of a block inside a section
# document equals a full rebuild (the page view's incremental re-layout), around tables,
# lists and stats blocks; long sections paginate per page setup; the cover page, numbered
# headings, {{variables}} (marked on screen only), formatted tables and charts as fixed-size
# text objects. Needs Qt Gui (text layout; charts are painted with QPainter — no QtCharts);
# AppPaths comes from a two-function stub.
fincept_add_test(tst_report_renderer
    tst_report_renderer.cpp
    "${PROJECT_SOURCE_DIR}/src/screens/report_builder/ReportRenderer.h"
    "${PROJECT_SOURCE_DIR}/src/screens/report_builder/ReportRenderer.cpp"
    "${PROJECT_SOURCE_DIR}/src/screens/report_builder/ReportChartObject.h"
    "${PROJECT_SOURCE_DIR}/src/screens/report_builder/ReportChartObject.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/report_builder/ReportChartCache.h"
    "${PROJECT_SOURCE_DIR}/src/services/report_builder/ReportChartCache.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/report_builder/ReportChartRenderer.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/report_builder/ReportAssets.cpp"
    "${PROJECT_SOURCE_DIR}/src/ui/charts/ChartAxisTicks.cpp"
    report_stubs/AppPathsStub.cpp)
target_link_libraries(tst_report_renderer PRIVATE Qt6::Widgets)
set_tests_properties(tst_report_renderer PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Report Builder second stage, pure core (header-only, Qt Core): formatted tables (column
# formats, totals, colour scales, JSON round trips, old tables unchanged), {{variables}} and
# the numbered outline, scheduled-regeneration maths (due / missed / on start), stale-save
# revisions, document JSON compatibility, chart specs.
fincept_add_test(tst_report_builder_features
    tst_report_builder_features.cpp)

# Report Builder "Economic calendar" source (services/report_builder/ReportEconCalendar.h): the
# recipe and its {{date}} / period variables, recorded data_economic_calendar connector envelopes
# (no network, no Python), the per-day call plan, table / list / empty / failed states, and the
# Weekly Market Wrap template. Qt Core only.
fincept_add_test(tst_report_econ_calendar
    tst_report_econ_calendar.cpp
    "${PROJECT_SOURCE_DIR}/src/services/data_sources/TabularData.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/data_sources/SourceError.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/connectors/ConnectorEnvelope.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/report_builder/ReportBuilderTemplates.cpp")

# Report Builder worker-thread exports: ZIP writer, .odt writer, Markdown writer (Qt Core + zlib).
fincept_add_test(tst_report_export_formats
    tst_report_export_formats.cpp
    "${PROJECT_SOURCE_DIR}/src/core/report/ReportZipWriter.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/report/ReportOdtWriter.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/report/ReportMarkdownWriter.cpp")
if(WIN32)
    target_link_libraries(tst_report_export_formats PRIVATE Qt6::ZlibPrivate)
else()
    target_link_libraries(tst_report_export_formats PRIVATE ZLIB::ZLIB)
endif()

# src/services/report_builder/ReportChartRenderer — Report Builder charts drawn with QPainter
# (QImage / PNG / vector PDF) from core/report/ReportChartSpec.h: axis ranges, tick maths,
# series / legend, bins, dual axis, deterministic pixels and cache keys. Qt Gui; offscreen.
fincept_add_test(tst_report_chart
    tst_report_chart.cpp
    "${PROJECT_SOURCE_DIR}/src/services/report_builder/ReportChartRenderer.cpp"
    "${PROJECT_SOURCE_DIR}/src/ui/charts/ChartAxisTicks.cpp")
target_link_libraries(tst_report_chart PRIVATE Qt6::Gui)
set_tests_properties(tst_report_chart PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Report Builder AI drafting (RB-7): facts from data blocks, the prompt, the invented-number
# check, the paragraph diff / partial accept, and the proposal session over a FAKE model
# backend (nothing reaches LlmService). Logger linked as tst_logger does.
fincept_add_test(tst_report_ai_draft
    tst_report_ai_draft.cpp
    "${PROJECT_SOURCE_DIR}/src/services/report_builder/ReportAiSession.h"
    "${PROJECT_SOURCE_DIR}/src/services/report_builder/ReportAiSession.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/logging/Logger.cpp")

# The `report` MCP core tool's handlers over a fake backend: reads answer at once, every
# write asks for approval with an exact description and changes nothing when refused,
# arguments are validated, results capped. Widgets only for the approval request header.
fincept_add_test(tst_report_tool
    tst_report_tool.cpp
    "${PROJECT_SOURCE_DIR}/src/mcp/core/ReportTool.cpp")
target_link_libraries(tst_report_tool PRIVATE Qt6::Widgets)

# The `python` MCP tool's run policy (src/mcp/core/PythonScriptPolicy.cpp): orders ask as kind order,
# LIVE, naming the venue the script sends them straight to; other asking scripts are kind code; a
# PAPER run refuses both; credentials never reach the approval dialog. QtCore only.
fincept_add_test(tst_python_script_policy
    tst_python_script_policy.cpp
    "${PROJECT_SOURCE_DIR}/src/mcp/core/PythonScriptPolicy.cpp"
    "${PROJECT_SOURCE_DIR}/src/mcp/server/ApprovalPolicy.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/logging/Logger.cpp")

# The ui_* MCP tool catalogue (schemas, descriptions, byte budgets, strict-client rules) and the
# argument validator every tool call goes through. QtCore only.
fincept_add_test(tst_mcp_ui_catalog
    tst_mcp_ui_catalog.cpp
    "${PROJECT_SOURCE_DIR}/src/mcp/core/CoreToolCatalog.cpp"
    "${PROJECT_SOURCE_DIR}/src/mcp/SchemaValidator.cpp")

# The MCP UI engine (src/mcp/core/Ui*.cpp) on real widgets, offscreen; the terminal is replaced by
# tests/stubs/mcp_ui_host_fake.cpp (UiHost.h + the approval prompt). Refs, editors, tables, batches,
# waits, modal dialogs, approvals / the one-order-step rule, secrets, snapshots, budgets, error codes.
fincept_add_test(tst_mcp_ui_engine
    tst_mcp_ui_engine.cpp
    stubs/mcp_ui_host_fake.cpp
    "${PROJECT_SOURCE_DIR}/src/mcp/core/UiModel.cpp"
    "${PROJECT_SOURCE_DIR}/src/mcp/core/UiRefs.cpp"
    "${PROJECT_SOURCE_DIR}/src/mcp/core/UiItemViews.cpp"
    "${PROJECT_SOURCE_DIR}/src/mcp/core/UiWeb.cpp"
    "${PROJECT_SOURCE_DIR}/src/mcp/core/UiObserve.cpp"
    "${PROJECT_SOURCE_DIR}/src/mcp/core/UiAct.cpp"
    "${PROJECT_SOURCE_DIR}/src/mcp/server/ApprovalPolicy.cpp"
    "${PROJECT_SOURCE_DIR}/src/mcp/core/UiActTarget.cpp"
    "${PROJECT_SOURCE_DIR}/src/mcp/core/UiActEdit.cpp"
    "${PROJECT_SOURCE_DIR}/src/mcp/core/UiActRisk.cpp"
    "${PROJECT_SOURCE_DIR}/src/mcp/core/UiActWeb.cpp"
    "${PROJECT_SOURCE_DIR}/src/mcp/core/UiActDiff.cpp"
    "${PROJECT_SOURCE_DIR}/src/mcp/core/UiRisk.cpp"
    "${PROJECT_SOURCE_DIR}/src/mcp/ResultStore.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/logging/Logger.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/agent/AgentActionScope.cpp")
# UiWeb.cpp drives QWebEngineView pages, so the engine suite needs WebEngine too.
target_link_libraries(tst_mcp_ui_engine PRIVATE Qt6::Widgets Qt6::Charts Qt6::Concurrent
    Qt6::WebEngineCore Qt6::WebEngineWidgets)
set_tests_properties(tst_mcp_ui_engine PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Data Sources / Data Mapping v2: the tabular model, the connector envelope (fincept_connector.py
# contract), the text-format parsers and error classes; the mapping engine (casts, dates, periods,
# per-cell errors, legacy conversion); per-type driver plans, post-processing and AWS SigV4. QtCore only.
fincept_add_test(tst_data_sources_core
    tst_data_sources_core.cpp
    "${PROJECT_SOURCE_DIR}/src/services/data_sources/TabularData.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/data_sources/SourceError.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/data_sources/TableParsers.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/connectors/ConnectorEnvelope.cpp")
fincept_add_test(tst_data_mapping_engine
    tst_data_mapping_engine.cpp
    "${PROJECT_SOURCE_DIR}/src/services/data_sources/TabularData.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/data_sources/SourceError.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/data_sources/TableParsers.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/data_mapping/MappingSpec.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/data_mapping/MappingEngine.cpp")
fincept_add_test(tst_data_source_drivers
    tst_data_source_drivers.cpp
    "${PROJECT_SOURCE_DIR}/src/services/data_sources/TabularData.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/data_sources/SourceError.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/data_sources/TableParsers.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/data_sources/ConnectionProfiles.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/data_sources/RequestSigning.cpp"
    "${PROJECT_SOURCE_DIR}/src/screens/data_sources/ConnectorRegistry.cpp"
    "${PROJECT_SOURCE_DIR}/src/screens/data_sources/connectors/RelationalDatabases.cpp"
    "${PROJECT_SOURCE_DIR}/src/screens/data_sources/connectors/NoSqlDatabases.cpp"
    "${PROJECT_SOURCE_DIR}/src/screens/data_sources/connectors/TimeSeriesDatabases.cpp"
    "${PROJECT_SOURCE_DIR}/src/screens/data_sources/connectors/FileSources.cpp"
    "${PROJECT_SOURCE_DIR}/src/screens/data_sources/connectors/ApiStreaming.cpp"
    "${PROJECT_SOURCE_DIR}/src/screens/data_sources/connectors/CloudStorage.cpp"
    "${PROJECT_SOURCE_DIR}/src/screens/data_sources/connectors/MarketData.cpp"
    "${PROJECT_SOURCE_DIR}/src/screens/data_sources/connectors/SearchWarehouse.cpp"
    "${PROJECT_SOURCE_DIR}/src/screens/data_sources/connectors/AlternativeData.cpp"
    "${PROJECT_SOURCE_DIR}/src/screens/data_sources/connectors/OpenBanking.cpp")
# Catalogue connector input forms: args hints / typed params -> fields -> argument lists. QtCore only.
fincept_add_test(tst_connector_args
    tst_connector_args.cpp
    "${PROJECT_SOURCE_DIR}/src/services/connectors/ConnectorArgs.cpp")
# Connection export / import file: secrets left out or encrypted with a passphrase (PBKDF2 +
# AES-256-GCM), wrong passphrase and tampering rejected, version-1 files read.
fincept_add_test(tst_connection_transfer
    tst_connection_transfer.cpp
    "${PROJECT_SOURCE_DIR}/src/services/data_sources/ConnectionTransfer.cpp")
target_link_libraries(tst_connection_transfer PRIVATE OpenSSL::Crypto)

# ── Core hardening (privacy, SQL logging, secure storage, keys, dock state) ──

# src/core/privacy/LogRedact.h — forwarders to the logger's redaction, which
# Logger.cpp defines.
fincept_add_test(tst_log_redact
    tst_log_redact.cpp
    "${PROJECT_SOURCE_DIR}/src/core/logging/Logger.cpp")

# src/app/DockStateFilter.h — header-only, zero app sources.
fincept_add_test(tst_dock_state_filter
    tst_dock_state_filter.cpp)
# src/app/DockInstanceGuard.h — header-only, zero app sources: one screen per dock
# id (build / join / defer, which duplicate survives, nav.switch_screen payload).
fincept_add_test(tst_dock_instance_guard
    tst_dock_instance_guard.cpp)

# db::statement_summary (the statement name Database writes into failure /
# slow-query log lines): SqliteSupport.cpp + the logger it logs through.
fincept_add_test(tst_sql_statement_summary
    tst_sql_statement_summary.cpp
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/SqliteSupport.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/logging/Logger.cpp")
target_link_libraries(tst_sql_statement_summary PRIVATE Qt6::Sql)

# SecureStorage facade over a FAKE backend + the wrapped-key rule. The test
# supplies make_default_secret_backend(): SecureStorageSqlite.cpp (SQLite,
# OpenSSL, DPAPI) is deliberately not listed.
fincept_add_test(tst_secure_storage
    tst_secure_storage.cpp
    "${PROJECT_SOURCE_DIR}/src/storage/secure/SecureStorage.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/logging/Logger.cpp")

# src/storage/secure/SecretMigration.h (write -> read back -> compare -> only then
# delete the old copy; every branch) + SecureStorage::store_verified over a FAKE backend.
fincept_add_test(tst_secret_migration
    tst_secret_migration.cpp
    "${PROJECT_SOURCE_DIR}/src/storage/secure/SecureStorage.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/logging/Logger.cpp")

# Callers of SecureStorage::read() over the same fake backend: connector key
# presence (Unreadable, never "missing"), the Data Sources -> env-key bridge (no
# write over an unreadable slot), ui::check_requirements ("Could not read secure
# storage — RETRY") and the MCP server's bearer-token rule (McpTokenPolicy.h,
# header-only: never re-minted after a failed read).
fincept_add_test(tst_secure_read_callers
    tst_secure_read_callers.cpp
    "${PROJECT_SOURCE_DIR}/src/storage/secure/SecureStorage.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/logging/Logger.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/connectors/ConnectorCatalog.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/connectors/ConnectorArgs.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/connectors/ConnectorCredentialBridge.cpp"
    "${PROJECT_SOURCE_DIR}/src/ui/widgets/Requirements.cpp")

# Call sites that must not treat a secure-storage read ERROR as "missing":
# the prediction credential store over the same fake backend.
fincept_add_test(tst_secure_read_sites
    tst_secure_read_sites.cpp
    "${PROJECT_SOURCE_DIR}/src/storage/secure/SecureStorage.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/logging/Logger.cpp"
    "${PROJECT_SOURCE_DIR}/src/services/prediction/PredictionCredentialStore.cpp")

# PinManager over the fake SecureStorage backend (the test also stubs
# SecurityAuditLog). PinManager derives keys with OpenSSL's PBKDF2 and falls back
# to QPasswordDigestor (Qt Network); PinManager.h listed for AUTOMOC.
fincept_add_test(tst_pin_manager_storage
    tst_pin_manager_storage.cpp
    "${PROJECT_SOURCE_DIR}/src/auth/PinManager.h"
    "${PROJECT_SOURCE_DIR}/src/auth/PinManager.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/secure/SecureStorage.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/logging/Logger.cpp")
target_link_libraries(tst_pin_manager_storage PRIVATE Qt6::Concurrent Qt6::Network OpenSSL::Crypto)

# src/core/process/ProcessTree — a real child tree (the test re-runs itself as
# the child and grandchild): kill_tree reaches the grandchild, release_tree
# leaves it running, a deleted QProcess reaps it (Windows kill-on-close).
fincept_add_test(tst_process_tree
    tst_process_tree.cpp
    "${PROJECT_SOURCE_DIR}/src/core/process/ProcessTree.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/process/HostExec.cpp")

# src/python/PythonRunner.h, namespace stream_detail (header-inline, QtCore only): which
# channel reaches the stream callback (RunOptions::stream_stdout) and the line splitter.
# No app sources: PythonRunner.cpp itself is deliberately NOT linked.
fincept_add_test(tst_python_stream_lines
    tst_python_stream_lines.cpp)

# src/ui/theme/FontSizeText.h — font sizes written with the clamped-offset
# marker and rewritten across base sizes (9→24→9, 24→9→24, any chain), plus Qt's
# own stylesheet / HTML parsers reading the marker as whitespace (offscreen).
fincept_add_test(tst_font_size_text
    tst_font_size_text.cpp)
target_link_libraries(tst_font_size_text PRIVATE Qt6::Widgets)
set_tests_properties(tst_font_size_text PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# src/ui/navigation/KeyRouting — the routing rule and the application router
# against real QShortcut / QAction / widgets (offscreen platform).
fincept_add_test(tst_key_routing
    tst_key_routing.cpp
    "${PROJECT_SOURCE_DIR}/src/ui/navigation/KeyRouting.cpp")
target_link_libraries(tst_key_routing PRIVATE Qt6::Widgets)
set_tests_properties(tst_key_routing PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# src/core/logging — the logger core (Logger.cpp is the whole unit; LogFields.h,
# LogRedact.h are its headers): fields, redaction, repeat suppression, rate limits,
# transitions, the audit sink, the bounded queue, retention and level overrides.
fincept_add_test(tst_logger
    tst_logger.cpp
    "${PROJECT_SOURCE_DIR}/src/core/logging/Logger.cpp")

# Edition co-existence (community edition beside the separate private build).
# src/core/config/EditionIdentity.h + EditionNames.h — header-only: every derived
# name differs from the private build's, single-instance sockets are per edition
# and per OS user, temp / screenshot names escape the private uninstaller's sweeps.
fincept_add_test(tst_edition_identity
    tst_edition_identity.cpp)
# The same values the app is built with (CMakeLists.txt "Edition identity").
target_compile_definitions(tst_edition_identity PRIVATE
    FINCEPT_EDITION_ID="${FINCEPT_EDITION_ID}"
    FINCEPT_EDITION_DATA_DIR_NAME="${FINCEPT_EDITION_DATA_DIR_NAME}"
    FINCEPT_EDITION_SETTINGS_ORG="${FINCEPT_EDITION_SETTINGS_ORG}"
    FINCEPT_EDITION_SETTINGS_APP="${FINCEPT_EDITION_SETTINGS_APP}"
    FINCEPT_EDITION_DESKTOP_ID="${FINCEPT_EDITION_DESKTOP_ID}"
    FINCEPT_EDITION_FLATPAK_ID="${FINCEPT_EDITION_FLATPAK_ID}"
    FINCEPT_EDITION_BUNDLE_ID="${FINCEPT_EDITION_BUNDLE_ID}"
    FINCEPT_EDITION_LINUX_PACKAGE="${FINCEPT_EDITION_LINUX_PACKAGE}"
    FINCEPT_EDITION_LINUX_PREFIX="${FINCEPT_EDITION_LINUX_PREFIX}"
    FINCEPT_EDITION_TEMP_PREFIX="${FINCEPT_EDITION_TEMP_PREFIX}"
    FINCEPT_EDITION_MCP_SERVER_NAME="${FINCEPT_EDITION_MCP_SERVER_NAME}")
# src/app/EditionRootDecision — whether the com.fincept.terminal folder may be
# adopted, on fake folder trees (fingerprints, links, databases, read-only).
fincept_add_test(tst_edition_root_decision
    tst_edition_root_decision.cpp
    "${PROJECT_SOURCE_DIR}/src/app/EditionRootDecision.cpp")
# Source scan: no new literal of a name shared with the private build in src/ or
# scripts/ (allow-list inside the test).
fincept_add_test(tst_edition_source_guard
    tst_edition_source_guard.cpp)
target_compile_definitions(tst_edition_source_guard PRIVATE FINCEPT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

# Source guard: every order / money / account control stays marked for AI agents
# (fincept_agent_risk — src/core/agent/AgentRiskMarkers.h). Pinned by file + expression + kind.
fincept_add_test(tst_agent_risk_marker_guard
    tst_agent_risk_marker_guard.cpp)
target_compile_definitions(tst_agent_risk_marker_guard PRIVATE FINCEPT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

# src/network/http/HttpOffThread.h — header-only, QtCore only: HttpClient's large-body
# path (get_large / post_large). Parse classification as get() reports it, the parse off
# the calling thread, delivery on the context's thread, none once the context is gone.
fincept_add_test(tst_http_off_thread
    tst_http_off_thread.cpp)

# src/network/http/HttpClient.{h,cpp} — the opt-in forms of the write verbs, end to end on a
# loopback socket (no outside network): put_large / patch_large / del_large send what the
# plain verb sends and answer what it answers, on the context's thread, never once the
# context is gone; post_raw / put_raw / patch_raw / del_raw hand a 2xx body over as it
# arrived ("ok", 204 empty); the 401 session re-send and the auth / credit / IP-block
# signals. HttpClient's two telemetry calls (ClientInfo, UsageReporter) are stubbed in the
# test. Needs Qt Network.
fincept_add_test(tst_http_client_forms
    tst_http_client_forms.cpp
    "${PROJECT_SOURCE_DIR}/src/network/http/HttpClient.h"
    "${PROJECT_SOURCE_DIR}/src/network/http/HttpClient.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/logging/Logger.cpp")
find_package(Qt6 ${_FINCEPT_QT_VER_ARGS} QUIET COMPONENTS Network)
target_link_libraries(tst_http_client_forms PRIVATE Qt6::Network)

# src/trading/PaperBookSymbol.h — header-only, pure (Qt Core): which paper-book row a symbol
# means (bare or "EXCH:SYM"), the one rule UnifiedTrading's close_position and
# place_smart_order (a workflow flatten on a local-paper account) find a position with.
fincept_add_test(tst_paper_book_symbol
    tst_paper_book_symbol.cpp)

# src/app/StartupPrewarm.h — header-only: the start-up pre-warm names free topics only
# (nothing that costs credits runs automatically), and main.cpp still filters through it.
fincept_add_test(tst_startup_prewarm
    tst_startup_prewarm.cpp)
target_compile_definitions(tst_startup_prewarm PRIVATE FINCEPT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

# src/services/notifications/providers/DeliveryOutcome.{h,cpp} — pure (Qt Core): delivered /
# failed(reason) from HTTP status, Content-Type and body for every notification provider (2xx is
# delivered whatever the body — Slack "ok", Discord 204 — plus each service's error-in-200
# convention); reasons scrubbed with log_scrub() (Logger.cpp).
fincept_add_test(tst_notification_delivery
    tst_notification_delivery.cpp
    "${PROJECT_SOURCE_DIR}/src/services/notifications/providers/DeliveryOutcome.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/logging/Logger.cpp")

# src/services/llm/ModelListParse.{h,cpp} — pure (Qt Core): a provider's model-list answer
# (OpenAI, Anthropic, OpenRouter-sized, Ollama, Gemini, Fincept) -> the sorted ids, as
# LlmService::fetch_models() runs it on a worker.
fincept_add_test(tst_model_list_parse
    tst_model_list_parse.cpp
    "${PROJECT_SOURCE_DIR}/src/services/llm/ModelListParse.cpp")

# src/screens/equity_research/EquityFinancialsAnalytics.{h,cpp} — pure (Qt Core): every ratio,
# growth, structure, waterfall / cash bridge, KPI and score (Piotroski, Altman Z / Z'', Beneish)
# of the Financials tab against hand-computed fixtures (industrial annual / quarterly / TTM, bank,
# loss-maker, sparse filing). EquityStatementModel.cpp builds the period columns (Qt Core only).
fincept_add_test(tst_equity_financials_analytics
    tst_equity_financials_analytics.cpp
    "${PROJECT_SOURCE_DIR}/src/screens/equity_research/EquityFinancialsAnalytics.cpp"
    "${PROJECT_SOURCE_DIR}/src/screens/equity_research/EquityStatementModel.cpp")

# One target that builds every suite and nothing of the app:
#   cmake --build <build> --target fincept_unit_tests && ctest --test-dir <build>
# (CI uses this, so a new suite is picked up without touching the workflow.)
get_property(_fincept_test_targets GLOBAL PROPERTY FINCEPT_TEST_TARGETS)
add_custom_target(fincept_unit_tests DEPENDS ${_fincept_test_targets})
set_target_properties(fincept_unit_tests PROPERTIES FOLDER "tests")
list(JOIN _fincept_test_targets ", " _fincept_test_list)
message(STATUS "Fincept tests (target fincept_unit_tests): ${_fincept_test_list}")
