#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

/// File Manager — upload, download, delete, search local files.
/// Stores files in app data directory with JSON metadata index.
class FileManagerScreen : public QWidget {
    Q_OBJECT
  public:
    explicit FileManagerScreen(QWidget* parent = nullptr);

  protected:
    void showEvent(QShowEvent* event) override;

  private:
    void build_ui();
    void load_files();
    void render_files();
    void upload_files();
    void download_file(const QString& file_id);
    void delete_file(const QString& file_id);
    void update_stats();

    // Storage helpers
    QString storage_dir() const;
    QString metadata_path() const;
    QJsonArray read_metadata() const;
    void write_metadata(const QJsonArray& files);
    QJsonObject find_file(const QString& id) const;

    static QString format_size(qint64 bytes);
    static QString file_type_color(const QString& mime);

    // UI
    QLineEdit* search_input_ = nullptr;
    QLabel* stats_label_ = nullptr;
    QWidget* file_container_ = nullptr;
    QVBoxLayout* file_layout_ = nullptr;
    QPushButton* upload_btn_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;

    // State
    QJsonArray files_;
    bool loaded_ = false;
};

} // namespace fincept::screens
