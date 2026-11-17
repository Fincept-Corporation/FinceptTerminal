#pragma once
// ReportAssets — pictures of a report (image blocks, the cover logo) travel WITH the report.
//
// Images are files on this computer (the Files store, a dropped file, an image another tab
// sent, kept content-addressed in <profile data>/report_assets/<sha1>.<ext>). A report
// synced to another computer through the library would lose them, so:
//   * embed_for_sync() — on the library worker, before a row is written: every picture up
//     to kMaxEmbedBytes (and kMaxEmbedTotal per report) is stored inside the block as
//     config["image_b64"] with its content address config["asset"]. The open document is
//     not touched; only the stored row carries the bytes.
//   * materialize() — on a worker, when a report is loaded (library, file): embedded
//     pictures missing on this computer are written into report_assets/ again.
//   * A picture too large to embed is marked "not synced" on the other computer (the page
//     says so; exports leave the block out) instead of showing a broken block.
// Qt Core + file I/O; callable from any thread (no shared mutable state).

#include "core/report/ReportDocument.h"

#include <QJsonObject>
#include <QString>

namespace fincept::services::report_assets {

inline constexpr qint64 kMaxEmbedBytes = 768 * 1024;      ///< one picture
inline constexpr qint64 kMaxEmbedTotal = 4 * 1024 * 1024; ///< all pictures of one report

inline constexpr char kB64Key[] = "image_b64";
inline constexpr char kAssetKey[] = "asset";

/// <profile data>/report_assets
QString assets_dir();

/// True for blocks that show a picture file (image, cover logo).
bool has_picture(const report::ReportComponent& c);

/// The picture file to draw: config path when it exists, else report_assets/<asset> when it
/// exists; empty otherwise.
QString picture_path(const report::ReportComponent& c);

/// The block names a picture that is not on this computer and carries no embedded copy:
/// it was added on another computer and was too large to sync.
bool picture_not_synced(const report::ReportComponent& c);

/// Worker: embed pictures into the report JSON `root` (components[].config). Returns the
/// number embedded now. Blocks that already carry a copy of the same asset are left as they are.
int embed_for_sync(QJsonObject& root);

/// Worker: write embedded pictures missing on this computer into assets_dir(). Returns the
/// number written.
int materialize(const report::ReportDocument& doc);

/// Write one block's embedded picture now when its file is missing (rare: a document not
/// loaded through materialize()). Returns the path, or empty.
QString materialize_one(const report::ReportComponent& c);

} // namespace fincept::services::report_assets
