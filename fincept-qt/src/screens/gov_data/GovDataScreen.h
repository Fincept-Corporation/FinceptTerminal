// src/screens/gov_data/GovDataScreen.h
#pragma once

#include "screens/IStatefulScreen.h"

#include <QFrame>
#include <QHideEvent>
#include <QLabel>
#include <QListWidget>
#include <QShowEvent>
#include <QStackedWidget>
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

  private slots:
    void on_provider_selected(int row);

  private:
    void     build_ui();
    QWidget* build_sidebar();
    QWidget* build_toolbar();
    QWidget* build_status_bar();
    void     activate_provider(int index);

    QListWidget*    provider_list_   = nullptr;
    QStackedWidget* panel_stack_     = nullptr;
    QLabel*         header_title_    = nullptr;
    QLabel*         header_subtitle_ = nullptr;
    QWidget*        header_bar_      = nullptr;
    QLabel*         status_portal_   = nullptr;
    QLabel*         status_country_  = nullptr;
    QLabel*         status_datasets_ = nullptr;

    int  active_index_      = -1;
    bool initial_load_done_ = false;
};

} // namespace fincept::screens
