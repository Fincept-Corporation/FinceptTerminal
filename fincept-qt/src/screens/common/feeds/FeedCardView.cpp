#include "screens/common/feeds/FeedCardView.h"

#include "ui/theme/Theme.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QFrame>
#include <QLabel>
#include <QMouseEvent>
#include <QScrollArea>
#include <QUrl>
#include <QVBoxLayout>

#include <utility>

namespace fincept::feeds {

namespace {

QString relative_time(qint64 ts) {
    if (ts == 0)
        return {};
    const qint64 d = QDateTime::currentSecsSinceEpoch() - ts;
    if (d < 60)
        return QStringLiteral("just now");
    if (d < 3600)
        return QString("%1m ago").arg(d / 60);
    if (d < 86400)
        return QString("%1h ago").arg(d / 3600);
    return QString("%1d ago").arg(d / 86400);
}

/// A QFrame that opens a URL on left-click.
class ClickableCard : public QFrame {
  public:
    ClickableCard(QString url, QWidget* parent) : QFrame(parent), url_(std::move(url)) {
        setCursor(url_.isEmpty() ? Qt::ArrowCursor : Qt::PointingHandCursor);
    }

  protected:
    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton && !url_.isEmpty())
            QDesktopServices::openUrl(QUrl(url_));
        QFrame::mousePressEvent(e);
    }

  private:
    QString url_;
};

} // namespace

FeedCardView::FeedCardView(QWidget* parent) : QWidget(parent) {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    scroll_ = new QScrollArea(this);
    scroll_->setWidgetResizable(true);
    scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_->setFrameShape(QFrame::NoFrame);
    auto* host = new QWidget;
    list_ = new QVBoxLayout(host);
    list_->setContentsMargins(6, 6, 6, 6);
    list_->setSpacing(6);
    list_->addStretch();
    scroll_->setWidget(host);
    outer->addWidget(scroll_);
}

QWidget* FeedCardView::build_card(const FeedItem& it) {
    using namespace fincept::ui;
    auto* card = new ClickableCard(it.link, nullptr);
    card->setObjectName("feedCard");
    card->setStyleSheet(QString("#feedCard{background:%1;border:1px solid %2;border-radius:4px;}"
                                "#feedCard:hover{background:%3;}")
                            .arg(colors::BG_SURFACE(), colors::BORDER_MED(), colors::BG_HOVER()));
    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(8, 6, 8, 6);
    v->setSpacing(3);

    const QString rel = relative_time(it.sort_ts);
    const QString when = rel.isEmpty() ? it.time : rel; // fall back to the absolute string
    auto* meta = new QLabel(when.isEmpty() ? QString("● %1").arg(it.source)
                                           : QString("● %1 · %2").arg(it.source, when));
    meta->setStyleSheet(QString("color:%1;font-size:10px;font-weight:700;").arg(colors::TEXT_TERTIARY()));
    if (!it.time.isEmpty())
        meta->setToolTip(it.time); // absolute parsed timestamp on hover
    v->addWidget(meta);

    auto* title = new QLabel(it.title);
    title->setWordWrap(true);
    title->setStyleSheet(QString("color:%1;font-size:12px;font-weight:600;").arg(colors::TEXT_PRIMARY()));
    v->addWidget(title);

    if (!it.summary.isEmpty()) {
        auto* sum = new QLabel(it.summary);
        sum->setWordWrap(true);
        sum->setStyleSheet(QString("color:%1;font-size:11px;").arg(colors::TEXT_SECONDARY()));
        v->addWidget(sum);
    }
    return card;
}

void FeedCardView::set_items(const QVector<FeedItem>& items) {
    // Clear existing cards (keep the trailing stretch at the end).
    while (list_->count() > 1) {
        QLayoutItem* item = list_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
    int idx = 0;
    for (const auto& it : items)
        list_->insertWidget(idx++, build_card(it));
}

} // namespace fincept::feeds
