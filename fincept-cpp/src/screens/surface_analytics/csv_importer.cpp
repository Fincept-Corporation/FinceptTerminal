// csv_importer.cpp — split into focused files.
// All implementations live in:
//   csv_importer_core.cpp             — schema table, get_csv_schema, parse_csv_file, derivatives vol importers
//   csv_importer_rates.cpp            — fixed income / rates importers (yield_curve through credit_trans)
//   csv_importer_commodities_risk.cpp — commodities and risk analytics importers
//   csv_importer_macro.cpp            — macro/equity analytics importers (liquidity through monetary)
//   csv_importer_helpers.h            — private static helpers (trim, col_idx, to_f, etc.)
