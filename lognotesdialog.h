#ifndef LOGNOTESDIALOG_H
#define LOGNOTESDIALOG_H

#include <QDialog>
#include "lognotes.h"
#include "markerpresets.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

//Asks for a log's notes (see lognotes.h): when a log is saved (Saving: Save with Notes, Save without Notes or Cancel),
//from File > Log Notes... (Editing: OK or Cancel), when capture starts (Starting: Keep Notes or Later) and when
//continuous logging stops (Finishing: Save with Notes or Save without Notes; the log is already written). The fields
//SavvyCAN fills in can be edited too. Templates, kept in the settings, fill in the typed fields, e.g. one per bench.
class LogNotesDialog : public QDialog
{
    Q_OBJECT

public:
    enum Mode { Saving, Editing, Starting, Finishing };
    enum Choice { Cancelled, WithNotes, WithoutNotes };

    explicit LogNotesDialog(Mode mode, QWidget *parent = nullptr);

    void setNotes(const LogNotes &notes);
    LogNotes notes() const;
    //Saving: the log's file name, and whether the notes go inside it (GVRET CSV) or in a .notes.md file next to it.
    //Editing: the log the notes are saved back to, or an empty name if they're only kept for the next save.
    //Starting: the continuous log being written, or an empty name. Finishing: the continuous log just written.
    void setTarget(const QString &fileName, bool insideLog);
    Choice choice() const { return chosen; }

    //settings
    static bool askOnSave();
    static void setAskOnSave(bool ask);
    static bool askOnCapture();
    static void setAskOnCapture(bool ask);
    //the typed fields of the last notes saved, to start the next ones from
    static LogNotes lastNotes();
    static void setLastNotes(const LogNotes &notes);
    static QStringList templateNames();
    //A template keeps the typed fields and, if there were any, the marker presets (with their keys).
    //hasPresets: whether the template has presets (presets is only set if it has)
    static bool loadTemplate(const QString &name, LogNotes &notes, MarkerPresets *presets = nullptr,
                             bool *hasPresets = nullptr);
    static void saveTemplate(const QString &name, const LogNotes &notes, const MarkerPresets &presets = MarkerPresets());
    static void deleteTemplate(const QString &name);

private:
    Mode mode;
    Choice chosen;
    LogNotes base; //what was set, keeping the fields the dialog doesn't show
    QLabel *lblTarget;
    QComboBox *cbTemplates;
    QPushButton *btnUseTemplate;
    QPushButton *btnDeleteTemplate;
    QLineEdit *editTitle;
    QLineEdit *editAuthor;
    QLineEdit *editTags;
    QPlainTextEdit *editSystem;
    QPlainTextEdit *editSetup;
    QPlainTextEdit *editScenario;
    QPlainTextEdit *editObservations;
    QPlainTextEdit *editBuses;
    QPlainTextEdit *editDbcFiles;
    QPlainTextEdit *editMarkers;
    QLabel *lblFilledIn;
    QCheckBox *cbAsk; //"ask when saving" (Saving, Finishing) or "ask when capture starts" (Starting)

    QPlainTextEdit *textBox(int lines, const QString &placeholder);
    void showUserFields(const LogNotes &notes);
    void refreshTemplates(const QString &select = QString());
    void useTemplate();
    void saveAsTemplate();
    void removeTemplate();
    void finish(Choice choice);
};

#endif // LOGNOTESDIALOG_H
