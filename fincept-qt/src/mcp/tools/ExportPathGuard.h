#pragma once
// ExportPathGuard.h — one confinement check for every MCP tool that writes (or
// reads) a caller-supplied filesystem path.
//
// Why this exists: `destination_path` / `path` on the export tools is raw
// model-supplied text. Four tools took it unvalidated —
// `download_managed_file`, `export_excel_sheet_csv`, `export_layout` /
// `import_layout`, and `report_export_pdf` — which handed the LLM arbitrary
// write, and (because `download_managed_file` called `QFile::remove(dest)`
// unconditionally, purely to work around `QFile::copy` refusing to overwrite)
// arbitrary *delete* as well. `destination_path="~/.ssh/id_rsa"` deleted the
// key and then reported "Copy failed". Chained with `write_managed_text_file`
// — whose bytes the model authors — it was arbitrary write-what-where, e.g. a
// .cmd dropped into the Startup folder for code execution at next login.
//
// The rule: every model-supplied path must resolve inside a single export
// directory. Canonicalise the PARENT (the destination file does not exist
// yet, so `QFileInfo(dest).canonicalFilePath()` would be empty) and require it
// to be the root itself or a descendant. Compare with a trailing separator, or
// `<root>_evil` passes a naive `startsWith(root)`.
//
// Relative paths are resolved against the export root rather than rejected —
// the model naturally writes `path="TSLA.csv"`, and silently resolving that
// against the process CWD is exactly the class of bug this guard exists to
// stop.

#include "core/config/AppPaths.h"

#include <QDir>
#include <QFileInfo>
#include <QString>

namespace fincept::mcp::tools {

/// The one directory model-supplied paths may touch: `<AppPaths::root()>/exports`.
/// Created on first use so canonicalPath() below always resolves.
inline QString export_root() {
    const QString root = QDir(AppPaths::root()).absoluteFilePath(QStringLiteral("exports"));
    QDir().mkpath(root);
    const QString canonical = QDir(root).canonicalPath();
    return canonical.isEmpty() ? QDir(root).absolutePath() : canonical;
}

struct ExportPath {
    QString path;  // absolute destination — only valid when error is empty
    QString error; // human-readable refusal, ready to hand to ToolResult::fail
    bool ok() const { return error.isEmpty(); }
};

/// Confine `requested` to export_root(). Relative paths are resolved against
/// the root; absolute paths must already live inside it. The parent directory
/// must exist — we deliberately do not mkpath() an attacker-chosen tree.
inline ExportPath resolve_export_path(const QString& requested) {
    ExportPath out;

    const QString raw = QDir::fromNativeSeparators(requested).trimmed();
    if (raw.isEmpty()) {
        out.error = QStringLiteral("Missing path");
        return out;
    }

    const QString root = export_root();
    if (root.isEmpty()) {
        out.error = QStringLiteral("Export directory is unavailable");
        return out;
    }

    // Relative → anchor at the root. Absolute → take as-is and let the
    // containment check below decide.
    const QString absolute =
        QFileInfo(raw).isAbsolute() ? QDir::cleanPath(raw) : QDir(root).absoluteFilePath(QDir::cleanPath(raw));

    const QFileInfo fi(absolute);
    if (fi.fileName().isEmpty()) {
        out.error = QStringLiteral("Path must name a file, not a directory");
        return out;
    }

    // Canonicalise the PARENT: the file itself may not exist yet, and
    // canonicalPath() on a non-existent path returns empty. This is also what
    // resolves `..` segments and symlinks that would otherwise escape.
    const QString parent = QDir(fi.absolutePath()).canonicalPath();
    if (parent.isEmpty()) {
        out.error = QStringLiteral("Destination directory does not exist. Write inside the export directory: ") + root;
        return out;
    }

#ifdef Q_OS_WIN
    constexpr Qt::CaseSensitivity kCase = Qt::CaseInsensitive;
#else
    constexpr Qt::CaseSensitivity kCase = Qt::CaseSensitive;
#endif
    // Trailing '/' matters: without it `<root>_evil` is a prefix match. Qt
    // canonical paths always use '/', including on Windows — do NOT use
    // QDir::separator() here, it is '\\' on Windows and would never match.
    const bool inside = parent.compare(root, kCase) == 0 ||
                        parent.startsWith(root + QLatin1Char('/'), kCase);
    if (!inside) {
        out.error = QStringLiteral("Path must be inside the export directory (") + root +
                    QStringLiteral("). Absolute paths elsewhere on disk are refused.");
        return out;
    }

    out.path = QDir(parent).absoluteFilePath(fi.fileName());
    return out;
}

} // namespace fincept::mcp::tools
