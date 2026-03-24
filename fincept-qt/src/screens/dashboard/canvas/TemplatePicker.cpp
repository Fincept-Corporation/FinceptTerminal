#include "screens/dashboard/canvas/TemplatePicker.h"

#include "screens/dashboard/canvas/DashboardTemplates.h"
#include "ui/theme/Theme.h"

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
        p.fillRect(rect(), QColor("#0a0a0a"));

        const int cols = 12;
        const int rows = 14;
        const float cw = float(width()) / cols;
        const float rh = float(height()) / rows;

        static const QColor colors[] = {
            QColor(217, 119, 6, 180), QColor(22, 163, 74, 160), QColor(37, 99, 235, 160),
            QColor(220, 38, 38, 160), QColor(8, 145, 178, 160), QColor(202, 138, 4, 160),
        };
        int ci = 0;
        for (const auto& item : items_) {
            QRectF r(item.cell.x * cw + 1, item.cell.y * rh + 1, item.cell.w * cw - 2, item.cell.h * rh - 2);
            p.fillRect(r, colors[ci++ % 6]);
        }
    }

  private:
    QVector<GridItem> items_;
};

TemplatePicker::TemplatePicker(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Choose Dashboard Template");
    setFixedSize(640, 400);
    setStyleSheet(QString("QDialog { background: %1; }"
                          "QPushButton { background: %2; border: 1px solid %3; color: %4; "
                          "  padding: 4px 12px; font-size: 10px; }"
                          "QPushButton:hover { border-color: %5; color: %5; }")
                      .arg(ui::colors::BG_BASE, ui::colors::BG_SURFACE, ui::colors::BORDER_MED,
                           ui::colors::TEXT_PRIMARY, ui::colors::AMBER));

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    auto* title = new QLabel("CHOOSE TEMPLATE");
    title->setStyleSheet(
        QString("color: %1; font-size: 11px; font-weight: bold; letter-spacing: 1px;").arg(ui::colors::AMBER));
    vl->addWidget(title);

    auto* sub = new QLabel("Select a template to reset your dashboard. Current layout will be replaced.");
    sub->setStyleSheet(QString("color: %1; font-size: 9px;").arg(ui::colors::TEXT_TERTIARY));
    sub->setWordWrap(true);
    vl->addWidget(sub);

    auto* grid_w = new QWidget;
    auto* grid = new QGridLayout(grid_w);
    grid->setSpacing(8);
    grid->setContentsMargins(0, 0, 0, 0);

    const auto templates = all_dashboard_templates();
    int col = 0, row = 0;
    for (const auto& tmpl : templates) {
        auto* card = new QFrame;
        card->setStyleSheet(QString("QFrame { background: %1; border: 1px solid %2; } "
                                    "QFrame:hover { border-color: %3; }")
                                .arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM, ui::colors::AMBER));

        auto* cl = new QVBoxLayout(card);
        cl->setContentsMargins(10, 8, 10, 8);
        cl->setSpacing(4);

        auto* preview = new TemplatePreview(tmpl.items);
        cl->addWidget(preview, 0, Qt::AlignHCenter);

        auto* name_lbl = new QLabel(tmpl.display_name);
        name_lbl->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: bold; background: transparent;")
                                    .arg(ui::colors::TEXT_PRIMARY));
        cl->addWidget(name_lbl);

        auto* desc_lbl = new QLabel(tmpl.description);
        desc_lbl->setStyleSheet(
            QString("color: %1; font-size: 9px; background: transparent;").arg(ui::colors::TEXT_SECONDARY));
        desc_lbl->setWordWrap(true);
        cl->addWidget(desc_lbl);

        auto* apply_btn = new QPushButton("APPLY");
        apply_btn->setFixedHeight(24);
        const QString tid = tmpl.id;
        connect(apply_btn, &QPushButton::clicked, this, [this, tid]() {
            emit template_selected(tid);
            accept();
        });
        cl->addWidget(apply_btn);

        grid->addWidget(card, row, col);
        if (++col >= 3) {
            col = 0;
            ++row;
        }
    }
    vl->addWidget(grid_w, 1);

    auto* bot = new QHBoxLayout;
    bot->addStretch();
    auto* cancel = new QPushButton("CANCEL");
    cancel->setFixedHeight(28);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    bot->addWidget(cancel);
    vl->addLayout(bot);
}

} // namespace fincept::screens
