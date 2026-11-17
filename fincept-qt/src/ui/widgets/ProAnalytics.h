#pragma once
// ProAnalytics — Pro analytics reports (/data/pro/* on the Fincept API) in the
// terminal: one renderer for every report, a window that hosts it, and the
// small toolbar button tabs use to open it for what the user is looking at.
//
// The server computes each report and sends it ready to render
// ({title, headline, tone, stats[], tables[], notes[]}), so a report added on
// the server needs no terminal change beyond a Kind and a path here.
//
//   Options positioning   put/call, max pain, walls, expected move, skew, gamma     (a symbol)
//   Technical rating      buy/sell rating, trend, performance, relative strength    (a symbol)
//   Portfolio risk        volatility, drawdown, VaR, beta, correlations             (holdings)
//   Market pulse          risk appetite, sector breadth and leadership              (no input)
//
// Billing: included with Pro; on Free a report costs credits, so it never runs
// by itself there — the window shows what the report is and its price, and RUN
// starts it. On Pro it runs when the window opens. Everything here stays hidden
// while the server has not announced the "pro_analytics" price category
// (billing::PlanAccess), i.e. against a server without these routes.

#include <QDialog>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

#include <functional>

class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QStackedWidget;
class QVBoxLayout;

namespace fincept::ui {

namespace pro {

enum class Kind { OptionsPositioning, TechnicalRating, PortfolioRisk, MarketPulse };

/// What a report is asked for. `symbol` for the two single-symbol reports;
/// `symbols` (+ optional `weights`, same order, any scale) for portfolio risk.
struct Request {
    Kind kind = Kind::MarketPulse;
    QString symbol;
    QStringList symbols;
    QVector<double> weights;
};

/// Short name of a report ("Options positioning"), translated.
QString title(Kind kind);

/// One sentence saying what the report contains, translated.
QString description(Kind kind);

/// True when the signed-in account's server offers Pro analytics.
bool available();

/// Opens the report window for `request` over the window of `origin` (one
/// window per kind: asking again re-targets and raises it).
void open(const Request& request, QWidget* origin);

/// A small toolbar button ("PRO  POSITIONING") that opens the report for
/// whatever `provider` returns when clicked. It hides itself while Pro
/// analytics are not available and re-styles with the theme. The button is
/// owned by `parent`.
QPushButton* make_button(Kind kind, std::function<Request()> provider, QWidget* parent);

} // namespace pro

/// Renders one report: intro (what it is, what it costs) → loading → result or
/// error. Embeddable anywhere; ProAnalyticsDialog hosts it.
class ProAnalyticsPanel : public QWidget {
    Q_OBJECT
  public:
    explicit ProAnalyticsPanel(QWidget* parent = nullptr);

    /// Shows the intro for `request`. With `autorun`, a report the plan includes
    /// (no credits) runs at once; otherwise it waits for RUN / run().
    void set_request(const pro::Request& request, bool autorun = true);
    const pro::Request& request() const { return request_; }

    /// Fetch the report now (a run in flight is superseded).
    void run();

  protected:
    void changeEvent(QEvent* event) override;

  private:
    enum Page { Intro = 0, Loading = 1, Failed = 2, Result = 3 };

    void retranslate();
    /// Marks RUN / RETRY as spending credits for outside agents while the plan
    /// does not include the report (core/agent/AgentRiskMarkers.h).
    void sync_risk_markers();
    void show_intro();
    void show_error(const QString& message, bool out_of_credits);
    void show_report(const QJsonObject& data, const QJsonObject& meta);
    bool request_ready(QString* why) const;
    QString request_path() const;

    pro::Request request_;
    quint64 generation_ = 0;

    QStackedWidget* pages_ = nullptr;
    QLabel* intro_title_ = nullptr;
    QLabel* intro_text_ = nullptr;
    QLabel* intro_cost_ = nullptr;
    QPushButton* run_btn_ = nullptr;
    QLabel* loading_label_ = nullptr;
    QLabel* error_label_ = nullptr;
    QPushButton* retry_btn_ = nullptr;
    QPushButton* plans_btn_ = nullptr;
    QScrollArea* result_scroll_ = nullptr;
};

/// Non-modal report window: an input row for the report's subject, the panel,
/// and REFRESH. Deleted on close.
class ProAnalyticsDialog : public QDialog {
    Q_OBJECT
  public:
    explicit ProAnalyticsDialog(pro::Kind kind, QWidget* parent = nullptr);

    void set_request(const pro::Request& request);
    pro::Kind kind() const { return kind_; }

  private:
    void apply_inputs();

    pro::Kind kind_;
    QLineEdit* symbol_edit_ = nullptr;  ///< symbol, or comma-separated holdings
    QLineEdit* weights_edit_ = nullptr; ///< portfolio risk only
    QPushButton* refresh_btn_ = nullptr;
    ProAnalyticsPanel* panel_ = nullptr;
};

} // namespace fincept::ui
