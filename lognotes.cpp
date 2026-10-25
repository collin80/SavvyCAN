#include "lognotes.h"

#include <QFile>
#include <QRegularExpression>

//First line of the notes. The version is for later formats; this one reads any version as best it can.
static const QString notesHeading = QStringLiteral("# SavvyCAN log notes v1");
static const QString notesHeadingStart = QStringLiteral("# SavvyCAN log notes");

//a header value is one line
static QString oneLine(const QString &text)
{
    QString line = text;
    line.replace(QLatin1String("\r\n"), QLatin1String(" ")).replace('\n', ' ').replace('\r', ' ');
    return line.trimmed();
}

//drop the blank lines at the start and end of a section, keeping the indentation of the others
static QString trimBlankLines(const QString &text)
{
    QStringList lines = text.split('\n');
    while (!lines.isEmpty() && lines.first().trimmed().isEmpty()) lines.removeFirst();
    while (!lines.isEmpty() && lines.last().trimmed().isEmpty()) lines.removeLast();
    return lines.join('\n');
}

//"- item" lines of a list section; lines without the dash are taken as items too
static QStringList listItems(const QString &text)
{
    QStringList items;
    for (const QString &line : text.split('\n'))
    {
        QString item = line.trimmed();
        if (item.startsWith(QLatin1String("- ")) || item.startsWith(QLatin1String("* "))) item = item.mid(2).trimmed();
        if (!item.isEmpty()) items.append(item);
    }
    return items;
}

bool LogNotes::isEmpty() const
{
    return title.isEmpty() && author.isEmpty() && tags.isEmpty() && system.isEmpty() && setup.isEmpty()
        && scenario.isEmpty() && observations.isEmpty() && markers.isEmpty() && !saved.isValid()
        && savvycanVersion.isEmpty() && logFile.isEmpty() && frameCount < 0 && unfilteredCount < 0 && !hasSpan
        && buses.isEmpty() && dbcFiles.isEmpty() && otherFields.isEmpty() && otherSections.isEmpty();
}

bool LogNotes::operator==(const LogNotes &other) const
{
    return title == other.title && author == other.author && tags == other.tags && system == other.system
        && setup == other.setup && scenario == other.scenario && observations == other.observations
        && markers == other.markers && saved == other.saved && savvycanVersion == other.savvycanVersion
        && logFile == other.logFile && frameCount == other.frameCount && unfilteredCount == other.unfilteredCount
        && hasSpan == other.hasSpan && (!hasSpan || (firstUs == other.firstUs && lastUs == other.lastUs))
        && buses == other.buses && dbcFiles == other.dbcFiles && otherFields == other.otherFields
        && otherSections == other.otherSections;
}

QString LogNotes::formatSeconds(qint64 us)
{
    const quint64 magnitude = us < 0 ? quint64(-(us + 1)) + 1 : quint64(us);
    return QString("%1%2.%3").arg(us < 0 ? "-" : "").arg(magnitude / 1000000).arg(magnitude % 1000000, 6, 10, QChar('0'));
}

bool LogNotes::parseSeconds(const QString &text, qint64 &us)
{
    static const QRegularExpression re(QStringLiteral("^(-?)(\\d{1,12})(?:\\.(\\d+))?$"));
    const QRegularExpressionMatch match = re.match(text.trimmed());
    if (!match.hasMatch()) return false;
    //the fraction as microseconds, without going through a double
    const QString fraction = (match.captured(3) + QStringLiteral("000000")).left(6);
    us = match.captured(2).toLongLong() * 1000000 + fraction.toLongLong();
    if (!match.captured(1).isEmpty()) us = -us;
    return true;
}

QString LogNotes::toMarkdown() const
{
    QStringList out;
    out.append(notesHeading);

    auto field = [&out](const QString &key, const QString &value)
    {
        const QString line = oneLine(value);
        if (!line.isEmpty()) out.append(key + ": " + line);
    };
    field("title", title);
    field("author", author);
    field("tags", tags.join(", "));
    if (saved.isValid()) field("saved", saved.toOffsetFromUtc(saved.offsetFromUtc()).toString(Qt::ISODate));
    field("savvycan", savvycanVersion);
    field("log", logFile);
    if (frameCount >= 0)
    {
        if (unfilteredCount >= 0) field("frames", QString("%1 (filtered from %2)").arg(frameCount).arg(unfilteredCount));
        else field("frames", QString::number(frameCount));
    }
    if (hasSpan) field("span", formatSeconds(firstUs) + " s to " + formatSeconds(lastUs) + " s");
    for (const auto &other : otherFields) field(other.first, other.second);

    auto section = [&out](const QString &heading, const QString &text)
    {
        const QString body = trimBlankLines(text);
        if (body.isEmpty()) return;
        out.append(QString());
        out.append("## " + heading);
        out.append(body);
    };
    auto listSection = [&section](const QString &heading, const QStringList &items)
    {
        QStringList lines;
        for (const QString &item : items) lines.append("- " + oneLine(item));
        section(heading, lines.join('\n'));
    };
    section("System", system);
    section("Setup", setup);
    section("Scenario", scenario);
    section("Observations", observations);
    for (const auto &other : otherSections) section(other.first, other.second);
    listSection("Buses", buses);
    listSection("DBC files", dbcFiles);
    QStringList markerLines;
    for (const LogMarker &marker : markers)
        markerLines.append(QString("- %1 s: %2").arg(formatSeconds(marker.timeUs), oneLine(marker.text)).trimmed());
    section("Markers", markerLines.join('\n'));

    return out.join('\n') + '\n';
}

bool LogNotes::fromMarkdown(const QString &markdown, LogNotes &notes)
{
    static const QRegularExpression fieldRe(QStringLiteral("^([A-Za-z][A-Za-z0-9 _-]*):\\s*(.*)$"));
    static const QRegularExpression framesRe(QStringLiteral("^(\\d+)(?:\\s*\\(filtered from (\\d+)\\))?$"));
    static const QRegularExpression spanRe(QStringLiteral("^(\\S+)\\s*s\\s+to\\s+(\\S+)\\s*s$"));
    static const QRegularExpression markerRe(QStringLiteral("^[-*]\\s*(-?[0-9.]+)\\s*s:?\\s*(.*)$"));

    LogNotes result;
    bool found = false;

    auto setField = [&result](const QString &key, const QString &value)
    {
        const QString name = key.trimmed().toLower();
        const QString text = value.trimmed();
        if (name == "title") result.title = text;
        else if (name == "author") result.author = text;
        else if (name == "tags")
        {
            result.tags.clear();
            for (const QString &tag : text.split(','))
                if (!tag.trimmed().isEmpty()) result.tags.append(tag.trimmed());
        }
        else if (name == "savvycan") result.savvycanVersion = text;
        else if (name == "log") result.logFile = text;
        else
        {
            //the fields SavvyCAN reads back; any that don't parse are kept as they are
            if (name == "saved")
            {
                const QDateTime saved = QDateTime::fromString(text, Qt::ISODate);
                if (saved.isValid())
                {
                    result.saved = saved;
                    return;
                }
            }
            else if (name == "frames")
            {
                const QRegularExpressionMatch match = framesRe.match(text);
                if (match.hasMatch())
                {
                    result.frameCount = match.captured(1).toLongLong();
                    result.unfilteredCount = match.captured(2).isEmpty() ? -1 : match.captured(2).toLongLong();
                    return;
                }
            }
            else if (name == "span")
            {
                const QRegularExpressionMatch match = spanRe.match(text);
                qint64 first, last;
                if (match.hasMatch() && parseSeconds(match.captured(1), first) && parseSeconds(match.captured(2), last))
                {
                    result.hasSpan = true;
                    result.firstUs = first;
                    result.lastUs = last;
                    return;
                }
            }
            result.otherFields.append(qMakePair(key.trimmed(), text));
        }
    };

    auto setSection = [&result](const QString &heading, const QString &rawText)
    {
        const QString name = heading.toLower();
        const QString text = trimBlankLines(rawText);
        if (name == "system") result.system = text;
        else if (name == "setup") result.setup = text;
        else if (name == "scenario") result.scenario = text;
        else if (name == "observations") result.observations = text;
        else if (name == "buses") result.buses = listItems(text);
        else if (name == "dbc files") result.dbcFiles = listItems(text);
        else if (name == "markers")
        {
            result.markers.clear();
            for (const QString &line : text.split('\n'))
            {
                const QRegularExpressionMatch match = markerRe.match(line.trimmed());
                LogMarker marker;
                if (match.hasMatch() && parseSeconds(match.captured(1), marker.timeUs))
                {
                    marker.text = match.captured(2).trimmed();
                    result.markers.append(marker);
                }
            }
        }
        else if (!text.isEmpty()) result.otherSections.append(qMakePair(heading, text));
    };

    QString heading;
    QStringList sectionLines;
    bool inHeader = true;
    QString text = markdown;
    text.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    for (const QString &line : text.split('\n'))
    {
        if (line.startsWith(QLatin1String("## ")))
        {
            if (!heading.isEmpty()) setSection(heading, sectionLines.join('\n'));
            heading = line.mid(3).trimmed();
            sectionLines.clear();
            inHeader = false;
            found = true;
        }
        else if (!inHeader) sectionLines.append(line);
        else if (line.startsWith(notesHeadingStart)) found = true;
        else
        {
            const QRegularExpressionMatch match = fieldRe.match(line);
            if (match.hasMatch())
            {
                setField(match.captured(1), match.captured(2));
                found = true;
            }
        }
    }
    if (!heading.isEmpty()) setSection(heading, sectionLines.join('\n'));

    if (found) notes = result;
    return found;
}

QString LogNotes::sidecarPath(const QString &logPath)
{
    return logPath + ".notes.md";
}

bool LogNotes::writeSidecar(const QString &logPath) const
{
    QFile file(sidecarPath(logPath));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) return false;
    const QByteArray data = toMarkdown().toUtf8();
    return file.write(data) == data.size();
}

bool LogNotes::readSidecar(const QString &logPath, LogNotes &notes)
{
    QFile file(sidecarPath(logPath));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    return fromMarkdown(QString::fromUtf8(file.readAll()), notes);
}

//Inside a native CSV log, commas are written as %2C (and % as %25) so that no notes line looks like a frame line to a
//loader that doesn't know about notes
static QByteArray escapeNotesLine(const QString &line)
{
    QByteArray bytes = line.toUtf8();
    bytes.replace('%', "%25");
    bytes.replace(',', "%2C");
    return bytes;
}

static QString unescapeNotesLine(const QByteArray &bytes)
{
    QByteArray out;
    out.reserve(bytes.size());
    for (qsizetype i = 0; i < bytes.size(); i++)
    {
        if (bytes.at(i) == '%' && i + 2 < bytes.size())
        {
            const QByteArray code = bytes.mid(i + 1, 2).toUpper();
            if (code == "2C") { out.append(','); i += 2; continue; }
            if (code == "25") { out.append('%'); i += 2; continue; }
        }
        out.append(bytes.at(i));
    }
    return QString::fromUtf8(out);
}

//a notes block line without its "# " (or "#")
static QByteArray notesLineContent(const QByteArray &line)
{
    QByteArray content = line.mid(1);
    if (content.startsWith(' ')) content.remove(0, 1);
    return content;
}

//The lines at the end of the file that start with '#' (and blank ones between them), with their file offsets, read
//backwards from the end so the frames aren't read again
bool LogNotes::readNativeCSVBlock(const QString &logPath, QList<BlockLine> &lines)
{
    QFile file(logPath);
    if (!file.open(QIODevice::ReadOnly)) return false;

    const qint64 chunkSize = 64 * 1024;
    qint64 tailStart = file.size(); //file offset of tail[0]
    QByteArray tail;
    qsizetype end = 0; //the tail up to here hasn't been looked at yet
    while (end > 0 || tailStart > 0)
    {
        qsizetype lineEnd = end;
        if (lineEnd > 0 && tail.at(lineEnd - 1) == '\n') lineEnd--;
        const qsizetype newline = lineEnd > 0 ? tail.lastIndexOf('\n', lineEnd - 1) : -1;
        if (newline < 0 && tailStart > 0)
        {
            //the start of this line isn't in the tail yet
            const qint64 length = qMin(chunkSize, tailStart);
            tailStart -= length;
            if (!file.seek(tailStart)) return false;
            const QByteArray chunk = file.read(length);
            if (chunk.size() != length) return false;
            tail.prepend(chunk);
            end += length;
            continue;
        }
        const qsizetype lineStart = newline + 1;
        QByteArray text = tail.mid(lineStart, lineEnd - lineStart);
        if (text.endsWith('\r')) text.chop(1);
        if (!isNotesLine(text) && !text.trimmed().isEmpty()) break;
        lines.prepend({tailStart + lineStart, text});
        end = lineStart;
    }
    return true;
}

bool LogNotes::readFromNativeCSV(const QString &logPath, LogNotes &notes)
{
    QList<BlockLine> lines;
    if (!readNativeCSVBlock(logPath, lines)) return false;

    //the block starts at the notes heading; '#' lines before it aren't SavvyCAN's
    qsizetype start = -1;
    for (qsizetype i = 0; i < lines.size(); i++)
        if (notesLineContent(lines.at(i).text).startsWith(notesHeadingStart.toUtf8())) start = i;
    if (start < 0) return false;

    QStringList markdown;
    for (qsizetype i = start; i < lines.size(); i++) markdown.append(unescapeNotesLine(notesLineContent(lines.at(i).text)));
    return fromMarkdown(markdown.join('\n'), notes);
}

bool LogNotes::writeToNativeCSV(const QString &logPath) const
{
    QList<BlockLine> lines;
    if (!readNativeCSVBlock(logPath, lines)) return false;
    qint64 blockStart = -1;
    for (const BlockLine &line : lines)
        if (notesLineContent(line.text).startsWith(notesHeadingStart.toUtf8())) blockStart = line.offset;

    QFile file(logPath);
    if (!file.open(QIODevice::ReadWrite)) return false;
    //same line ends as the rest of the file (saved in text mode: CRLF on Windows)
    const QByteArray lineEnd = file.readLine().endsWith("\r\n") ? QByteArray("\r\n") : QByteArray("\n");

    if (blockStart >= 0 && !file.resize(blockStart)) return false;
    if (isEmpty()) return true;

    if (file.size() > 0)
    {
        if (!file.seek(file.size() - 1)) return false;
        //the last frame line had no line end
        if (file.read(1) != "\n" && file.write(lineEnd) != lineEnd.size()) return false;
    }
    if (!file.seek(file.size())) return false;

    QByteArray block;
    QStringList markdown = toMarkdown().split('\n');
    if (!markdown.isEmpty() && markdown.last().isEmpty()) markdown.removeLast();
    for (const QString &line : markdown)
    {
        block.append(line.isEmpty() ? QByteArray("#") : "# " + escapeNotesLine(line));
        block.append(lineEnd);
    }
    return file.write(block) == block.size();
}
