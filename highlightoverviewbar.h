#ifndef HIGHLIGHTOVERVIEWBAR_H
#define HIGHLIGHTOVERVIEWBAR_H

#include <QWidget>
#include "canframemodel.h"

class QTableView;
class QScrollBar;

class HighlightOverviewBar : public QWidget
{
    Q_OBJECT

public:
    explicit HighlightOverviewBar(QWidget *parent = nullptr);

    void setup(CommFrameModel *model, QTableView *tableView);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    CommFrameModel *m_model = nullptr;
    QScrollBar *m_scrollBar = nullptr;
};

#endif // HIGHLIGHTOVERVIEWBAR_H
