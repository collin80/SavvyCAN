#ifndef DBCHANDLER_H
#define DBCHANDLER_H

#include <QObject>
#include "dbc_classes.h"
#include "can_structs.h"

/*
 * TODO:
 * Finish coding up the decoupled design
 *
*/


class DBCSignalHandler: public QObject
{
    Q_OBJECT
public:
    DBC_SIGNAL *findSignalByName(QString name);
    DBC_SIGNAL *findSignalByIdx(int idx);
    bool addSignal(DBC_SIGNAL &sig);
    bool removeSignal(DBC_SIGNAL *sig);
    bool removeSignal(int idx);
    bool removeSignal(QString name);
    void removeAllSignals();
    int getCount();
private:
    QList<DBC_SIGNAL> sigs; //signals is a reserved word or I'd have used that
};

class DBCMessageHandler: public QObject
{
    Q_OBJECT
public:
    DBC_MESSAGE *findMsgByID(uint32_t id);
    DBC_MESSAGE *findMsgByIdx(int idx);
    DBC_MESSAGE *findMsgByName(QString name);
    bool addMessage(DBC_MESSAGE &msg);
    bool removeMessage(DBC_MESSAGE *msg);
    bool removeMessageByIndex(int idx);
    bool removeMessage(uint32_t ID);
    bool removeMessage(QString name);
    void removeAllMessages();
    int getCount();
private:
    QList<DBC_MESSAGE> messages;
};

//technically there should be a node handler too but I'm sort of treating nodes as second class
//citizens since they aren't really all that important (to me anyway)
class DBCFile: public QObject
{
    Q_OBJECT
public:
    DBCFile();
    DBCFile(const DBCFile& cpy);
    DBCFile& operator=(const DBCFile& cpy);
    DBC_NODE *findNodeByName(QString name);
    DBC_NODE *findNodeByIdx(int idx);
    void saveFile(QString);
    void loadFile(QString);
    QString getFullFilename();
    QString getFilename();
    QString getPath();
    int getAssocBus();
    void setAssocBus(int bus);

    DBCMessageHandler *messageHandler;
    QList<DBC_NODE> dbc_nodes;
private:
    QString fileName;
    QString filePath;
    int assocBuses; //-1 = all buses, 0 = first bus, 1 = second bus
};

class DBCHandler: public QObject
{
    Q_OBJECT
public:
    DBCFile* loadDBCFile(int);
    void saveDBCFile(int);
    void removeDBCFile(int);
    void removeAllFiles();
    void swapFiles(int pos1, int pos2);
    DBC_MESSAGE* findMessage(const CANFrame &frame);
    int getFileCount();
    DBCFile* getFileByIdx(int idx);
    DBCFile* getFileByName(QString name);
    int createBlankFile();

private:
    QList<DBCFile> loadedFiles;
};

#endif // DBCHANDLER_H
