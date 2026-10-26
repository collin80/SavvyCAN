#include "lognotesdialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

static const char *askOnSaveKey = "LogNotes/AskOnSave";
static const char *lastNotesKey = "LogNotes/Last";
static const char *templatesKey = "LogNotes/Templates";

LogNotesDialog::LogNotesDialog(Mode mode, QWidget *parent)
    : QDialog(parent), mode(mode), chosen(Cancelled)
{
    setWindowTitle(mode == Saving ? tr("Log Notes for This Log") : tr("Log Notes"));
    QVBoxLayout *layout = new QVBoxLayout(this);

    lblTarget = new QLabel(this);
    lblTarget->setWordWrap(true);
    layout->addWidget(lblTarget);

    QHBoxLayout *templateRow = new QHBoxLayout();
    templateRow->addWidget(new QLabel(tr("Template:"), this));
    cbTemplates = new QComboBox(this);
    cbTemplates->setMinimumContentsLength(16);
    templateRow->addWidget(cbTemplates, 1);
    btnUseTemplate = new QPushButton(tr("Use"), this);
    btnUseTemplate->setToolTip(tr("Replace the typed fields with the template's"));
    templateRow->addWidget(btnUseTemplate);
    QPushButton *btnSaveTemplate = new QPushButton(tr("Save as Template..."), this);
    btnSaveTemplate->setToolTip(tr("Keep the typed fields (not the markers or the filled-in fields) as a template"));
    templateRow->addWidget(btnSaveTemplate);
    btnDeleteTemplate = new QPushButton(tr("Delete"), this);
    templateRow->addWidget(btnDeleteTemplate);
    layout->addLayout(templateRow);

    QFormLayout *form = new QFormLayout();
    editTitle = new QLineEdit(this);
    editTitle->setPlaceholderText(tr("What this log is, in a line"));
    form->addRow(tr("Title:"), editTitle);
    editAuthor = new QLineEdit(this);
    form->addRow(tr("Author:"), editAuthor);
    editTags = new QLineEdit(this);
    editTags->setPlaceholderText(tr("Comma separated, e.g. hvac, lin, bench"));
    form->addRow(tr("Tags:"), editTags);
    editSystem = textBox(2, tr("Vehicle or system under test: model, year, ECU part and software numbers"));
    form->addRow(tr("System:"), editSystem);
    editSetup = textBox(3, tr("How the logger was connected: tap point, wiring, termination, interface settings"));
    form->addRow(tr("Setup:"), editSetup);
    editScenario = textBox(4, tr("The steps or flows this log should capture, in order"));
    form->addRow(tr("Scenario:"), editScenario);
    editObservations = textBox(3, tr("What was seen: anything unexpected, known issues"));
    form->addRow(tr("Observations:"), editObservations);
    layout->addLayout(form);

    QGroupBox *filledIn = new QGroupBox(tr("Filled in by SavvyCAN (can be edited)"), this);
    QFormLayout *filledForm = new QFormLayout(filledIn);
    editBuses = textBox(2, tr("One bus per line: number, interface, bitrate"));
    filledForm->addRow(tr("Buses:"), editBuses);
    editDbcFiles = textBox(1, tr("One DBC or LDF file per line"));
    filledForm->addRow(tr("DBC files:"), editDbcFiles);
    editMarkers = textBox(2, tr("One per line, e.g. 12.4 s: AC on"));
    filledForm->addRow(tr("Markers:"), editMarkers);
    lblFilledIn = new QLabel(filledIn);
    lblFilledIn->setWordWrap(true);
    filledForm->addRow(lblFilledIn);
    layout->addWidget(filledIn);

    cbAskOnSave = new QCheckBox(tr("Ask for notes when saving logs (also in Preferences)"), this);
    cbAskOnSave->setChecked(askOnSave());
    cbAskOnSave->setVisible(mode == Saving);
    layout->addWidget(cbAskOnSave);

    QDialogButtonBox *buttons = new QDialogButtonBox(this);
    if (mode == Saving)
    {
        QPushButton *btnSave = buttons->addButton(tr("Save with Notes"), QDialogButtonBox::AcceptRole);
        btnSave->setDefault(true);
        connect(btnSave, &QPushButton::clicked, this, [this]() { finish(WithNotes); });
        QPushButton *btnSkip = buttons->addButton(tr("Save without Notes"), QDialogButtonBox::DestructiveRole);
        connect(btnSkip, &QPushButton::clicked, this, [this]() { finish(WithoutNotes); });
        QPushButton *btnCancel = buttons->addButton(QDialogButtonBox::Cancel);
        btnCancel->setToolTip(tr("Don't save the log"));
        connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    }
    else
    {
        buttons->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(buttons, &QDialogButtonBox::accepted, this, [this]() { finish(WithNotes); });
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    }
    layout->addWidget(buttons);

    connect(btnUseTemplate, &QPushButton::clicked, this, &LogNotesDialog::useTemplate);
    connect(btnSaveTemplate, &QPushButton::clicked, this, &LogNotesDialog::saveAsTemplate);
    connect(btnDeleteTemplate, &QPushButton::clicked, this, &LogNotesDialog::removeTemplate);
    connect(cbTemplates, &QComboBox::currentIndexChanged, this, [this]()
    {
        btnUseTemplate->setEnabled(cbTemplates->currentIndex() > 0);
        btnDeleteTemplate->setEnabled(cbTemplates->currentIndex() > 0);
    });
    refreshTemplates();

    //names for the tests
    cbTemplates->setObjectName("templates");
    btnUseTemplate->setObjectName("useTemplate");
    editTitle->setObjectName("title");
    editTags->setObjectName("tags");
    editScenario->setObjectName("scenario");
    editMarkers->setObjectName("markers");
    cbAskOnSave->setObjectName("askOnSave");

    resize(640, sizeHint().height());
    editTitle->setFocus();
}

QPlainTextEdit *LogNotesDialog::textBox(int lines, const QString &placeholder)
{
    QPlainTextEdit *box = new QPlainTextEdit(this);
    box->setPlaceholderText(placeholder);
    box->setTabChangesFocus(true);
    box->setFixedHeight(box->fontMetrics().lineSpacing() * (lines + 1) + 2 * box->frameWidth()
                        + int(box->document()->documentMargin() * 2));
    return box;
}

void LogNotesDialog::setNotes(const LogNotes &notes)
{
    base = notes;
    showUserFields(notes);
    editBuses->setPlainText(notes.buses.join('\n'));
    editDbcFiles->setPlainText(notes.dbcFiles.join('\n'));
    QStringList markers;
    for (const LogMarker &marker : notes.markers)
        markers.append(QString("%1 s: %2").arg(LogNotes::formatSeconds(marker.timeUs), marker.text).trimmed());
    editMarkers->setPlainText(markers.join('\n'));

    QStringList info;
    if (notes.frameCount >= 0)
    {
        if (notes.unfilteredCount >= 0)
            info.append(tr("%1 frames (filtered from %2)").arg(notes.frameCount).arg(notes.unfilteredCount));
        else info.append(tr("%1 frames").arg(notes.frameCount));
    }
    if (notes.hasSpan)
        info.append(tr("%1 s to %2 s").arg(LogNotes::formatSeconds(notes.firstUs), LogNotes::formatSeconds(notes.lastUs)));
    if (notes.saved.isValid())
        info.append(tr("saved %1").arg(QLocale().toString(notes.saved.toLocalTime(), QLocale::ShortFormat)));
    if (!notes.savvycanVersion.isEmpty()) info.append(tr("SavvyCAN %1").arg(notes.savvycanVersion));
    lblFilledIn->setText(info.join(QStringLiteral(" · ")));
    lblFilledIn->setVisible(!info.isEmpty());
}

void LogNotesDialog::showUserFields(const LogNotes &notes)
{
    editTitle->setText(notes.title);
    editAuthor->setText(notes.author);
    editTags->setText(notes.tags.join(", "));
    editSystem->setPlainText(notes.system);
    editSetup->setPlainText(notes.setup);
    editScenario->setPlainText(notes.scenario);
    editObservations->setPlainText(notes.observations);
}

//the lines of a list box, without empty ones
static QStringList boxLines(const QPlainTextEdit *box)
{
    QStringList lines;
    for (const QString &line : box->toPlainText().split('\n'))
        if (!line.trimmed().isEmpty()) lines.append(line.trimmed());
    return lines;
}

LogNotes LogNotesDialog::notes() const
{
    LogNotes notes = base;
    notes.title = editTitle->text().trimmed();
    notes.author = editAuthor->text().trimmed();
    notes.tags.clear();
    for (const QString &tag : editTags->text().split(','))
        if (!tag.trimmed().isEmpty()) notes.tags.append(tag.trimmed());
    notes.system = editSystem->toPlainText();
    notes.setup = editSetup->toPlainText();
    notes.scenario = editScenario->toPlainText();
    notes.observations = editObservations->toPlainText();
    notes.buses = boxLines(editBuses);
    notes.dbcFiles = boxLines(editDbcFiles);

    //the markers are read the way the Markdown's Markers section is
    QStringList markerLines;
    for (const QString &line : boxLines(editMarkers))
        markerLines.append(line.startsWith("- ") || line.startsWith("* ") ? line : "- " + line);
    LogNotes parsed;
    LogNotes::fromMarkdown("## Markers\n" + markerLines.join('\n'), parsed);
    notes.markers = parsed.markers;
    return notes;
}

void LogNotesDialog::setTarget(const QString &fileName, bool insideLog)
{
    const QString name = fileName.toHtmlEscaped();
    if (mode == Saving)
    {
        if (insideLog) lblTarget->setText(tr("Saving <b>%1</b>. The notes are saved inside the log.").arg(name));
        else lblTarget->setText(tr("Saving <b>%1</b>. This file type can't hold notes, so they're saved in "
                                   "<b>%2</b> next to it: keep that file with the log.")
                                    .arg(name, LogNotes::sidecarPath(fileName).toHtmlEscaped()));
    }
    else
    {
        if (fileName.isEmpty()) lblTarget->setText(tr("No log file is open: the notes are kept for the next log saved."));
        else if (insideLog) lblTarget->setText(tr("The notes are saved inside <b>%1</b>.").arg(name));
        else lblTarget->setText(tr("The notes are saved in <b>%1</b> next to the log.")
                                    .arg(LogNotes::sidecarPath(fileName).toHtmlEscaped()));
    }
}

void LogNotesDialog::refreshTemplates(const QString &select)
{
    cbTemplates->clear();
    cbTemplates->addItem(tr("(pick a template)"));
    cbTemplates->addItems(templateNames());
    const int index = select.isEmpty() ? 0 : cbTemplates->findText(select);
    cbTemplates->setCurrentIndex(index < 0 ? 0 : index);
    btnUseTemplate->setEnabled(cbTemplates->currentIndex() > 0);
    btnDeleteTemplate->setEnabled(cbTemplates->currentIndex() > 0);
}

void LogNotesDialog::useTemplate()
{
    LogNotes fields;
    if (cbTemplates->currentIndex() > 0 && loadTemplate(cbTemplates->currentText(), fields))
    {
        base.otherFields = fields.otherFields;
        base.otherSections = fields.otherSections;
        showUserFields(fields);
    }
}

void LogNotesDialog::saveAsTemplate()
{
    const QString current = cbTemplates->currentIndex() > 0 ? cbTemplates->currentText() : QString();
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Save as Template"), tr("Template name:"), QLineEdit::Normal,
                                               current, &ok).trimmed();
    if (!ok || name.isEmpty()) return;
    if (templateNames().contains(name) && QMessageBox::question(this, tr("Save as Template"),
            tr("Replace the template \"%1\"?").arg(name)) != QMessageBox::Yes) return;
    saveTemplate(name, notes());
    refreshTemplates(name);
}

void LogNotesDialog::removeTemplate()
{
    if (cbTemplates->currentIndex() <= 0) return;
    const QString name = cbTemplates->currentText();
    if (QMessageBox::question(this, tr("Delete Template"), tr("Delete the template \"%1\"?").arg(name)) != QMessageBox::Yes)
        return;
    deleteTemplate(name);
    refreshTemplates();
}

void LogNotesDialog::finish(Choice choice)
{
    chosen = choice;
    if (mode == Saving) setAskOnSave(cbAskOnSave->isChecked());
    accept();
}

bool LogNotesDialog::askOnSave()
{
    return QSettings().value(askOnSaveKey, true).toBool();
}

void LogNotesDialog::setAskOnSave(bool ask)
{
    QSettings().setValue(askOnSaveKey, ask);
}

LogNotes LogNotesDialog::lastNotes()
{
    LogNotes notes;
    LogNotes::fromMarkdown(QSettings().value(lastNotesKey).toString(), notes);
    return notes.userFields();
}

void LogNotesDialog::setLastNotes(const LogNotes &notes)
{
    QSettings().setValue(lastNotesKey, notes.userFields().toMarkdown());
}

QStringList LogNotesDialog::templateNames()
{
    QStringList names = QSettings().value(templatesKey).toMap().keys();
    names.sort(Qt::CaseInsensitive);
    return names;
}

bool LogNotesDialog::loadTemplate(const QString &name, LogNotes &notes)
{
    const QVariantMap templates = QSettings().value(templatesKey).toMap();
    if (!templates.contains(name)) return false;
    LogNotes parsed;
    LogNotes::fromMarkdown(templates.value(name).toString(), parsed);
    notes = parsed.userFields();
    return true;
}

void LogNotesDialog::saveTemplate(const QString &name, const LogNotes &notes)
{
    QSettings settings;
    QVariantMap templates = settings.value(templatesKey).toMap();
    templates.insert(name, notes.userFields().toMarkdown());
    settings.setValue(templatesKey, templates);
}

void LogNotesDialog::deleteTemplate(const QString &name)
{
    QSettings settings;
    QVariantMap templates = settings.value(templatesKey).toMap();
    templates.remove(name);
    settings.setValue(templatesKey, templates);
}
