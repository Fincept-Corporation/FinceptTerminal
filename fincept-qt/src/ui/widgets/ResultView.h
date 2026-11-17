#pragma once
// ResultView — shows any analytics result (a JSON payload) as a designed view
// built from the shared kit: headline KpiStrip, the right chart per series /
// matrix / distribution (TimeSeriesChart, BarChart, DonutChart,
// DistributionChart, FanChart, MatrixHeatmap, Gauge) with its data table one
// click away, sortable DataTables with locale-formatted numbers, collapsible
// sections for nested objects, captions for long text, a secondary RAW view and
// explicit loading / empty / error states. Shapes it does not recognise fall
// back to a key / value table — never to a blank pane.
//
//   auto* view = new ui::ResultView(this);
//   view->options().request = body;          // echoed inputs go under "Inputs"
//   view->show_loading(tr("Computing…"));
//   ...
//   view->show_result(payload);
//   view->add_custom_widget(my_chart);       // optional, placed under the KPIs
//
// Classification rules: ui/widgets/ResultLayout.h.

#include "ui/widgets/ResultLayout.h"

#include <QJsonValue>
#include <QList>
#include <QPointer>
#include <QString>
#include <QWidget>

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QScrollArea;
class QStackedWidget;
class QVBoxLayout;

namespace fincept::ui {

class LoadingOverlay;

class ResultView : public QWidget {
    Q_OBJECT
  public:
    explicit ResultView(QWidget* parent = nullptr);

    /// Rules for the next show_result() (units, KPIs, hidden keys, ...).
    void set_options(const result::Options& options);
    result::Options& options() { return options_; }
    const result::Options& options() const { return options_; }

    /// Own scroll area around the content (default true). Turn off when the
    /// host already scrolls; the view then grows to its content.
    void set_scrollable(bool on);
    /// The RAW toggle above the content (default true).
    void set_raw_toggle_visible(bool on);

    void show_idle(const QString& message);
    /// Keeps the current content (dimmed under the overlay) until the result arrives.
    void show_loading(const QString& message = QString());
    void show_error(const QString& message);
    /// `raw` is what RAW shows (default: `payload`).
    void show_result(const QJsonValue& payload, const QJsonValue& raw = QJsonValue(QJsonValue::Undefined));
    /// Clear back to the idle message without changing it.
    void clear();

    /// A caller-built widget placed under the headline KPIs; owned by the view
    /// and removed by the next show_*().
    void add_custom_widget(QWidget* widget);
    /// A caller-built widget placed at the end of the content.
    void append_custom_widget(QWidget* widget);

    /// A titled card in the view's style around `content` — for caller-built
    /// charts passed to add_custom_widget().
    static QWidget* make_card(const QString& title, QWidget* content, const QString& subtitle = QString());

    bool has_result() const { return has_result_; }
    bool is_loading() const { return loading_; }
    /// The rendered summary page (for "copy as image").
    QWidget* content_widget() const;

  protected:
    void changeEvent(QEvent* e) override;

  private:
    void rebuild();
    void clear_content();
    void drop_custom_widgets();
    void set_page(int page);
    void stop_loading();
    void restyle();

    // Block renderers (ResultView.cpp)
    void render_children(const result::Block& section, QVBoxLayout* into, int depth);
    QWidget* render_block(const result::Block& b, int depth);
    QWidget* render_kpis(const result::Block& b, int depth);
    QWidget* render_fields(const result::Block& b);
    QWidget* render_table(const result::Block& b);
    QWidget* render_caption(const result::Block& b);
    QWidget* render_list(const result::Block& b);
    QWidget* render_section(const result::Block& b, int depth);
    QWidget* render_chart_card(const result::Block& b);
    QWidget* make_line(const result::Block& b);
    QWidget* make_bars(const result::Block& b);
    QWidget* make_donut(const result::Block& b);
    QWidget* make_distribution(const result::Block& b, QWidget** stats);
    QWidget* make_fan(const result::Block& b, QWidget** stats);
    QWidget* make_heatmap(const result::Block& b);
    QWidget* make_gauge(const result::Block& b);
    QWidget* make_data_table(const result::Block& b);

    result::Options options_;
    QJsonValue payload_;
    QJsonValue raw_;
    bool has_result_ = false;
    bool loading_ = false;
    bool scrollable_ = true;
    QString idle_text_;

    QWidget* bar_ = nullptr;
    QPushButton* raw_btn_ = nullptr;
    QStackedWidget* stack_ = nullptr; // 0 message, 1 content, 2 raw
    QLabel* message_ = nullptr;
    QScrollArea* scroll_ = nullptr;
    QWidget* page_ = nullptr;         // page 1 (the scroll area or the content itself)
    QWidget* content_ = nullptr;
    QVBoxLayout* content_layout_ = nullptr;
    int custom_index_ = 0;            // where add_custom_widget inserts
    QList<QPointer<QWidget>> custom_top_;
    QList<QPointer<QWidget>> custom_end_;
    QPlainTextEdit* raw_text_ = nullptr;
    LoadingOverlay* overlay_ = nullptr;
};

} // namespace fincept::ui
