#include "screens/dashboard/widgets/WebScraperWidget.h"

#include "core/logging/Logger.h"
#include "ui/tables/DataTable.h"
#include "ui/theme/Theme.h"

#include <QByteArray>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QRegularExpressionMatchIterator>
#include <QSpinBox>
#include <QStringDecoder>
#include <QStringList>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QXmlStreamReader>

#include <algorithm>

namespace fincept::screens::widgets {

namespace {

constexpr qsizetype kMaxRowsRendered = 5000; // safety cap for very large tables
constexpr qsizetype kMaxHtmlBytes = 10 * 1024 * 1024;
const char* kDefaultUA = "FinceptTerminal/1.0 (compatible; WebScraperWidget)";

// Decode common HTML entities + numeric char refs. Not exhaustive but covers
// the overwhelming majority of real-world tables.
QString decode_html_entities(QString s) {
    static const QHash<QString, QString> kNamed = {
        {"amp", "&"},    {"lt", "<"},     {"gt", ">"},    {"quot", "\""},
        {"apos", "'"},   {"nbsp", " "},   {"ndash", "–"}, {"mdash", "—"},
        {"hellip", "…"}, {"laquo", "«"},  {"raquo", "»"}, {"copy", "©"},
        {"reg", "®"},    {"trade", "™"},  {"euro", "€"},  {"pound", "£"},
        {"yen", "¥"},    {"cent", "¢"},   {"deg", "°"},   {"plusmn", "±"},
        {"times", "×"},  {"divide", "÷"}, {"middot", "·"}, {"bull", "•"},
        {"larr", "←"},   {"rarr", "→"},   {"uarr", "↑"},  {"darr", "↓"},
        {"lsquo", "'"},  {"rsquo", "'"},  {"ldquo", "“"}, {"rdquo", "”"},
    };
    static const QRegularExpression kEntity(
        QStringLiteral("&(#x[0-9a-fA-F]+|#[0-9]+|[a-zA-Z]+[0-9]*);"));
    QString out;
    out.reserve(s.size());
    int pos = 0;
    auto it = kEntity.globalMatch(s);
    while (it.hasNext()) {
        const auto m = it.next();
        out.append(s.mid(pos, m.capturedStart() - pos));
        const QString ref = m.captured(1);
        if (ref.startsWith("#x") || ref.startsWith("#X")) {
            bool ok = false;
            const char32_t cp = ref.mid(2).toUInt(&ok, 16);
            if (ok && cp)
                out.append(QString::fromUcs4(&cp, 1));
        } else if (ref.startsWith('#')) {
            bool ok = false;
            const char32_t cp = ref.mid(1).toUInt(&ok, 10);
            if (ok && cp)
                out.append(QString::fromUcs4(&cp, 1));
        } else {
            auto nit = kNamed.find(ref.toLower());
            if (nit != kNamed.end())
                out.append(nit.value());
            else
                out.append(m.captured(0)); // leave unknown entity as-is
        }
        pos = m.capturedEnd();
    }
    out.append(s.mid(pos));
    return out;
}

// Strip tags and collapse whitespace within a single table cell.
QString strip_tags(const QString& frag) {
    static const QRegularExpression kScript(
        QStringLiteral("<script[^>]*>.*?</script>"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression kStyle(
        QStringLiteral("<style[^>]*>.*?</style>"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression kBr(QStringLiteral("<br\\s*/?>|</(p|div|li|tr)>"),
                                        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression kTag(QStringLiteral("<[^>]+>"));
    static const QRegularExpression kWs(QStringLiteral("\\s+"));

    QString s = frag;
    s.remove(kScript);
    s.remove(kStyle);
    s.replace(kBr, " ");
    s.remove(kTag);
    s = decode_html_entities(s);
    s.replace(kWs, " ");
    return s.trimmed();
}

// Guess encoding: 1) explicit override, 2) charset in Content-Type,
// 3) <meta charset=…> sniff, 4) utf-8.
QString detect_charset(const QByteArray& head_bytes, const QString& content_type,
                       const QString& override_enc) {
    if (!override_enc.isEmpty())
        return override_enc;
    static const QRegularExpression kCT(QStringLiteral("charset\\s*=\\s*([\\w\\-]+)"),
                                        QRegularExpression::CaseInsensitiveOption);
    if (const auto m = kCT.match(content_type); m.hasMatch())
        return m.captured(1);
    const QString head = QString::fromLatin1(head_bytes.left(2048));
    static const QRegularExpression kMeta(
        QStringLiteral("<meta[^>]+charset\\s*=\\s*[\"']?([\\w\\-]+)"),
        QRegularExpression::CaseInsensitiveOption);
    if (const auto m = kMeta.match(head); m.hasMatch())
        return m.captured(1);
    return QStringLiteral("utf-8");
}

// Flatten a JSON value into a row cell string.
QString json_scalar(const QJsonValue& v) {
    if (v.isBool())
        return v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    if (v.isDouble()) {
        const double d = v.toDouble();
        if (d == qint64(d))
            return QString::number(qint64(d));
        return QString::number(d, 'g', 10);
    }
    if (v.isString())
        return v.toString();
    if (v.isNull())
        return {};
    if (v.isArray())
        return QString::fromUtf8(QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact));
    if (v.isObject())
        return QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
    return {};
}

QJsonValue walk_json_path(const QJsonValue& root, const QString& dotted) {
    if (dotted.isEmpty())
        return root;
    QJsonValue cur = root;
    const QStringList parts = dotted.split('.', Qt::SkipEmptyParts);
    for (const QString& p : parts) {
        bool idx_ok = false;
        const int idx = p.toInt(&idx_ok);
        if (idx_ok && cur.isArray()) {
            cur = cur.toArray().at(idx);
        } else if (cur.isObject()) {
            cur = cur.toObject().value(p);
        } else {
            return {};
        }
    }
    return cur;
}

} // namespace

// ─── ctor / dtor ──────────────────────────────────────────────────────────

WebScraperWidget::WebScraperWidget(const QJsonObject& cfg, QWidget* parent)
    : BaseWidget("WEB SCRAPER", parent) {
    net_ = new QNetworkAccessManager(this);
    connect(net_, &QNetworkAccessManager::finished, this, &WebScraperWidget::handle_reply);

    auto_timer_ = new QTimer(this);
    auto_timer_->setSingleShot(false);
    connect(auto_timer_, &QTimer::timeout, this, &WebScraperWidget::on_auto_refresh_tick);

    build_ui();
    set_configurable(true);
    apply_styles();

    connect(this, &BaseWidget::refresh_requested, this, &WebScraperWidget::on_refresh_clicked);

    apply_config(cfg);
}

WebScraperWidget::~WebScraperWidget() {
    if (pending_reply_) {
        pending_reply_->abort();
        pending_reply_->deleteLater();
    }
}

// ─── UI ───────────────────────────────────────────────────────────────────

void WebScraperWidget::build_ui() {
    auto* vl = content_layout();
    vl->setContentsMargins(8, 6, 8, 8);
    vl->setSpacing(6);

    auto* header_row = new QHBoxLayout();
    header_row->setContentsMargins(0, 0, 0, 0);
    header_row->setSpacing(8);

    format_badge_ = new QLabel("—");
    format_badge_->setObjectName("scraperFormatBadge");
    header_row->addWidget(format_badge_);

    table_combo_ = new QComboBox();
    table_combo_->setObjectName("scraperTableCombo");
    table_combo_->setMinimumContentsLength(12);
    table_combo_->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    connect(table_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &WebScraperWidget::on_table_selected);
    header_row->addWidget(table_combo_, 1);

    status_label_ = new QLabel("Configure a URL via the gear icon");
    status_label_->setObjectName("scraperStatus");
    status_label_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    header_row->addWidget(status_label_, 2);

    vl->addLayout(header_row);

    table_ = new ui::DataTable();
    table_->verticalHeader()->setVisible(false);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table_->setSortingEnabled(true);
    vl->addWidget(table_, 1);
}

void WebScraperWidget::apply_styles() {
    const QString badge_css = QString("color:%1;background:%2;border-radius:3px;"
                                       "padding:2px 6px;font-size:10px;font-weight:700;")
                                  .arg(ui::colors::TEXT_ON_ACCENT())
                                  .arg(ui::colors::AMBER());
    if (format_badge_)
        format_badge_->setStyleSheet(badge_css);

    const QString status_css =
        QString("color:%1;font-size:11px;background:transparent;").arg(ui::colors::TEXT_TERTIARY());
    if (status_label_)
        status_label_->setStyleSheet(status_css);
}

void WebScraperWidget::on_theme_changed() {
    apply_styles();
}

// ─── Config ───────────────────────────────────────────────────────────────

QJsonObject WebScraperWidget::config() const {
    QJsonObject o;
    o.insert("url", url_);
    o.insert("table_index", table_index_);
    o.insert("refresh_sec", refresh_sec_);
    if (!user_agent_.isEmpty())
        o.insert("user_agent", user_agent_);
    if (!headers_.isEmpty()) {
        QJsonObject h;
        for (auto it = headers_.cbegin(); it != headers_.cend(); ++it)
            h.insert(it.key(), it.value());
        o.insert("headers", h);
    }
    if (!encoding_.isEmpty())
        o.insert("encoding", encoding_);
    if (!json_path_.isEmpty())
        o.insert("json_path", json_path_);
    if (!force_format_.isEmpty())
        o.insert("force_format", force_format_);
    return o;
}

void WebScraperWidget::apply_config(const QJsonObject& cfg) {
    url_ = cfg.value("url").toString().trimmed();
    table_index_ = cfg.value("table_index").toInt(0);
    refresh_sec_ = cfg.value("refresh_sec").toInt(0);
    user_agent_ = cfg.value("user_agent").toString();
    headers_.clear();
    const QJsonObject h = cfg.value("headers").toObject();
    for (auto it = h.constBegin(); it != h.constEnd(); ++it)
        headers_.insert(it.key(), it.value().toString());
    encoding_ = cfg.value("encoding").toString();
    json_path_ = cfg.value("json_path").toString();
    force_format_ = cfg.value("force_format").toString();

    apply_timer_state();

    if (!url_.isEmpty() && isVisible())
        start_fetch();
    else if (url_.isEmpty())
        set_status("Configure a URL via the gear icon");
}

void WebScraperWidget::apply_timer_state() {
    if (refresh_sec_ > 0)
        auto_timer_->setInterval(refresh_sec_ * 1000);
    if (!isVisible() || refresh_sec_ <= 0) {
        auto_timer_->stop();
    } else if (!auto_timer_->isActive()) {
        auto_timer_->start();
    }
}

// ─── Lifecycle ────────────────────────────────────────────────────────────

void WebScraperWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    apply_timer_state();
    if (!url_.isEmpty() && tables_.isEmpty())
        start_fetch();
}

void WebScraperWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e);
    auto_timer_->stop();
}

// ─── Fetch ────────────────────────────────────────────────────────────────

void WebScraperWidget::on_refresh_clicked() {
    if (url_.isEmpty())
        return;
    start_fetch();
}

void WebScraperWidget::on_auto_refresh_tick() {
    if (url_.isEmpty() || pending_reply_)
        return;
    start_fetch();
}

void WebScraperWidget::start_fetch() {
    const QUrl url(url_);
    if (!url.isValid() || url.scheme().isEmpty()) {
        set_status("Invalid URL", true);
        return;
    }
    if (pending_reply_) {
        pending_reply_->abort();
        pending_reply_->deleteLater();
        pending_reply_.clear();
    }

    set_loading(true);
    set_status("Fetching…");

    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  user_agent_.isEmpty() ? QString::fromLatin1(kDefaultUA) : user_agent_);
    req.setRawHeader("Accept",
                     "text/html,application/xhtml+xml,application/json,text/csv,application/xml;q=0.9,*/*;q=0.8");
    req.setRawHeader("Accept-Language", "en-US,en;q=0.9");
    for (auto it = headers_.cbegin(); it != headers_.cend(); ++it)
        req.setRawHeader(it.key().toUtf8(), it.value().toUtf8());

    pending_reply_ = net_->get(req);
}

void WebScraperWidget::handle_reply(QNetworkReply* reply) {
    reply->deleteLater();
    if (reply != pending_reply_.data())
        return;
    pending_reply_.clear();
    set_loading(false);

    if (reply->error() != QNetworkReply::NoError) {
        set_status(QString("Fetch failed: %1").arg(reply->errorString()), true);
        LOG_WARN("WebScraper", QString("Fetch %1 failed: %2").arg(url_, reply->errorString()));
        return;
    }

    const QByteArray body = reply->readAll();
    if (body.isEmpty()) {
        set_status("Empty response", true);
        return;
    }
    const QString ct = reply->header(QNetworkRequest::ContentTypeHeader).toString();
    parse_payload(body, ct);
}

// ─── Parsing ──────────────────────────────────────────────────────────────

void WebScraperWidget::parse_payload(const QByteArray& body, const QString& content_type) {
    tables_.clear();

    const QString fmt_override = force_format_.toLower();
    QString detected_fmt;

    auto try_html = [&]() {
        const QString enc = detect_charset(body, content_type, encoding_);
        auto decoder = QStringDecoder(enc.toUtf8().constData());
        QString text = decoder.isValid() ? decoder.decode(body.left(kMaxHtmlBytes))
                                          : QString::fromUtf8(body.left(kMaxHtmlBytes));
        tables_ = parse_html_tables(text);
        if (!tables_.isEmpty())
            detected_fmt = "HTML";
    };
    auto try_json = [&]() {
        tables_ = parse_json_tables(body);
        if (!tables_.isEmpty())
            detected_fmt = "JSON";
    };
    auto try_csv = [&](QChar delim, const QString& label) {
        const QString enc = detect_charset(body, content_type, encoding_);
        auto decoder = QStringDecoder(enc.toUtf8().constData());
        QString text = decoder.isValid() ? decoder.decode(body) : QString::fromUtf8(body);
        tables_ = parse_csv_tables(text, delim);
        if (!tables_.isEmpty())
            detected_fmt = label;
    };
    auto try_xml = [&]() {
        tables_ = parse_xml_tables(body);
        if (!tables_.isEmpty())
            detected_fmt = "XML";
    };

    if (fmt_override == "html") {
        try_html();
    } else if (fmt_override == "json") {
        try_json();
    } else if (fmt_override == "csv") {
        try_csv(',', "CSV");
    } else if (fmt_override == "tsv") {
        try_csv('\t', "TSV");
    } else if (fmt_override == "xml") {
        try_xml();
    } else {
        // Auto-detect by Content-Type first, then by content sniff.
        const QString ct = content_type.toLower();
        if (ct.contains("json"))
            try_json();
        else if (ct.contains("csv"))
            try_csv(',', "CSV");
        else if (ct.contains("tab-separated") || ct.contains("tsv"))
            try_csv('\t', "TSV");
        else if (ct.contains("xml") || ct.contains("rss") || ct.contains("atom"))
            try_xml();
        else if (ct.contains("html"))
            try_html();

        // Fallback sniff if Content-Type was wrong or missing.
        if (tables_.isEmpty()) {
            const QByteArray head = body.left(256).trimmed();
            if (head.startsWith('{') || head.startsWith('['))
                try_json();
            else if (head.startsWith("<?xml") || head.startsWith("<rss") ||
                     head.startsWith("<feed"))
                try_xml();
            else if (head.contains("<table") || head.contains("<TABLE") || head.contains("<html") ||
                     head.contains("<!DOCTYPE"))
                try_html();
        }
        // Last-ditch: try HTML (covers text/plain with embedded HTML), then CSV.
        if (tables_.isEmpty())
            try_html();
        if (tables_.isEmpty())
            try_csv(',', "CSV");
    }

    last_format_ = detected_fmt;
    format_badge_->setText(detected_fmt.isEmpty() ? QStringLiteral("—") : detected_fmt);

    // Repopulate table-picker combo.
    const int prev_index = table_index_;
    QSignalBlocker block(table_combo_);
    table_combo_->clear();
    for (int i = 0; i < tables_.size(); ++i)
        table_combo_->addItem(QString("%1 (%2 rows)")
                                  .arg(tables_[i].label.isEmpty() ? QString("Table %1").arg(i + 1)
                                                                   : tables_[i].label)
                                  .arg(tables_[i].rows.size()));
    if (tables_.isEmpty()) {
        table_combo_->setEnabled(false);
        table_->clear_data();
        table_->set_headers({});
        set_status("No tabular data found — site may require JavaScript. Try its JSON API URL instead.",
                   true);
        return;
    }
    table_combo_->setEnabled(tables_.size() > 1);
    const int idx = std::clamp<int>(prev_index, 0, static_cast<int>(tables_.size()) - 1);
    table_index_ = idx;
    table_combo_->setCurrentIndex(idx);
    render_selected_table();
    set_status(QString("Loaded %1 table%2").arg(tables_.size()).arg(tables_.size() == 1 ? "" : "s"));
}

void WebScraperWidget::on_table_selected(int index) {
    if (index < 0 || index >= tables_.size())
        return;
    if (index == table_index_)
        return;
    table_index_ = index;
    render_selected_table();
    emit config_changed(config());
}

void WebScraperWidget::render_selected_table() {
    if (table_index_ < 0 || table_index_ >= tables_.size())
        return;
    const ScrapedTable& t = tables_[table_index_];
    table_->setSortingEnabled(false);
    table_->set_headers(t.headers);
    const qsizetype n = std::min<qsizetype>(t.rows.size(), kMaxRowsRendered);
    QVector<QStringList> display = t.rows.mid(0, n);
    table_->set_data(display);
    table_->setSortingEnabled(true);
    if (t.rows.size() > kMaxRowsRendered) {
        set_status(QString("Showing first %1 of %2 rows").arg(n).arg(t.rows.size()));
    }
}

void WebScraperWidget::set_status(const QString& msg, bool error) {
    if (!status_label_)
        return;
    status_label_->setText(msg);
    const QColor c = error ? ui::colors::NEGATIVE() : ui::colors::TEXT_TERTIARY();
    status_label_->setStyleSheet(
        QString("color:%1;font-size:11px;background:transparent;").arg(c.name()));
}

// ─── HTML parser ──────────────────────────────────────────────────────────

QVector<ScrapedTable> WebScraperWidget::parse_html_tables(const QString& html) const {
    QVector<ScrapedTable> out;

    static const QRegularExpression kTableRe(
        QStringLiteral("<table\\b[^>]*>(.*?)</table>"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression kRowRe(
        QStringLiteral("<tr\\b[^>]*>(.*?)</tr>"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression kCellRe(
        QStringLiteral("<(t[hd])\\b([^>]*)>(.*?)</\\1>"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression kColspanRe(QStringLiteral("colspan\\s*=\\s*\"?(\\d+)\"?"),
                                                QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression kCaptionRe(
        QStringLiteral("<caption[^>]*>(.*?)</caption>"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);

    int table_idx = 0;
    auto tit = kTableRe.globalMatch(html);
    while (tit.hasNext()) {
        const auto tm = tit.next();
        const QString inner = tm.captured(1);
        ++table_idx;

        ScrapedTable tbl;
        const auto cap = kCaptionRe.match(inner);
        if (cap.hasMatch()) {
            const QString c = strip_tags(cap.captured(1));
            if (!c.isEmpty())
                tbl.label = c.left(60);
        }
        if (tbl.label.isEmpty())
            tbl.label = QString("Table %1").arg(table_idx);

        QVector<QStringList> rows;
        qsizetype max_cols = 0;
        bool first_row_is_header = false;

        auto rit = kRowRe.globalMatch(inner);
        int row_no = 0;
        while (rit.hasNext()) {
            const auto rm = rit.next();
            const QString row_html = rm.captured(1);
            QStringList cells;
            bool row_has_th = false;
            auto cit = kCellRe.globalMatch(row_html);
            while (cit.hasNext()) {
                const auto cm = cit.next();
                const QString tag = cm.captured(1).toLower();
                const QString attrs = cm.captured(2);
                const QString content = strip_tags(cm.captured(3));
                int colspan = 1;
                if (const auto cs = kColspanRe.match(attrs); cs.hasMatch())
                    colspan = std::clamp(cs.captured(1).toInt(), 1, 16);
                cells.append(content);
                for (int i = 1; i < colspan; ++i)
                    cells.append(QString());
                if (tag == "th")
                    row_has_th = true;
            }
            if (cells.isEmpty())
                continue;
            if (row_no == 0 && row_has_th)
                first_row_is_header = true;
            max_cols = std::max<qsizetype>(max_cols, cells.size());
            rows.append(cells);
            ++row_no;
        }
        if (rows.isEmpty())
            continue;

        if (first_row_is_header) {
            tbl.headers = rows.takeFirst();
        } else {
            tbl.headers.reserve(max_cols);
            for (qsizetype i = 0; i < max_cols; ++i)
                tbl.headers.append(QString("Col %1").arg(i + 1));
        }
        // Pad ragged rows to max_cols.
        for (QStringList& r : rows) {
            while (r.size() < max_cols)
                r.append(QString());
        }
        while (tbl.headers.size() < max_cols)
            tbl.headers.append(QString("Col %1").arg(tbl.headers.size() + 1));

        tbl.rows = rows;
        out.append(std::move(tbl));
    }
    return out;
}

// ─── JSON parser ──────────────────────────────────────────────────────────

QVector<ScrapedTable> WebScraperWidget::parse_json_tables(const QByteArray& body) const {
    QVector<ScrapedTable> out;
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(body, &err);
    if (err.error != QJsonParseError::NoError)
        return out;

    QJsonValue root = doc.isArray() ? QJsonValue(doc.array()) : QJsonValue(doc.object());
    if (!json_path_.isEmpty())
        root = walk_json_path(root, json_path_);

    auto table_from_array = [](const QJsonArray& arr, const QString& label) -> ScrapedTable {
        ScrapedTable t;
        t.label = label;
        if (arr.isEmpty())
            return t;
        // Array of objects → union of keys becomes headers.
        bool obj_array = true;
        for (const auto& v : arr) {
            if (!v.isObject()) {
                obj_array = false;
                break;
            }
        }
        if (obj_array) {
            QStringList keys;
            QHash<QString, int> key_idx;
            for (const auto& v : arr) {
                for (const QString& k : v.toObject().keys()) {
                    if (!key_idx.contains(k)) {
                        key_idx.insert(k, keys.size());
                        keys.append(k);
                    }
                }
            }
            t.headers = keys;
            for (const auto& v : arr) {
                const QJsonObject o = v.toObject();
                QStringList row;
                row.reserve(keys.size());
                for (const QString& k : keys)
                    row.append(json_scalar(o.value(k)));
                t.rows.append(row);
            }
            return t;
        }
        // Array of arrays.
        bool arr_array = true;
        qsizetype max_cols = 0;
        for (const auto& v : arr) {
            if (!v.isArray()) {
                arr_array = false;
                break;
            }
            max_cols = std::max<qsizetype>(max_cols, v.toArray().size());
        }
        if (arr_array) {
            for (qsizetype i = 0; i < max_cols; ++i)
                t.headers.append(QString("Col %1").arg(i + 1));
            for (const auto& v : arr) {
                const QJsonArray inner = v.toArray();
                QStringList row;
                row.reserve(max_cols);
                for (qsizetype i = 0; i < max_cols; ++i)
                    row.append(json_scalar(inner.at(i)));
                t.rows.append(row);
            }
            return t;
        }
        // Array of scalars.
        t.headers = {QStringLiteral("value")};
        for (const auto& v : arr)
            t.rows.append({json_scalar(v)});
        return t;
    };

    if (root.isArray()) {
        ScrapedTable t = table_from_array(root.toArray(), QStringLiteral("root[]"));
        if (!t.rows.isEmpty())
            out.append(std::move(t));
    } else if (root.isObject()) {
        const QJsonObject o = root.toObject();
        // Surface every array-valued field at this level as its own table.
        for (auto it = o.constBegin(); it != o.constEnd(); ++it) {
            if (it.value().isArray()) {
                ScrapedTable t = table_from_array(it.value().toArray(), it.key() + "[]");
                if (!t.rows.isEmpty())
                    out.append(std::move(t));
            }
        }
        // Fallback: render the object itself as key/value table.
        if (out.isEmpty()) {
            ScrapedTable kv;
            kv.label = QStringLiteral("object");
            kv.headers = {QStringLiteral("key"), QStringLiteral("value")};
            for (auto it = o.constBegin(); it != o.constEnd(); ++it)
                kv.rows.append({it.key(), json_scalar(it.value())});
            if (!kv.rows.isEmpty())
                out.append(std::move(kv));
        }
    } else {
        // Scalar root after json_path walk.
        ScrapedTable kv;
        kv.label = json_path_.isEmpty() ? QStringLiteral("value") : json_path_;
        kv.headers = {QStringLiteral("value")};
        kv.rows.append({json_scalar(root)});
        out.append(std::move(kv));
    }
    return out;
}

// ─── CSV / TSV parser ─────────────────────────────────────────────────────

QVector<ScrapedTable> WebScraperWidget::parse_csv_tables(const QString& text, QChar delim) const {
    // Proper RFC-4180-ish parser: quoted fields, escaped quotes, embedded newlines.
    QVector<QStringList> rows;
    QStringList cur_row;
    QString cur_field;
    bool in_quotes = false;
    qsizetype max_cols = 0;
    for (qsizetype i = 0; i < text.size(); ++i) {
        const QChar c = text[i];
        if (in_quotes) {
            if (c == '"') {
                if (i + 1 < text.size() && text[i + 1] == '"') {
                    cur_field.append('"');
                    ++i;
                } else {
                    in_quotes = false;
                }
            } else {
                cur_field.append(c);
            }
        } else {
            if (c == '"' && cur_field.isEmpty()) {
                in_quotes = true;
            } else if (c == delim) {
                cur_row.append(cur_field);
                cur_field.clear();
            } else if (c == '\r') {
                // swallow; \n will flush
            } else if (c == '\n') {
                cur_row.append(cur_field);
                cur_field.clear();
                max_cols = std::max<qsizetype>(max_cols, cur_row.size());
                rows.append(cur_row);
                cur_row.clear();
            } else {
                cur_field.append(c);
            }
        }
    }
    if (!cur_field.isEmpty() || !cur_row.isEmpty()) {
        cur_row.append(cur_field);
        max_cols = std::max<qsizetype>(max_cols, cur_row.size());
        rows.append(cur_row);
    }
    if (rows.isEmpty() || max_cols < 2)
        return {};

    ScrapedTable t;
    t.label = delim == '\t' ? QStringLiteral("TSV") : QStringLiteral("CSV");
    t.headers = rows.takeFirst();
    while (t.headers.size() < max_cols)
        t.headers.append(QString("Col %1").arg(t.headers.size() + 1));
    for (QStringList& r : rows) {
        while (r.size() < max_cols)
            r.append(QString());
    }
    t.rows = rows;
    return {t};
}

// ─── XML / RSS / Atom parser ──────────────────────────────────────────────

QVector<ScrapedTable> WebScraperWidget::parse_xml_tables(const QByteArray& body) const {
    QVector<ScrapedTable> out;
    QXmlStreamReader xml(body);

    // Collect all element records keyed by tag name, together with their
    // child scalar fields. Repeating element names with >1 occurrence and at
    // least one shared field become tables.
    struct Record {
        QMap<QString, QString> fields;
    };
    QHash<QString, QVector<Record>> by_tag;

    while (!xml.atEnd() && !xml.hasError()) {
        const auto tok = xml.readNext();
        if (tok != QXmlStreamReader::StartElement)
            continue;
        // Only consider non-root repeated elements that have attributes OR
        // child scalar elements — this covers RSS <item>, Atom <entry>, and
        // arbitrary <row>/<record> elements.
        const QString tag = xml.name().toString();
        Record rec;
        // Pull attributes.
        for (const auto& attr : xml.attributes())
            rec.fields.insert(QStringLiteral("@") + attr.name().toString(), attr.value().toString());
        // Read children as scalars (one level deep; nested gets serialized).
        int depth = 1;
        while (!xml.atEnd() && depth > 0) {
            const auto t = xml.readNext();
            if (t == QXmlStreamReader::StartElement) {
                const QString ck = xml.name().toString();
                QString val;
                int sub_depth = 1;
                while (!xml.atEnd() && sub_depth > 0) {
                    const auto tt = xml.readNext();
                    if (tt == QXmlStreamReader::StartElement)
                        ++sub_depth;
                    else if (tt == QXmlStreamReader::EndElement)
                        --sub_depth;
                    else if (tt == QXmlStreamReader::Characters)
                        val.append(xml.text().toString());
                }
                const QString norm = val.simplified();
                if (!norm.isEmpty())
                    rec.fields.insert(ck, norm);
            } else if (t == QXmlStreamReader::EndElement) {
                --depth;
            }
        }
        if (!rec.fields.isEmpty())
            by_tag[tag].append(rec);
    }

    // Any tag with >= 2 records and at least one common field → a table.
    for (auto it = by_tag.cbegin(); it != by_tag.cend(); ++it) {
        const QVector<Record>& recs = it.value();
        if (recs.size() < 2)
            continue;
        // Union of field names across records.
        QStringList keys;
        QHash<QString, int> key_idx;
        for (const auto& r : recs) {
            for (auto fit = r.fields.cbegin(); fit != r.fields.cend(); ++fit) {
                if (!key_idx.contains(fit.key())) {
                    key_idx.insert(fit.key(), keys.size());
                    keys.append(fit.key());
                }
            }
        }
        if (keys.isEmpty())
            continue;
        ScrapedTable t;
        t.label = QStringLiteral("<%1> × %2").arg(it.key()).arg(recs.size());
        t.headers = keys;
        for (const auto& r : recs) {
            QStringList row;
            row.reserve(keys.size());
            for (const QString& k : keys)
                row.append(r.fields.value(k));
            t.rows.append(row);
        }
        out.append(std::move(t));
    }

    // Prefer RSS/Atom "item"/"entry" tables first for usability.
    std::stable_sort(out.begin(), out.end(), [](const ScrapedTable& a, const ScrapedTable& b) {
        auto rank = [](const QString& label) {
            if (label.startsWith("<item>"))
                return 0;
            if (label.startsWith("<entry>"))
                return 1;
            return 2;
        };
        return rank(a.label) < rank(b.label);
    });
    return out;
}

// ─── Config dialog ────────────────────────────────────────────────────────

QDialog* WebScraperWidget::make_config_dialog(QWidget* parent) {
    auto* dlg = new QDialog(parent);
    dlg->setWindowTitle("Configure — Web Scraper");
    dlg->resize(520, 420);
    auto* form = new QFormLayout(dlg);

    auto* url_edit = new QLineEdit(dlg);
    url_edit->setText(url_);
    url_edit->setPlaceholderText("https://example.com/table-page or API endpoint");
    form->addRow("URL", url_edit);

    auto* refresh_spin = new QSpinBox(dlg);
    refresh_spin->setRange(0, 86400);
    refresh_spin->setSuffix(" sec");
    refresh_spin->setSpecialValueText("manual only");
    refresh_spin->setValue(refresh_sec_);
    form->addRow("Auto-refresh", refresh_spin);

    auto* ua_edit = new QLineEdit(dlg);
    ua_edit->setText(user_agent_);
    ua_edit->setPlaceholderText(QString::fromLatin1(kDefaultUA));
    form->addRow("User-Agent", ua_edit);

    auto* fmt_combo = new QComboBox(dlg);
    fmt_combo->addItems({"Auto-detect", "HTML", "JSON", "CSV", "TSV", "XML"});
    const QMap<QString, int> fmt_idx = {{"", 0},    {"html", 1}, {"json", 2},
                                         {"csv", 3}, {"tsv", 4},  {"xml", 5}};
    fmt_combo->setCurrentIndex(fmt_idx.value(force_format_.toLower(), 0));
    form->addRow("Format", fmt_combo);

    auto* enc_edit = new QLineEdit(dlg);
    enc_edit->setText(encoding_);
    enc_edit->setPlaceholderText("auto (from Content-Type / <meta>)");
    form->addRow("Encoding", enc_edit);

    auto* json_path_edit = new QLineEdit(dlg);
    json_path_edit->setText(json_path_);
    json_path_edit->setPlaceholderText("e.g. data.items  (JSON only)");
    form->addRow("JSON path", json_path_edit);

    auto* headers_edit = new QPlainTextEdit(dlg);
    QString hdr_text;
    for (auto it = headers_.cbegin(); it != headers_.cend(); ++it)
        hdr_text.append(QString("%1: %2\n").arg(it.key(), it.value()));
    headers_edit->setPlainText(hdr_text);
    headers_edit->setPlaceholderText("One per line:\nAuthorization: Bearer abc\nX-API-Key: xyz");
    headers_edit->setMaximumHeight(100);
    form->addRow("Extra headers", headers_edit);

    auto* help = new QLabel(
        "Auto-detects HTML tables, JSON arrays/objects, CSV/TSV, XML/RSS/Atom. "
        "If the page renders tables via JavaScript, use its underlying API URL instead.");
    help->setWordWrap(true);
    help->setStyleSheet(QString("color:%1;font-size:11px;").arg(ui::colors::TEXT_TERTIARY()));
    form->addRow(help);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    form->addRow(buttons);

    connect(buttons, &QDialogButtonBox::accepted, dlg, [this, dlg, url_edit, refresh_spin, ua_edit,
                                                         fmt_combo, enc_edit, json_path_edit,
                                                         headers_edit]() {
        QJsonObject cfg;
        cfg.insert("url", url_edit->text().trimmed());
        cfg.insert("refresh_sec", refresh_spin->value());
        cfg.insert("table_index", 0); // reset on URL change
        if (!ua_edit->text().isEmpty())
            cfg.insert("user_agent", ua_edit->text());

        const QStringList fmts = {"", "html", "json", "csv", "tsv", "xml"};
        const int fi = fmt_combo->currentIndex();
        if (fi > 0 && fi < fmts.size())
            cfg.insert("force_format", fmts[fi]);

        if (!enc_edit->text().isEmpty())
            cfg.insert("encoding", enc_edit->text());
        if (!json_path_edit->text().isEmpty())
            cfg.insert("json_path", json_path_edit->text());

        QJsonObject hdrs;
        const QStringList lines = headers_edit->toPlainText().split('\n', Qt::SkipEmptyParts);
        for (const QString& l : lines) {
            const int colon = l.indexOf(':');
            if (colon <= 0)
                continue;
            const QString k = l.left(colon).trimmed();
            const QString v = l.mid(colon + 1).trimmed();
            if (!k.isEmpty())
                hdrs.insert(k, v);
        }
        if (!hdrs.isEmpty())
            cfg.insert("headers", hdrs);

        apply_config(cfg);
        emit config_changed(cfg);
        dlg->accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    return dlg;
}

} // namespace fincept::screens::widgets
