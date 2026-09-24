#ifndef TST_BLF_H
#define TST_BLF_H

#include <QObject>

class TestBLF : public QObject
{
    Q_OBJECT
private slots:
    void loadObjectBoundary();
    void loadSplitObject();
    void loadCanFd64();
};

#endif // TST_BLF_H
