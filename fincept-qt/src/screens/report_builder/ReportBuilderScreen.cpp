#include "screens/report_builder/ReportBuilderScreen.h"

#include "core/session/ScreenStateManager.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "services/file_manager/FileManagerService.h"
#include "services/markets/MarketDataService.h"
#include "ui/theme/Theme.h"

#include <QApplication>
#include <QComboBox>
#include <QDateEdit>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPointer>
#include <QPrintPreviewDialog>
#include <QPrinter>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QTextDocumentWriter>
#include <QTextFrame>
#include <QVBoxLayout>

namespace fincept::screens {

// ── Undo Commands ──────────────────────────────────────────────────────────────

AddComponentCommand::AddComponentCommand(ReportBuilderScreen* screen, const ReportComponent& comp, int insert_at)
    : QUndoCommand("Add " + comp.type), screen_(screen), comp_(comp), insert_at_(insert_at) {}

void AddComponentCommand::redo() {
    screen_->add_component_direct(comp_, insert_at_);
}

void AddComponentCommand::undo() {
    screen_->remove_component_direct(insert_at_);
}

RemoveComponentCommand::RemoveComponentCommand(ReportBuilderScreen* screen, int index)
    : QUndoCommand("Remove component"), screen_(screen), index_(index) {
    if (index >= 0 && index < screen->components().size()) {
        saved_ = screen->components()[index];
    }
}

void RemoveComponentCommand::redo() {
    screen_->remove_component_direct(index_);
}

void RemoveComponentCommand::undo() {
    screen_->add_component_direct(saved_, index_);
}

UpdateComponentCommand::UpdateComponentCommand(ReportBuilderScreen* screen, int index, const QString& old_content,
                                               const QMap<QString, QString>& old_config, const QString& new_content,
                                               const QMap<QString, QString>& new_config)
    : QUndoCommand("Edit component"),
      screen_(screen),
      index_(index),
      old_content_(old_content),
      new_content_(new_content),
      old_config_(old_config),
      new_config_(new_config) {}

void UpdateComponentCommand::redo() {
    screen_->update_component_direct(index_, new_content_, new_config_);
}

void UpdateComponentCommand::undo() {
    screen_->update_component_direct(index_, old_content_, old_config_);
}

bool UpdateComponentCommand::mergeWith(const QUndoCommand* other) {
    if (other->id() != id())
        return false;
    auto* cmd = static_cast<const UpdateComponentCommand*>(other);
    if (cmd->index_ != index_)
        return false;
    new_content_ = cmd->new_content_;
    new_config_ = cmd->new_config_;
    return true;
}

MoveComponentCommand::MoveComponentCommand(ReportBuilderScreen* screen, int from, int to)
    : QUndoCommand("Move component"), screen_(screen), from_(from), to_(to) {}

void MoveComponentCommand::redo() {
    screen_->swap_components(from_, to_);
    screen_->selected() = to_;
    screen_->refresh_canvas();
    screen_->refresh_structure();
}

void MoveComponentCommand::undo() {
    screen_->swap_components(to_, from_);
    screen_->selected() = from_;
    screen_->refresh_canvas();
    screen_->refresh_structure();
}

// ── Direct (no-undo-push) operations ──────────────────────────────────────────

void ReportBuilderScreen::add_component_direct(const ReportComponent& comp, int at) {
    if (at >= 0 && at <= components_.size()) {
        components_.insert(at, comp);
    } else {
        components_.append(comp);
    }
    refresh_canvas();
    refresh_structure();
    ScreenStateManager::instance().notify_changed(this);
}

void ReportBuilderScreen::remove_component_direct(int index) {
    if (index >= 0 && index < components_.size()) {
        components_.removeAt(index);
        if (selected_ >= components_.size())
            selected_ = components_.size() - 1;
        refresh_canvas();
        refresh_structure();
        select_component(selected_);
        ScreenStateManager::instance().notify_changed(this);
    }
}

void ReportBuilderScreen::update_component_direct(int index, const QString& content,
                                                  const QMap<QString, QString>& config) {
    if (index >= 0 && index < components_.size()) {
        components_[index].content = content;
        for (auto it = config.cbegin(); it != config.cend(); ++it) {
            components_[index].config[it.key()] = it.value();
        }
        refresh_canvas();
    }
}

void ReportBuilderScreen::swap_components(int a, int b) {
    if (a >= 0 && b >= 0 && a < components_.size() && b < components_.size()) {
        components_.swapItemsAt(a, b);
    }
}

// ── Constructor ───────────────────────────────────────────────────────────────

ReportBuilderScreen::ReportBuilderScreen(QWidget* parent) : QWidget(parent) {
    setStyleSheet(QString("background: %1;").arg(ui::colors::DARK()));
    metadata_.date = QDateTime::currentDateTime().toString("yyyy-MM-dd");

    // Create undo stack first — build_toolbar() connects to it
    undo_stack_ = new QUndoStack(this);

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    vl->addWidget(build_toolbar());

    auto* sep = new QFrame;
    sep->setFixedHeight(1);
    sep->setStyleSheet(QString("background: %1; border: none;").arg(ui::colors::BORDER()));
    vl->addWidget(sep);

    splitter_ = new QSplitter(Qt::Horizontal);
    splitter_->setHandleWidth(1);
    splitter_->setStyleSheet(QString("QSplitter::handle { background: %1; }"
                                     "QSplitter::handle:hover { background: %2; }")
                                 .arg(ui::colors::BORDER_DIM(), ui::colors::BORDER_BRIGHT()));

    comp_toolbar_ = new ComponentToolbar;
    canvas_ = new DocumentCanvas;
    properties_ = new PropertiesPanel;

    splitter_->addWidget(comp_toolbar_);
    splitter_->addWidget(canvas_);
    splitter_->addWidget(properties_);
    splitter_->setSizes({240, 700, 280});

    vl->addWidget(splitter_, 1);

    autosave_ = new QTimer(this);
    autosave_->setInterval(60000);
    connect(autosave_, &QTimer::timeout, this, &ReportBuilderScreen::on_auto_save);

    // ── Toolbar file/doc actions ──────────────────────────────────────────────
    connect(comp_toolbar_, &ComponentToolbar::new_report_requested, this, &ReportBuilderScreen::on_new);
    connect(comp_toolbar_, &ComponentToolbar::open_report_requested, this, &ReportBuilderScreen::on_open);
    connect(comp_toolbar_, &ComponentToolbar::recent_reports_requested, this, &ReportBuilderScreen::show_recent_dialog);
    connect(comp_toolbar_, &ComponentToolbar::templates_requested, this, &ReportBuilderScreen::show_template_dialog);
    connect(comp_toolbar_, &ComponentToolbar::theme_requested, this, &ReportBuilderScreen::show_theme_dialog);
    connect(comp_toolbar_, &ComponentToolbar::metadata_requested, this, &ReportBuilderScreen::show_metadata_dialog);

    // ── Font signal ───────────────────────────────────────────────────────────
    connect(comp_toolbar_, &ComponentToolbar::font_changed, this,
            [this](const QString& family, int size, bool bold, bool italic) {
                QTextCharFormat fmt;
                fmt.setFontFamilies({family});
                fmt.setFontPointSize(size);
                fmt.setFontWeight(bold ? QFont::Bold : QFont::Normal);
                fmt.setFontItalic(italic);
                canvas_->text_edit()->mergeCurrentCharFormat(fmt);
            });

    // ── Component signals ─────────────────────────────────────────────────────
    connect(comp_toolbar_, &ComponentToolbar::add_component, this, &ReportBuilderScreen::add_component);
    connect(comp_toolbar_, &ComponentToolbar::structure_selected, this, &ReportBuilderScreen::select_component);
    connect(comp_toolbar_, &ComponentToolbar::delete_item, this, &ReportBuilderScreen::remove_component);

    connect(comp_toolbar_, &ComponentToolbar::duplicate, this, [this](int idx) {
        if (idx >= 0 && idx < components_.size()) {
            ReportComponent copy = components_[idx];
            copy.id = next_id_++;
            undo_stack_->push(new AddComponentCommand(this, copy, idx + 1));
        }
    });
    connect(comp_toolbar_, &ComponentToolbar::move_up, this, [this](int idx) {
        if (idx > 0)
            undo_stack_->push(new MoveComponentCommand(this, idx, idx - 1));
    });
    connect(comp_toolbar_, &ComponentToolbar::move_down, this, [this](int idx) {
        if (idx >= 0 && idx < components_.size() - 1)
            undo_stack_->push(new MoveComponentCommand(this, idx, idx + 1));
    });

    // ── Properties signals ────────────────────────────────────────────────────
    connect(properties_, &PropertiesPanel::content_changed, this, [this](int idx, const QString& content) {
        if (idx < 0 || idx >= components_.size())
            return;
        auto old_content = components_[idx].content;
        auto old_config = components_[idx].config;
        auto new_config = old_config;
        undo_stack_->push(new UpdateComponentCommand(this, idx, old_content, old_config, content, new_config));
    });

    connect(
        properties_, &PropertiesPanel::config_changed, this, [this](int idx, const QString& key, const QString& val) {
            if (idx < 0 || idx >= components_.size())
                return;

            // ── stats_block: fetch real fundamentals via get_info ─────────
            if (components_[idx].type == "stats_block" && key == "fetch_ticker") {
                QString sym = val;
                components_[idx].config["fetch_ticker"] = "";
                components_[idx].config["data"] = "(Loading " + sym + "...)";
                refresh_canvas();
                QPointer<ReportBuilderScreen> self = this;
                fincept::services::MarketDataService::instance().fetch_info(
                    sym, [self, idx, sym](bool ok, fincept::services::InfoData info) {
                        if (!self)
                            return;
                        QMetaObject::invokeMethod(
                            self,
                            [self, idx, sym, ok, info]() {
                                if (!self || idx >= self->components_.size())
                                    return;
                                QStringList lines;
                                if (!ok) {
                                    lines << "Error: Could not fetch data for " + sym;
                                } else {
                                    // Format helper: show "—" for zero/missing values
                                    auto fmt_dbl = [](double v, int dec = 2) -> QString {
                                        return v != 0 ? QString::number(v, 'f', dec) : "—";
                                    };
                                    auto fmt_pct = [](double v) -> QString {
                                        return v != 0 ? QString::number(v * 100, 'f', 2) + "%" : "—";
                                    };
                                    auto fmt_mcap = [](double v) -> QString {
                                        if (v <= 0)
                                            return "—";
                                        if (v >= 1e12)
                                            return QString::number(v / 1e12, 'f', 2) + "T";
                                        if (v >= 1e9)
                                            return QString::number(v / 1e9, 'f', 2) + "B";
                                        if (v >= 1e6)
                                            return QString::number(v / 1e6, 'f', 2) + "M";
                                        return QString::number(v, 'f', 0);
                                    };
                                    auto fmt_vol = [](double v) -> QString {
                                        if (v <= 0)
                                            return "—";
                                        if (v >= 1e9)
                                            return QString::number(v / 1e9, 'f', 2) + "B";
                                        if (v >= 1e6)
                                            return QString::number(v / 1e6, 'f', 2) + "M";
                                        if (v >= 1e3)
                                            return QString::number(v / 1e3, 'f', 1) + "K";
                                        return QString::number(v, 'f', 0);
                                    };

                                    if (!info.name.isEmpty())
                                        lines << "Company: " + info.name;
                                    if (!info.sector.isEmpty())
                                        lines << "Sector: " + info.sector;
                                    if (!info.industry.isEmpty())
                                        lines << "Industry: " + info.industry;
                                    if (!info.country.isEmpty())
                                        lines << "Country: " + info.country;
                                    lines << "Market Cap: " + fmt_mcap(info.market_cap);
                                    lines << "P/E Ratio: " + fmt_dbl(info.pe_ratio);
                                    lines << "Forward P/E: " + fmt_dbl(info.forward_pe);
                                    lines << "Price/Book: " + fmt_dbl(info.price_to_book);
                                    lines << "Dividend Yield: " + fmt_pct(info.dividend_yield);
                                    lines << "Beta: " + fmt_dbl(info.beta);
                                    lines << "52W High: " + fmt_dbl(info.week52_high);
                                    lines << "52W Low: " + fmt_dbl(info.week52_low);
                                    lines << "Avg Volume: " + fmt_vol(info.avg_volume);
                                    lines << "ROE: " + fmt_pct(info.roe);
                                    lines << "Profit Margin: " + fmt_pct(info.profit_margin);
                                    if (info.debt_to_equity != 0)
                                        lines << "Debt/Equity: " + fmt_dbl(info.debt_to_equity);
                                    if (info.current_ratio != 0)
                                        lines << "Current Ratio: " + fmt_dbl(info.current_ratio);
                                    if (info.eps != 0)
                                        lines << "Rev/Share: " + fmt_dbl(info.eps);
                                }
                                self->components_[idx].config["data"] = lines.join("\n");
                                self->components_[idx].config["fetch_ticker"] = "";
                                self->refresh_canvas();
                                if (self->selected_ == idx)
                                    self->properties_->show_properties(&self->components_[idx], idx);
                            },
                            Qt::QueuedConnection);
                    });
                return;
            }

            // ── chart: hub-driven 6mo/1d history fetch ─────────────────────
            if (components_[idx].type == "chart" && key == "fetch_history") {
                QString sym = val;
                components_[idx].config["fetch_history"] = "";
                components_[idx].config["data"] = ""; // clear stale data
                refresh_canvas();

                // Use 6-month daily history by default. Topic format:
                //   market:history:<sym>:<period>:<interval>
                const QString topic =
                    QStringLiteral("market:history:") + sym + QStringLiteral(":6mo:1d");
                auto& hub = fincept::datahub::DataHub::instance();

                // Rendering helper — writes the chart data into components_[idx]
                // from a HistoryPoint vector.
                QPointer<ReportBuilderScreen> self = this;
                auto apply = [self, idx, sym](const QVector<fincept::services::HistoryPoint>& history) {
                    if (!self || idx >= self->components_.size())
                        return;
                    if (history.isEmpty()) {
                        self->components_[idx].config["data"] = "";
                        self->refresh_canvas();
                        return;
                    }
                    QStringList pts, lbls;
                    int step = qMax(1, history.size() / 20); // max ~20 points
                    for (int i = 0; i < history.size(); i += step) {
                        const auto& pt = history[i];
                        pts << QString::number(pt.close, 'f', 2);
                        QDateTime dt = QDateTime::fromSecsSinceEpoch(pt.timestamp);
                        lbls << dt.toString("MMM d");
                    }
                    if ((history.size() - 1) % step != 0) {
                        pts << QString::number(history.last().close, 'f', 2);
                        lbls << QDateTime::fromSecsSinceEpoch(history.last().timestamp)
                                    .toString("MMM d");
                    }
                    self->components_[idx].config["data"] = pts.join(",");
                    self->components_[idx].config["labels"] = lbls.join(",");
                    if (self->components_[idx].config.value("title").isEmpty() ||
                        self->components_[idx].config.value("title") == "Chart")
                        self->components_[idx].config["title"] = sym + " (6mo)";
                    self->refresh_canvas();
                    if (self->selected_ == idx)
                        self->properties_->show_properties(&self->components_[idx], idx);
                };

                // Cached snapshot first (instant render if data is fresh on the hub).
                QVariant cached = hub.peek(topic);
                if (cached.isValid() && cached.canConvert<QVector<fincept::services::HistoryPoint>>()) {
                    apply(cached.value<QVector<fincept::services::HistoryPoint>>());
                    return;
                }

                // Otherwise: subscribe-once, force-kick the producer. The first
                // delivery renders then tears down the subscription so the
                // report-builder doesn't keep receiving updates it doesn't want.
                hub.subscribe<QVector<fincept::services::HistoryPoint>>(
                    this, topic,
                    [self, topic, apply](const QVector<fincept::services::HistoryPoint>& history) {
                        if (!self)
                            return;
                        apply(history);
                        fincept::datahub::DataHub::instance().unsubscribe(self, topic);
                    });
                hub.request(topic, /*force=*/true);
                return;
            }

            // For market_data "status=loading", trigger a hub-driven quote fetch.
            if (components_[idx].type == "market_data" && key == "status" && val == "loading") {
                QString sym = components_[idx].config.value("symbol", "");
                if (sym.isEmpty()) {
                    return; // don't push undo for status change
                }
                components_[idx].config["status"] = "loading";
                refresh_canvas();

                const QString topic = QStringLiteral("market:quote:") + sym;
                auto& hub = fincept::datahub::DataHub::instance();
                QPointer<ReportBuilderScreen> self = this;

                auto apply = [self, idx](const fincept::services::QuoteData& q) {
                    if (!self || idx >= self->components_.size())
                        return;
                    self->components_[idx].config["price"] = QString::number(q.price, 'f', 2);
                    self->components_[idx].config["change"] = QString::number(q.change, 'f', 2);
                    self->components_[idx].config["change_pct"] =
                        QString::number(q.change_pct, 'f', 2);
                    self->components_[idx].config["name"] = q.name;
                    self->components_[idx].config["high"] =
                        q.high > 0 ? QString::number(q.high, 'f', 2) : "";
                    self->components_[idx].config["low"] =
                        q.low > 0 ? QString::number(q.low, 'f', 2) : "";
                    self->components_[idx].config["volume"] =
                        q.volume > 0 ? QString::number(q.volume, 'f', 0) : "";
                    self->components_[idx].config["status"] = "ok";
                    self->refresh_canvas();
                    if (self->selected_ == idx) {
                        self->properties_->show_properties(&self->components_[idx], idx);
                    }
                };

                // Fast path: render from cached quote if fresh on the hub.
                QVariant cached = hub.peek(topic);
                if (cached.isValid() && cached.canConvert<fincept::services::QuoteData>()) {
                    apply(cached.value<fincept::services::QuoteData>());
                    return; // don't push undo for status change
                }

                // Otherwise: subscribe-once then tear down — this is a
                // one-shot user refresh, not a live ticker.
                hub.subscribe<fincept::services::QuoteData>(
                    this, topic,
                    [self, topic, apply](const fincept::services::QuoteData& q) {
                        if (!self)
                            return;
                        apply(q);
                        fincept::datahub::DataHub::instance().unsubscribe(self, topic);
                    });
                hub.request(topic, /*force=*/true);
                return; // don't push undo for status change
            }

            auto old_content = components_[idx].content;
            auto old_config = components_[idx].config;
            auto new_config = old_config;
            new_config[key] = val;
            undo_stack_->push(new UpdateComponentCommand(this, idx, old_content, old_config, old_content, new_config));
        });

    connect(properties_, &PropertiesPanel::duplicate_requested, this, [this](int idx) {
        if (idx >= 0 && idx < components_.size()) {
            ReportComponent copy = components_[idx];
            copy.id = next_id_++;
            undo_stack_->push(new AddComponentCommand(this, copy, idx + 1));
        }
    });
    connect(properties_, &PropertiesPanel::delete_requested, this, &ReportBuilderScreen::remove_component);

    // ── Canvas drag-and-drop ──────────────────────────────────────────────────
    connect(canvas_, &DocumentCanvas::image_dropped, this, [this](const QString& path) {
        // If selected component is an image, update its path
        if (selected_ >= 0 && selected_ < components_.size() && components_[selected_].type == "image") {
            auto old_content = components_[selected_].content;
            auto old_config = components_[selected_].config;
            auto new_config = old_config;
            new_config["path"] = path;
            new_config["clipboard"] = "";
            undo_stack_->push(
                new UpdateComponentCommand(this, selected_, old_content, old_config, old_content, new_config));
            select_component(selected_);
        } else {
            // Insert a new image component
            ReportComponent comp;
            comp.id = next_id_++;
            comp.type = "image";
            comp.config["path"] = path;
            comp.config["width"] = "400";
            comp.config["align"] = "left";
            int at = (selected_ >= 0 && selected_ < components_.size()) ? selected_ + 1 : components_.size();
            undo_stack_->push(new AddComponentCommand(this, comp, at));
            selected_ = at;
            select_component(selected_);
        }
    });

    refresh_canvas();
}

// ── Top Toolbar ───────────────────────────────────────────────────────────────

QWidget* ReportBuilderScreen::build_toolbar() {
    auto* bar = new QWidget(this);
    bar->setFixedHeight(36);
    bar->setStyleSheet(QString("background: %1;").arg(ui::colors::HEADER()));

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(8, 0, 8, 0);
    hl->setSpacing(6);

    auto* title = new QLabel("REPORT BUILDER");
    title->setStyleSheet(
        QString("color: %1; font-size: 14px; font-weight: bold; background: transparent;").arg(ui::colors::AMBER()));
    hl->addWidget(title);

    hl->addStretch();

    auto make_btn = [&](const char* text) {
        auto* b = new QPushButton(text);
        b->setFixedHeight(26);
        hl->addWidget(b);
        return b;
    };

    auto* undo_btn = make_btn("Undo");
    undo_btn->setEnabled(false);
    connect(undo_stack_, &QUndoStack::canUndoChanged, undo_btn, &QPushButton::setEnabled);
    connect(undo_btn, &QPushButton::clicked, undo_stack_, &QUndoStack::undo);

    auto* redo_btn = make_btn("Redo");
    redo_btn->setEnabled(false);
    connect(undo_stack_, &QUndoStack::canRedoChanged, redo_btn, &QPushButton::setEnabled);
    connect(redo_btn, &QPushButton::clicked, undo_stack_, &QUndoStack::redo);

    make_btn("|")->setEnabled(false);

    auto* open_btn = make_btn("Open");
    connect(open_btn, &QPushButton::clicked, this, &ReportBuilderScreen::on_open);

    auto* save_btn = make_btn("Save");
    connect(save_btn, &QPushButton::clicked, this, &ReportBuilderScreen::on_save);

    auto* pdf_btn = make_btn("Export PDF");
    pdf_btn->setStyleSheet(QString("QPushButton { color: %1; font-weight: bold; }").arg(ui::colors::AMBER()));
    connect(pdf_btn, &QPushButton::clicked, this, &ReportBuilderScreen::on_export_pdf);

    auto* preview_btn = make_btn("Preview");
    connect(preview_btn, &QPushButton::clicked, this, &ReportBuilderScreen::on_preview);

    return bar;
}

// ── Component operations (push to undo stack) ─────────────────────────────────

void ReportBuilderScreen::add_component(const QString& type) {
    ReportComponent comp;
    comp.id = next_id_++;
    comp.type = type;

    if (type == "table") {
        comp.config["rows"] = "3";
        comp.config["cols"] = "3";
    }

    int at = (selected_ >= 0 && selected_ < components_.size()) ? selected_ + 1 : components_.size();
    undo_stack_->push(new AddComponentCommand(this, comp, at));
    selected_ = at;
    select_component(selected_);
}

void ReportBuilderScreen::select_component(int index) {
    selected_ = index;
    if (index >= 0 && index < components_.size()) {
        properties_->show_properties(&components_[index], index);
    } else {
        properties_->show_empty();
    }
    refresh_canvas();
}

void ReportBuilderScreen::remove_component(int index) {
    if (index >= 0 && index < components_.size()) {
        undo_stack_->push(new RemoveComponentCommand(this, index));
    }
}

void ReportBuilderScreen::update_component(int index, const QString& content, const QMap<QString, QString>& config) {
    if (index >= 0 && index < components_.size()) {
        auto old_content = components_[index].content;
        auto old_config = components_[index].config;
        undo_stack_->push(new UpdateComponentCommand(this, index, old_content, old_config, content, config));
    }
}

// ── Refresh ───────────────────────────────────────────────────────────────────

void ReportBuilderScreen::refresh_canvas() {
    canvas_->render(components_, metadata_, theme_, selected_);
}

void ReportBuilderScreen::refresh_structure() {
    QStringList items;
    for (const auto& c : components_) {
        QString label = c.type.toUpper();
        if (!c.content.isEmpty())
            label += ": " + c.content.left(28);
        else if (c.type == "market_data")
            label += ": " + c.config.value("symbol", "?");
        else if (c.type == "chart")
            label += ": " + c.config.value("title", "Chart");
        else if (c.type == "stats_block")
            label += ": " + c.config.value("title", "Stats");
        else if (c.type == "sparkline")
            label += ": " + c.config.value("title", "");
        else if (c.type == "callout")
            label += " [" + c.config.value("style", "info") + "]";
        items << label;
    }
    comp_toolbar_->update_structure(items, selected_);
}

// ── Serialization ─────────────────────────────────────────────────────────────

QString ReportBuilderScreen::serialize_to_json() const {
    QJsonObject root;

    // Metadata
    QJsonObject meta;
    meta["title"] = metadata_.title;
    meta["author"] = metadata_.author;
    meta["company"] = metadata_.company;
    meta["date"] = metadata_.date;
    meta["header_left"] = metadata_.header_left;
    meta["header_center"] = metadata_.header_center;
    meta["header_right"] = metadata_.header_right;
    meta["footer_left"] = metadata_.footer_left;
    meta["footer_center"] = metadata_.footer_center;
    meta["footer_right"] = metadata_.footer_right;
    meta["show_page_numbers"] = metadata_.show_page_numbers;
    root["metadata"] = meta;

    // Theme
    root["theme"] = theme_.name;

    // Components
    QJsonArray comps;
    for (const auto& c : components_) {
        QJsonObject obj;
        obj["id"] = c.id;
        obj["type"] = c.type;
        obj["content"] = c.content;
        QJsonObject cfg;
        for (auto it = c.config.cbegin(); it != c.config.cend(); ++it) {
            cfg[it.key()] = it.value();
        }
        obj["config"] = cfg;
        comps.append(obj);
    }
    root["components"] = comps;
    root["next_id"] = next_id_;

    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

bool ReportBuilderScreen::deserialize_from_json(const QString& json) {
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isObject())
        return false;
    QJsonObject root = doc.object();

    // Metadata
    if (root.contains("metadata")) {
        QJsonObject m = root["metadata"].toObject();
        metadata_.title = m.value("title").toString("Untitled Report");
        metadata_.author = m.value("author").toString("Analyst");
        metadata_.company = m.value("company").toString("Fincept Corporation");
        metadata_.date = m.value("date").toString();
        metadata_.header_left = m.value("header_left").toString();
        metadata_.header_center = m.value("header_center").toString();
        metadata_.header_right = m.value("header_right").toString();
        metadata_.footer_left = m.value("footer_left").toString();
        metadata_.footer_center = m.value("footer_center").toString("Page {page}");
        metadata_.footer_right = m.value("footer_right").toString();
        metadata_.show_page_numbers = m.value("show_page_numbers").toBool(true);
    }

    // Theme
    if (root.contains("theme")) {
        QString tname = root["theme"].toString();
        if (tname == "Dark Corporate")
            theme_ = report_themes::dark_corporate();
        else if (tname == "Bloomberg Terminal")
            theme_ = report_themes::bloomberg();
        else if (tname == "Midnight Blue")
            theme_ = report_themes::midnight_blue();
        else
            theme_ = report_themes::light_professional();
    }

    // Components
    components_.clear();
    next_id_ = 1;
    if (root.contains("components")) {
        for (const auto& val : root["components"].toArray()) {
            QJsonObject obj = val.toObject();
            ReportComponent c;
            c.id = obj["id"].toInt(next_id_);
            c.type = obj["type"].toString();
            c.content = obj["content"].toString();
            for (const auto& k : obj["config"].toObject().keys()) {
                c.config[k] = obj["config"].toObject()[k].toString();
            }
            components_.append(c);
            if (c.id >= next_id_)
                next_id_ = c.id + 1;
        }
    }
    if (root.contains("next_id"))
        next_id_ = root["next_id"].toInt(next_id_);

    selected_ = components_.isEmpty() ? -1 : 0;
    undo_stack_->clear();
    return true;
}

// ── Recent files ──────────────────────────────────────────────────────────────

void ReportBuilderScreen::update_recent(const QString& path) {
    QSettings s("Fincept", "FinceptTerminal");
    QStringList recent = s.value("report_builder/recent").toStringList();
    recent.removeAll(path);
    recent.prepend(path);
    if (recent.size() > 10)
        recent = recent.mid(0, 10);
    s.setValue("report_builder/recent", recent);
}

QStringList ReportBuilderScreen::load_recent() const {
    QSettings s("Fincept", "FinceptTerminal");
    return s.value("report_builder/recent").toStringList();
}

// ── Dialogs ───────────────────────────────────────────────────────────────────

void ReportBuilderScreen::show_recent_dialog() {
    QStringList recent = load_recent();

    auto* dlg = new QDialog(this);
    dlg->setWindowTitle("Recent Reports");
    dlg->setMinimumWidth(500);
    dlg->setStyleSheet(
        QString("QDialog { background: %1; color: %2; }"
                "QListWidget { background: %3; color: %2; border: 1px solid %4; }"
                "QListWidget::item { padding: 6px; }"
                "QListWidget::item:selected { background: %5; }"
                "QPushButton { background: %3; color: %2; border: 1px solid %4; padding: 6px 16px; }"
                "QPushButton:hover { background: %5; }")
            .arg(ui::colors::DARK(), ui::colors::WHITE(), ui::colors::PANEL(), ui::colors::BORDER(), ui::colors::BG_RAISED()));

    auto* vl = new QVBoxLayout(dlg);
    auto* lbl = new QLabel("Select a report to open:");
    lbl->setStyleSheet(QString("color: %1;").arg(ui::colors::MUTED()));
    vl->addWidget(lbl);

    auto* list = new QListWidget;
    if (recent.isEmpty()) {
        list->addItem("(No recent reports)");
    } else {
        list->addItems(recent);
    }
    vl->addWidget(list, 1);

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Open | QDialogButtonBox::Cancel);
    vl->addWidget(bb);

    connect(bb, &QDialogButtonBox::accepted, dlg, [this, list, recent, dlg]() {
        int row = list->currentRow();
        if (row >= 0 && row < recent.size()) {
            load_report(recent[row]);
        }
        dlg->accept();
    });
    connect(bb, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    connect(list, &QListWidget::itemDoubleClicked, dlg, [this, list, recent, dlg](QListWidgetItem*) {
        int row = list->currentRow();
        if (row >= 0 && row < recent.size()) {
            load_report(recent[row]);
        }
        dlg->accept();
    });

    dlg->exec();
    dlg->deleteLater();
}

void ReportBuilderScreen::show_template_dialog() {
    // category → list of {name, description}
    struct TemplateEntry {
        QString name;
        QString description;
    };
    struct Category {
        QString label;
        QVector<TemplateEntry> entries;
    };

    const QVector<Category> categories = {
        {"General Purpose",
         {
             {"Blank Report", "Empty document with title and date — start from scratch."},
             {"Meeting Notes", "Agenda, attendees, discussion points, action items, next steps."},
             {"Investment Memo", "Executive summary, thesis, opportunity, risks, recommendation."},
         }},
        {"Retail Investor",
         {
             {"Stock Research", "Company overview, financials, valuation, thesis, and risks."},
             {"Portfolio Review", "Holdings, performance, allocation pie, risk metrics, notes."},
             {"Watchlist Report", "Tracked tickers with price, change, target, and notes columns."},
             {"Dividend Income Report", "Dividend stocks table, yield chart, income projection, calendar."},
         }},
        {"Trader",
         {
             {"Daily Market Brief", "Indices snapshot, top movers, news highlights, watchlist."},
             {"Trade Journal", "Trade log table, P&L chart, win rate stats, lessons learned."},
             {"Technical Analysis", "Price chart placeholder, indicators table, S/R levels, setup."},
             {"Pre-Market Checklist", "Macro events, key levels, planned trades, risk per trade."},
         }},
        {"Institutional / Analyst",
         {
             {"Equity Research Report", "Full buy-side template: overview, financials, DCF, comparables."},
             {"Earnings Review", "Revenue/EPS vs estimates, guidance, segment breakdown, outlook."},
             {"M&A Deal Summary", "Deal overview, rationale, comparables, synergies, valuation."},
             {"Sector Deep Dive", "Sector overview, sub-industries, key players, trends, risks."},
         }},
        {"Economist / Macro",
         {
             {"Macro Economic Summary", "GDP, inflation, unemployment, central bank policy, outlook."},
             {"Country Risk Report", "Political, economic, financial, market risk tables and charts."},
             {"Central Bank Monitor", "Rate decisions, forward guidance, balance sheet, implications."},
         }},
        {"Crypto / Digital Assets",
         {
             {"Crypto Research Report", "Project overview, tokenomics, on-chain metrics, price chart."},
             {"DeFi Protocol Analysis", "Protocol overview, TVL, revenue, risks, token valuation."},
             {"Crypto Portfolio Review", "Holdings, allocation, cost basis table, P&L, risk exposure."},
         }},
        {"Fixed Income",
         {
             {"Bond Research Report", "Issuer overview, credit profile, yield analysis, recommendation."},
             {"Yield Curve Analysis", "Curve chart, spread table, duration/convexity, macro drivers."},
         }},
        {"Quant / Risk",
         {
             {"Quant Strategy Report", "Strategy description, backtest results, stats, drawdown chart."},
             {"Risk Management Report", "VaR, CVaR, stress tests, correlation matrix, recommendations."},
         }},
        {"Corporate / Business",
         {
             {"Business Performance", "KPI dashboard, revenue chart, cost breakdown, targets vs actual."},
             {"Project Status Report", "Objectives, milestones table, status, risks, next actions."},
             {"Financial Statement", "Income statement, balance sheet, cash flow tables."},
         }},
    };

    auto* dlg = new QDialog(this);
    dlg->setWindowTitle("Report Templates");
    dlg->setMinimumSize(680, 520);
    dlg->setStyleSheet(QString("QDialog    { background: %1; color: %2; }"
                               "QListWidget { background: %3; color: %2; border: 1px solid %4; }"
                               "QListWidget::item { padding: 6px 8px; font-size: 12px; }"
                               "QListWidget::item:selected { background: %5; color: %6; }"
                               "QListWidget::item[category=true] { color: %6; font-weight: bold; "
                               "  background: %7; padding: 4px 8px; font-size: 11px; }"
                               "QPushButton { background: %3; color: %2; border: 1px solid %4; padding: 6px 16px; }"
                               "QPushButton:hover { background: %5; }"
                               "QLabel { background: transparent; }"
                               "QSplitter::handle { background: %4; }")
                           .arg(ui::colors::DARK(), ui::colors::WHITE(), ui::colors::PANEL(), ui::colors::BORDER(),
                                ui::colors::AMBER_DIM(), ui::colors::AMBER(), ui::colors::BG_RAISED()));

    auto* hl = new QHBoxLayout(dlg);
    hl->setSpacing(0);
    hl->setContentsMargins(0, 0, 0, 0);

    // Left: category + template list
    auto* left = new QWidget(this);
    left->setMinimumWidth(280);
    auto* lvl = new QVBoxLayout(left);
    lvl->setContentsMargins(12, 12, 8, 12);
    lvl->setSpacing(6);

    auto* pick_lbl = new QLabel("Choose a template:");
    pick_lbl->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: bold;").arg(ui::colors::MUTED()));
    lvl->addWidget(pick_lbl);

    auto* list = new QListWidget;
    list->setSpacing(1);

    // Populate: category header items + template items
    for (const auto& cat : categories) {
        auto* cat_item = new QListWidgetItem("  " + cat.label.toUpper());
        cat_item->setFlags(Qt::NoItemFlags); // not selectable
        cat_item->setForeground(QColor(ui::colors::AMBER()));
        QFont f = cat_item->font();
        f.setBold(true);
        f.setPointSize(10);
        cat_item->setFont(f);
        cat_item->setBackground(QColor(ui::colors::BG_RAISED()));
        list->addItem(cat_item);

        for (const auto& tmpl : cat.entries) {
            auto* item = new QListWidgetItem("    " + tmpl.name);
            item->setData(Qt::UserRole, tmpl.name);
            item->setData(Qt::UserRole + 1, tmpl.description);
            list->addItem(item);
        }
    }
    lvl->addWidget(list, 1);
    hl->addWidget(left);

    // Separator
    auto* sep = new QFrame;
    sep->setFixedWidth(1);
    sep->setStyleSheet(QString("background: %1;").arg(ui::colors::BORDER()));
    hl->addWidget(sep);

    // Right: description + preview
    auto* right = new QWidget(this);
    right->setMinimumWidth(280);
    auto* rvl = new QVBoxLayout(right);
    rvl->setContentsMargins(12, 12, 12, 12);
    rvl->setSpacing(8);

    auto* tmpl_name_lbl = new QLabel("Select a template");
    tmpl_name_lbl->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: bold;").arg(ui::colors::AMBER()));
    tmpl_name_lbl->setWordWrap(true);
    rvl->addWidget(tmpl_name_lbl);

    auto* desc_lbl = new QLabel("Pick a template from the list to see a description.");
    desc_lbl->setWordWrap(true);
    desc_lbl->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    desc_lbl->setStyleSheet(QString("color: %1; font-size: 12px; line-height: 160%;").arg(ui::colors::GRAY()));
    rvl->addWidget(desc_lbl, 1);

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    rvl->addWidget(bb);
    hl->addWidget(right);

    // Update description panel on selection
    connect(list, &QListWidget::currentItemChanged, this,
            [tmpl_name_lbl, desc_lbl](QListWidgetItem* cur, QListWidgetItem*) {
                if (!cur || !(cur->flags() & Qt::ItemIsSelectable))
                    return;
                tmpl_name_lbl->setText(cur->data(Qt::UserRole).toString());
                desc_lbl->setText(cur->data(Qt::UserRole + 1).toString());
            });

    // Double-click accepts
    connect(list, &QListWidget::itemDoubleClicked, dlg, [this, dlg](QListWidgetItem* item) {
        if (!item || !(item->flags() & Qt::ItemIsSelectable))
            return;
        QString tname = item->data(Qt::UserRole).toString();
        if (!tname.isEmpty()) {
            apply_template(tname);
            dlg->accept();
        }
    });

    connect(bb, &QDialogButtonBox::accepted, dlg, [this, list, dlg]() {
        auto* cur = list->currentItem();
        if (cur && (cur->flags() & Qt::ItemIsSelectable)) {
            QString tname = cur->data(Qt::UserRole).toString();
            if (!tname.isEmpty())
                apply_template(tname);
        }
        dlg->accept();
    });
    connect(bb, &QDialogButtonBox::rejected, dlg, &QDialog::reject);

    dlg->exec();
    dlg->deleteLater();
}

void ReportBuilderScreen::show_theme_dialog() {
    const QList<ReportTheme> themes = {
        report_themes::light_professional(),
        report_themes::dark_corporate(),
        report_themes::bloomberg(),
        report_themes::midnight_blue(),
    };

    auto* dlg = new QDialog(this);
    dlg->setWindowTitle("Report Theme");
    dlg->setMinimumWidth(380);
    dlg->setStyleSheet(
        QString("QDialog { background: %1; color: %2; }"
                "QComboBox, QPushButton { background: %3; color: %2; border: 1px solid %4; padding: 4px 8px; }"
                "QLabel { background: transparent; color: %2; }")
            .arg(ui::colors::DARK(), ui::colors::WHITE(), ui::colors::PANEL(), ui::colors::BORDER()));

    auto* vl = new QVBoxLayout(dlg);
    vl->addWidget(new QLabel("Select a color theme for your report:"));

    auto* combo = new QComboBox;
    for (const auto& t : themes)
        combo->addItem(t.name);
    // Select current theme
    for (int i = 0; i < themes.size(); ++i) {
        if (themes[i].name == theme_.name) {
            combo->setCurrentIndex(i);
            break;
        }
    }
    vl->addWidget(combo);

    auto* preview_lbl = new QLabel;
    preview_lbl->setFixedHeight(40);
    preview_lbl->setAlignment(Qt::AlignCenter);
    auto update_preview = [themes, preview_lbl](int idx) {
        const auto& t = themes[idx];
        preview_lbl->setStyleSheet(QString("background: %1; color: %2; border: 2px solid %3; font-weight: bold;")
                                       .arg(t.page_bg, t.heading_color, t.accent_color));
        preview_lbl->setText(t.name + " — Sample Text");
    };
    connect(combo, &QComboBox::currentIndexChanged, this, update_preview);
    update_preview(combo->currentIndex());
    vl->addWidget(preview_lbl);

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    vl->addWidget(bb);

    connect(bb, &QDialogButtonBox::accepted, dlg, [this, combo, themes, dlg]() {
        theme_ = themes[combo->currentIndex()];
        refresh_canvas();
        dlg->accept();
    });
    connect(bb, &QDialogButtonBox::rejected, dlg, &QDialog::reject);

    dlg->exec();
    dlg->deleteLater();
}

void ReportBuilderScreen::show_metadata_dialog() {
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle("Report Metadata");
    dlg->setMinimumWidth(420);
    dlg->setStyleSheet(
        QString("QDialog { background: %1; color: %2; }"
                "QLineEdit { background: %3; color: %2; border: 1px solid %4; padding: 4px; }"
                "QLabel { background: transparent; color: %2; }"
                "QPushButton { background: %3; color: %2; border: 1px solid %4; padding: 6px 16px; }"
                "QPushButton:hover { background: %5; }")
            .arg(ui::colors::DARK(), ui::colors::WHITE(), ui::colors::PANEL(), ui::colors::BORDER(), ui::colors::BG_RAISED()));

    auto* vl = new QVBoxLayout(dlg);
    auto* form = new QFormLayout;
    form->setSpacing(8);

    auto* title_edit = new QLineEdit(metadata_.title);
    auto* author_edit = new QLineEdit(metadata_.author);
    auto* company_edit = new QLineEdit(metadata_.company);
    auto* date_edit = new QLineEdit(metadata_.date);
    date_edit->setPlaceholderText("yyyy-MM-dd");

    auto lbl_style = QString("color: %1;").arg(ui::colors::GRAY());
    auto* tl = new QLabel("Title:");
    tl->setStyleSheet(lbl_style);
    auto* al = new QLabel("Author:");
    al->setStyleSheet(lbl_style);
    auto* cl = new QLabel("Company:");
    cl->setStyleSheet(lbl_style);
    auto* dl = new QLabel("Date:");
    dl->setStyleSheet(lbl_style);

    form->addRow(tl, title_edit);
    form->addRow(al, author_edit);
    form->addRow(cl, company_edit);
    form->addRow(dl, date_edit);
    vl->addLayout(form);

    // Header/Footer section
    auto* hf_lbl = new QLabel("HEADER / FOOTER");
    hf_lbl->setStyleSheet(
        QString("color: %1; font-size: 11px; font-weight: bold; padding-top: 10px;").arg(ui::colors::MUTED()));
    vl->addWidget(hf_lbl);

    auto* hf_form = new QFormLayout;
    hf_form->setSpacing(8);

    auto* hl_edit = new QLineEdit(metadata_.header_left);
    auto* hc_edit = new QLineEdit(metadata_.header_center);
    auto* hr_edit = new QLineEdit(metadata_.header_right);
    auto* fl_edit = new QLineEdit(metadata_.footer_left);
    auto* fc_edit = new QLineEdit(metadata_.footer_center);
    auto* fr_edit = new QLineEdit(metadata_.footer_right);

    hl_edit->setPlaceholderText("Left");
    hc_edit->setPlaceholderText("Center");
    hr_edit->setPlaceholderText("Right");
    fl_edit->setPlaceholderText("Left");
    fc_edit->setPlaceholderText("Center (use {page})");
    fr_edit->setPlaceholderText("Right");

    auto f_row = [&](const char* left_text, QLineEdit* l, QLineEdit* c, QLineEdit* r) {
        auto* row = new QWidget(this);
        row->setStyleSheet("background: transparent;");
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        rl->addWidget(l);
        rl->addWidget(c);
        rl->addWidget(r);
        auto* lbl2 = new QLabel(left_text);
        lbl2->setStyleSheet(lbl_style);
        hf_form->addRow(lbl2, row);
    };
    f_row("Header:", hl_edit, hc_edit, hr_edit);
    f_row("Footer:", fl_edit, fc_edit, fr_edit);
    vl->addLayout(hf_form);

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    vl->addWidget(bb);

    connect(bb, &QDialogButtonBox::accepted, dlg, [=, this]() {
        metadata_.title = title_edit->text();
        metadata_.author = author_edit->text();
        metadata_.company = company_edit->text();
        metadata_.date = date_edit->text();
        metadata_.header_left = hl_edit->text();
        metadata_.header_center = hc_edit->text();
        metadata_.header_right = hr_edit->text();
        metadata_.footer_left = fl_edit->text();
        metadata_.footer_center = fc_edit->text();
        metadata_.footer_right = fr_edit->text();
        refresh_canvas();
        dlg->accept();
    });
    connect(bb, &QDialogButtonBox::rejected, dlg, &QDialog::reject);

    dlg->exec();
    dlg->deleteLater();
}

// ── Templates ─────────────────────────────────────────────────────────────────

void ReportBuilderScreen::apply_template(const QString& name) {
    components_.clear();
    undo_stack_->clear();
    selected_ = -1;
    next_id_ = 1;
    metadata_ = ReportMetadata{};
    metadata_.date = QDateTime::currentDateTime().toString("yyyy-MM-dd");

    auto add = [&](const QString& type, const QString& content = {}, const QMap<QString, QString>& cfg = {}) {
        ReportComponent c;
        c.id = next_id_++;
        c.type = type;
        c.content = content;
        c.config = cfg;
        components_.append(c);
    };

    // ── General Purpose ───────────────────────────────────────────────────────

    if (name == "Blank Report") {
        metadata_.title = "Untitled Report";
        add("heading", "Title");
        add("text", "Start writing here...");

    } else if (name == "Meeting Notes") {
        metadata_.title = "Meeting Notes";
        add("toc");
        add("heading", "Meeting Details");
        add("table", {}, {{"rows", "4"}, {"cols", "2"}}); // Date, Time, Location, Chair
        add("heading", "Attendees");
        add("list", "Name — Role\nName — Role\nName — Role");
        add("heading", "Agenda");
        add("list", "Item 1\nItem 2\nItem 3");
        add("divider");
        add("heading", "Discussion Points");
        add("text", "Summarise key discussion points here.");
        add("heading", "Decisions Made");
        add("list", "Decision 1\nDecision 2");
        add("heading", "Action Items");
        add("table", {}, {{"rows", "5"}, {"cols", "3"}}); // Action | Owner | Due Date
        add("heading", "Next Meeting");
        add("text", "Date / time / agenda for next meeting.");

    } else if (name == "Investment Memo") {
        metadata_.title = "Investment Memo";
        add("toc");
        add("heading", "Executive Summary");
        add("text", "One-paragraph summary of the investment opportunity and recommendation.");
        add("heading", "Opportunity");
        add("text", "Describe the investment opportunity, market context, and why now.");
        add("heading", "Investment Thesis");
        add("list", "Thesis point 1\nThesis point 2\nThesis point 3");
        add("heading", "Financial Snapshot");
        add("table", {}, {{"rows", "5"}, {"cols", "3"}});
        add("heading", "Valuation");
        add("chart", {},
            {{"chart_type", "bar"},
             {"title", "Valuation Scenarios"},
             {"data", "80,100,130"},
             {"labels", "Bear,Base,Bull"}});
        add("heading", "Key Risks");
        add("list", "Risk 1\nRisk 2\nRisk 3");
        add("heading", "Recommendation");
        add("text", "State your recommendation and conviction level.");
        add("divider");
        add("quote", "This memo is for internal use only. Not investment advice.");

        // ── Retail Investor ───────────────────────────────────────────────────────

    } else if (name == "Stock Research") {
        metadata_.title = "Stock Research";
        add("toc");
        add("heading", "Company Overview");
        add("text", "Describe the company, its business model, products/services, and competitive position.");
        add("market_data", {}, {{"symbol", "AAPL"}});
        add("heading", "Financial Highlights");
        add("table", {}, {{"rows", "6"}, {"cols", "4"}}); // Metric | FY-2 | FY-1 | TTM
        add("heading", "Earnings History");
        add("chart", {},
            {{"chart_type", "bar"},
             {"title", "Revenue vs Net Income"},
             {"data", "120,130,145,160"},
             {"labels", "FY21,FY22,FY23,FY24"}});
        add("heading", "Valuation Metrics");
        add("table", {}, {{"rows", "5"}, {"cols", "3"}}); // Metric | Company | Sector Avg
        add("heading", "Investment Thesis");
        add("text", "Why you are considering this stock.");
        add("heading", "Risks");
        add("list", "Regulatory\nCompetition\nMacro headwinds\nExecution risk");
        add("heading", "Price Target");
        add("text", "Your price target and timeline.");
        add("divider");
        add("quote", "For personal research purposes only. Not financial advice.");

    } else if (name == "Portfolio Review") {
        metadata_.title = "Portfolio Review";
        add("toc");
        add("heading", "Holdings Summary");
        add("table", {}, {{"rows", "8"}, {"cols", "6"}}); // Ticker | Name | Qty | Cost | Price | P&L%
        add("heading", "Portfolio Performance");
        add("chart", {},
            {{"chart_type", "line"},
             {"title", "Portfolio Value"},
             {"data", "10000,10500,9800,11200,12000,11500,13000"}});
        add("heading", "Asset Allocation");
        add("chart", {},
            {{"chart_type", "pie"},
             {"title", "Allocation"},
             {"data", "40,30,20,10"},
             {"labels", "Equities,Fixed Income,Crypto,Cash"}});
        add("heading", "Top Performers");
        add("table", {}, {{"rows", "4"}, {"cols", "3"}});
        add("heading", "Underperformers");
        add("table", {}, {{"rows", "4"}, {"cols", "3"}});
        add("heading", "Risk Metrics");
        add("table", {}, {{"rows", "5"}, {"cols", "2"}}); // Beta, Sharpe, Max DD, Volatility, VaR
        add("heading", "Notes & Next Steps");
        add("text", "Commentary on portfolio health and planned changes.");

    } else if (name == "Watchlist Report") {
        metadata_.title = "Watchlist Report";
        metadata_.date = QDateTime::currentDateTime().toString("yyyy-MM-dd");
        add("heading", "Watchlist Snapshot");
        add("market_data", {}, {{"symbol", "AAPL"}});
        add("market_data", {}, {{"symbol", "MSFT"}});
        add("market_data", {}, {{"symbol", "GOOGL"}});
        add("market_data", {}, {{"symbol", "AMZN"}});
        add("market_data", {}, {{"symbol", "TSLA"}});
        add("divider");
        add("heading", "Watchlist Detail");
        add("table", {}, {{"rows", "8"}, {"cols", "6"}}); // Ticker | Price | Chg% | Target | Stop | Notes
        add("heading", "Observations");
        add("text", "Key observations and setup notes for tracked names.");

    } else if (name == "Dividend Income Report") {
        metadata_.title = "Dividend Income Report";
        add("toc");
        add("heading", "Dividend Holdings");
        add("table", {}, {{"rows", "8"}, {"cols", "6"}}); // Ticker | Shares | Div/Share | Freq | Yield | Annual $
        add("heading", "Annual Income Projection");
        add("chart", {},
            {{"chart_type", "bar"},
             {"title", "Monthly Dividend Income"},
             {"data", "200,180,220,210,195,230,215,240,200,220,210,250"},
             {"labels", "Jan,Feb,Mar,Apr,May,Jun,Jul,Aug,Sep,Oct,Nov,Dec"}});
        add("heading", "Yield by Holding");
        add("chart", {},
            {{"chart_type", "pie"},
             {"title", "Income Contribution"},
             {"data", "30,25,20,15,10"},
             {"labels", "VZ,T,PFE,KO,MO"}});
        add("heading", "Dividend Calendar");
        add("table", {}, {{"rows", "8"}, {"cols", "4"}}); // Ticker | Ex-Date | Pay Date | Amount
        add("heading", "Growth Targets");
        add("text", "Target yield, DRIP strategy, and reinvestment notes.");

        // ── Trader ────────────────────────────────────────────────────────────────

    } else if (name == "Daily Market Brief") {
        metadata_.title = "Daily Market Brief";
        metadata_.date = QDateTime::currentDateTime().toString("yyyy-MM-dd");
        add("heading", "Market Overview");
        add("market_data", {}, {{"symbol", "^GSPC"}});
        add("market_data", {}, {{"symbol", "^DJI"}});
        add("market_data", {}, {{"symbol", "^IXIC"}});
        add("market_data", {}, {{"symbol", "^VIX"}});
        add("market_data", {}, {{"symbol", "DX-Y.NYB"}});
        add("divider");
        add("heading", "Sector Performance");
        add("chart", {},
            {{"chart_type", "bar"},
             {"title", "Sector Returns Today"},
             {"data", "0.5,-0.3,1.2,-0.8,0.4,0.9,-0.2,1.1,0.3,-0.5,0.7"},
             {"labels", "XLK,XLF,XLV,XLE,XLI,XLY,XLP,XLC,XLB,XLU,XLRE"}});
        add("heading", "Top Movers");
        add("table", {}, {{"rows", "7"}, {"cols", "4"}}); // Ticker | Price | Chg% | Volume
        add("heading", "Key Events & News");
        add("text", "Enter today's key market events, earnings, and macro news.");
        add("heading", "Watchlist Levels");
        add("table", {}, {{"rows", "6"}, {"cols", "5"}}); // Ticker | Price | Support | Resistance | Bias
        add("heading", "Commentary");
        add("text", "Market tone, breadth observations, and outlook.");

    } else if (name == "Trade Journal") {
        metadata_.title = "Trade Journal";
        metadata_.date = QDateTime::currentDateTime().toString("yyyy-MM-dd");
        add("toc");
        add("heading", "Trade Log");
        add("table", {}, {{"rows", "10"}, {"cols", "8"}}); // Date | Symbol | Dir | Entry | Exit | Size | P&L | Notes
        add("heading", "P&L Summary");
        add("chart", {},
            {{"chart_type", "line"}, {"title", "Cumulative P&L"}, {"data", "0,150,-80,320,210,480,390,620"}});
        add("heading", "Statistics");
        add("table", {},
            {{"rows", "7"}, {"cols", "2"}}); // Win Rate, Avg Win, Avg Loss, R:R, Max DD, Profit Factor, Trades
        add("heading", "Win/Loss Breakdown");
        add("chart", {},
            {{"chart_type", "pie"}, {"title", "Win vs Loss"}, {"data", "62,38"}, {"labels", "Wins,Losses"}});
        add("heading", "Best Trades");
        add("table", {}, {{"rows", "4"}, {"cols", "5"}});
        add("heading", "Worst Trades");
        add("table", {}, {{"rows", "4"}, {"cols", "5"}});
        add("heading", "Lessons Learned");
        add("text", "What worked, what didn't, and adjustments for next session.");
        add("heading", "Rules Checklist");
        add("list", "Followed entry rules\nRespected stop loss\nDid not revenge trade\nPositioned sized "
                    "correctly\nLogged all trades");

    } else if (name == "Technical Analysis") {
        metadata_.title = "Technical Analysis";
        add("heading", "Instrument");
        add("market_data", {}, {{"symbol", "AAPL"}});
        add("heading", "Price Chart");
        add("image", {}, {{"width", "700"}, {"caption", "Daily chart — add screenshot here"}, {"align", "center"}});
        add("heading", "Key Levels");
        add("table", {}, {{"rows", "6"}, {"cols", "3"}}); // Level Type | Price | Notes
        add("heading", "Indicators");
        add("table", {}, {{"rows", "6"}, {"cols", "3"}}); // Indicator | Value | Signal
        add("heading", "Pattern / Setup");
        add("text", "Describe the chart pattern or setup and the rationale.");
        add("heading", "Trade Plan");
        add("table", {}, {{"rows", "4"}, {"cols", "3"}}); // Scenario | Entry | Target | Stop
        add("heading", "Bias");
        add("quote", "Bullish / Bearish / Neutral — state your directional bias and timeframe.");

    } else if (name == "Pre-Market Checklist") {
        metadata_.title = "Pre-Market Checklist";
        metadata_.date = QDateTime::currentDateTime().toString("yyyy-MM-dd");
        add("heading", "Overnight Summary");
        add("market_data", {}, {{"symbol", "ES=F"}});
        add("market_data", {}, {{"symbol", "NQ=F"}});
        add("market_data", {}, {{"symbol", "CL=F"}});
        add("market_data", {}, {{"symbol", "GC=F"}});
        add("divider");
        add("heading", "Economic Calendar");
        add("table", {}, {{"rows", "5"}, {"cols", "4"}}); // Time | Event | Forecast | Previous
        add("heading", "Earnings Today");
        add("table", {}, {{"rows", "5"}, {"cols", "3"}}); // Symbol | Time | EPS Est
        add("heading", "Key Levels to Watch");
        add("table", {}, {{"rows", "5"}, {"cols", "4"}}); // Symbol | Support | Resistance | Pivot
        add("heading", "Planned Trades");
        add("table", {}, {{"rows", "4"}, {"cols", "5"}}); // Symbol | Direction | Entry | Stop | Target
        add("heading", "Risk Parameters");
        add("table", {}, {{"rows", "4"}, {"cols", "2"}}); // Max daily loss | Per-trade risk | Max positions | etc.
        add("heading", "Mental Checklist");
        add("list",
            "Reviewed overnight news\nIdentified key levels\nSet max daily loss\nNo FOMO trades\nLog every trade");

        // ── Institutional / Analyst ───────────────────────────────────────────────

    } else if (name == "Equity Research Report") {
        metadata_.title = "Equity Research Report";
        add("toc");
        add("heading", "Company Overview");
        add("text", "Business description, history, products/services, and competitive moat.");
        add("market_data", {}, {{"symbol", "AAPL"}});
        add("heading", "Industry & Market");
        add("text", "Industry dynamics, total addressable market, and competitive landscape.");
        add("heading", "Financial Analysis");
        add("table", {}, {{"rows", "8"}, {"cols", "5"}}); // Income statement summary
        add("chart", {},
            {{"chart_type", "bar"},
             {"title", "Revenue & EPS Growth"},
             {"data", "100,115,130,148,169"},
             {"labels", "FY20,FY21,FY22,FY23,FY24"}});
        add("heading", "Valuation");
        add("table", {}, {{"rows", "5"}, {"cols", "4"}}); // DCF, P/E, EV/EBITDA, comparables
        add("chart", {},
            {{"chart_type", "bar"},
             {"title", "Valuation vs Peers"},
             {"data", "22,18,25,19,28"},
             {"labels", "AAPL,MSFT,GOOGL,META,AMZN"}});
        add("heading", "Investment Thesis");
        add("list", "Catalyst 1\nCatalyst 2\nCatalyst 3");
        add("heading", "Risks");
        add("list", "Regulatory risk\nCompetitive pressure\nMacro sensitivity\nExecution risk");
        add("heading", "Rating & Price Target");
        add("table", {}, {{"rows", "3"}, {"cols", "3"}}); // Rating | Target | Timeframe
        add("divider");
        add("quote", "This report is for informational purposes only. Not investment advice.");

    } else if (name == "Earnings Review") {
        metadata_.title = "Earnings Review";
        metadata_.date = QDateTime::currentDateTime().toString("yyyy-MM-dd");
        add("market_data", {}, {{"symbol", "AAPL"}});
        add("heading", "Results vs Estimates");
        add("table", {}, {{"rows", "5"}, {"cols", "4"}}); // Metric | Actual | Estimate | Surprise%
        add("heading", "Revenue Breakdown");
        add("chart", {},
            {{"chart_type", "bar"},
             {"title", "Revenue by Segment"},
             {"data", "42,18,10,8,6"},
             {"labels", "iPhone,Services,Mac,iPad,Wearables"}});
        add("heading", "Margin Analysis");
        add("table", {}, {{"rows", "4"}, {"cols", "3"}}); // Metric | This Q | Last Q
        add("heading", "Guidance");
        add("table", {}, {{"rows", "4"}, {"cols", "4"}}); // Metric | Q+1 Guide | Q+1 Est | Delta
        add("heading", "Management Commentary");
        add("text", "Key quotes and commentary from the earnings call.");
        add("heading", "Our Take");
        add("text", "Analysis of results relative to expectations and impact on thesis.");
        add("heading", "Revised Estimates");
        add("table", {}, {{"rows", "4"}, {"cols", "3"}});

    } else if (name == "M&A Deal Summary") {
        metadata_.title = "M&A Deal Summary";
        add("toc");
        add("heading", "Deal Overview");
        add("table", {}, {{"rows", "6"}, {"cols", "2"}}); // Acquirer, Target, Value, Structure, Close Date, Advisors
        add("heading", "Strategic Rationale");
        add("list", "Strategic fit\nMarket expansion\nSynergy opportunity\nDefensive rationale");
        add("heading", "Comparable Transactions");
        add("table", {}, {{"rows", "7"}, {"cols", "5"}}); // Date | Acquirer | Target | Value | EV/EBITDA
        add("heading", "Synergy Analysis");
        add("chart", {},
            {{"chart_type", "bar"},
             {"title", "Synergy Build-Up ($M)"},
             {"data", "50,120,200,250"},
             {"labels", "Yr1,Yr2,Yr3,Yr4"}});
        add("heading", "Valuation Bridge");
        add("table", {}, {{"rows", "5"}, {"cols", "3"}});
        add("heading", "Key Risks");
        add("list", "Integration risk\nRegulatory clearance\nCulture clash\nPremium paid\nFinancing risk");
        add("heading", "Timeline");
        add("table", {}, {{"rows", "5"}, {"cols", "3"}}); // Milestone | Expected Date | Status
        add("quote", "Deal subject to regulatory approval and shareholder vote.");

    } else if (name == "Sector Deep Dive") {
        metadata_.title = "Sector Deep Dive";
        add("toc");
        add("heading", "Sector Overview");
        add("text", "Define the sector, key sub-industries, and economic significance.");
        add("heading", "Market Size & Growth");
        add("chart", {},
            {{"chart_type", "bar"},
             {"title", "Market Size ($B)"},
             {"data", "800,900,1020,1160,1320"},
             {"labels", "2020,2021,2022,2023,2024E"}});
        add("heading", "Key Players");
        add("table", {}, {{"rows", "8"}, {"cols", "4"}}); // Company | Mkt Cap | Rev | Market Share%
        add("heading", "Competitive Dynamics");
        add("text", "Porter's five forces or similar competitive analysis framework.");
        add("heading", "Structural Trends");
        add("list", "Trend 1\nTrend 2\nTrend 3\nTrend 4");
        add("heading", "Valuation Comparison");
        add("chart", {},
            {{"chart_type", "bar"},
             {"title", "P/E Ratio by Company"},
             {"data", "22,19,25,17,30"},
             {"labels", "Co1,Co2,Co3,Co4,Co5"}});
        add("heading", "Risks & Headwinds");
        add("list", "Regulatory pressure\nTechnology disruption\nCyclicality\nGeopolitical exposure");
        add("heading", "Investment Opportunities");
        add("text", "Best positioned names and sub-themes to play.");

        // ── Economist / Macro ─────────────────────────────────────────────────────

    } else if (name == "Macro Economic Summary") {
        metadata_.title = "Macro Economic Summary";
        metadata_.date = QDateTime::currentDateTime().toString("yyyy-MM-dd");
        add("toc");
        add("heading", "Global Snapshot");
        add("table", {}, {{"rows", "6"}, {"cols", "4"}}); // Country | GDP | Inflation | Rates
        add("heading", "GDP Growth");
        add("chart", {},
            {{"chart_type", "line"},
             {"title", "GDP Growth Rate (%)"},
             {"data", "2.1,2.3,1.8,2.5,2.2,1.9,2.4"},
             {"labels", "Q1 23,Q2 23,Q3 23,Q4 23,Q1 24,Q2 24,Q3 24"}});
        add("heading", "Inflation");
        add("chart", {},
            {{"chart_type", "line"},
             {"title", "CPI YoY (%)"},
             {"data", "6.0,5.2,4.3,3.7,3.2,2.9,2.6"},
             {"labels", "Q1 23,Q2 23,Q3 23,Q4 23,Q1 24,Q2 24,Q3 24"}});
        add("heading", "Labour Market");
        add("table", {}, {{"rows", "4"}, {"cols", "3"}}); // Unemployment | NFP | Wages
        add("heading", "Central Bank Policy");
        add("table", {}, {{"rows", "6"}, {"cols", "4"}}); // Central Bank | Rate | Next Meeting | Bias
        add("heading", "Key Risks to Outlook");
        add("list",
            "Geopolitical escalation\nCredit tightening\nChina slowdown\nEnergy price shock\nElection uncertainty");
        add("heading", "Outlook");
        add("text", "Macro outlook summary and base-case scenario for the next 6–12 months.");

    } else if (name == "Country Risk Report") {
        metadata_.title = "Country Risk Report";
        add("toc");
        add("heading", "Country Overview");
        add("table", {}, {{"rows", "6"}, {"cols", "2"}}); // GDP, Population, Currency, Rating, HDI, etc.
        add("heading", "Political Risk");
        add("text", "Government stability, rule of law, corruption index, and geopolitical exposure.");
        add("heading", "Economic Risk");
        add("table", {}, {{"rows", "6"}, {"cols", "3"}}); // Indicator | Value | Trend
        add("chart", {},
            {{"chart_type", "line"},
             {"title", "GDP Growth vs Inflation"},
             {"data", "3.0,3.4,2.8,1.9,2.5"},
             {"labels", "2020,2021,2022,2023,2024E"}});
        add("heading", "Financial Risk");
        add("table", {}, {{"rows", "5"}, {"cols", "3"}}); // Debt/GDP, CA Balance, FX Reserves, etc.
        add("heading", "Market Risk");
        add("table", {}, {{"rows", "4"}, {"cols", "3"}}); // Equity Market | Bond Yields | Currency
        add("heading", "Overall Risk Rating");
        add("table", {}, {{"rows", "5"}, {"cols", "3"}}); // Risk Category | Score | Weight
        add("heading", "Outlook");
        add("text", "Summary of country risk outlook and key watchpoints.");

    } else if (name == "Central Bank Monitor") {
        metadata_.title = "Central Bank Monitor";
        metadata_.date = QDateTime::currentDateTime().toString("yyyy-MM-dd");
        add("heading", "Rate Decisions");
        add("table", {}, {{"rows", "7"}, {"cols", "4"}}); // Central Bank | Current Rate | Last Change | Next Meeting
        add("heading", "Rate Path");
        add("chart", {},
            {{"chart_type", "line"},
             {"title", "Policy Rate (%)"},
             {"data", "0.25,0.5,1.0,2.0,3.5,4.5,5.25,5.25,5.0"},
             {"labels", "Jan22,Mar22,Jun22,Sep22,Dec22,Mar23,Jun23,Sep23,Dec23"}});
        add("heading", "Forward Guidance Summary");
        add("text", "Summarise the forward guidance from major central banks.");
        add("heading", "Balance Sheet");
        add("chart", {},
            {{"chart_type", "bar"},
             {"title", "Balance Sheet Size ($T)"},
             {"data", "8.9,8.5,8.1,7.8,7.4"},
             {"labels", "Q4 22,Q1 23,Q2 23,Q3 23,Q4 23"}});
        add("heading", "Market Implications");
        add("text", "Impact on equities, bonds, FX, and commodities.");
        add("heading", "Key Upcoming Events");
        add("table", {}, {{"rows", "5"}, {"cols", "3"}}); // Date | Event | Expected

        // ── Crypto / Digital Assets ───────────────────────────────────────────────

    } else if (name == "Crypto Research Report") {
        metadata_.title = "Crypto Research Report";
        add("toc");
        add("heading", "Project Overview");
        add("text", "Describe the project, its purpose, technology, and value proposition.");
        add("market_data", {}, {{"symbol", "BTC-USD"}});
        add("heading", "Tokenomics");
        add("table", {}, {{"rows", "7"}, {"cols", "2"}}); // Total Supply, Circulating, Market Cap, FDV, etc.
        add("chart", {},
            {{"chart_type", "pie"},
             {"title", "Token Distribution"},
             {"data", "20,15,30,25,10"},
             {"labels", "Team,Investors,Community,Treasury,Public"}});
        add("heading", "On-Chain Metrics");
        add("table", {}, {{"rows", "6"}, {"cols", "3"}}); // Metric | Value | Trend
        add("heading", "Price History");
        add("chart", {},
            {{"chart_type", "line"},
             {"title", "Price (USD)"},
             {"data", "16000,20000,25000,30000,28000,42000,38000,52000"}});
        add("heading", "Competitive Landscape");
        add("text", "Key competitors and differentiation.");
        add("heading", "Risks");
        add("list",
            "Regulatory risk\nSmart contract bugs\nCompetitor displacement\nLiquidity risk\nTeam/execution risk");
        add("heading", "Investment Thesis");
        add("text", "Thesis, catalysts, and price targets.");
        add("quote", "Crypto assets are highly volatile. This is not financial advice.");

    } else if (name == "DeFi Protocol Analysis") {
        metadata_.title = "DeFi Protocol Analysis";
        add("toc");
        add("heading", "Protocol Overview");
        add("text", "What the protocol does, how it works, and what problem it solves.");
        add("heading", "Key Metrics");
        add("table", {}, {{"rows", "7"}, {"cols", "3"}}); // TVL | Revenue | Users | Fees | Token Price | MC | FDV
        add("heading", "TVL History");
        add("chart", {},
            {{"chart_type", "line"},
             {"title", "Total Value Locked ($M)"},
             {"data", "500,800,1200,900,1500,2000,1800,2400"}});
        add("heading", "Revenue & Fees");
        add("chart", {},
            {{"chart_type", "bar"},
             {"title", "Monthly Protocol Revenue ($K)"},
             {"data", "120,150,180,140,200,230,210,250"}});
        add("heading", "Governance & Token");
        add("text", "Token utility, governance rights, staking mechanics, and emission schedule.");
        add("heading", "Risks");
        add("list", "Smart contract exploit\nOracle manipulation\nRegulatory action\nLiquidity "
                    "fragmentation\nFork/governance risk");
        add("heading", "Valuation");
        add("text", "P/S, P/TVL, or other relevant DeFi valuation metrics and comparables.");

    } else if (name == "Crypto Portfolio Review") {
        metadata_.title = "Crypto Portfolio Review";
        metadata_.date = QDateTime::currentDateTime().toString("yyyy-MM-dd");
        add("heading", "Holdings");
        add("table", {}, {{"rows", "8"}, {"cols", "6"}}); // Asset | Qty | Cost | Price | Value | P&L%
        add("heading", "Current Prices");
        add("market_data", {}, {{"symbol", "BTC-USD"}});
        add("market_data", {}, {{"symbol", "ETH-USD"}});
        add("market_data", {}, {{"symbol", "SOL-USD"}});
        add("heading", "Allocation");
        add("chart", {},
            {{"chart_type", "pie"},
             {"title", "Portfolio Allocation"},
             {"data", "50,25,15,10"},
             {"labels", "BTC,ETH,SOL,Other"}});
        add("heading", "Performance");
        add("chart", {},
            {{"chart_type", "line"},
             {"title", "Portfolio Value (USD)"},
             {"data", "10000,9500,12000,11000,15000,13000,18000"}});
        add("heading", "Notes");
        add("text", "Rebalancing notes, conviction levels, and next moves.");

        // ── Fixed Income ──────────────────────────────────────────────────────────

    } else if (name == "Bond Research Report") {
        metadata_.title = "Bond Research Report";
        add("toc");
        add("heading", "Issuer Overview");
        add("text", "Issuer description, business profile, and credit history.");
        add("heading", "Bond Details");
        add("table", {}, {{"rows", "8"}, {"cols", "2"}}); // ISIN, Coupon, Maturity, Rating, Face Value, etc.
        add("heading", "Credit Profile");
        add("table", {}, {{"rows", "5"}, {"cols", "4"}}); // Metric | FY-2 | FY-1 | TTM
        add("heading", "Yield Analysis");
        add("chart", {},
            {{"chart_type", "line"},
             {"title", "Yield vs Benchmark"},
             {"data", "3.0,3.2,3.5,3.8,4.1,4.3,4.0"},
             {"labels", "Jan,Feb,Mar,Apr,May,Jun,Jul"}});
        add("heading", "Duration & Convexity");
        add("table", {}, {{"rows", "4"}, {"cols", "2"}});
        add("heading", "Comparable Bonds");
        add("table", {}, {{"rows", "5"}, {"cols", "5"}}); // Issuer | Rating | Coupon | Maturity | YTM
        add("heading", "Recommendation");
        add("text", "Buy / Hold / Sell with rationale and target spread.");

    } else if (name == "Yield Curve Analysis") {
        metadata_.title = "Yield Curve Analysis";
        metadata_.date = QDateTime::currentDateTime().toString("yyyy-MM-dd");
        add("heading", "Yield Curve Shape");
        add("chart", {},
            {{"chart_type", "line"},
             {"title", "US Treasury Yield Curve"},
             {"data", "5.3,5.1,4.9,4.7,4.6,4.5,4.4,4.3"},
             {"labels", "3M,6M,1Y,2Y,3Y,5Y,10Y,30Y"}});
        add("heading", "Key Spreads");
        add("table", {}, {{"rows", "5"}, {"cols", "3"}}); // Spread | Current | 1M Ago
        add("heading", "Historical Comparison");
        add("chart", {},
            {{"chart_type", "bar"},
             {"title", "10Y-2Y Spread (bps)"},
             {"data", "-80,-60,-40,-20,5,15,30"},
             {"labels", "Oct23,Nov23,Dec23,Jan24,Feb24,Mar24,Apr24"}});
        add("heading", "Duration & Convexity");
        add("table", {}, {{"rows", "4"}, {"cols", "3"}});
        add("heading", "Macro Drivers");
        add("text", "Explain the key macro factors driving current curve shape.");
        add("heading", "Implications");
        add("list",
            "Recession signal interpretation\nFed policy implications\nSector rotation impact\nCurrency effects");

        // ── Quant / Risk ──────────────────────────────────────────────────────────

    } else if (name == "Quant Strategy Report") {
        metadata_.title = "Quant Strategy Report";
        add("toc");
        add("heading", "Strategy Description");
        add("text", "Describe the strategy, signal generation logic, universe, and execution rules.");
        add("heading", "Backtest Parameters");
        add("table", {}, {{"rows", "6"}, {"cols", "2"}}); // Start Date, End Date, Universe, Frequency, etc.
        add("heading", "Performance Summary");
        add("table", {}, {{"rows", "8"}, {"cols", "2"}}); // CAGR, Sharpe, Sortino, Max DD, Win Rate, etc.
        add("heading", "Equity Curve");
        add("chart", {},
            {{"chart_type", "line"},
             {"title", "Strategy vs Benchmark"},
             {"data", "100,108,115,112,124,131,128,142,150"},
             {"labels", "Jan,Feb,Mar,Apr,May,Jun,Jul,Aug,Sep"}});
        add("heading", "Drawdown Analysis");
        add("chart", {},
            {{"chart_type", "bar"},
             {"title", "Monthly Drawdowns (%)"},
             {"data", "0,-2.1,-0.5,-3.2,-1.1,0,-1.8,-0.4,0"}});
        add("heading", "Factor Exposure");
        add("table", {}, {{"rows", "6"}, {"cols", "3"}}); // Factor | Beta | t-stat
        add("heading", "Stress Tests");
        add("table", {}, {{"rows", "5"}, {"cols", "3"}}); // Scenario | P&L | Max DD
        add("heading", "Limitations & Risks");
        add("list",
            "Overfitting risk\nTransaction cost sensitivity\nRegime change\nCapacity constraints\nData snooping bias");

    } else if (name == "Risk Management Report") {
        metadata_.title = "Risk Management Report";
        metadata_.date = QDateTime::currentDateTime().toString("yyyy-MM-dd");
        add("toc");
        add("heading", "Portfolio Risk Summary");
        add("table", {}, {{"rows", "6"}, {"cols", "2"}}); // VaR, CVaR, Beta, Volatility, Sharpe, Max DD
        add("heading", "VaR Analysis");
        add("chart", {},
            {{"chart_type", "bar"},
             {"title", "VaR by Confidence Level"},
             {"data", "1.2,1.8,2.5,3.1"},
             {"labels", "90%,95%,99%,99.9%"}});
        add("heading", "Stress Test Results");
        add("table", {}, {{"rows", "7"}, {"cols", "3"}}); // Scenario | P&L Impact | % Portfolio
        add("heading", "Correlation Matrix");
        add("text", "Paste or describe key pairwise correlations between holdings.");
        add("heading", "Concentration Risk");
        add("chart", {},
            {{"chart_type", "pie"},
             {"title", "Position Concentration"},
             {"data", "25,20,18,15,12,10"},
             {"labels", "Pos1,Pos2,Pos3,Pos4,Pos5,Other"}});
        add("heading", "Liquidity Risk");
        add("table", {}, {{"rows", "5"}, {"cols", "3"}}); // Position | ADV | Days to Liquidate
        add("heading", "Recommendations");
        add("list", "Reduce concentration in top 3 positions\nAdd hedges for macro exposure\nReview stop-loss "
                    "levels\nRebalance sector exposure");

        // ── Corporate / Business ──────────────────────────────────────────────────

    } else if (name == "Business Performance") {
        metadata_.title = "Business Performance Report";
        metadata_.date = QDateTime::currentDateTime().toString("yyyy-MM-dd");
        add("toc");
        add("heading", "Executive Summary");
        add("text", "One-paragraph summary of business performance this period.");
        add("heading", "Key Performance Indicators");
        add("table", {}, {{"rows", "8"}, {"cols", "4"}}); // KPI | Target | Actual | Variance
        add("heading", "Revenue");
        add("chart", {},
            {{"chart_type", "bar"},
             {"title", "Revenue ($M)"},
             {"data", "42,45,48,44,51,53,50,55"},
             {"labels", "Q1 22,Q2 22,Q3 22,Q4 22,Q1 23,Q2 23,Q3 23,Q4 23"}});
        add("heading", "Cost Breakdown");
        add("chart", {},
            {{"chart_type", "pie"},
             {"title", "Cost Structure"},
             {"data", "40,25,20,15"},
             {"labels", "COGS,R&D,S&M,G&A"}});
        add("heading", "Department Performance");
        add("table", {}, {{"rows", "6"}, {"cols", "4"}});
        add("heading", "Targets vs Actuals");
        add("chart", {},
            {{"chart_type", "bar"},
             {"title", "Target vs Actual"},
             {"data", "100,95,100,102,100,88"},
             {"labels", "Jan,Feb,Mar,Apr,May,Jun"}});
        add("heading", "Issues & Risks");
        add("list", "Issue 1\nIssue 2\nIssue 3");
        add("heading", "Next Period Targets");
        add("table", {}, {{"rows", "5"}, {"cols", "3"}});

    } else if (name == "Project Status Report") {
        metadata_.title = "Project Status Report";
        metadata_.date = QDateTime::currentDateTime().toString("yyyy-MM-dd");
        add("heading", "Project Overview");
        add("table", {}, {{"rows", "5"}, {"cols", "2"}}); // Name, Owner, Start, End, Budget
        add("heading", "Overall Status");
        add("table", {}, {{"rows", "4"}, {"cols", "3"}}); // Dimension | Status | Notes (Scope/Time/Cost/Quality)
        add("heading", "Milestones");
        add("table", {}, {{"rows", "7"}, {"cols", "4"}}); // Milestone | Due Date | Status | Owner
        add("heading", "Progress This Period");
        add("text", "Summarise completed work and progress against plan.");
        add("heading", "Issues & Blockers");
        add("table", {}, {{"rows", "5"}, {"cols", "4"}}); // Issue | Impact | Owner | Resolution Date
        add("heading", "Risks");
        add("table", {}, {{"rows", "5"}, {"cols", "4"}}); // Risk | Likelihood | Impact | Mitigation
        add("heading", "Next Steps");
        add("list", "Action 1 — Owner — Due date\nAction 2 — Owner — Due date\nAction 3 — Owner — Due date");

    } else if (name == "Financial Statement") {
        metadata_.title = "Financial Statement";
        add("toc");
        add("heading", "Income Statement");
        add("table", {}, {{"rows", "12"}, {"cols", "4"}}); // Item | FY-2 | FY-1 | FY Current
        add("chart", {},
            {{"chart_type", "bar"},
             {"title", "Revenue & Net Income"},
             {"data", "400,450,500,420,480,530"},
             {"labels", "H1 22,H2 22,H1 23,H2 23,H1 24,H2 24"}});
        add("page_break");
        add("heading", "Balance Sheet");
        add("table", {}, {{"rows", "14"}, {"cols", "3"}}); // Item | Prior Year | Current Year
        add("page_break");
        add("heading", "Cash Flow Statement");
        add("table", {}, {{"rows", "12"}, {"cols", "3"}});
        add("heading", "Key Ratios");
        add("table", {}, {{"rows", "8"}, {"cols", "3"}}); // Ratio | Prior Year | Current Year
        add("divider");
        add("quote", "Prepared in accordance with applicable accounting standards.");
    }

    selected_ = components_.isEmpty() ? -1 : 0;
    refresh_canvas();
    refresh_structure();
    if (!components_.isEmpty())
        select_component(0);
}

// ── File I/O ──────────────────────────────────────────────────────────────────

void ReportBuilderScreen::load_report(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Open Report", "Could not open file:\n" + path);
        return;
    }
    QString json = QString::fromUtf8(f.readAll());
    f.close();

    if (!deserialize_from_json(json)) {
        QMessageBox::warning(this, "Open Report", "Could not parse report file:\n" + path);
        return;
    }

    current_file_ = path;
    update_recent(path);
    refresh_canvas();
    refresh_structure();
    if (!components_.isEmpty())
        select_component(0);
}

void ReportBuilderScreen::on_new() {
    auto answer = QMessageBox::question(this, "New Report", "Create a new report? Unsaved changes will be lost.",
                                        QMessageBox::Yes | QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    components_.clear();
    undo_stack_->clear();
    selected_ = -1;
    next_id_ = 1;
    current_file_ = {};
    metadata_ = ReportMetadata{};
    metadata_.date = QDateTime::currentDateTime().toString("yyyy-MM-dd");

    properties_->show_empty();
    refresh_canvas();
    refresh_structure();
}

void ReportBuilderScreen::on_open() {
    QString path = QFileDialog::getOpenFileName(this, "Open Report", "", "Fincept Report (*.fincept);;JSON (*.json)");
    if (path.isEmpty())
        return;
    load_report(path);
}

void ReportBuilderScreen::on_save() {
    QString path = current_file_;
    if (path.isEmpty()) {
        path = QFileDialog::getSaveFileName(this, "Save Report", metadata_.title,
                                            "Fincept Report (*.fincept);;JSON (*.json)");
        if (path.isEmpty())
            return;
        current_file_ = path;
    }

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Save Report", "Could not save to:\n" + path);
        return;
    }
    f.write(serialize_to_json().toUtf8());
    f.close();
    update_recent(path);

    // Register with File Manager so it appears in the Files tab
    services::FileManagerService::instance().import_file(path, "report_builder");
}

void ReportBuilderScreen::on_auto_save() {
    if (autosave_path_.isEmpty()) {
        autosave_path_ =
            QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/fincept_report_autosave.fincept";
    }
    QFile f(autosave_path_);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write(serialize_to_json().toUtf8());
    }
}

void ReportBuilderScreen::on_export_pdf() {
    QString path = QFileDialog::getSaveFileName(this, "Export PDF", metadata_.title, "PDF (*.pdf)");
    if (path.isEmpty())
        return;

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(path);
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setPageMargins(QMarginsF(15, 20, 15, 20), QPageLayout::Millimeter);

    // Determine if we need header/footer rendering
    bool has_header =
        !metadata_.header_left.isEmpty() || !metadata_.header_center.isEmpty() || !metadata_.header_right.isEmpty();
    bool has_footer = !metadata_.footer_left.isEmpty() || !metadata_.footer_center.isEmpty() ||
                      !metadata_.footer_right.isEmpty() || metadata_.show_page_numbers;

    if (!has_header && !has_footer) {
        // Simple path — let Qt handle it
        canvas_->text_edit()->document()->print(&printer);
    } else {
        // Manual render with header/footer via QPainter
        QTextDocument* doc = canvas_->text_edit()->document()->clone(this);

        // Shrink document margins to leave room for header/footer
        QTextFrameFormat root_fmt = doc->rootFrame()->frameFormat();
        root_fmt.setTopMargin(has_header ? 24 : root_fmt.topMargin());
        root_fmt.setBottomMargin(has_footer ? 24 : root_fmt.bottomMargin());
        doc->rootFrame()->setFrameFormat(root_fmt);

        QPainter painter;
        if (!painter.begin(&printer)) {
            QMessageBox::warning(this, "Export PDF", "Failed to start PDF painter.");
            doc->deleteLater();
            return;
        }

        QRectF page_rect = printer.pageRect(QPrinter::DevicePixel);
        doc->setPageSize(page_rect.size());

        int total_pages = doc->pageCount();

        auto draw_hf_line = [&](int page, bool is_header) {
            QString left = is_header ? metadata_.header_left : metadata_.footer_left;
            QString center = is_header ? metadata_.header_center : metadata_.footer_center;
            QString right = is_header ? metadata_.header_right : metadata_.footer_right;

            // Replace {page} and {total}
            auto replace_tokens = [&](QString s) {
                s.replace("{page}", QString::number(page));
                s.replace("{total}", QString::number(total_pages));
                return s;
            };
            left = replace_tokens(left);
            center = replace_tokens(center);
            right = replace_tokens(right);

            double y = is_header ? 4 : page_rect.height() - 16;
            QFont hf_font;
            hf_font.setPointSizeF(7);
            painter.setFont(hf_font);
            painter.setPen(QColor("#888888"));

            QRectF r(page_rect.left() + 8, y, page_rect.width() - 16, 14);
            if (!left.isEmpty())
                painter.drawText(r, Qt::AlignLeft | Qt::AlignVCenter, left);
            if (!center.isEmpty())
                painter.drawText(r, Qt::AlignHCenter | Qt::AlignVCenter, center);
            if (!right.isEmpty())
                painter.drawText(r, Qt::AlignRight | Qt::AlignVCenter, right);

            // Thin rule under header / above footer
            painter.setPen(QPen(QColor("#cccccc"), 0.5));
            double rule_y = is_header ? y + 14 : y - 2;
            painter.drawLine(QPointF(page_rect.left() + 8, rule_y), QPointF(page_rect.right() - 8, rule_y));
        };

        for (int page = 1; page <= total_pages; ++page) {
            if (page > 1)
                printer.newPage();

            painter.save();
            // Clip to content area (leave header/footer bands)
            double content_top = has_header ? 20 : 0;
            double content_bottom = has_footer ? page_rect.height() - 20 : page_rect.height();
            painter.setClipRect(QRectF(page_rect.left(), content_top, page_rect.width(), content_bottom - content_top));

            // Paint the page content
            painter.translate(0, -(page - 1) * page_rect.height() + content_top);
            doc->drawContents(&painter);
            painter.restore();

            if (has_header)
                draw_hf_line(page, true);
            if (has_footer)
                draw_hf_line(page, false);
        }

        painter.end();
        doc->deleteLater();
    }

    QMessageBox::information(this, "Export PDF", "Report exported successfully to:\n" + path);

    // Register exported PDF with File Manager
    services::FileManagerService::instance().import_file(path, "report_builder");
}

void ReportBuilderScreen::on_preview() {
    QPrinter printer(QPrinter::HighResolution);
    printer.setPageSize(QPageSize(QPageSize::A4));

    QPrintPreviewDialog preview(&printer, this);
    connect(&preview, &QPrintPreviewDialog::paintRequested, this,
            [this](QPrinter* p) { canvas_->text_edit()->document()->print(p); });
    preview.exec();
}

void ReportBuilderScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    autosave_->start();
}

void ReportBuilderScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    autosave_->stop();
}

// ── IStatefulScreen ───────────────────────────────────────────────────────────

QVariantMap ReportBuilderScreen::save_state() const {
    return {{"current_file", current_file_}};
}

void ReportBuilderScreen::restore_state(const QVariantMap& state) {
    // current_file_ is only a reference path — the actual content lives in
    // the undo stack and components_ vector. Just restore the title display.
    const QString file = state.value("current_file").toString();
    if (!file.isEmpty())
        current_file_ = file;
}

} // namespace fincept::screens
