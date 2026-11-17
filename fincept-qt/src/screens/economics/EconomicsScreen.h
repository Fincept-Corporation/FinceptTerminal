// src/screens/economics/EconomicsScreen.h
// Economics Data Explorer — panel-based shell.
// Hosts 33 per-source panels in a QStackedWidget.
// Left source-badge bar drives panel switching.
// Each panel is lazy-constructed on first activation.
#pragma once

#include "screens/common/ISectionLinked.h"
#include "screens/common/IStatefulScreen.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMap>
#include <QScrollArea>
#include <QStackedWidget>
#include <QWidget>

namespace fincept::ui {
struct ThemeTokens;
}

namespace fincept::screens {

class EconPanelBase;

class EconomicsScreen : public QWidget, public IStatefulScreen, public ISectionLinked {
    Q_OBJECT
  public:
    explicit EconomicsScreen(QWidget* parent = nullptr);

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "economics"; }
    int state_version() const override { return 1; }

    /// ISectionLinked — a source panel by its id ("fred", "imf", "cftc" … — the badge ids; also
    /// "panel:<id>"), and Market Models' views as "market_models/<view>" (cycle | erp | curve; or the
    /// view as focus). The badge's own switch: a free panel shows its remembered result or loads on
    /// show; a billed one keeps its own RUN gate.
    bool open_section(const QString& section, const QString& focus) override;

  protected:
    void changeEvent(QEvent* event) override;
    /// The first source panel is built on the first show (not in the constructor), so a restored
    /// session builds only the panel it opens on.
    void showEvent(QShowEvent* event) override;
    // Turns an ordinary (vertical) mouse wheel over the source bar into sideways scrolling: the bar
    // holds more sources than fit, and it has no scroll bar of its own.
    bool eventFilter(QObject* watched, QEvent* event) override;

  private:
    void build_ui();
    void switch_to(const QString& source_id);
    /// One selector stylesheet for the header, badge bar and stack — set once, before the children
    /// exist; ThemeManager's re-style pass keeps it in step with theme and font changes.
    QString screen_style() const;
    /// Header / badge heights follow the font size (re-fitted once an appearance change settled).
    void fit_to_font();
    void retranslateUi();

    // badge bar + stacked panels
    QWidget* header_ = nullptr;
    QLabel* title_ = nullptr;
    QLabel* subtitle_ = nullptr;
    QScrollArea* scroll_ = nullptr;
    QWidget* badge_bar_ = nullptr;
    QStackedWidget* stack_ = nullptr;

    // source_id → (badge widget, panel widget or nullptr if not yet built)
    struct SourceEntry {
        QString id;
        QString label;
        QString color;
        QWidget* badge = nullptr;
        EconPanelBase* panel = nullptr;
    };
    QList<SourceEntry> sources_;
    QString active_id_;
    // Saved per-panel state ("<id>_panel") for panels that have not been built yet (panels are
    // lazy). Applied when the panel is created, and written back by save_state() so a panel the
    // user hasn't opened this session keeps the state it was saved with.
    QVariantMap pending_panel_states_;

    EconPanelBase* get_or_create_panel(SourceEntry& entry);
};

} // namespace fincept::screens
