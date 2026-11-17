#pragma once
#include "screens/common/ISectionLinked.h"
#include "screens/common/IStatefulScreen.h"
#include "services/file_manager/FileManagerService.h"

#include <QEvent>
#include <QWidget>

class QButtonGroup;
class QLabel;
class QLineEdit;
class QModelIndex;
class QProgressBar;
class QPushButton;
class QSplitter;
class QStandardItemModel;
class QTableView;
class QTableWidget;
class QTextEdit;
class QTimer;
class QVBoxLayout;

namespace fincept::ui {
class EmptyState;
}

namespace fincept::screens {

class FileManagerFilterProxy;

/// File Manager Screen — every file tracked by FileManagerService in one
/// sortable table (model/view, so thousands of files stay responsive), with
/// source filter chips, search, inline preview, rename, open / reveal, bulk
/// delete and a storage summary.
class FileManagerScreen : public QWidget, public IStatefulScreen, public ISectionLinked {
    Q_OBJECT
  public:
    explicit FileManagerScreen(QWidget* parent = nullptr);

    /// ISectionLinked — "all" | "source:<sourceScreen tag>" (the source chips, e.g. "source:code_editor") |
    /// "upload" (the UPLOAD file picker; nothing is imported without the user's pick).
    /// focus = search text (typed into the search box); "" + focus searches the current chip.
    bool open_section(const QString& section, const QString& focus) override;

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "file_manager"; }
    int state_version() const override { return 1; }

  signals:
    void open_file_in_screen(const QString& route_id, const QString& file_path);

  protected:
    void showEvent(QShowEvent* event) override;
    void changeEvent(QEvent* event) override;

  private:
    void build_ui();
    void build_filter_bar(QVBoxLayout* root);
    QWidget* build_preview_panel();
    void retranslateUi();

    void reload_model();
    void apply_filter();
    void upload_files();
    void download_file(const QString& file_id);
    void delete_files(const QStringList& ids);
    void rename_file(const QString& file_id);
    void open_with(const QString& file_id);
    void open_external(const QString& file_id);
    void activate(const QModelIndex& index);
    void show_context_menu(const QPoint& pos);
    void show_preview(const QString& file_id);
    void clear_preview();
    /// What the preview pane says instead of text: a binary file, an unreadable or empty one.
    enum class PreviewNote { Binary, Unreadable, EmptyFile };
    void show_preview_note(PreviewNote note, const QString& route);
    /// The list's designed state: no files at all, or none matching the search / chip.
    void update_list_state(bool none, bool filtered_out);
    void update_stats();
    void update_bulk_bar();
    QStringList selected_ids() const;
    QString current_id() const;

    static QString format_size(qint64 bytes);
    /// `name` = the file's display name: some routes go by extension (.fincept reports are JSON).
    static QString open_with_label(const QString& mime, const QString& name = {});
    static QString route_for_mime(const QString& mime, const QString& name = {});

    // ── Header / filter ──────────────────────────────────────────────────
    QLabel* title_label_ = nullptr;
    QLabel* subtitle_label_ = nullptr;
    QLabel* stats_label_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;
    QPushButton* upload_btn_ = nullptr;
    QLineEdit* search_input_ = nullptr;
    QTimer* search_debounce_ = nullptr;
    QButtonGroup* chip_group_ = nullptr;
    QString active_screen_filter_; // "" = all
    QProgressBar* quota_bar_ = nullptr;
    QLabel* quota_label_ = nullptr;
    /// Soft budget for the storage bar — the terminal never deletes files on its own.
    static constexpr qint64 kQuotaBytes = 500LL * 1024 * 1024;

    // ── File table ───────────────────────────────────────────────────────
    QStandardItemModel* model_ = nullptr;
    FileManagerFilterProxy* proxy_ = nullptr;
    QTableView* table_ = nullptr;
    ui::EmptyState* list_state_ = nullptr; ///< no files / no match (instead of a bare header)
    QWidget* bulk_bar_ = nullptr;
    QLabel* bulk_sel_label_ = nullptr;
    QPushButton* bulk_delete_btn_ = nullptr;
    QPushButton* bulk_clear_btn_ = nullptr;

    // ── Preview panel ────────────────────────────────────────────────────
    QWidget* preview_panel_ = nullptr;
    QLabel* preview_header_label_ = nullptr;
    QLabel* preview_title_ = nullptr;
    QLabel* preview_meta_ = nullptr;
    QTextEdit* preview_text_ = nullptr;
    QTableWidget* preview_table_ = nullptr;
    ui::EmptyState* preview_state_ = nullptr; ///< binary / unreadable / empty file, with its actions
    PreviewNote preview_note_ = PreviewNote::Binary;
    QString preview_route_; ///< screen the previewed file opens in ("" = none)
    QString preview_id_;

    QSplitter* splitter_ = nullptr;
    bool loaded_ = false;
    bool first_fill_done_ = false; ///< the first fill previews the newest file
};

} // namespace fincept::screens
