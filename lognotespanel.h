#ifndef LOGNOTESPANEL_H
#define LOGNOTESPANEL_H

#include <QDockWidget>
#include "lognotes.h"

class QLabel;
class QListWidget;
class QTextBrowser;

//Main window panel showing the notes of the loaded log (or of the capture, before it's saved), with its markers
class LogNotesPanel : public QDockWidget
{
    Q_OBJECT

public:
    explicit LogNotesPanel(QWidget *parent = nullptr);

    //source: where the notes are from, e.g. "run1.csv", or empty for notes not saved with a log yet
    void setNotes(const LogNotes &notes, const QString &source);
    //the notes as the panel shows them, apart from the markers (they're in a list of their own)
    static QString toHtml(const LogNotes &notes);
    static QString markerText(const LogMarker &marker);

signals:
    void editRequested();
    void addMarkerRequested();
    //a marker was double-clicked: go to that point of the log
    void markerActivated(qint64 timeUs);

private:
    QTextBrowser *browser;
    QLabel *lblMarkers;
    QListWidget *listMarkers;
    QLabel *lblSource;
};

#endif // LOGNOTESPANEL_H
