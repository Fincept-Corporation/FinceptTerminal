#pragma once
// LoggingSection.h — global log level, JSON output mode, log file path,
// and per-tag level overrides for fincept::Logger.

#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

class QPushButton;

namespace fincept::screens {

class LoggingSection : public QWidget {
    Q_OBJECT
  public:
    explicit LoggingSection(QWidget* parent = nullptr);

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void build_ui();

    /// Re-apply tr() lookups to every widget whose text we keep a handle to.
    /// Called from changeEvent() on QEvent::LanguageChange.
    void retranslateUi();

    QComboBox*   log_global_level_ = nullptr;
    QWidget*     log_tag_list_     = nullptr; // VBox container for tag rows
    QVBoxLayout* log_tag_layout_   = nullptr;
    QCheckBox*   log_json_mode_    = nullptr;
    QLabel*      log_path_label_   = nullptr;

    // Static text widgets cached for retranslateUi.
    QLabel* section_title_ = nullptr;
    QLabel* global_lbl_    = nullptr;
    QLabel* global_desc_   = nullptr;
    QLabel* fmt_title_     = nullptr;
    QLabel* fmt_desc_      = nullptr;
    QLabel* path_title_    = nullptr;
    QLabel* tag_title_     = nullptr;
    QLabel* tag_desc_      = nullptr;
    QPushButton* open_folder_btn_ = nullptr;
    QPushButton* copy_path_btn_   = nullptr;
    QPushButton* add_btn_         = nullptr;
    QPushButton* save_btn_        = nullptr;
};

} // namespace fincept::screens
