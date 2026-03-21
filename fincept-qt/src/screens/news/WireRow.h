#pragma once
#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>
#include "services/news/NewsService.h"

namespace fincept::screens {

/// Single dense wire row — time | priority dot | source | headline | sentiment | tickers.
class WireRow : public QWidget {
    Q_OBJECT
public:
    explicit WireRow(const fincept::services::NewsArticle& article, QWidget* parent = nullptr);

    void set_selected(bool selected);
    QString article_id() const { return article_id_; }

signals:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void update_style();

    QString article_id_;
    fincept::services::Priority priority_;
    bool    is_hot_ = false;
    bool    selected_ = false;
    bool    hovered_ = false;

    QWidget* dot_ = nullptr;
};

} // namespace fincept::screens
