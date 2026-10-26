#include "markerpresets.h"

#include <QCoreApplication>
#include <QRegularExpression>
#include <QSettings>

static const QString sectionHeading = QStringLiteral("## Marker presets");
static const char *settingsKey = "LogNotes/MarkerPresets";

MarkerPresets::MarkerPresets()
    : undoKey(QKeyCombination(Qt::ControlModifier | Qt::ShiftModifier, Qt::Key_M))
{
}

bool MarkerPresets::operator==(const MarkerPresets &other) const
{
    return presets == other.presets && nextKey == other.nextKey && undoKey == other.undoKey;
}

QString MarkerPresets::keyUser(const QKeySequence &key, int skip, bool skipNext, bool skipUndo) const
{
    if (key.isEmpty()) return QString();
    for (int i = 0; i < presets.count(); i++)
        if (i != skip && presets[i].key == key)
            return QCoreApplication::translate("MarkerPresets", "the preset \"%1\"").arg(presets[i].text);
    if (!skipNext && nextKey == key) return QCoreApplication::translate("MarkerPresets", "Next preset");
    if (!skipUndo && undoKey == key) return QCoreApplication::translate("MarkerPresets", "Undo Last Marker");
    return QString();
}

static QString keyText(const QKeySequence &key)
{
    return key.isEmpty() ? QStringLiteral("(none)") : key.toString(QKeySequence::PortableText);
}

static QKeySequence keyFrom(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty() || trimmed == "(none)") return QKeySequence();
    return QKeySequence::fromString(trimmed, QKeySequence::PortableText);
}

QString MarkerPresets::toMarkdown() const
{
    QStringList lines;
    lines.append(sectionHeading);
    lines.append("Next key: " + keyText(nextKey));
    lines.append("Undo key: " + keyText(undoKey));
    for (const MarkerPreset &preset : presets)
    {
        const QString text = QString(preset.text).replace('\n', ' ').trimmed();
        if (preset.key.isEmpty()) lines.append("- " + text);
        else lines.append(QString("- [%1] %2").arg(keyText(preset.key), text));
    }
    return lines.join('\n') + '\n';
}

bool MarkerPresets::fromMarkdown(const QString &markdown, MarkerPresets &presets)
{
    static const QRegularExpression keyedRe(QStringLiteral("^-\\s+\\[([^\\]]*)\\]\\s*(.*)$"));
    MarkerPresets result;
    result.undoKey = QKeySequence();
    bool inSection = false, found = false;
    QString text = markdown;
    text.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    for (const QString &rawLine : text.split('\n'))
    {
        const QString line = rawLine.trimmed();
        if (rawLine.startsWith(QLatin1String("## ")))
        {
            inSection = rawLine.trimmed().compare(sectionHeading, Qt::CaseInsensitive) == 0;
            found = found || inSection;
            continue;
        }
        if (!inSection || line.isEmpty()) continue;
        if (line.startsWith(QLatin1String("Next key:"), Qt::CaseInsensitive)) result.nextKey = keyFrom(line.mid(9));
        else if (line.startsWith(QLatin1String("Undo key:"), Qt::CaseInsensitive)) result.undoKey = keyFrom(line.mid(9));
        else if (line.startsWith(QLatin1String("- ")) || line.startsWith(QLatin1String("* ")))
        {
            MarkerPreset preset;
            const QRegularExpressionMatch match = keyedRe.match(line);
            if (match.hasMatch())
            {
                preset.key = keyFrom(match.captured(1));
                preset.text = match.captured(2).trimmed();
            }
            else preset.text = line.mid(2).trimmed();
            if (!preset.text.isEmpty()) result.presets.append(preset);
        }
    }
    if (found) presets = result;
    return found;
}

QString MarkerPresets::withoutSection(const QString &markdown)
{
    QStringList kept;
    bool inSection = false;
    QString text = markdown;
    text.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    for (const QString &line : text.split('\n'))
    {
        if (line.startsWith(QLatin1String("## ")))
            inSection = line.trimmed().compare(sectionHeading, Qt::CaseInsensitive) == 0;
        if (!inSection) kept.append(line);
    }
    return kept.join('\n');
}

QStringList MarkerPresets::scenarioSteps(const QString &scenario)
{
    static const QRegularExpression stepRe(QStringLiteral("^(?:\\d+[.)]|[-*•])\\s+(.+)$"));
    QStringList steps, plain;
    for (const QString &line : scenario.split('\n'))
    {
        if (line.trimmed().isEmpty()) continue;
        plain.append(line.trimmed());
        if (line.at(0).isSpace()) continue; //a detail under a step
        const QRegularExpressionMatch match = stepRe.match(line.trimmed());
        if (match.hasMatch()) steps.append(match.captured(1).trimmed());
    }
    return steps.isEmpty() ? plain : steps;
}

void MarkerPresets::addScenarioSteps(const QString &scenario)
{
    int digit = 1;
    for (const QString &step : scenarioSteps(scenario))
    {
        MarkerPreset preset;
        preset.text = step;
        while (digit <= 9)
        {
            const QKeySequence key(QKeyCombination(Qt::ControlModifier, Qt::Key(Qt::Key_0 + digit)));
            digit++;
            if (keyUser(key).isEmpty())
            {
                preset.key = key;
                break;
            }
        }
        presets.append(preset);
    }
}

MarkerPresets MarkerPresets::load()
{
    MarkerPresets presets;
    const QString saved = QSettings().value(settingsKey).toString();
    if (!saved.isEmpty()) fromMarkdown(saved, presets);
    return presets;
}

void MarkerPresets::save() const
{
    QSettings().setValue(settingsKey, toMarkdown());
}
