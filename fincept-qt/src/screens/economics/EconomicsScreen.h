// src/screens/economics/EconomicsScreen.h
// Economics Data Explorer — panel-based shell.
// Hosts 32 per-source panels in a QStackedWidget.
// Left source-badge bar drives panel switching.
// Each panel is lazy-constructed on first activation.
#pragma once

#include "screens/IStatefulScreen.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMap>
#include <QScrollArea>
#include <QStackedWidget>
#include <QWidget>

namespace fincept::ui { struct ThemeTokens; }

namespace fincept::screens {

class EconPanelBase;

class EconomicsScreen : public QWidget, public IStatefulScreen {
    Q_OBJECT
  public:
    explicit EconomicsScreen(QWidget* parent = nullptr);

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "economics"; }
    int state_version() const override { return 1; }

  private:
    void build_ui();
    void switch_to(const QString& source_id);
    void refresh_theme();

    // badge bar + stacked panels
    QWidget*        header_      = nullptr;
    QLabel*         title_       = nullptr;
    QLabel*         subtitle_    = nullptr;
    QScrollArea*    scroll_      = nullptr;
    QWidget*        badge_bar_   = nullptr;
    QStackedWidget* stack_       = nullptr;

    // source_id → (badge widget, panel widget or nullptr if not yet built)
    struct SourceEntry {
        QString      id;
        QString      label;
        QString      color;
        QWidget*     badge   = nullptr;
        EconPanelBase* panel = nullptr;
    };
    QList<SourceEntry> sources_;
    QString            active_id_;

    EconPanelBase* get_or_create_panel(SourceEntry& entry);
};

} // namespace fincept::screens
