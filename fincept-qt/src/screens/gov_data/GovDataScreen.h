// src/screens/gov_data/GovDataScreen.h
#pragma once

#include "screens/common/IStatefulScreen.h"

#include <QFrame>
#include <QHideEvent>
#include <QLabel>
#include <QListWidget>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QVector>
#include <QWidget>

namespace fincept::screens {

class GovDataScreen : public QWidget, public IStatefulScreen {
    Q_OBJECT
  public:
    explicit GovDataScreen(QWidget* parent = nullptr);

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "gov_data"; }
    int state_version() const override { return 1; }

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;

  private slots:
    void on_provider_selected(int row);
    void refresh_theme();

  private:
    void build_ui();
    QWidget* build_sidebar();
    QWidget* build_toolbar();
    QWidget* build_status_bar();
    /// Construct (and register in panel_stack_) the panel for provider `index`.
    /// Called lazily on first activation — see P2 in CLAUDE.md.
    QWidget* make_panel(int index);
    void activate_provider(int index);
    void retranslateUi();

    QListWidget* provider_list_ = nullptr;
    QStackedWidget* panel_stack_ = nullptr;
    /// provider index → panel widget, nullptr until first activation.
    QVector<QWidget*> panels_;
    QLabel* header_title_ = nullptr;
    QLabel* header_subtitle_ = nullptr;
    QWidget* header_bar_ = nullptr;
    QLabel* status_portal_ = nullptr;
    QLabel* status_country_ = nullptr;
    QLabel* provider_badge_ = nullptr;
    QLabel* sidebar_count_ = nullptr;
    QLabel* sidebar_title_ = nullptr;
    QLabel* status_govt_lbl_ = nullptr;
    QLabel* status_portal_lbl_ = nullptr;
    QLabel* status_country_lbl_ = nullptr;
    QLabel* status_ready_lbl_ = nullptr;

    QString active_provider_color_;
    int active_index_ = -1;
    bool initial_load_done_ = false;
};

} // namespace fincept::screens
