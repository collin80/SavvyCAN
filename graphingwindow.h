#ifndef GRAPHINGWINDOW_H
#define GRAPHINGWINDOW_H

#include <QDialog>

namespace Ui {
class GraphingWindow;
}

class GraphingWindow : public QDialog
{
    Q_OBJECT

public:
    explicit GraphingWindow(QWidget *parent = 0);
    ~GraphingWindow();

private:
    Ui::GraphingWindow *ui;
};

#endif // GRAPHINGWINDOW_H
