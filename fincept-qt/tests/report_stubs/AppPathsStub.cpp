// AppPathsStub.cpp — the two AppPaths folders the Report Builder renderer uses, for
// tst_report_renderer only (the real AppPaths pulls in ProfileManager and the profile
// layout). Everything goes under the system temp folder.

#include "core/config/AppPaths.h"

#include <QDir>

namespace fincept {

QString AppPaths::data() {
    return QDir::tempPath() + QStringLiteral("/fincept_tst_report/data");
}

QString AppPaths::cache() {
    return QDir::tempPath() + QStringLiteral("/fincept_tst_report/cache");
}

} // namespace fincept
