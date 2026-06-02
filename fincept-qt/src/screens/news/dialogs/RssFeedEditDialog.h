#pragma once
#include "services/news/NewsService.h"

#include <QComboBox>
#include <QDialog>
#include <QEvent>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>

namespace fincept::screens {

/// Small modal for adding a new feed or editing an existing one.
/// Includes an async "Test URL" button that fetches the URL and reports
/// whether the response looks like RSS/Atom XML.
class RssFeedEditDialog : public QDialog {
    Q_OBJECT
  public:
    /// `is_builtin_id` should be true when editing a built-in feed — the
    /// id field is then read-only (the id is the join key against
    /// default_feeds()).
    RssFeedEditDialog(const services::RSSFeed& initial, bool is_builtin_id, QWidget* parent = nullptr);

    services::RSSFeed feed() const;

  protected:
    void changeEvent(QEvent* event) override;

  private slots:
    void on_test();
    void try_accept();

  private:
    void build_ui();

    /// Re-apply tr() lookups to every widget whose text we keep a handle to.
    /// Called from changeEvent() on QEvent::LanguageChange.
    void retranslateUi();

    services::RSSFeed initial_;
    bool is_builtin_id_;

    QFormLayout* form_ = nullptr;
    QLineEdit* id_input_ = nullptr;
    QLineEdit* name_input_ = nullptr;
    QLineEdit* url_input_ = nullptr;
    QLineEdit* source_input_ = nullptr;
    QComboBox* category_combo_ = nullptr;
    QComboBox* region_combo_ = nullptr;
    QSpinBox* tier_spin_ = nullptr;

    QPushButton* test_btn_ = nullptr;
    QPushButton* ok_btn_ = nullptr;
    QPushButton* cancel_btn_ = nullptr;
    QLabel* test_status_ = nullptr;

    bool last_test_ok_ = false;
    bool test_run_ = false;
};

} // namespace fincept::screens
