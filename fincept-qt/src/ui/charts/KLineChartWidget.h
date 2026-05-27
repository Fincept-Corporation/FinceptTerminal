#pragma once

#include <QJsonArray>
#include <QQueue>
#include <QString>
#include <QWidget>

#ifdef HAS_QT_WEBENGINE
class QWebEngineView;
#endif

namespace fincept::ui {

class KLineChartWidget : public QWidget {
    Q_OBJECT
public:
    explicit KLineChartWidget(QWidget* parent = nullptr);

    void set_candles(const QJsonArray& candles);
    void update_candle(const QJsonObject& candle);
    void add_indicator(const QString& name, const QString& pane_id = QString());
    void remove_indicator(const QString& name, const QString& pane_id = QString());
    void clear();

    bool is_available() const;

signals:
    void chart_ready();

protected:
    void showEvent(QShowEvent* e) override;

private:
    void ensure_web_view();
    void run_js(const QString& js);
    void flush_pending();
    void on_load_finished(bool ok);

#ifdef HAS_QT_WEBENGINE
    QWebEngineView* web_view_ = nullptr;
#endif
    bool page_ready_ = false;
    bool initialized_ = false;
    QQueue<QString> pending_js_;
};

} // namespace fincept::ui
