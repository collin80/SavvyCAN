#ifndef LOGNOTES_H
#define LOGNOTES_H

//Notes saved with a log: what it's for, how it was captured and what was seen, so the log file needs no separate
//engineering notes. They're Markdown (see docs/log-notes-roadmap.md), written at the end of a native CSV log with
//each line starting with "# ", or into a <log file>.notes.md file next to a log of any other format.

#include <QDateTime>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>
#include "can_structs.h"

//A point in a log the user marked ("AC on"), on the same clock as the frames' time stamps
struct LogMarker
{
    qint64 timeUs = 0;
    QString text;

    bool operator==(const LogMarker &other) const { return timeUs == other.timeUs && text == other.text; }
};

class LogNotes
{
public:
    //typed by the user
    QString title;
    QString author;
    QStringList tags;
    QString system;       //vehicle or system under test
    QString setup;        //how the logger was connected
    QString scenario;     //steps or flows the log should capture
    QString observations; //what was seen, known issues
    QVector<LogMarker> markers;

    //filled in by SavvyCAN when saving, editable
    QDateTime saved;
    QString savvycanVersion;
    QString logFile;          //file name the notes were saved with
    qint64 frameCount = -1;   //-1: not known
    qint64 unfilteredCount = -1; //frames before filtering, or -1 if the whole list was saved
    bool hasSpan = false;     //time span of the frames (time stamps can be negative, so no -1 for "not known")
    qint64 firstUs = 0;
    qint64 lastUs = 0;
    QStringList buses;        //one line per bus: interface, port, bitrate...
    QStringList dbcFiles;     //one line per DBC or LDF file, with its bus

    //header keys and sections this version doesn't know, kept so saving again doesn't drop them
    QList<QPair<QString, QString>> otherFields;
    QList<QPair<QString, QString>> otherSections;

    bool isEmpty() const;
    bool operator==(const LogNotes &other) const;

    //The fields the user types (title to observations, and the keys and sections this version doesn't know), as kept
    //for the next log and in templates: no markers and none of the fields SavvyCAN fills in
    LogNotes userFields() const;
    void setUserFields(const LogNotes &from);
    bool hasUserFields() const { return !userFields().isEmpty(); }
    //adds a marker, keeping them in time order (after any others at the same time)
    void addMarker(const LogMarker &marker);
    //frame count and time span of the frames being saved; unfilteredCount is the count before filtering, or -1
    void setFrames(const QVector<CommFrame> &frames, qint64 unfilteredCount = -1);

    QString toMarkdown() const;
    //false if the text has nothing that could be notes
    static bool fromMarkdown(const QString &markdown, LogNotes &notes);

    //<log file>.notes.md, keeping the log's extension so run1.csv and run1.blf don't share notes
    static QString sidecarPath(const QString &logPath);
    bool writeSidecar(const QString &logPath) const;
    static bool readSidecar(const QString &logPath, LogNotes &notes);
    static bool removeSidecar(const QString &logPath);

    //Replace the notes block at the end of a native CSV log with these notes (or just remove it if they're empty).
    //Only the block is rewritten; the frames are left as they are.
    bool writeToNativeCSV(const QString &logPath) const;
    //Read the notes block at the end of a native CSV log, without reading the frames again
    static bool readFromNativeCSV(const QString &logPath, LogNotes &notes);
    //A line of the notes block (the native CSV loader skips these)
    static bool isNotesLine(const QByteArray &line) { return line.startsWith('#'); }

    //seconds with six decimals ("12.400000"), and back
    static QString formatSeconds(qint64 us);
    static bool parseSeconds(const QString &text, qint64 &us);

private:
    struct BlockLine
    {
        qint64 offset;
        QByteArray text;
    };
    static bool readNativeCSVBlock(const QString &logPath, QList<BlockLine> &lines);
};

#endif // LOGNOTES_H
