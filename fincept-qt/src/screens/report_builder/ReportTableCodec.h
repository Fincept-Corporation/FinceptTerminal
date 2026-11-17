#pragma once
// Moved to core/report/ReportTableCodec.h so services and other tabs can build
// tables without including screen code. This shim keeps the old spelling working.

#include "core/report/ReportTableCodec.h"

namespace fincept::screens::report_table {
using ::fincept::report::table::encode;
using ::fincept::report::table::encode_cell;
using ::fincept::report::table::parse;
} // namespace fincept::screens::report_table
