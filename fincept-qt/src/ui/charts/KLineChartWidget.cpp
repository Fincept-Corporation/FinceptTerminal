#include "ui/charts/KLineChartWidget.h"

#include "core/logging/Logger.h"

#ifdef HAS_QT_WEBENGINE
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineView>
#include <QWebEnginePage>
#endif

#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLibraryInfo>
#include <QMenu>
#include <QMouseEvent>
#include <QPointer>
#include <QShowEvent>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace fincept::ui {

#ifdef HAS_QT_WEBENGINE
namespace {
// QWebEngineView delivers context-menu events to the view subclass in Qt 6, so
// we override contextMenuEvent to replace the default web menu with our native
// trading menu. The price under the cursor is resolved asynchronously via JS.
class ChartWebView : public QWebEngineView {
  public:
    explicit ChartWebView(KLineChartWidget* host) : QWebEngineView(host), host_(host) {}

  protected:
    void contextMenuEvent(QContextMenuEvent* e) override {
        if (host_ && host_->trading_menu_enabled()) {
            host_->show_chart_context_menu(e->pos().y(), e->globalPos());
            e->accept();
            return; // suppress the default WebEngine context menu
        }
        QWebEngineView::contextMenuEvent(e);
    }

  private:
    KLineChartWidget* host_;
};

// HAS_QT_WEBENGINE is a COMPILE-time flag (Qt WebEngine was present on the build
// machine). It says nothing about whether the *runtime* the app actually ships
// with is complete. Qt WebEngine spawns a helper process — QtWebEngineProcess
// (.exe on Windows) — and loads ICU/resource .pak files alongside it. If the
// deployment is missing the helper (e.g. the installer didn't bundle it), the
// Chromium bootstrap calls abort() the instant a QWebEngineView is constructed,
// taking the whole app down — and it cannot be caught in C++. So we must detect
// a broken/absent runtime BEFORE creating any view and degrade to the Qt-Charts
// fallback. Probe the locations Qt searches for the helper: the Qt
// libexec dir, the app dir, and an app/bin subdir.
bool webengine_runtime_available() {
#ifdef Q_OS_WIN
    const QString proc = QStringLiteral("QtWebEngineProcess.exe");
#else
    const QString proc = QStringLiteral("QtWebEngineProcess");
#endif
    const QString app = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QLibraryInfo::path(QLibraryInfo::LibraryExecutablesPath) + QStringLiteral("/") + proc,
        app + QStringLiteral("/") + proc,
        app + QStringLiteral("/bin/") + proc,
    };
    for (const QString& c : candidates) {
        if (QFileInfo::exists(c))
            return true;
    }
    return false;
}
} // namespace
#endif

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
    // Computed once: the runtime layout can't change while the app is running.
    static const bool ok = []() {
        const bool present = webengine_runtime_available();
        if (!present)
            LOG_WARN("KLineChart",
                     "Qt WebEngine runtime not found (QtWebEngineProcess missing from "
                     "deployment) — falling back to the Qt-Charts chart");
        return present;
    }();
    return ok;
#else
    return false;
#endif
}

void KLineChartWidget::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    ensure_web_view();
    install_chart_event_filter();
}

void KLineChartWidget::ensure_web_view() {
#ifdef HAS_QT_WEBENGINE
    if (initialized_)
        return;
    // Defense-in-depth: never create a QWebEngineView when the runtime helper is
    // absent — doing so aborts the process. Callers normally gate on
    // is_available(), but guard here too so any direct user is crash-safe.
    if (!webengine_runtime_available())
        return;
    initialized_ = true;

    web_view_ = new ChartWebView(this);
    web_view_->setObjectName("klineWebView");

    // The bundled chart html/js is local and changes across releases — never
    // serve a stale cached copy, or edits silently fail to take effect.
    web_view_->page()->profile()->setHttpCacheType(QWebEngineProfile::NoCache);

    auto* settings = web_view_->page()->settings();
    settings->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
    settings->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, true);
    settings->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, false);

    connect(web_view_, &QWebEngineView::loadFinished,
            this, &KLineChartWidget::on_load_finished);

    const QString html_path = QCoreApplication::applicationDirPath()
                              + QStringLiteral("/resources/charts/klinechart.html");
    QUrl url = QUrl::fromLocalFile(html_path);
    url.setQuery(QStringLiteral("v=%1").arg(QDateTime::currentMSecsSinceEpoch())); // cache-bust
    web_view_->load(url);

    layout()->addWidget(web_view_);
#endif
}

void KLineChartWidget::on_load_finished(bool ok) {
    if (!ok) return;
    page_ready_ = true;
    flush_pending();
    install_chart_event_filter(); // render widget exists by now
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

void KLineChartWidget::set_position_line(double price, const QString& label, const QString& color_hex) {
    run_js(QStringLiteral("window.setPositionLine(%1,'%2','%3')")
               .arg(price, 0, 'f', 6)
               .arg(label, color_hex));
}

void KLineChartWidget::clear_position_line() {
    run_js(QStringLiteral("window.clearPositionLine()"));
}

void KLineChartWidget::clear() {
    run_js(QStringLiteral("window.clearChart()"));
}

void KLineChartWidget::show_chart_context_menu(int local_y, const QPoint& global_pos) {
    if (menu_open_)
        return; // a paired event for the same right-click — ignore
    menu_open_ = true;

    // The menu MUST be shown synchronously, in direct response to the right-click
    // event. On macOS a QMenu popped later (e.g. from an async runJavaScript
    // callback) silently fails to display. So exec() here, then resolve the
    // cursor price asynchronously only once the user has actually chosen Buy/Sell.
    QMenu menu;
    QAction* buy_act = menu.addAction(tr("Buy"));
    menu.addAction(tr("Sell")); // non-buy, non-watchlist choice ⇒ Sell (see below)
    menu.addSeparator();
    QAction* wl_act = menu.addAction(tr("Add to watchlist"));

    QAction* chosen = menu.exec(global_pos);
    // Clear the guard after this event burst so the right-click's paired event
    // (some platforms deliver both a mouse-press and a ContextMenu) can't pop a
    // second menu, but the next genuine right-click can.
    QPointer<KLineChartWidget> self = this;
    QTimer::singleShot(0, this, [self] { if (self) self->menu_open_ = false; });

    if (!chosen)
        return;
    if (chosen == wl_act) {
        emit add_to_watchlist_requested();
        return;
    }
    const bool is_buy = (chosen == buy_act);

#ifdef HAS_QT_WEBENGINE
    if (web_view_ && page_ready_) {
        QPointer<KLineChartWidget> self = this;
        web_view_->page()->runJavaScript(
            QStringLiteral("window.priceAtY(%1)").arg(local_y),
            [self, is_buy](const QVariant& v) {
                if (!self)
                    return;
                bool ok = false;
                const double price = v.toDouble(&ok);
                self->emit_trade(is_buy, (ok && price > 0) ? price : -1.0);
            });
        return;
    }
#endif
    Q_UNUSED(local_y);
    emit_trade(is_buy, -1.0); // no chart price — the ticket falls back to the LTP
}

void KLineChartWidget::emit_trade(bool is_buy, double price) {
    if (is_buy)
        emit buy_requested(price);
    else
        emit sell_requested(price);
}

void KLineChartWidget::install_chart_event_filter() {
#ifdef HAS_QT_WEBENGINE
    if (!web_view_ || !trading_menu_)
        return;
    // The render widget (focusProxy) is created lazily — after the page's render
    // process is up. Retry until it exists, then filter it for right-clicks.
    QWidget* proxy = web_view_->focusProxy();
    if (!proxy) {
        QPointer<KLineChartWidget> self = this;
        QTimer::singleShot(50, this, [self] { if (self) self->install_chart_event_filter(); });
        return;
    }
    if (proxy == filtered_proxy_)
        return; // already filtering this one
    if (filtered_proxy_)
        filtered_proxy_->removeEventFilter(this);
    proxy->installEventFilter(this);
    filtered_proxy_ = proxy;
#endif
}

bool KLineChartWidget::eventFilter(QObject* obj, QEvent* ev) {
#ifdef HAS_QT_WEBENGINE
    if (trading_menu_ && obj == filtered_proxy_) {
        if (ev->type() == QEvent::ContextMenu) {
            auto* cme = static_cast<QContextMenuEvent*>(ev);
            show_chart_context_menu(cme->pos().y(), cme->globalPos());
            return true; // consume — no default Chromium menu
        }
        if (ev->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(ev);
            if (me->button() == Qt::RightButton) {
                show_chart_context_menu(static_cast<int>(me->position().y()),
                                        me->globalPosition().toPoint());
                return true;
            }
        }
    }
#endif
    return QWidget::eventFilter(obj, ev);
}

} // namespace fincept::ui
