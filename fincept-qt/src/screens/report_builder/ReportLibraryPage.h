#pragma once
// ReportLibraryPage — the Report Builder's start page and report library.
//
// So the tab never opens on a blank page: the report that is open (CONTINUE), templates
// with previews (live-data templates first, the user's own templates, the built-ins), NEW,
// IMPORT, and every saved report (the `reports` table, synced when cloud sync is on) with
// search, open, rename, duplicate, export and delete with undo. All library I/O is
// asynchronous (services::ReportLibrary); lists are model/view.

#include "services/report_builder/ReportLibrary.h"

#include <QVariantMap>
#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QListView;
class QPushButton;
class QStackedWidget;
class QStandardItemModel;
class QTableView;
class QTimer;

namespace fincept::ui {
class EmptyState;
class SymbolSearchBox;
} // namespace fincept::ui

namespace fincept::screens {

class RbLibraryModel;
class RbWireframe;

class ReportLibraryPage : public QWidget {
    Q_OBJECT
  public:
    explicit ReportLibraryPage(QWidget* parent = nullptr);

    /// The report open in the editor ("Continue" card); id 0 = not in the library yet.
    void set_current(qint64 library_id, const QString& title, int blocks);
    /// Re-read reports and templates (asynchronously).
    void reload();

  signals:
    void continue_requested();
    void open_requested(qint64 report_id);
    void new_requested();
    void import_requested();
    /// `template_id`: a built-in name, or "user:<id>"; `params` = {"symbol"} / {"portfolio_id"}.
    void template_requested(const QString& template_id, const QVariantMap& params);
    /// A library row was deleted for good (after the undo period) / its delete undone.
    void report_deleted(qint64 id);
    void report_restored(qint64 id);

  protected:
    void showEvent(QShowEvent* e) override;
    void changeEvent(QEvent* e) override;

  private:
    void build_ui();
    void reload_reports();
    void reload_templates();
    void on_template_selected();
    void on_report_selected();
    void create_from_template();
    void delete_selected();
    void export_selected();
    void rename_selected();
    void retranslateUi();
    qint64 selected_report_id() const;
    services::LibraryEntry selected_entry() const;

    QLabel* title_ = nullptr;
    QLineEdit* search_ = nullptr;
    QPushButton* new_btn_ = nullptr;
    QPushButton* import_btn_ = nullptr;
    QPushButton* continue_btn_ = nullptr;
    QLabel* current_lbl_ = nullptr;

    QLabel* tpl_heading_ = nullptr;
    QComboBox* tpl_filter_ = nullptr;
    QListView* tpl_view_ = nullptr;
    QStandardItemModel* tpl_model_ = nullptr;
    QLabel* tpl_desc_ = nullptr;
    QWidget* tpl_param_row_ = nullptr;
    QLabel* tpl_param_lbl_ = nullptr;
    ui::SymbolSearchBox* tpl_symbol_ = nullptr;
    QComboBox* tpl_portfolio_ = nullptr;
    QPushButton* tpl_create_btn_ = nullptr;
    QString tpl_symbol_value_;

    QLabel* rep_heading_ = nullptr;
    QStackedWidget* rep_stack_ = nullptr;
    QTableView* rep_view_ = nullptr;
    RbLibraryModel* rep_model_ = nullptr;
    ui::EmptyState* rep_empty_ = nullptr;
    RbWireframe* preview_ = nullptr;
    QLineEdit* rename_edit_ = nullptr;
    QLabel* preview_info_ = nullptr;
    QPushButton* open_btn_ = nullptr;
    QPushButton* dup_btn_ = nullptr;
    QPushButton* export_btn_ = nullptr;
    QPushButton* delete_btn_ = nullptr;

    QWidget* undo_bar_ = nullptr;
    QLabel* undo_lbl_ = nullptr;
    QPushButton* undo_btn_ = nullptr;
    QTimer* undo_timer_ = nullptr;
    qint64 undo_id_ = 0;

    QTimer* search_timer_ = nullptr;
    QVector<services::TemplateEntry> templates_;
    qint64 current_id_ = 0;
    bool dirty_ = true;
    quint64 list_request_ = 0;
};

} // namespace fincept::screens
