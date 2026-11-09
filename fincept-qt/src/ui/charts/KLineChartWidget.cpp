#include "ui/charts/KLineChartWidget.h"

#include "core/config/AppPaths.h"
#include "core/logging/Logger.h"

#ifdef HAS_QT_WEBENGINE
#    include <QWebEnginePage>
#    include <QWebEngineProfile>
#    include <QWebEngineSettings>
#    include <QWebEngineView>
#endif

#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QJsonArray>
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
// fallback. Probe the locations Qt searches for the helper: the
// QTWEBENGINEPROCESS_PATH override, the Qt libexec dir, the app dir, an app/bin
// subdir and, on macOS, the Helpers/ app inside QtWebEngineCore.framework.
// Presence is necessary but not sufficient — see g_kline_renderer_broken.
bool webengine_runtime_available() {
#    ifdef Q_OS_WIN
    const QString proc = QStringLiteral("QtWebEngineProcess.exe");
#    else
    const QString proc = QStringLiteral("QtWebEngineProcess");
#    endif
    // When this override is set Qt searches ONLY it (and aborts if it's wrong),
    // so the probe must not fall through to the default locations.
    const QString override_path = qEnvironmentVariable("QTWEBENGINEPROCESS_PATH");
    if (!override_path.isEmpty())
        return QFileInfo::exists(override_path);

    const QString app = QCoreApplication::applicationDirPath();
    QStringList candidates = {
        QLibraryInfo::path(QLibraryInfo::LibraryExecutablesPath) + QStringLiteral("/") + proc,
        app + QStringLiteral("/") + proc,
        app + QStringLiteral("/bin/") + proc,
    };
#    ifdef Q_OS_MACOS
    // A framework build never puts the helper in libexec/, so without these the
    // probe failed on every Mac and KLineChart was silently never used there.
    for (const QString& helpers : {QStringLiteral("/QtWebEngineCore.framework/Helpers/"),
                                   QStringLiteral("/QtWebEngineCore.framework/Versions/A/Helpers/")}) {
        const QString rel = helpers + proc + QStringLiteral(".app/Contents/MacOS/") + proc;
        candidates << app + QStringLiteral("/../Frameworks") + rel           // deployed .app
                   << QLibraryInfo::path(QLibraryInfo::LibrariesPath) + rel; // dev build vs. Qt install
    }
#    endif
    for (const QString& c : candidates) {
        if (QFileInfo::exists(c))
            return true;
    }
    return false;
}

// The helper being present does not mean it can run (#411): with
// resources/v8_context_snapshot.bin missing from the deployment, every renderer
// hits a Chromium FATAL (exit 0x80000003) during startup — the view stays blank
// and loadFinished(true) never arrives. Once any chart sees its renderer die
// before the page came up, later charts skip WebEngine for the rest of the
// session instead of each spawning (and losing) another renderer. GUI-thread only.
bool g_kline_renderer_broken = false;

QString kline_termination_status_name(QWebEnginePage::RenderProcessTerminationStatus status) {
    switch (status) {
        case QWebEnginePage::NormalTerminationStatus:
            return QStringLiteral("normal");
        case QWebEnginePage::AbnormalTerminationStatus:
            return QStringLiteral("abnormal");
        case QWebEnginePage::CrashedTerminationStatus:
            return QStringLiteral("crashed");
        case QWebEnginePage::KilledTerminationStatus:
            return QStringLiteral("killed");
    }
    return QStringLiteral("unknown");
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
    static const bool present = []() {
        const bool found = webengine_runtime_available();
        if (!found)
            LOG_WARN("KLineChart", "Qt WebEngine runtime not found (QtWebEngineProcess missing from "
                                   "deployment) — falling back to the Qt-Charts chart");
        return found;
    }();
    return present && !g_kline_renderer_broken;
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
    // absent — doing so aborts the process — or once a renderer has already
    // failed to start this session. Callers normally gate on is_available(), but
    // guard here too so any direct user is crash-safe.
    if (!is_available())
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

    connect(web_view_, &QWebEngineView::loadFinished, this, &KLineChartWidget::on_load_finished);
    // A dead renderer never reports loadFinished(true); without this the page
    // stays blank forever and every set_candles() sits in pending_js_ (#411).
    connect(web_view_->page(), &QWebEnginePage::renderProcessTerminated, this,
            [this](QWebEnginePage::RenderProcessTerminationStatus status, int exit_code) {
                if (status == QWebEnginePage::NormalTerminationStatus)
                    return;
                fail_runtime(QStringLiteral("renderer process %1 (exit code 0x%2)")
                                 .arg(kline_termination_status_name(status))
                                 .arg(static_cast<quint32>(exit_code), 8, 16, QLatin1Char('0')));
            });

    // Resolved across every shipped layout — on the macOS bundle the page lives
    // in Contents/Resources/, not beside the binary, and a missing page meant a
    // permanently blank chart there.
    const QString html_path = AppPaths::bundled_resource(QStringLiteral("charts/klinechart.html"));
    QUrl url = QUrl::fromLocalFile(html_path);
    url.setQuery(QStringLiteral("v=%1").arg(QDateTime::currentMSecsSinceEpoch())); // cache-bust
    web_view_->load(url);

    layout()->addWidget(web_view_);
#endif
}

void KLineChartWidget::on_load_finished(bool ok) {
    if (failed_)
        return;
    if (!ok) {
        // We load exactly one local page, so a failed load means the chart can
        // never come up (missing klinechart.html or a renderer that died while
        // loading). A late failure on an already-working page is left alone.
        if (!page_ready_)
            fail_runtime(QStringLiteral("chart page failed to load"));
        return;
    }
    page_ready_ = true;
    flush_pending();
    install_chart_event_filter(); // render widget exists by now
    emit chart_ready();
}

void KLineChartWidget::fail_runtime(const QString& reason) {
    if (failed_)
        return;
    failed_ = true;
    const bool was_ready = page_ready_;
    page_ready_ = false;
    pending_js_.clear();

#ifdef HAS_QT_WEBENGINE
    // A renderer that dies before the page ever loaded is a broken runtime, not
    // a one-off crash — stop later charts from trying WebEngine at all.
    if (!was_ready)
        g_kline_renderer_broken = true;
    if (web_view_) {
        // We may be inside one of the view's own signals — never delete it here.
        web_view_->hide();
        web_view_->deleteLater();
        web_view_ = nullptr;
    }
#endif

    LOG_ERROR("KLineChart", QStringLiteral("Qt WebEngine chart failed: %1 (%2) — falling back to the native chart")
                                .arg(reason, was_ready ? QStringLiteral("after the page loaded")
                                                       : QStringLiteral("before the page loaded")));

    // Queued: hosts typically deleteLater() this widget in response, and we may
    // still be on the Chromium callback stack.
    QPointer<KLineChartWidget> self = this;
    QTimer::singleShot(0, this, [self] {
        if (self)
            emit self->runtime_failed();
    });
}

void KLineChartWidget::run_js(const QString& js) {
#ifdef HAS_QT_WEBENGINE
    if (failed_)
        return; // the view is gone; the host is switching to its native chart
    if (!page_ready_) {
        // The page may never become ready (load failed / runtime missing) while
        // callers keep pushing candles; bound the backlog instead of growing
        // forever. Dropping the oldest keeps the most recent chart state.
        constexpr qsizetype kKlinePendingJsCap = 512;
        while (pending_js_.size() >= kKlinePendingJsCap)
            pending_js_.dequeue();
        pending_js_.enqueue(js);
        return;
    }
    if (!web_view_)
        return;
    web_view_->page()->runJavaScript(js);
#else
    Q_UNUSED(js);
#endif
}

void KLineChartWidget::flush_pending() {
#ifdef HAS_QT_WEBENGINE
    if (!web_view_)
        return;
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
        dst[QStringLiteral("timestamp")] =
            static_cast<double>(src[QStringLiteral("timestamp")].toVariant().toLongLong()) * 1000.0;
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
    dst[QStringLiteral("timestamp")] =
        static_cast<double>(candle[QStringLiteral("timestamp")].toVariant().toLongLong()) * 1000.0;
    dst[QStringLiteral("open")] = candle[QStringLiteral("open")].toDouble();
    dst[QStringLiteral("high")] = candle[QStringLiteral("high")].toDouble();
    dst[QStringLiteral("low")] = candle[QStringLiteral("low")].toDouble();
    dst[QStringLiteral("close")] = candle[QStringLiteral("close")].toDouble();
    dst[QStringLiteral("volume")] = candle[QStringLiteral("volume")].toDouble();

    const QString json = QString::fromUtf8(QJsonDocument(dst).toJson(QJsonDocument::Compact));
    run_js(QStringLiteral("window.updateCandle(%1)").arg(json));
}

namespace {
// Build `fn.apply(null,[...])` with every argument JSON-encoded. Interpolating
// values straight into '%1' quotes lets any value containing a quote terminate
// the literal and append arbitrary script — and this page runs with
// JavascriptEnabled + LocalContentCanAccessFileUrls, so injected script reads
// local files. Callers currently pass trusted values (theme colours, formatted
// numbers, fixed indicator names), but the chart takes indicator/pane names
// from callers that may one day be symbol- or config-derived; encoding here
// makes that safe by construction rather than by convention.
QString js_call(const QString& fn, const QJsonArray& args) {
    const QString json = QString::fromUtf8(QJsonDocument(args).toJson(QJsonDocument::Compact));
    return QStringLiteral("%1.apply(null,%2)").arg(fn, json);
}
} // namespace

void KLineChartWidget::add_indicator(const QString& name, const QString& pane_id) {
    if (pane_id.isEmpty())
        run_js(js_call(QStringLiteral("window.addIndicator"), {name}));
    else
        run_js(js_call(QStringLiteral("window.addIndicator"), {name, pane_id}));
}

void KLineChartWidget::remove_indicator(const QString& name, const QString& pane_id) {
    if (pane_id.isEmpty())
        run_js(js_call(QStringLiteral("window.removeIndicator"), {name}));
    else
        run_js(js_call(QStringLiteral("window.removeIndicator"), {name, pane_id}));
}

void KLineChartWidget::set_position_line(double price, const QString& label, const QString& color_hex) {
    run_js(js_call(QStringLiteral("window.setPositionLine"), {price, label, color_hex}));
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
    QTimer::singleShot(0, this, [self] {
        if (self)
            self->menu_open_ = false;
    });

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
        web_view_->page()->runJavaScript(QStringLiteral("window.priceAtY(%1)").arg(local_y),
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
        // showEvent() and on_load_finished() both call in here; without this flag
        // every call started ANOTHER self-rescheduling 50 ms chain, and all of
        // them kept spinning if the render widget never appeared.
        if (proxy_retry_pending_)
            return;
        proxy_retry_pending_ = true;
        QPointer<KLineChartWidget> self = this;
        QTimer::singleShot(50, this, [self] {
            if (!self)
                return;
            self->proxy_retry_pending_ = false;
            self->install_chart_event_filter();
        });
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
                show_chart_context_menu(static_cast<int>(me->position().y()), me->globalPosition().toPoint());
                return true;
            }
        }
    }
#endif
    return QWidget::eventFilter(obj, ev);
}

} // namespace fincept::ui
