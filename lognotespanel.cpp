#include "lognotespanel.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>

LogNotesPanel::LogNotesPanel(QWidget *parent)
    : QDockWidget(tr("Log Notes"), parent)
{
    setObjectName("LogNotesPanel"); //so the main window can save where it is
    QWidget *body = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(body);
    layout->setContentsMargins(4, 4, 4, 4);

    browser = new QTextBrowser(body);
    browser->setOpenExternalLinks(false);
    layout->addWidget(browser, 3);

    lblMarkers = new QLabel(tr("Markers (double-click one to go to it):"), body);
    layout->addWidget(lblMarkers);
    listMarkers = new QListWidget(body);
    listMarkers->setObjectName("markers");
    layout->addWidget(listMarkers, 1);
    connect(listMarkers, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item)
    {
        emit markerActivated(item->data(Qt::UserRole).toLongLong());
    });

    //marker presets: a button each (filled in by setPresets)
    presetBox = new QWidget(body);
    presetBox->setObjectName("presetBox");
    QVBoxLayout *presetLayout = new QVBoxLayout(presetBox);
    presetLayout->setContentsMargins(0, 0, 0, 0);
    presetGrid = new QGridLayout();
    presetGrid->setSpacing(4);
    presetLayout->addLayout(presetGrid);
    lblNext = new QLabel(presetBox);
    presetLayout->addWidget(lblNext);
    layout->addWidget(presetBox);

    QHBoxLayout *presetRow = new QHBoxLayout();
    QPushButton *btnUndo = new QPushButton(tr("Undo Last Marker"), body);
    btnUndo->setToolTip(tr("Remove the marker added last"));
    connect(btnUndo, &QPushButton::clicked, this, &LogNotesPanel::undoMarkerRequested);
    presetRow->addWidget(btnUndo);
    QPushButton *btnPresets = new QPushButton(tr("Presets..."), body);
    btnPresets->setToolTip(tr("Marker texts prepared before the test, each with a key"));
    connect(btnPresets, &QPushButton::clicked, this, &LogNotesPanel::presetsRequested);
    presetRow->addWidget(btnPresets);
    presetRow->addStretch(1);
    layout->addLayout(presetRow);

    QHBoxLayout *row = new QHBoxLayout();
    lblSource = new QLabel(body);
    lblSource->setWordWrap(true);
    row->addWidget(lblSource, 1);
    QPushButton *btnMarker = new QPushButton(tr("Add Marker"), body);
    btnMarker->setToolTip(tr("Mark this point of the log (Ctrl+M)"));
    connect(btnMarker, &QPushButton::clicked, this, &LogNotesPanel::addMarkerRequested);
    row->addWidget(btnMarker);
    QPushButton *btnEdit = new QPushButton(tr("Edit..."), body);
    connect(btnEdit, &QPushButton::clicked, this, &LogNotesPanel::editRequested);
    row->addWidget(btnEdit);
    layout->addLayout(row);

    setWidget(body);
    setNotes(LogNotes(), QString());
    setPresets(MarkerPresets());
}

void LogNotesPanel::setPresets(const MarkerPresets &presets)
{
    //a button may be the one being clicked (adding its marker refreshes the panel): detached now, deleted later
    while (QLayoutItem *item = presetGrid->takeAt(0))
    {
        if (QWidget *widget = item->widget())
        {
            widget->hide();
            widget->setParent(nullptr);
            widget->deleteLater();
        }
        delete item;
    }
    const int next = presets.nextKey.isEmpty() ? -1 : presets.next();
    for (int i = 0; i < presets.presets.count(); i++)
    {
        const MarkerPreset &preset = presets.presets[i];
        QString text = preset.text;
        if (!preset.key.isEmpty()) text += "  (" + preset.key.toString(QKeySequence::NativeText) + ")";
        QPushButton *button = new QPushButton(text, presetBox);
        button->setObjectName(QString("preset%1").arg(i));
        button->setToolTip(tr("Add the marker \"%1\" now").arg(preset.text));
        if (i == next)
        {
            QFont bold = button->font();
            bold.setBold(true);
            button->setFont(bold);
        }
        connect(button, &QPushButton::clicked, this, [this, i]() { emit presetActivated(i); });
        presetGrid->addWidget(button, i / 2, i % 2);
    }
    if (presets.nextKey.isEmpty() || presets.isEmpty()) lblNext->clear();
    else if (next < 0)
        lblNext->setText(tr("All %1 presets added (%2 starts again after Clear Frames)")
                         .arg(presets.presets.count()).arg(presets.nextKey.toString(QKeySequence::NativeText)));
    else
        lblNext->setText(tr("Next (%1): %2 (%3 of %4)").arg(presets.nextKey.toString(QKeySequence::NativeText),
                                                           presets.presets[next].text).arg(next + 1)
                                                      .arg(presets.presets.count()));
    lblNext->setVisible(!lblNext->text().isEmpty());
    presetBox->setVisible(!presets.isEmpty());
}

QString LogNotesPanel::markerText(const LogMarker &marker)
{
    return QString("%1 s: %2").arg(LogNotes::formatSeconds(marker.timeUs), marker.text).trimmed();
}

void LogNotesPanel::setNotes(const LogNotes &notes, const QString &source)
{
    if (notes.isEmpty())
        browser->setHtml(tr("<p><i>No notes for this log.</i></p><p>Edit... adds notes on its setup and scenario; "
                            "they're saved with the log.</p>"));
    else browser->setHtml(toHtml(notes));

    listMarkers->clear();
    for (const LogMarker &marker : notes.markers)
    {
        QListWidgetItem *item = new QListWidgetItem(markerText(marker), listMarkers);
        item->setData(Qt::UserRole, marker.timeUs);
    }
    lblMarkers->setVisible(!notes.markers.isEmpty());
    listMarkers->setVisible(!notes.markers.isEmpty());

    if (source.isEmpty()) lblSource->setText(notes.isEmpty() ? QString() : tr("Not saved with a log yet"));
    else lblSource->setText(tr("From %1").arg(source));
}

//a section's text, keeping its line breaks and indentation
static QString textHtml(const QString &text)
{
    return "<p style=\"white-space: pre-wrap\">" + text.toHtmlEscaped() + "</p>";
}

static QString listHtml(const QStringList &items)
{
    QString html = "<ul style=\"margin-left: 0px; -qt-list-indent: 1\">";
    for (const QString &item : items) html += "<li>" + item.toHtmlEscaped() + "</li>";
    return html + "</ul>";
}

QString LogNotesPanel::toHtml(const LogNotes &notes)
{
    QString html;
    html += "<h3>" + (notes.title.isEmpty() ? tr("(no title)") : notes.title).toHtmlEscaped() + "</h3>";

    QStringList facts;
    auto fact = [&facts](const QString &name, const QString &value)
    {
        if (!value.isEmpty()) facts.append("<tr><td><b>" + name.toHtmlEscaped() + "</b>&nbsp;</td><td>"
                                           + value.toHtmlEscaped() + "</td></tr>");
    };
    fact(tr("Author"), notes.author);
    fact(tr("Tags"), notes.tags.join(", "));
    if (notes.saved.isValid()) fact(tr("Saved"), QLocale().toString(notes.saved.toLocalTime(), QLocale::ShortFormat));
    fact(tr("Log"), notes.logFile);
    if (notes.frameCount >= 0)
        fact(tr("Frames"), notes.unfilteredCount >= 0 ? tr("%1 (filtered from %2)").arg(notes.frameCount).arg(notes.unfilteredCount)
                                                      : QString::number(notes.frameCount));
    if (notes.hasSpan)
        fact(tr("Time"), tr("%1 s to %2 s").arg(LogNotes::formatSeconds(notes.firstUs), LogNotes::formatSeconds(notes.lastUs)));
    fact(tr("SavvyCAN"), notes.savvycanVersion);
    for (const auto &other : notes.otherFields) fact(other.first, other.second);
    if (!facts.isEmpty()) html += "<table>" + facts.join(QString()) + "</table>";

    auto section = [&html](const QString &heading, const QString &body)
    {
        if (!body.trimmed().isEmpty()) html += "<h4>" + heading.toHtmlEscaped() + "</h4>" + textHtml(body.trimmed());
    };
    section(tr("System"), notes.system);
    section(tr("Setup"), notes.setup);
    section(tr("Scenario"), notes.scenario);
    section(tr("Observations"), notes.observations);
    for (const auto &other : notes.otherSections) section(other.first, other.second);
    if (!notes.buses.isEmpty()) html += "<h4>" + tr("Buses") + "</h4>" + listHtml(notes.buses);
    if (!notes.dbcFiles.isEmpty()) html += "<h4>" + tr("DBC files") + "</h4>" + listHtml(notes.dbcFiles);
    return html;
}
