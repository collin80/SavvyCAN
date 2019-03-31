#ifndef SNIFFERDELEGATE_H
#define SNIFFERDELEGATE_H

#include <QItemDelegate>

class SnifferDelegate : public QItemDelegate
{
    Q_OBJECT
public:
    explicit SnifferDelegate(QWidget *parent = nullptr);
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const;
    bool getFadeInactive();
    void setFadeInactive(bool val);

signals:

public slots:

private:
    QBrush blackBrush, whiteBrush, redBrush, greenBrush, grayBrush;
    QFont mainFont;
    QFontInfo* mainFontInfo;
    bool  mFadeInactive;
    bool  mDarkMode;
};

#endif
