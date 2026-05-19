#include "screens/dashboard/canvas/TemplatePicker.h"

#include "screens/dashboard/canvas/DashboardTemplates.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <array>

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

namespace fincept::screens {

// Mini preview widget — paints scaled-down widget rectangles
class TemplatePreview : public QWidget {
  public:
    explicit TemplatePreview(const QVector<GridItem>& items, QWidget* parent = nullptr)
        : QWidget(parent), items_(items) {
        setFixedSize(120, 70);
    }

  protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.fillRect(rect(), QColor(ui::colors::BG_SURFACE()));

        const int cols = 12;
        const int rows = 14;
        const float cw = float(width()) / cols;
        const float rh = float(height()) / rows;

        static const auto make_colors = []() {
            const auto& t = ui::ThemeManager::instance().tokens();
            return std::array<QColor, 6>{
                QColor(t.accent),    QColor(t.positive), QColor(t.info),
                QColor(t.negative),  QColor(t.cyan),     QColor(t.warning),
            };
        };
        const auto colors = make_colors();
        // apply 160-180 alpha for preview readability
        auto tinted = [&](int i) {
            QColor c = colors[i % 6];
            c.setAlpha(i == 0 ? 180 : 160);
            return c;
        };
        int ci = 0;
        for (const auto& item : items_) {
            QRectF r(item.cell.x * cw + 1, item.cell.y * rh + 1, item.cell.w * cw - 2, item.cell.h * rh - 2);
            p.fillRect(r, tinted(ci++));
        }
    }

  private:
    QVector<GridItem> items_;
};

TemplatePicker::TemplatePicker(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Choose Dashboard Template"));
    setFixedSize(640, 400);
    setStyleSheet(QString("QDialog { background: %1; }"
                          "QPushButton { background: %2; border: 1px solid %3; color: %4; "
                          "  padding: 4px 12px; font-size: 10px; }"
                          "QPushButton:hover { border-color: %5; color: %5; }")
                      .arg(ui::colors::BG_BASE(), ui::colors::BG_SURFACE(), ui::colors::BORDER_MED(),
                           ui::colors::TEXT_PRIMARY(), ui::colors::AMBER()));

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    title_lbl_ = new QLabel(tr("CHOOSE TEMPLATE"));
    title_lbl_->setStyleSheet(
        QString("color: %1; font-size: 11px; font-weight: bold; letter-spacing: 1px;").arg(ui::colors::AMBER()));
    vl->addWidget(title_lbl_);

    sub_lbl_ = new QLabel(tr("Select a template to reset your dashboard. Current layout will be replaced."));
    sub_lbl_->setStyleSheet(QString("color: %1; font-size: 9px;").arg(ui::colors::TEXT_TERTIARY()));
    sub_lbl_->setWordWrap(true);
    vl->addWidget(sub_lbl_);

    auto* grid_w = new QWidget(this);
    auto* grid = new QGridLayout(grid_w);
    grid->setSpacing(8);
    grid->setContentsMargins(0, 0, 0, 0);

    const auto templates = all_dashboard_templates();
    int col = 0, row = 0;
    for (const auto& tmpl : templates) {
        auto* card = new QFrame;
        card->setStyleSheet(QString("QFrame { background: %1; border: 1px solid %2; } "
                                    "QFrame:hover { border-color: %3; }")
                                .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(), ui::colors::AMBER()));

        auto* cl = new QVBoxLayout(card);
        cl->setContentsMargins(10, 8, 10, 8);
        cl->setSpacing(4);

        auto* preview = new TemplatePreview(tmpl.items);
        cl->addWidget(preview, 0, Qt::AlignHCenter);

        auto* name_lbl = new QLabel(template_display_name_tr(tmpl));
        name_lbl->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: bold; background: transparent;")
                                    .arg(ui::colors::TEXT_PRIMARY()));
        cl->addWidget(name_lbl);

        auto* desc_lbl = new QLabel(template_description_tr(tmpl));
        desc_lbl->setStyleSheet(
            QString("color: %1; font-size: 9px; background: transparent;").arg(ui::colors::TEXT_SECONDARY()));
        desc_lbl->setWordWrap(true);
        cl->addWidget(desc_lbl);

        auto* apply_btn = new QPushButton(tr("APPLY"));
        apply_btn->setFixedHeight(24);
        const QString tid = tmpl.id;
        connect(apply_btn, &QPushButton::clicked, this, [this, tid]() {
            emit template_selected(tid);
            accept();
        });
        cl->addWidget(apply_btn);

        cards_.append({name_lbl, desc_lbl, apply_btn, tmpl.id});

        grid->addWidget(card, row, col);
        if (++col >= 3) {
            col = 0;
            ++row;
        }
    }
    vl->addWidget(grid_w, 1);

    auto* bot = new QHBoxLayout;
    bot->addStretch();
    cancel_btn_ = new QPushButton(tr("CANCEL"));
    cancel_btn_->setFixedHeight(28);
    connect(cancel_btn_, &QPushButton::clicked, this, &QDialog::reject);
    bot->addWidget(cancel_btn_);
    vl->addLayout(bot);
}

void TemplatePicker::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QDialog::changeEvent(event);
}

void TemplatePicker::retranslateUi() {
    setWindowTitle(tr("Choose Dashboard Template"));
    if (title_lbl_)  title_lbl_->setText(tr("CHOOSE TEMPLATE"));
    if (sub_lbl_)    sub_lbl_->setText(tr("Select a template to reset your dashboard. Current layout will be replaced."));
    if (cancel_btn_) cancel_btn_->setText(tr("CANCEL"));
    const auto templates = all_dashboard_templates();
    for (auto& c : cards_) {
        if (c.apply) c.apply->setText(tr("APPLY"));
        for (const auto& t : templates) {
            if (t.id == c.id) {
                if (c.name) c.name->setText(template_display_name_tr(t));
                if (c.desc) c.desc->setText(template_description_tr(t));
                break;
            }
        }
    }
}

} // namespace fincept::screens
