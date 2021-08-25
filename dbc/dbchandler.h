#ifndef DBCHANDLER_H
#define DBCHANDLER_H

#include <QObject>
#include "dbc_classes.h"
#include "can_structs.h"

    typedef enum
    {
        EXACT,
        J1939,
        GMLAN
    } MatchingCriteria_t;

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
    void sort();

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
    DBC_MESSAGE *findMsgByPartialName(QString name);
    bool addMessage(DBC_MESSAGE &msg);
    bool removeMessage(DBC_MESSAGE *msg);
    bool removeMessageByIndex(int idx);
    bool removeMessage(uint32_t ID);
    bool removeMessage(QString name);
    void removeAllMessages();
    int getCount();
    MatchingCriteria_t getMatchingCriteria();
    void setMatchingCriteria(MatchingCriteria_t mc);
    void setFilterLabeling( bool labelFiltering );
    bool filterLabeling();
    void sort();

private:
    QList<DBC_MESSAGE> messages;
    MatchingCriteria_t matchingCriteria;
    bool filterLabelingEnabled;
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
    DBC_ATTRIBUTE *findAttributeByName(QString name, DBC_ATTRIBUTE_TYPE type = ATTR_TYPE_ANY);
    DBC_ATTRIBUTE *findAttributeByIdx(int idx);
    void findAttributesByType(DBC_ATTRIBUTE_TYPE typ, QList<DBC_ATTRIBUTE> *list);
    bool saveFile(QString);
    bool loadFile(QString);
    QString getFullFilename();
    QString getFilename();
    QString getPath();
    int getAssocBus();
    void setAssocBus(int bus);
    void setDirtyFlag();
    bool getDirtyFlag();
    void sort();

    DBCMessageHandler *messageHandler;
    QList<DBC_NODE> dbc_nodes;
    QList<DBC_ATTRIBUTE> dbc_attributes;
private:
    QString fileName;
    QString filePath;
    int assocBuses; //-1 = all buses, 0 = first bus, 1 = second bus, etc.
    bool isDirty; //has the file been modified?

    bool parseAttribute(QString inpString, DBC_ATTRIBUTE &attr);
    QVariant processAttributeVal(QString input, DBC_ATTRIBUTE_VAL_TYPE typ);
    DBC_SIGNAL* parseSignalLine(QString line, DBC_MESSAGE *msg);
    bool parseSignalMultiplexValueLine(QString line);
    DBC_MESSAGE* parseMessageLine(QString line);
    bool parseValueLine(QString line);
    bool parseAttributeLine(QString line);
    bool parseDefaultAttrLine(QString line);
};

class DBCHandler: public QObject
{
    Q_OBJECT
public:
    DBCFile* loadDBCFile(QString filename);
    DBCFile* loadDBCFile(int);
    void saveDBCFile(int);
    void removeDBCFile(int);
    void removeAllFiles();
    void swapFiles(int pos1, int pos2);
    DBC_MESSAGE* findMessage(const CANFrame &frame);
    DBC_MESSAGE* findMessage(const QString msgName);
    DBC_MESSAGE* findMessage(uint32_t id);
    DBC_MESSAGE* findMessageForFilter(uint32_t id, MatchingCriteria_t * matchingCriteria);
    int getFileCount();
    DBCFile* getFileByIdx(int idx);
    DBCFile* getFileByName(QString name);
    int createBlankFile();
    DBCFile* loadJSONFile(QString);
    DBCFile* loadSecretCSVFile(QString);
    static DBCHandler *getReference();

private:
    QList<DBCFile> loadedFiles;

    DBCHandler();
    static DBCHandler *instance;
};

#endif // DBCHANDLER_H
