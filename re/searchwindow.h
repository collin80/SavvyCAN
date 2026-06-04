#ifndef SEARCHWINDOW_H
#define SEARCHWINDOW_H

#include <QDialog>
#include "../can_structs.h"
#include "../dbc/dbchandler.h"

namespace Ui {
class SearchWindow;
}

class SearchWindow : public QDialog
{
    Q_OBJECT

public:
    explicit SearchWindow(const QVector<CommFrame> *frames, DBCHandler *handler, QWidget *parent = nullptr);
    ~SearchWindow();

    void updatedFrames(int numFrames);

signals:
    void jumpToFrameIndex(int frameIndex);

private slots:
    void loadNodes();
    void loadMessages(int idx);
    void loadSignals(int idx);
    void copySignalToParamsUI();
    void bitfieldClicked(int bit);
    void handleStartBitUpdate();
    void handleDataLenUpdate();
    void drawBitfield();
    void onSearchClicked();
    void onNextClicked();
    void onPreviousClicked();
    void onValueModeChanged();

private:
    void performSearch();
    double decodeValue(const CommFrame &frame) const;
    void updateNavigationState();

    Ui::SearchWindow *ui;
    const QVector<CommFrame> *modelFrames;
    DBCHandler *dbcHandler;
    DBC_SIGNAL *assocSignal;
    int startBit;
    int dataLen;

    QVector<int> matchingIndices;
    int currentMatchIndex;
};

#endif // SEARCHWINDOW_H
