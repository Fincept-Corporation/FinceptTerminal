#pragma once

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTextEdit>
#include <QWidget>

namespace fincept::screens {

/// Analyzer descriptor within a category.
struct AltAnalyzer {
    QString id;   // Python command name
    QString name; // Display name
};

/// Category of alternative investments.
struct AltCategory {
    QString id;
    QString name;
    QString color; // accent hex
    QList<AltAnalyzer> analyzers;
};

/// Alternative Investments Analysis Screen.
/// 10 categories, 27 analyzers backed by Python analytics modules.
/// Categories: Bonds, Real Estate, Hedge Funds, Commodities, Private Capital,
/// Annuities, Structured Products, Inflation Protected, Strategies, Digital Assets.
class AltInvestmentsScreen : public QWidget {
    Q_OBJECT
  public:
    explicit AltInvestmentsScreen(QWidget* parent = nullptr);

  private slots:
    void on_category_changed(int index);
    void on_analyzer_changed(int index);
    void on_analyze();

  private:
    void setup_ui();
    QWidget* create_header();
    QWidget* create_left_panel();
    QWidget* create_center_panel();
    QWidget* create_right_panel();
    QWidget* create_status_bar();

    QWidget* build_form_for(const QString& analyzer_id);
    void run_analysis(const QString& command, const QJsonObject& data);
    void display_verdict(const QJsonObject& result);
    void display_error(const QString& error);
    void set_loading(bool loading);

    // Categories
    QList<AltCategory> categories_;
    int active_category_ = 0;
    int active_analyzer_ = 0;
    bool loading_ = false;

    // UI — Left panel
    QList<QPushButton*> cat_btns_;

    // UI — Center
    QComboBox* analyzer_combo_ = nullptr;
    QStackedWidget* form_stack_ = nullptr;
    QPushButton* analyze_btn_ = nullptr;
    QLabel* center_title_ = nullptr;

    // Input fields (reused across forms)
    QLineEdit* input_name_ = nullptr;
    QDoubleSpinBox* input_value1_ = nullptr;
    QDoubleSpinBox* input_value2_ = nullptr;
    QDoubleSpinBox* input_value3_ = nullptr;
    QDoubleSpinBox* input_value4_ = nullptr;
    QDoubleSpinBox* input_value5_ = nullptr;
    QDoubleSpinBox* input_value6_ = nullptr;
    QComboBox* input_type_ = nullptr;

    // UI — Right panel (verdict)
    QLabel* verdict_badge_ = nullptr;
    QLabel* verdict_recommendation_ = nullptr;
    QTextEdit* verdict_details_ = nullptr;
    QLabel* verdict_rating_ = nullptr;

    // UI — Status
    QLabel* status_category_ = nullptr;
    QLabel* status_analyzer_ = nullptr;
};

} // namespace fincept::screens
