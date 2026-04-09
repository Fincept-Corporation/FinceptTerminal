#pragma once

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

struct AltAnalyzer {
    QString id;          // Python CLI command (e.g. "high-yield")
    QString name;        // Display name
    QString description; // Short description shown in header
};

struct AltCategory {
    QString id;
    QString name;
    QString color;
    QString icon;
    QList<AltAnalyzer> analyzers;
};

/// Form field descriptor — drives dynamic form generation per analyzer.
struct AltField {
    QString key;   // JSON key sent to Python
    QString label; // Display label
    enum Type { Text, Spin, Combo } type = Spin;
    double default_val = 0.0;
    double min_val = 0.0;
    double max_val = 1e9;
    int decimals = 2;
    QString prefix;
    QString suffix;
    QString combo_items;     // pipe-separated, used when type==Combo
    bool divide_100 = false; // divide UI value by 100 before sending (for % fields)
};

class AltInvestmentsScreen : public QWidget {
    Q_OBJECT
  public:
    explicit AltInvestmentsScreen(QWidget* parent = nullptr);

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private slots:
    void on_category_changed(int index);
    void on_analyzer_changed(int index);
    void on_analyze();

  private:
    // Setup
    void setup_ui();
    QWidget* create_header();
    QWidget* create_left_panel();
    QWidget* create_center_panel();
    QWidget* create_right_panel();
    QWidget* create_status_bar();
    void rebuild_form(int cat, int ana);

    // Data / execution
    void run_analysis(const QString& command, const QJsonObject& data);
    QJsonObject collect_form_data() const;

    // Display helpers
    void display_verdict(const QJsonObject& result, const QString& command);
    void display_error(const QString& error);
    void set_loading(bool loading);
    void append_metric_row(QWidget* parent, QVBoxLayout* vl, const QString& key, const QJsonValue& val, int depth = 0);
    QString format_value(const QJsonValue& v) const;

    // Category/analyzer data
    QList<AltCategory> categories_;
    int active_category_ = 0;
    int active_analyzer_ = 0;
    bool loading_ = false;

    // Left panel
    QList<QPushButton*> cat_btns_;

    // Center
    QLabel* center_title_ = nullptr;
    QLabel* center_desc_ = nullptr;
    QComboBox* analyzer_combo_ = nullptr;
    QWidget* form_container_ = nullptr; // holds dynamic form rows
    QVBoxLayout* form_layout_ = nullptr;
    QPushButton* analyze_btn_ = nullptr;

    // Dynamic form fields (rebuilt per analyzer)
    QList<AltField> current_fields_;
    QList<QWidget*> field_widgets_; // parallel to current_fields_

    // Right panel — verdict
    QLabel* verdict_badge_ = nullptr;
    QLabel* verdict_rating_ = nullptr;
    QLabel* verdict_rec_ = nullptr;
    QWidget* metrics_container_ = nullptr;
    QVBoxLayout* metrics_layout_ = nullptr;
    QScrollArea* metrics_scroll_ = nullptr;

    // Status bar
    QLabel* status_category_ = nullptr;
    QLabel* status_analyzer_ = nullptr;
};

} // namespace fincept::screens
