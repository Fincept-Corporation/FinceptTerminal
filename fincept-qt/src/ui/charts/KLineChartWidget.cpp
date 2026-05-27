#include "ui/charts/KLineChartWidget.h"

#ifdef HAS_QT_WEBENGINE
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebEngineSettings>
#endif

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QShowEvent>
#include <QUrl>
#include <QVBoxLayout>

namespace fincept::ui {

KLineChartWidget::KLineChartWidget(QWidget* parent) : QWidget(parent) {
    setObjectName("klineChartWidget");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

#ifndef HAS_QT_WEBENGINE
    auto* fallback = new QLabel(this);
    fallback->setObjectName("klineFallback");
    fallback->setText(QStringLiteral("Advanced charting unavailable.\nInstall Qt WebEngine to enable KLineChart."));
    fallback->setAlignment(Qt::AlignCenter);
    layout->addWidget(fallback, 1);
#endif
}

bool KLineChartWidget::is_available() const {
#ifdef HAS_QT_WEBENGINE
    return true;
#else
    return false;
#endif
}

void KLineChartWidget::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    ensure_web_view();
}

void KLineChartWidget::ensure_web_view() {
#ifdef HAS_QT_WEBENGINE
    if (initialized_)
        return;
    initialized_ = true;

    web_view_ = new QWebEngineView(this);
    web_view_->setObjectName("klineWebView");

    auto* settings = web_view_->page()->settings();
    settings->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
    settings->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, true);
    settings->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, false);

    connect(web_view_, &QWebEngineView::loadFinished,
            this, &KLineChartWidget::on_load_finished);

    const QString html_path = QCoreApplication::applicationDirPath()
                              + QStringLiteral("/resources/charts/klinechart.html");
    web_view_->load(QUrl::fromLocalFile(html_path));

    layout()->addWidget(web_view_);
#endif
}

void KLineChartWidget::on_load_finished(bool ok) {
    if (!ok) return;
    page_ready_ = true;
    flush_pending();
    emit chart_ready();
}

void KLineChartWidget::run_js(const QString& js) {
#ifdef HAS_QT_WEBENGINE
    if (!page_ready_) {
        pending_js_.enqueue(js);
        return;
    }
    if (!web_view_) return;
    web_view_->page()->runJavaScript(js);
#else
    Q_UNUSED(js);
#endif
}

void KLineChartWidget::flush_pending() {
#ifdef HAS_QT_WEBENGINE
    if (!web_view_) return;
    while (!pending_js_.isEmpty()) {
        web_view_->page()->runJavaScript(pending_js_.dequeue());
    }
#endif
}

void KLineChartWidget::set_candles(const QJsonArray& candles) {
    if (candles.isEmpty())
        return;

    QJsonArray converted;
    for (const auto& v : candles) {
        QJsonObject src = v.toObject();
        QJsonObject dst;
        dst[QStringLiteral("timestamp")] = static_cast<double>(src[QStringLiteral("timestamp")].toVariant().toLongLong()) * 1000.0;
        dst[QStringLiteral("open")] = src[QStringLiteral("open")].toDouble();
        dst[QStringLiteral("high")] = src[QStringLiteral("high")].toDouble();
        dst[QStringLiteral("low")] = src[QStringLiteral("low")].toDouble();
        dst[QStringLiteral("close")] = src[QStringLiteral("close")].toDouble();
        dst[QStringLiteral("volume")] = src[QStringLiteral("volume")].toDouble();
        converted.append(dst);
    }

    const QString json = QString::fromUtf8(QJsonDocument(converted).toJson(QJsonDocument::Compact));
    run_js(QStringLiteral("window.setCandles(%1)").arg(json));
}

void KLineChartWidget::update_candle(const QJsonObject& candle) {
    QJsonObject dst;
    dst[QStringLiteral("timestamp")] = static_cast<double>(candle[QStringLiteral("timestamp")].toVariant().toLongLong()) * 1000.0;
    dst[QStringLiteral("open")] = candle[QStringLiteral("open")].toDouble();
    dst[QStringLiteral("high")] = candle[QStringLiteral("high")].toDouble();
    dst[QStringLiteral("low")] = candle[QStringLiteral("low")].toDouble();
    dst[QStringLiteral("close")] = candle[QStringLiteral("close")].toDouble();
    dst[QStringLiteral("volume")] = candle[QStringLiteral("volume")].toDouble();

    const QString json = QString::fromUtf8(QJsonDocument(dst).toJson(QJsonDocument::Compact));
    run_js(QStringLiteral("window.updateCandle(%1)").arg(json));
}

void KLineChartWidget::add_indicator(const QString& name, const QString& pane_id) {
    if (pane_id.isEmpty())
        run_js(QStringLiteral("window.addIndicator('%1')").arg(name));
    else
        run_js(QStringLiteral("window.addIndicator('%1','%2')").arg(name, pane_id));
}

void KLineChartWidget::remove_indicator(const QString& name, const QString& pane_id) {
    if (pane_id.isEmpty())
        run_js(QStringLiteral("window.removeIndicator('%1')").arg(name));
    else
        run_js(QStringLiteral("window.removeIndicator('%1','%2')").arg(name, pane_id));
}

void KLineChartWidget::clear() {
    run_js(QStringLiteral("window.clearChart()"));
}

} // namespace fincept::ui
