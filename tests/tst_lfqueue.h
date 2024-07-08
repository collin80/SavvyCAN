#ifndef TST_LFQUEUE_H
#define TST_LFQUEUE_H

#include <QObject>

class TestLFQueue : public QObject
{
  Q_OBJECT
private:
private slots:
  void setSize_data();
  void setSize();
  void exchange_data();
  void exchange();
};

#endif  // TST_LFQUEUE_H
