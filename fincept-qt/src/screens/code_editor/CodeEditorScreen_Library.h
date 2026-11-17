#pragma once
// CodeEditorScreen_Library — where notebooks come from:
//
//   NotebookLibraryPanel  the rail's LIBRARY page: search, a tree of RECENT /
//                         TEMPLATES (by category) / MY NOTEBOOKS, and a preview of
//                         the selected entry (tags, summary, headings) with OPEN.
//   NotebookStartView     the editor column when no notebook is open: NEW / OPEN,
//                         a restore strip for an unsaved notebook, recent
//                         notebooks, and the templates as cards (featured first).
//
// Neither opens anything itself: they emit what the user picked. Templates are
// never run on open.

#include <QDateTime>
#include <QString>
#include <QVector>
#include <QWidget>

class QGridLayout;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace fincept::screens {

struct RecentNotebook {
    QString path;
    QDateTime opened;
};

/// "2 h ago" / "yesterday" / "12 Nov" for a recent notebook.
QString nb_relative_time(const QDateTime& when);

class NotebookLibraryPanel : public QWidget {
    Q_OBJECT
  public:
    explicit NotebookLibraryPanel(QWidget* parent = nullptr);
    /// Rebuild the tree (cheap: catalog is cached, saved files are listed once).
    void refresh(const QVector<RecentNotebook>& recent);

  signals:
    void open_template(int catalog_index);
    void open_path(const QString& path);

  protected:
    void changeEvent(QEvent* e) override;

  private:
    void rebuild();
    void apply_filter(const QString& text);
    void show_preview(QTreeWidgetItem* item);
    void open_item(QTreeWidgetItem* item);
    void retranslate();

    QLineEdit* search_ = nullptr;
    QTreeWidget* tree_ = nullptr;
    QWidget* preview_ = nullptr;
    QLabel* preview_title_ = nullptr;
    QLabel* preview_meta_ = nullptr;
    QLabel* preview_text_ = nullptr;
    QPushButton* preview_open_ = nullptr;
    QLabel* title_ = nullptr;
    QVector<RecentNotebook> recent_;
};

class NotebookStartView : public QWidget {
    Q_OBJECT
  public:
    explicit NotebookStartView(QWidget* parent = nullptr);
    void refresh(const QVector<RecentNotebook>& recent, const QDateTime& recovery_saved_at);

  signals:
    void new_requested();
    void open_requested();
    void open_template(int catalog_index);
    void open_path(const QString& path);
    void recover_requested();

  protected:
    void resizeEvent(QResizeEvent* e) override;
    void changeEvent(QEvent* e) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

  private:
    void build_cards();
    void reflow();
    void retranslate();

    QLabel* hero_ = nullptr;
    QLabel* hero_sub_ = nullptr;
    QPushButton* new_btn_ = nullptr;
    QPushButton* open_btn_ = nullptr;
    QWidget* recovery_ = nullptr;
    QLabel* recovery_text_ = nullptr;
    QPushButton* recovery_btn_ = nullptr;
    QWidget* recent_box_ = nullptr;
    QLabel* recent_title_ = nullptr;
    QListWidget* recent_list_ = nullptr;
    QLabel* templates_title_ = nullptr;
    QLabel* templates_caption_ = nullptr;
    QWidget* cards_host_ = nullptr;
    QGridLayout* cards_ = nullptr;
    QVector<QWidget*> card_widgets_;
    QLabel* keys_ = nullptr;
    int columns_ = 0;
    QDateTime recovery_at_;
};

} // namespace fincept::screens
