// src/screens/calendars/CalendarsScreen.h
#pragma once
#include "screens/common/ISectionLinked.h"
#include "screens/common/IStatefulScreen.h"

#include <QVector>
#include <QWidget>

class QLabel;
class QTabWidget;

namespace fincept::screens {

class CalendarPanel;

/// CALENDARS — earnings, IPOs, stock splits and economic events from the Fincept
/// /data API, one sub-tab each (CalendarPanel). A sub-tab loads the first time it is
/// shown; the screen itself sends nothing until it is opened.
class CalendarsScreen : public QWidget, public IStatefulScreen, public ISectionLinked {
    Q_OBJECT
  public:
    explicit CalendarsScreen(QWidget* parent = nullptr);

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return QStringLiteral("calendars"); }

    /// ISectionLinked — "earnings" | "ipos" | "splits" | "economic" (the sub-tab keys; also as
    /// "tab:<key>"). Selecting a tab only shows it: nothing is requested (LOAD stays the user's).
    bool open_section(const QString& section, const QString& focus) override;

  public slots:
    /// F5 / View ▸ Refresh (WindowFrame::refresh_focused_panel): the sub-tab on screen.
    void refresh();

  protected:
    void showEvent(QShowEvent* event) override;
    void changeEvent(QEvent* event) override;

  private:
    void build_ui();
    void retranslate();
    /// Load the visible sub-tab if it has not loaded yet.
    void activate_current();

    QLabel* title_label_ = nullptr;
    QLabel* hint_label_ = nullptr;
    QTabWidget* tabs_ = nullptr;
    QVector<CalendarPanel*> panels_;
};

} // namespace fincept::screens
