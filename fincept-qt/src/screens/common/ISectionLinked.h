#pragma once
#include <QString>

namespace fincept::screens {

/// Opt-in interface for screens that can be opened at one of their sub-tabs /
/// panels — the generic deep link behind nav.switch_screen {"screen_id": ...,
/// "section": ..., "focus": ...} (WindowFrame), used by the command bar.
///
/// Usage:
///   class MyScreen : public QWidget, public ISectionLinked { ... };
///
/// Header-only; no Q_OBJECT. Settings keeps its own open_section() (it predates
/// this interface) and is served first by WindowFrame.
class ISectionLinked {
  public:
    virtual ~ISectionLinked() = default;

    /// Switch to a sub-tab / panel. `section` is a stable lower-case id published
    /// by the screen (e.g. "runs", "panel:fred", "module:garch/view"); `focus`
    /// optionally names a control or item inside it. Returns false for an
    /// unknown section — the caller then leaves the screen as it opened. Called
    /// on the GUI thread after the screen is materialised and shown.
    virtual bool open_section(const QString& section, const QString& focus) = 0;
};

} // namespace fincept::screens
