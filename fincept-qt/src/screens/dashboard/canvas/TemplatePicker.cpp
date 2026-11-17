#include "screens/dashboard/canvas/TemplatePicker.h"

#include "core/market/MarketProfile.h"
#include "screens/dashboard/canvas/WidgetRegistry.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include <algorithm>

namespace fincept::screens {

namespace {

QString tp_tile_name(const QString& type_id) {
    if (const auto* m = WidgetRegistry::instance().find(type_id))
        return WidgetRegistry::display_name_tr(*m);
    return type_id;
}

/// Paint the template's grid into `r`: one categorical colour per tile, the desk
/// note outlined in the accent, tile names when `labels` and the cell is large
/// enough. Colours are read from the theme at paint time.
void tp_paint_grid(QPainter& p, const QRectF& r, const QVector<GridItem>& items, bool labels) {
    p.fillRect(r, QColor(ui::colors::BG_BASE()));
    const int rows = std::max(1, max_row(items));
    const double cw = r.width() / 12.0;
    const double rh = r.height() / rows;
    const double gap = labels ? 2.0 : 1.0;
    QFont f = ui::fonts::font(-4, QFont::Bold);
    p.setFont(f);
    const QFontMetrics fm(f);
    int ci = 0;
    for (const auto& item : items) {
        const QRectF cell(r.left() + item.cell.x * cw + gap, r.top() + item.cell.y * rh + gap,
                          item.cell.w * cw - 2 * gap, item.cell.h * rh - 2 * gap);
        const bool note = item.id == QLatin1String("desk_note");
        QColor c = note ? QColor(ui::colors::BG_RAISED()) : ui::colors::chart_color(ci++);
        if (!note)
            c.setAlpha(labels ? 120 : 170);
        p.fillRect(cell, c);
        if (note) {
            p.setPen(QPen(QColor(ui::colors::AMBER()), 1));
            p.drawRect(cell.adjusted(0.5, 0.5, -0.5, -0.5));
        }
        if (labels && cell.width() > 40 && cell.height() > fm.height() + 4) {
            p.setPen(QColor(ui::colors::TEXT_PRIMARY()));
            const QRectF tr = cell.adjusted(5, 3, -5, -3);
            p.drawText(tr, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                       fm.elidedText(tp_tile_name(item.id), Qt::ElideRight, int(tr.width()) * 2));
        }
    }
}

QPixmap tp_thumbnail(const QVector<GridItem>& items, const QSize& size, qreal dpr) {
    QPixmap pm(size * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, false);
    tp_paint_grid(p, QRectF(QPointF(0, 0), QSizeF(size)), items, false);
    return pm;
}

} // namespace

/// Large labelled schematic of the selected template.
class TemplatePreview : public QWidget {
  public:
    explicit TemplatePreview(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumSize(360, 220);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }
    void set_items(const QVector<GridItem>& items) {
        items_ = items;
        update();
    }

  protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, false);
        // Keep the canvas's proportions: 12 columns of ~100 px, rows of 44 px.
        const int rows = std::max(1, max_row(items_));
        const double aspect = (12.0 * 100.0) / (rows * 44.0);
        QRectF r = rect().adjusted(1, 1, -1, -1);
        if (r.width() / r.height() > aspect)
            r.setWidth(r.height() * aspect);
        else
            r.setHeight(r.width() / aspect);
        p.setPen(QPen(QColor(ui::colors::BORDER_DIM()), 1));
        p.drawRect(r.adjusted(-1, -1, 0, 0));
        tp_paint_grid(p, r, items_, true);
    }

  private:
    QVector<GridItem> items_;
};

TemplatePicker::TemplatePicker(const QString& current_id, bool current_is_pristine, QWidget* parent)
    : QDialog(parent), current_id_(current_id), current_pristine_(current_is_pristine) {
    setObjectName("dashTemplatePicker");
    setWindowTitle(tr("Dashboard Templates"));
    resize(1000, 660);
    apply_styles(); // before the children exist: they are styled once, at creation

    templates_ = all_dashboard_templates();

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(16, 14, 16, 14);
    vl->setSpacing(10);

    title_lbl_ = new QLabel(this);
    title_lbl_->setObjectName("tpTitle");
    vl->addWidget(title_lbl_);
    sub_lbl_ = new QLabel(this);
    sub_lbl_->setObjectName("tpSub");
    sub_lbl_->setWordWrap(true);
    vl->addWidget(sub_lbl_);

    auto* body = new QHBoxLayout;
    body->setSpacing(14);

    list_ = new QListWidget(this);
    list_->setObjectName("tpList");
    list_->setIconSize(QSize(84, 56));
    list_->setWordWrap(true);
    list_->setUniformItemSizes(false);
    list_->setSpacing(1);
    list_->setFixedWidth(370);
    list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    body->addWidget(list_);

    auto* detail = new QVBoxLayout;
    detail->setSpacing(6);
    name_lbl_ = new QLabel(this);
    name_lbl_->setObjectName("tpName");
    detail->addWidget(name_lbl_);
    desc_lbl_ = new QLabel(this);
    desc_lbl_->setObjectName("tpDesc");
    desc_lbl_->setWordWrap(true);
    detail->addWidget(desc_lbl_);
    preview_ = new TemplatePreview(this);
    detail->addWidget(preview_, 1); // takes every pixel the text below leaves
    meta_lbl_ = new QLabel(this);
    meta_lbl_->setObjectName("tpMeta");
    meta_lbl_->setWordWrap(true);
    meta_lbl_->setTextFormat(Qt::RichText);
    detail->addWidget(meta_lbl_);
    tiles_lbl_ = new QLabel(this);
    tiles_lbl_->setObjectName("tpTiles");
    tiles_lbl_->setWordWrap(true);
    tiles_lbl_->setTextFormat(Qt::RichText);
    tiles_lbl_->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    detail->addWidget(tiles_lbl_, 0);
    body->addLayout(detail, 1);
    vl->addLayout(body, 1);

    auto* bot = new QHBoxLayout;
    bot->addStretch();
    cancel_btn_ = new QPushButton(this);
    cancel_btn_->setObjectName("tpCancel");
    cancel_btn_->setMinimumHeight(28);
    connect(cancel_btn_, &QPushButton::clicked, this, &QDialog::reject);
    bot->addWidget(cancel_btn_);
    apply_btn_ = new QPushButton(this);
    apply_btn_->setObjectName("tpApply");
    apply_btn_->setMinimumHeight(28);
    apply_btn_->setDefault(true);
    connect(apply_btn_, &QPushButton::clicked, this, &TemplatePicker::accept_current);
    bot->addWidget(apply_btn_);
    vl->addLayout(bot);

    build_list();
    connect(list_, &QListWidget::currentRowChanged, this, &TemplatePicker::show_details);
    connect(list_, &QListWidget::itemDoubleClicked, this, [this]() { accept_current(); });
    retranslateUi();

    int start = 0;
    for (int i = 0; i < templates_.size(); ++i)
        if (templates_[i].id == current_id_)
            start = i;
    list_->setCurrentRow(start);

    // Never inside theme_changed (restyling / rebuilding the list there stalls the
    // switch): once the appearance has settled; the sheet only when it changed.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() {
        apply_styles();
        build_list();
        show_details(list_->currentRow());
    });
}

void TemplatePicker::apply_styles() {
    const QString sheet =
        QString("QDialog#dashTemplatePicker{background:%1;}"
                "QLabel#tpTitle{color:%2;font-size:%3px;font-weight:bold;letter-spacing:1px;}"
                "QLabel#tpSub{color:%4;font-size:%5px;}"
                "QLabel#tpName{color:%6;font-size:%7px;font-weight:bold;}"
                "QLabel#tpDesc{color:%4;font-size:%5px;}"
                "QLabel#tpMeta{color:%4;font-size:%5px;}"
                "QLabel#tpTiles{color:%6;font-size:%5px;}"
                "QListWidget#tpList{background:%8;border:1px solid %9;color:%6;font-size:%5px;outline:0;}"
                "QListWidget#tpList::item{padding:6px 4px;border-bottom:1px solid %9;}"
                "QListWidget#tpList::item:selected{background:%10;color:%6;border-left:2px solid %2;}"
                "QListWidget#tpList::item:hover{background:%11;}"
                "QPushButton#tpCancel{background:%8;border:1px solid %12;color:%6;padding:4px 16px;font-size:%5px;}"
                "QPushButton#tpCancel:hover{border-color:%2;color:%2;}"
                "QPushButton#tpApply{background:%2;border:1px solid %2;color:%13;padding:4px 18px;font-size:%5px;"
                "font-weight:bold;}"
                "QPushButton#tpApply:hover{background:%14;}")
            .arg(ui::colors::BG_BASE(), ui::colors::AMBER(), ui::fonts::px(-2), ui::colors::TEXT_SECONDARY(),
                 ui::fonts::px(-3), ui::colors::TEXT_PRIMARY(), ui::fonts::px(0), ui::colors::BG_SURFACE(),
                 ui::colors::BORDER_DIM())
            .arg(ui::colors::ACCENT_BG(), ui::colors::BG_HOVER(), ui::colors::BORDER_MED(),
                 ui::colors::TEXT_ON_ACCENT(), ui::colors::AMBER_DIM());
    if (styleSheet() != sheet)
        setStyleSheet(sheet);
}

void TemplatePicker::build_list() {
    const int keep = list_->currentRow();
    list_->blockSignals(true);
    list_->clear();
    const qreal dpr = devicePixelRatioF();
    for (const auto& t : templates_) {
        QString text = template_display_name_tr(t);
        if (t.id == current_id_)
            text += QStringLiteral("  ·  ") + tr("CURRENT");
        text += QStringLiteral("\n") + template_description_tr(t);
        auto* item = new QListWidgetItem(QIcon(tp_thumbnail(t.items, QSize(84, 56), dpr)), text, list_);
        item->setData(Qt::UserRole, t.id);
        item->setToolTip(template_description_tr(t));
        if (t.id == current_id_) {
            QFont f = item->font();
            f.setBold(true);
            item->setFont(f);
            item->setForeground(QColor(ui::colors::AMBER()));
        }
    }
    list_->blockSignals(false);
    if (keep >= 0)
        list_->setCurrentRow(keep);
}

void TemplatePicker::show_details(int row) {
    if (row < 0 || row >= templates_.size())
        return;
    const auto& t = templates_[row];
    name_lbl_->setText(t.id == current_id_ ? tr("%1  ·  current").arg(template_display_name_tr(t))
                                           : template_display_name_tr(t));
    desc_lbl_->setText(template_description_tr(t));
    preview_->set_items(t.items);

    QStringList meta;
    meta << tr("%n tile(s) · compact density · fills the width at every window size", "", int(t.items.size()));
    if (!t.scope.isEmpty())
        meta << template_scope_tr(t).toHtmlEscaped();
    else if (!t.region.isEmpty())
        meta << tr("Pinned to %1 — these tiles show %1 whatever your region selector says")
                    .arg(core::market::market_profile_name(core::market::market_profile(t.region)).toHtmlEscaped());
    else
        meta << tr("Follows your market region (%1)")
                    .arg(core::market::market_profile_name(core::market::MarketProfileStore::instance().current())
                             .toHtmlEscaped());
    const QString req = template_requirement_tr(t);
    if (!req.isEmpty())
        meta << QStringLiteral("<span style='color:%1'>%2</span>").arg(ui::colors::WARNING(), req.toHtmlEscaped());
    if (!current_pristine_)
        meta << QStringLiteral("<span style='color:%1'>%2</span>")
                    .arg(ui::colors::WARNING(),
                         tr("Your current layout has changes of its own — you will be asked to save it before it is "
                            "replaced.")
                             .toHtmlEscaped());
    meta_lbl_->setText(meta.join(QStringLiteral("<br>")));

    // Tiles in reading order (top to bottom, left to right) with their size.
    auto items = t.items;
    std::stable_sort(items.begin(), items.end(), [](const GridItem& a, const GridItem& b) {
        return a.cell.y != b.cell.y ? a.cell.y < b.cell.y : a.cell.x < b.cell.x;
    });
    QStringList tiles;
    for (const auto& it : items)
        tiles << QStringLiteral("%1 <span style='color:%2'>%3×%4</span>")
                     .arg(tp_tile_name(it.id).toHtmlEscaped(), ui::colors::TEXT_TERTIARY())
                     .arg(it.cell.w)
                     .arg(it.cell.h);
    tiles_lbl_->setText(QStringLiteral("<b>%1</b> ").arg(tr("Tiles:")) + tiles.join(QStringLiteral(" · ")));
    apply_btn_->setText(t.id == current_id_ ? tr("RE-APPLY TEMPLATE") : tr("APPLY TEMPLATE"));
}

void TemplatePicker::accept_current() {
    const int row = list_->currentRow();
    if (row < 0 || row >= templates_.size())
        return;
    emit template_selected(templates_[row].id);
    accept();
}

void TemplatePicker::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QDialog::changeEvent(event);
}

void TemplatePicker::retranslateUi() {
    setWindowTitle(tr("Dashboard Templates"));
    title_lbl_->setText(tr("DASHBOARD TEMPLATES"));
    sub_lbl_->setText(tr("Each template fills the dashboard in compact density, with every tile set up to show data "
                         "at once and a desk note you can edit. Applying one replaces the current layout — save it "
                         "first from LAYOUTS if you want to come back to it."));
    cancel_btn_->setText(tr("CANCEL"));
    build_list();
    show_details(list_->currentRow());
}

} // namespace fincept::screens
