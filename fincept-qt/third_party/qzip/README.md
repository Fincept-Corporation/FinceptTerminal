# Vendored QZipReader / QZipWriter

Qt removed the private `QZipReader` / `QZipWriter` classes from QtCore in **Qt 6.13**
(see FinceptTerminal issue #347). QXlsx — our Excel (.xlsx) backend, including its
upstream `master` — still `#include <private/qzipreader_p.h>`, so it fails to build
against Qt ≥ 6.13.

Per the Qt maintainer's guidance (option 1: *copy the code into the project*), this
directory vendors Qt's own zip implementation so the **Excel tab keeps working** on
Qt 6.13+.

## Contents / provenance
- `qzip.cpp` — from `qt/qtbase`, branch `6.8`, `src/corelib/io/qzip.cpp` (verbatim).
- `private/qzipreader_p.h`, `private/qzipwriter_p.h` — from Qt 6.8.3 SDK. Two local
  edits only: `#include <QtCore/private/qglobal_p.h>` → `<QtCore/qglobal.h>` (drop the
  private-header dependency), and `Q_CORE_EXPORT` removed from the class declarations
  (this is a plain static lib, not a Qt module — no dllimport/dllexport).

License: `LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only`
(the same terms under which the project already ships Qt).

## Wiring (see fincept-qt/CMakeLists.txt)
Built into a `fincept_qzip` static lib and linked into QXlsx **only** when
`Qt6_VERSION >= 6.13` (or `-DFINCEPT_FORCE_VENDOR_QZIP=ON` to test the path on older
Qt). On Qt < 6.13 it is completely inert — Qt still provides the class.

## Updating
If Qt's on-disk zip format handling changes, refresh `qzip.cpp` from the matching
`qt/qtbase` branch and re-apply the two header edits above.
