#pragma once
// FiiDiiFlowsWidget — dashboard tile with daily institutional net flows (FII and
// DII, ₹ Crore, NSE cash market) for the last N sessions: two summary figures,
// a small two-lane bar chart and a compact table, newest session first.
//
// Pure DataHub consumer: subscribes to `fno:fii_dii:daily` (FiiDiiService,
// payload QVector<FiiDiiDay> ascending by date, rolling 30 days). Cadence is the
// hub's (TTL 1 h / min_interval 30 min); the title-bar refresh forces a fetch.
// Double-click opens the F&O screen (its FII/DII tab has the full history).
//
// Config (persisted): { "days": 10 }   (5..30)

#include "screens/dashboard/widgets/BaseWidget.h"
#include "services/options/FiiDiiTypes.h"

#include <QPixmap>
#include <QVariant>
#include <QVector>

class QLabel;
class QTableWidget;

namespace fincept::screens::widgets {

/// Two-lane net-flow bar chart (FII on top, DII below, shared scale; green =
/// net buy, red = net sell). Painted into a cached QPixmap that is rebuilt only
/// on new data, resize or theme change (P9). Hover shows the session's values.
class FiiDiiFlowsChart : public QWidget {
    Q_OBJECT
  public:
    explicit FiiDiiFlowsChart(QWidget* parent = nullptr);
    /// Sessions ascending by date (oldest left).
    void set_days(const QVector<services::options::FiiDiiDay>& days);
    /// Drop the cached pixmap so the next paint re-reads the theme tokens.
    void refresh_theme();

  protected:
    void paintEvent(QPaintEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void changeEvent(QEvent* e) override;
    bool event(QEvent* e) override;

  private:
    void rebuild_cache();
    /// Session index under x (widget coords), or -1.
    int index_at(int x) const;

    QVector<services::options::FiiDiiDay> days_;
    QPixmap cache_;
    bool dirty_ = true;
    // Plot geometry of the last render — used to map hover x to a session.
    int plot_left_ = 0;
    int plot_width_ = 0;
};

class FiiDiiFlowsWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit FiiDiiFlowsWidget(const QJsonObject& cfg = {}, QWidget* parent = nullptr);

    QJsonObject config() const override;
    void apply_config(const QJsonObject& cfg) override;

  protected:
    QDialog* make_config_dialog(QWidget* parent) override;
    void on_theme_changed() override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void retranslateUi() override;

  private:
    void apply_styles();
    void apply_texts();
    void hub_subscribe();
    void on_data(const QVariant& v);
    void on_error(const QString& error);
    void render();

    int days_ = 10;
    QVector<services::options::FiiDiiDay> all_days_; ///< last payload, ascending
    bool have_data_ = false;                         ///< a non-empty payload is on screen
    bool refresh_failed_ = false;
    bool hub_active_ = false;

    QWidget* root_ = nullptr;
    QLabel* fii_caption_ = nullptr;
    QLabel* fii_value_ = nullptr;
    QLabel* dii_caption_ = nullptr;
    QLabel* dii_value_ = nullptr;
    FiiDiiFlowsChart* chart_ = nullptr;
    QTableWidget* table_ = nullptr;
    QLabel* footer_label_ = nullptr;
};

} // namespace fincept::screens::widgets
