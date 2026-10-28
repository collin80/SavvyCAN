#ifndef MARKERPRESETSDIALOG_H
#define MARKERPRESETSDIALOG_H

#include <QDialog>
#include "markerpresets.h"

class QKeySequenceEdit;
class QLabel;
class QPushButton;
class QTableWidget;

//Sets up the marker presets: their texts, in order, and a key for each, plus the Next preset and Undo Last Marker
//keys. A key something already uses is refused (see ShortcutCheck), one other programs often use is warned about.
class MarkerPresetsDialog : public QDialog
{
    Q_OBJECT

public:
    //scenario: the current notes' Scenario, for From Scenario (empty: the button is off)
    MarkerPresetsDialog(const MarkerPresets &presets, const QString &scenario, QWidget *parent = nullptr);

    MarkerPresets presets() const;
    //what the last key check said (empty when the key was free), for the tests
    QString lastKeyMessage() const;

    //sets a row's key (or the Next/Undo key: row -1/-2) as if it was pressed in its box; false if it was refused
    bool setKey(int row, const QKeySequence &key);

private:
    MarkerPresets edited;
    QString scenario;
    QTableWidget *table;
    QKeySequenceEdit *editNext;
    QKeySequenceEdit *editUndo;
    QLabel *lblMessage;
    QPushButton *btnRemove;
    QPushButton *btnUp;
    QPushButton *btnDown;
    bool filling = false;

    void fillTable(int select = -1);
    void readTexts();
    void keyChanged(int row, QKeySequenceEdit *edit);
    void addPreset();
    void removePreset();
    void movePreset(int by);
    void addFromScenario();
    void updateButtons();
};

#endif // MARKERPRESETSDIALOG_H
