#ifndef MARKERPRESETS_H
#define MARKERPRESETS_H

//Marker texts prepared before a test, each with a key of its own, so a marker goes into a running log with one key
//press and no typing (see docs/log-notes-roadmap.md section 8). Kept in the settings and in log notes templates, as
//Markdown:
//
//  ## Marker presets
//  Next key: Ctrl+N
//  Undo key: Ctrl+Shift+M
//  - [Ctrl+1] Ignition on
//  - AC on

#include <QKeySequence>
#include <QString>
#include <QStringList>
#include <QVector>

struct MarkerPreset
{
    QString text;
    QKeySequence key; //empty: no key (its button in the Log Notes panel still adds it)

    bool operator==(const MarkerPreset &other) const { return text == other.text && key == other.key; }
};

class MarkerPresets
{
public:
    MarkerPresets();

    QVector<MarkerPreset> presets;
    QKeySequence nextKey; //adds the presets in order, one per press; empty: none
    QKeySequence undoKey; //Undo Last Marker

    bool isEmpty() const { return presets.isEmpty(); }
    bool operator==(const MarkerPresets &other) const;

    //the preset the Next key adds now, or -1 once they've all been added
    int next() const { return nextIndex < presets.count() ? nextIndex : -1; }
    void advance() { if (nextIndex < presets.count()) nextIndex++; }
    void stepBack() { if (nextIndex > 0) nextIndex--; }
    void reset() { nextIndex = 0; }

    //what already has this key (a preset's text, "Next preset", "Undo Last Marker"), or empty; skip: a preset not to
    //count (the one being changed), -1 for none
    QString keyUser(const QKeySequence &key, int skip = -1, bool skipNext = false, bool skipUndo = false) const;

    QString toMarkdown() const;
    //false if the text has no "## Marker presets" section
    static bool fromMarkdown(const QString &markdown, MarkerPresets &presets);
    //the markdown without its "## Marker presets" section (for log notes, which don't keep presets)
    static QString withoutSection(const QString &markdown);

    //the steps of a Scenario: its numbered or bulleted lines (not the indented ones under them), without the number
    //or bullet; every non-empty line if it has neither
    static QStringList scenarioSteps(const QString &scenario);
    //adds them as presets, giving each the next of Ctrl+1 to Ctrl+9 nothing has yet
    void addScenarioSteps(const QString &scenario);

    //the presets in use, kept between sessions
    static MarkerPresets load();
    void save() const;

private:
    int nextIndex = 0;
};

#endif // MARKERPRESETS_H
