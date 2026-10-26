#ifndef LOGNOTESPANEL_H
#define LOGNOTESPANEL_H

#include <QDockWidget>
#include "lognotes.h"

class QLabel;
class QTextBrowser;

//Main window panel showing the notes of the loaded log (or of the capture, before it's saved)
class LogNotesPanel : public QDockWidget
{
    Q_OBJECT

public:
    explicit LogNotesPanel(QWidget *parent = nullptr);

    //source: where the notes are from, e.g. "run1.csv", or empty for notes not saved with a log yet
    void setNotes(const LogNotes &notes, const QString &source);
    //the notes as the panel shows them
    static QString toHtml(const LogNotes &notes);

signals:
    void editRequested();

private:
    QTextBrowser *browser;
    QLabel *lblSource;
};

#endif // LOGNOTESPANEL_H
