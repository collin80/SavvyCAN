#ifndef SCRIPTCONTAINER_H
#define SCRIPTCONTAINER_H

#include <QJSEngine>
#include <QTimer>
#include <qlistwidget.h>

class ScriptContainer : public QObject
{
    Q_OBJECT

public:
    ScriptContainer();
    QString fileName;
    QString filePath;
    QString scriptText;

public slots:
    void compileScript();
    void setErrorWidget(QListWidget *list);
    void setFilter(QJSValue id, QJSValue mask, QJSValue bus);
    void setTickInterval(QJSValue interval);
    void clearFilters();
    void sendFrame(QJSValue id, QJSValue length, QJSValue data);

private slots:
    void tick();

private:
    QJSEngine scriptEngine;
    QJSValue compiledScript;
    QJSValue setupFunction;
    QJSValue gotFrameFunction;
    QJSValue tickFunction;
    QTimer timer;
    QListWidget *errorWidget;
};

#endif // SCRIPTCONTAINER_H
