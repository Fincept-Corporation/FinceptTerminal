#pragma once
#include "services/news/NewsService.h"

#include <QDialog>
#include <QEvent>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVector>

namespace fincept::screens {

/// Modal dialog letting the user add, edit, disable, and remove RSS feeds.
///
/// Built-in feeds (from `NewsService::default_feeds()`) can be disabled or
/// have their fields overridden; "Reset" deletes the overlay row so the
/// built-in default reappears unchanged on next load.
/// User-added feeds (id prefixed "usr-") can be edited or deleted outright.
///
/// Mutations go through `NewsService` (which writes to `RssFeedRepository`
/// and emits `feeds_changed`); the dialog itself doesn't talk to the DB.
class RssFeedManagerDialog : public QDialog {
    Q_OBJECT
  public:
    explicit RssFeedManagerDialog(QWidget* parent = nullptr);

    /// True if the user mutated any feed during this session — caller
    /// (NewsScreen) uses this to decide whether to trigger a refresh.
    bool changed() const { return dirty_; }

  protected:
    void changeEvent(QEvent* event) override;

  private slots:
    void on_add();
    void on_edit();
    void on_delete_or_reset();
    void on_toggle_enabled();
    void on_test_url();
    void on_selection_changed();

  private:
    void build_ui();
    void reload_table();
    int selected_row() const;

    /// Re-apply tr() lookups to every widget whose text we keep a handle to.
    /// Called from changeEvent() on QEvent::LanguageChange.
    void retranslateUi();

    QLabel* header_label_ = nullptr;
    QTableWidget* table_ = nullptr;
    QPushButton* add_btn_ = nullptr;
    QPushButton* edit_btn_ = nullptr;
    QPushButton* delete_btn_ = nullptr;
    QPushButton* toggle_btn_ = nullptr;
    QPushButton* test_btn_ = nullptr;
    QPushButton* close_btn_ = nullptr;
    QLabel* status_label_ = nullptr;

    QVector<services::NewsService::EditorFeed> rows_;
    bool dirty_ = false;
};

} // namespace fincept::screens
