#pragma once
#include "screens/IStatefulScreen.h"
#include "services/file_manager/FileManagerService.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QTableWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

/// File Manager Screen — lists all files tracked by FileManagerService.
/// Features: filter chips, sort controls, inline preview, bulk delete, quota bar.
class FileManagerScreen : public QWidget, public IStatefulScreen {
    Q_OBJECT
  public:
    explicit FileManagerScreen(QWidget* parent = nullptr);

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "file_manager"; }
    int state_version() const override { return 1; }

  signals:
    void open_file_in_screen(const QString& route_id, const QString& file_path);

  protected:
    void showEvent(QShowEvent* event) override;

  private:
    void build_ui();
    void build_filter_bar(QVBoxLayout* root);
    void build_quota_bar(QVBoxLayout* root);
    QWidget* build_preview_panel();

    void render_files();
    void upload_files();
    void download_file(const QString& file_id);
    void delete_file(const QString& file_id);
    void delete_selected();
    void open_with(const QString& file_id);
    void show_preview(const QString& file_id);
    void clear_preview();
    void update_stats();
    void update_bulk_bar();

    static QString format_size(qint64 bytes);
    static QString file_type_color(const QString& mime);
    static QString open_with_label(const QString& mime);
    static QString route_for_mime(const QString& mime);

    // ── Search / filter bar ──────────────────────────────────────────────
    QLineEdit* search_input_ = nullptr;
    QComboBox* sort_combo_ = nullptr;
    // Screen filter chips — one QPushButton per source screen + "All"
    QWidget* chips_bar_ = nullptr;
    QButtonGroup* chip_group_ = nullptr;
    QString active_screen_filter_; // "" = all

    // ── Quota bar ────────────────────────────────────────────────────────
    QProgressBar* quota_bar_ = nullptr;
    QLabel* quota_label_ = nullptr;
    static constexpr qint64 kQuotaBytes = 500LL * 1024 * 1024; // 500 MB

    // ── File list ────────────────────────────────────────────────────────
    QWidget* file_container_ = nullptr;
    QVBoxLayout* file_layout_ = nullptr;
    QVector<QPushButton*> check_btns_; // per-card checkbox buttons
    QVector<QString> check_ids_;       // parallel file IDs
    QPushButton* bulk_delete_btn_ = nullptr;
    QWidget* bulk_bar_ = nullptr;

    // ── Preview panel ────────────────────────────────────────────────────
    QWidget* preview_panel_ = nullptr;
    QLabel* preview_title_ = nullptr;
    QLabel* preview_meta_ = nullptr;
    QTextEdit* preview_text_ = nullptr;
    QTableWidget* preview_table_ = nullptr;
    QLabel* preview_empty_ = nullptr;

    // ── Header controls ──────────────────────────────────────────────────
    QLabel* stats_label_ = nullptr;
    QPushButton* upload_btn_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;

    // ── Splitter ─────────────────────────────────────────────────────────
    QSplitter* splitter_ = nullptr;

    bool loaded_ = false;
};

} // namespace fincept::screens
