#ifndef ISOTP_INTERPRETERWINDOW_H
#define ISOTP_INTERPRETERWINDOW_H

#include <QDialog>
#include <QListWidgetItem>
#include "bus_protocols/isotp_handler.h"
#include "bus_protocols/uds_handler.h"

class ISOTP_MESSAGE;
class ISOTP_HANDLER;

namespace Ui {
class ISOTP_InterpreterWindow;
}


class ISOTP_InterpreterModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Column {
        Time,
        ID,
        Bus,
        Dir,
        Len,
        Data,
    };

    explicit ISOTP_InterpreterModel(QObject * parent = nullptr)
        : QAbstractTableModel(parent)
    {}

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    ISOTP_MESSAGE getMessage(int idx) const;
    QString getMessageVerbose(int idx) const;

public slots:
    void clear();
    void addMessage(ISOTP_MESSAGE msg);

private:
    QVector<ISOTP_MESSAGE> messages;

    static QString getDataString(const ISOTP_MESSAGE & msg);
};


class ISOTP_InterpreterWindow : public QDialog
{
    Q_OBJECT

public:
    explicit ISOTP_InterpreterWindow(const QVector<CANFrame> *frames, QWidget *parent = 0);
    ~ISOTP_InterpreterWindow();
    void showEvent(QShowEvent*);

private slots:
    void newISOMessage(ISOTP_MESSAGE msg);
    void newUDSMessage(UDS_MESSAGE msg);
    void showDetailView();
    void updatedFrames(int);
    void clearList();
    void saveList();
    void listFilterItemChanged(QListWidgetItem *item);
    void filterAll();
    void filterNone();
    void interpretCapturedFrames();
    void useExtendedAddressing(bool checked);

private:
    Ui::ISOTP_InterpreterWindow *ui;
    ISOTP_HANDLER *decoder;
    UDS_HANDLER *udsDecoder;

    ISOTP_InterpreterModel msgModel;
    const QVector<CANFrame> *rawFrames;
    QHash<int, bool> idFilters;

    void closeEvent(QCloseEvent *event);
    bool eventFilter(QObject *obj, QEvent *event);
    void readSettings();
    void writeSettings();

};

#endif // ISOTP_INTERPRETERWINDOW_H
