// DataSourcesSection.cpp — connection list, stats, and bulk actions panel.

#include "screens/settings/DataSourcesSection.h"

#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "screens/settings/SettingsRowHelpers.h"
#include "screens/settings/SettingsStyles.h"
#include "storage/repositories/DataSourceRepository.h"
#include "ui/theme/Theme.h"

#include <QCheckBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMap>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QString>
#include <QVBoxLayout>
#include <QVector>

namespace fincept::screens {

namespace {

QString ds_transport_badge(const QString& type) {
    if (type == "websocket") return "WS";
    if (type == "rest_api")  return "REST";
    if (type == "sql")       return "SQL";
    return type.left(4).toUpper();
}

// Local copy — keeps StorageSection's helper private to that translation unit.
QFrame* make_panel(const QString& title) {
    auto* panel = new QFrame;
    panel->setStyleSheet(QString("QFrame{background:%1;border:1px solid %2;}")
                             .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));

    auto* pvl = new QVBoxLayout(panel);
    pvl->setContentsMargins(0, 0, 0, 0);
    pvl->setSpacing(0);

    auto* hdr = new QWidget(nullptr);
    hdr->setFixedHeight(34);
    hdr->setStyleSheet(QString("background:%1;border-bottom:1px solid %2;")
                           .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    auto* hhl = new QHBoxLayout(hdr);
    hhl->setContentsMargins(12, 0, 12, 0);
    auto* lbl = new QLabel(title);
    lbl->setStyleSheet(QString("color:%1;font-weight:700;letter-spacing:0.5px;background:transparent;")
                           .arg(ui::colors::AMBER()));
    hhl->addWidget(lbl);
    hhl->addStretch();
    pvl->addWidget(hdr);

    return panel;
}

} // namespace

DataSourcesSection::DataSourcesSection(QWidget* parent) : QWidget(parent) {
    host_layout_ = new QVBoxLayout(this);
    host_layout_->setContentsMargins(0, 0, 0, 0);
    host_layout_->setSpacing(0);
    rebuild();
}

void DataSourcesSection::rebuild() {
    if (content_) {
        host_layout_->removeWidget(content_);
        content_->deleteLater();
    }
    content_ = build_content();
    host_layout_->addWidget(content_);
}

QWidget* DataSourcesSection::build_content() {
    using namespace settings_styles;

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(QString("QScrollArea{border:none;background:transparent;}"
                                  "QScrollBar:vertical{width:6px;background:transparent;}"
                                  "QScrollBar::handle:vertical{background:%1;border-radius:3px;}"
                                  "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
                              .arg(ui::colors::BORDER_MED()));

    auto* page = new QWidget(this);
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(14, 14, 14, 14);
    vl->setSpacing(10);

    // Title + Open Full Screen button
    auto* title_row = new QWidget(this);
    title_row->setStyleSheet("background:transparent;");
    auto* trl = new QHBoxLayout(title_row);
    trl->setContentsMargins(0, 0, 0, 0);
    auto* t = new QLabel("DATA SOURCES");
    t->setStyleSheet(section_title_ss());
    trl->addWidget(t);
    trl->addStretch();

    auto* open_full = new QPushButton("OPEN FULL SCREEN");
    open_full->setFixedHeight(24);
    open_full->setCursor(Qt::PointingHandCursor);
    open_full->setStyleSheet(
        QString("QPushButton{background:rgba(217,119,6,0.1);color:%1;border:1px solid %2;font-weight:700;padding:0 10px;}"
                "QPushButton:hover{background:%1;color:%3;}")
            .arg(ui::colors::AMBER(), ui::colors::AMBER_DIM(), ui::colors::BG_BASE()));
    connect(open_full, &QPushButton::clicked, this,
            []() { EventBus::instance().publish("nav.switch_screen", {{"screen", "data_sources"}}); });
    trl->addWidget(open_full);
    vl->addWidget(title_row);

    auto* info = new QLabel("Quick management of configured connections. "
                            "For full browsing, adding, testing, and import/export use the full screen.");
    info->setWordWrap(true);
    info->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_SECONDARY()));
    vl->addWidget(info);
    vl->addSpacing(4);

    // Stats strip
    auto result = DataSourceRepository::instance().list_all();
    auto connections = result.is_ok() ? result.value() : QVector<DataSource>{};

    int total = connections.size();
    int active = 0;
    QSet<QString> providers;
    QMap<QString, int> cat_counts;
    for (const auto& ds : connections) {
        if (ds.enabled) ++active;
        providers.insert(ds.provider);
        QString cat = ds.category.isEmpty() ? "other" : ds.category;
        cat_counts[cat]++;
    }

    {
        auto* stat_row = new QWidget(this);
        stat_row->setStyleSheet("background:transparent;");
        auto* shl = new QHBoxLayout(stat_row);
        shl->setContentsMargins(0, 0, 0, 0);
        shl->setSpacing(8);

        auto make_stat = [&](const QString& label, int value, const QString& color) {
            auto* box = new QFrame;
            box->setStyleSheet(QString("QFrame{background:%1;border:1px solid %2;}")
                                   .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
            auto* bx = new QVBoxLayout(box);
            bx->setContentsMargins(10, 8, 10, 10);
            bx->setSpacing(2);
            bx->setAlignment(Qt::AlignCenter);

            auto* val = new QLabel(QString::number(value));
            val->setAlignment(Qt::AlignCenter);
            val->setStyleSheet(QString("color:%1;font-size:18px;font-weight:700;background:transparent;").arg(color));
            bx->addWidget(val);

            auto* ll = new QLabel(label);
            ll->setAlignment(Qt::AlignCenter);
            ll->setStyleSheet(
                QString("color:%1;font-weight:700;background:transparent;").arg(ui::colors::TEXT_SECONDARY()));
            bx->addWidget(ll);

            shl->addWidget(box, 1);
        };

        make_stat("TOTAL",     total,             ui::colors::CYAN());
        make_stat("ACTIVE",    active,            ui::colors::POSITIVE());
        make_stat("INACTIVE",  total - active,    ui::colors::TEXT_DIM());
        make_stat("PROVIDERS", providers.size(),  ui::colors::AMBER());
        vl->addWidget(stat_row);
    }

    vl->addSpacing(6);

    // Connections table
    if (connections.isEmpty()) {
        auto* empty_panel = make_panel("CONNECTIONS");
        auto* empty_body = new QWidget(this);
        empty_body->setStyleSheet("background:transparent;");
        auto* evl = new QVBoxLayout(empty_body);
        evl->setContentsMargins(12, 16, 12, 16);
        auto* elbl = new QLabel(
            "No data sources configured. Open the full Data Sources screen to browse and add connectors.");
        elbl->setWordWrap(true);
        elbl->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_SECONDARY()));
        evl->addWidget(elbl);
        static_cast<QVBoxLayout*>(empty_panel->layout())->addWidget(empty_body);
        vl->addWidget(empty_panel);
    } else {
        QMap<QString, QVector<DataSource>> grouped;
        for (const auto& ds : connections) {
            QString cat = ds.category.isEmpty() ? "other" : ds.category;
            grouped[cat].append(ds);
        }

        auto* panel = make_panel("CONNECTIONS");
        auto* body = new QWidget(this);
        body->setStyleSheet("background:transparent;");
        auto* bvl = new QVBoxLayout(body);
        bvl->setContentsMargins(0, 0, 0, 0);
        bvl->setSpacing(0);

        // Table header
        auto* th = new QWidget(this);
        th->setFixedHeight(26);
        th->setStyleSheet(QString("background:%1;border-bottom:1px solid %2;")
                              .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
        auto* thl = new QHBoxLayout(th);
        thl->setContentsMargins(12, 0, 12, 0);
        thl->setSpacing(6);

        auto add_th = [&](const QString& text, int width = 0) {
            auto* lbl = new QLabel(text);
            lbl->setStyleSheet(
                QString("color:%1;font-weight:700;background:transparent;").arg(ui::colors::TEXT_DIM()));
            if (width > 0) {
                lbl->setFixedWidth(width);
                lbl->setAlignment(Qt::AlignCenter);
            }
            thl->addWidget(lbl, width > 0 ? 0 : 1);
        };
        add_th("SOURCE");
        add_th("TYPE",     44);
        add_th("CATEGORY", 80);
        add_th("ON",       30);
        add_th("DEL",      30);
        bvl->addWidget(th);

        int row_idx = 0;
        for (auto it = grouped.constBegin(); it != grouped.constEnd(); ++it) {
            auto* grp = new QWidget(this);
            grp->setFixedHeight(24);
            grp->setStyleSheet(QString("background:%1;border-bottom:1px solid %2;")
                                   .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
            auto* ghl = new QHBoxLayout(grp);
            ghl->setContentsMargins(12, 0, 12, 0);
            auto* glbl = new QLabel(it.key().toUpper() + QString("  (%1)").arg(it.value().size()));
            glbl->setStyleSheet(QString("color:%1;font-weight:700;letter-spacing:0.5px;background:transparent;")
                                    .arg(ui::colors::AMBER_DIM()));
            ghl->addWidget(glbl);
            bvl->addWidget(grp);

            for (const auto& ds : it.value()) {
                auto* row = new QWidget(this);
                row->setFixedHeight(26);
                row->setStyleSheet(
                    QString("background:%1;border-bottom:1px solid %2;")
                        .arg((row_idx % 2) ? ui::colors::ROW_ALT() : ui::colors::BG_BASE(),
                             ui::colors::BORDER_DIM()));

                auto* rhl = new QHBoxLayout(row);
                rhl->setContentsMargins(12, 0, 12, 0);
                rhl->setSpacing(6);

                QString name = ds.display_name.isEmpty() ? ds.alias : ds.display_name;
                auto* name_lbl = new QLabel(name);
                name_lbl->setStyleSheet(
                    QString("color:%1;background:transparent;")
                        .arg(ds.enabled ? ui::colors::TEXT_PRIMARY() : ui::colors::TEXT_TERTIARY()));
                rhl->addWidget(name_lbl, 1);

                auto* badge = new QLabel(ds_transport_badge(ds.type));
                badge->setFixedWidth(44);
                badge->setAlignment(Qt::AlignCenter);
                badge->setStyleSheet(
                    QString("color:%1;font-weight:700;background:%2;border:1px solid %3;")
                        .arg(ui::colors::TEXT_TERTIARY(), ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
                rhl->addWidget(badge);

                auto* cat_lbl = new QLabel(ds.category.isEmpty() ? "—" : ds.category);
                cat_lbl->setFixedWidth(80);
                cat_lbl->setAlignment(Qt::AlignCenter);
                cat_lbl->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_TERTIARY()));
                rhl->addWidget(cat_lbl);

                auto* toggle = new QCheckBox;
                toggle->setChecked(ds.enabled);
                toggle->setFixedWidth(30);
                toggle->setStyleSheet(check_ss());
                QString source_id = ds.id;
                connect(toggle, &QCheckBox::toggled, this, [source_id, name_lbl](bool checked) {
                    DataSourceRepository::instance().set_enabled(source_id, checked);
                    name_lbl->setStyleSheet(
                        QString("color:%1;background:transparent;")
                            .arg(checked ? ui::colors::TEXT_PRIMARY() : ui::colors::TEXT_TERTIARY()));
                    LOG_INFO("DataSources", (checked ? "Enabled: " : "Disabled: ") + source_id);
                });
                rhl->addWidget(toggle);

                auto* del_btn = new QPushButton("X");
                del_btn->setFixedSize(22, 18);
                del_btn->setCursor(Qt::PointingHandCursor);
                del_btn->setStyleSheet(
                    QString("QPushButton{background:transparent;color:%1;border:none;font-weight:700;}"
                            "QPushButton:hover{color:%2;}")
                        .arg(ui::colors::TEXT_DIM(), ui::colors::NEGATIVE()));
                connect(del_btn, &QPushButton::clicked, this, [this, source_id, name, row]() {
                    auto answer = QMessageBox::warning(this, "Delete Connection",
                                                       "Delete connection \"" + name + "\"?\n\nThis cannot be undone.",
                                                       QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
                    if (answer != QMessageBox::Yes) return;

                    DataSourceRepository::instance().remove(source_id);
                    row->hide();
                    LOG_INFO("DataSources", "Deleted: " + source_id);
                });
                rhl->addWidget(del_btn);

                bvl->addWidget(row);
                ++row_idx;
            }
        }

        static_cast<QVBoxLayout*>(panel->layout())->addWidget(body);
        vl->addWidget(panel);
    }

    vl->addSpacing(10);

    // Bulk actions
    {
        auto* panel = make_panel("BULK ACTIONS");
        auto* body = new QWidget(this);
        body->setStyleSheet("background:transparent;");
        auto* bvl = new QVBoxLayout(body);
        bvl->setContentsMargins(10, 8, 10, 8);
        bvl->setSpacing(6);

        auto* btn_row = new QWidget(this);
        btn_row->setStyleSheet("background:transparent;");
        auto* brhl = new QHBoxLayout(btn_row);
        brhl->setContentsMargins(0, 0, 0, 0);
        brhl->setSpacing(8);

        auto* enable_all = new QPushButton("ENABLE ALL");
        enable_all->setFixedHeight(24);
        enable_all->setStyleSheet(QString("QPushButton{background:rgba(22,163,74,0.1);color:%1;border:1px solid "
                                          "%1;font-weight:700;padding:0 10px;}"
                                          "QPushButton:hover{background:%1;color:%2;}")
                                      .arg(ui::colors::POSITIVE(), ui::colors::BG_BASE()));
        connect(enable_all, &QPushButton::clicked, this, [this]() {
            auto r = DataSourceRepository::instance().list_all();
            if (!r.is_ok()) return;
            for (const auto& ds : r.value())
                DataSourceRepository::instance().set_enabled(ds.id, true);
            LOG_INFO("DataSources", "All sources enabled");
            rebuild();
        });
        brhl->addWidget(enable_all);

        auto* disable_all = new QPushButton("DISABLE ALL");
        disable_all->setFixedHeight(24);
        disable_all->setStyleSheet(
            QString("QPushButton{background:rgba(220,38,38,0.1);color:%1;border:1px solid %3;font-weight:700;padding:0 "
                    "10px;}"
                    "QPushButton:hover{background:%1;color:%2;}")
                .arg(ui::colors::NEGATIVE(), ui::colors::TEXT_PRIMARY(), ui::colors::NEGATIVE_DIM()));
        connect(disable_all, &QPushButton::clicked, this, [this]() {
            auto answer = QMessageBox::warning(this, "Disable All", "Disable all data source connections?",
                                               QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
            if (answer != QMessageBox::Yes) return;
            auto r = DataSourceRepository::instance().list_all();
            if (!r.is_ok()) return;
            for (const auto& ds : r.value())
                DataSourceRepository::instance().set_enabled(ds.id, false);
            LOG_INFO("DataSources", "All sources disabled");
            rebuild();
        });
        brhl->addWidget(disable_all);

        auto* delete_all = new QPushButton("DELETE ALL");
        delete_all->setFixedHeight(24);
        delete_all->setStyleSheet(
            QString("QPushButton{background:%1;color:%2;border:2px solid %2;font-weight:700;padding:0 10px;}"
                    "QPushButton:hover{background:%2;color:%3;}")
                .arg(ui::colors::BG_RAISED(), ui::colors::NEGATIVE(), ui::colors::TEXT_PRIMARY()));
        connect(delete_all, &QPushButton::clicked, this, [this]() {
            auto answer = QMessageBox::critical(
                this, "Delete All Connections",
                "Permanently delete ALL data source connections?\n\n"
                "This cannot be undone. You can re-add them from the full Data Sources screen.",
                QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
            if (answer != QMessageBox::Yes) return;

            auto r = DataSourceRepository::instance().list_all();
            if (!r.is_ok()) return;
            for (const auto& ds : r.value())
                DataSourceRepository::instance().remove(ds.id);
            LOG_INFO("DataSources", "All connections deleted");
            rebuild();
        });
        brhl->addWidget(delete_all);

        brhl->addStretch();
        bvl->addWidget(btn_row);

        auto* note = new QLabel("For adding new connections, testing connectivity, and import/export, "
                                "use the full Data Sources screen.");
        note->setWordWrap(true);
        note->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_DIM()));
        bvl->addWidget(note);

        static_cast<QVBoxLayout*>(panel->layout())->addWidget(body);
        vl->addWidget(panel);
    }

    vl->addStretch();
    scroll->setWidget(page);
    return scroll;
}

} // namespace fincept::screens
