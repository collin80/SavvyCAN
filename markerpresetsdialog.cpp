#include "markerpresetsdialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>
#include "shortcutcheck.h"

static QKeySequenceEdit *keyEdit(QWidget *parent)
{
    QKeySequenceEdit *edit = new QKeySequenceEdit(parent);
    edit->setClearButtonEnabled(true);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    edit->setMaximumSequenceLength(1);
#endif
    return edit;
}

MarkerPresetsDialog::MarkerPresetsDialog(const MarkerPresets &presets, const QString &scenario, QWidget *parent)
    : QDialog(parent), edited(presets), scenario(scenario)
{
    setWindowTitle(tr("Marker Presets"));
    QVBoxLayout *layout = new QVBoxLayout(this);

    QLabel *help = new QLabel(tr("Each preset adds its marker at once with its key, or its button in the Log Notes "
                                 "panel: at the newest frame while capturing, else at the frame clicked. Ctrl+M still "
                                 "asks for any text. The keys work in every SavvyCAN window."), this);
    help->setWordWrap(true);
    layout->addWidget(help);

    QHBoxLayout *middle = new QHBoxLayout();
    table = new QTableWidget(0, 2, this);
    table->setObjectName("presets");
    table->setHorizontalHeaderLabels({ tr("Marker text"), tr("Key") });
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    table->setColumnWidth(1, 150);
    table->verticalHeader()->setVisible(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    middle->addWidget(table, 1);

    QVBoxLayout *buttons = new QVBoxLayout();
    QPushButton *btnAdd = new QPushButton(tr("Add"), this);
    btnRemove = new QPushButton(tr("Remove"), this);
    btnUp = new QPushButton(tr("Move Up"), this);
    btnDown = new QPushButton(tr("Move Down"), this);
    QPushButton *btnScenario = new QPushButton(tr("From Scenario"), this);
    btnScenario->setToolTip(tr("Add the notes' Scenario steps as presets, with Ctrl+1 to Ctrl+9"));
    btnScenario->setEnabled(!MarkerPresets::scenarioSteps(scenario).isEmpty());
    for (QPushButton *button : { btnAdd, btnRemove, btnUp, btnDown, btnScenario }) buttons->addWidget(button);
    buttons->addStretch(1);
    middle->addLayout(buttons);
    layout->addLayout(middle, 1);

    QFormLayout *form = new QFormLayout();
    editNext = keyEdit(this);
    editNext->setObjectName("nextKey");
    editNext->setToolTip(tr("Adds the presets in order, one per press"));
    editNext->setKeySequence(edited.nextKey);
    form->addRow(tr("Next preset key:"), editNext);
    editUndo = keyEdit(this);
    editUndo->setObjectName("undoKey");
    editUndo->setKeySequence(edited.undoKey);
    form->addRow(tr("Undo Last Marker key:"), editUndo);
    layout->addLayout(form);

    lblMessage = new QLabel(this);
    lblMessage->setObjectName("message");
    lblMessage->setWordWrap(true);
    layout->addWidget(lblMessage);

    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(box);

    connect(btnAdd, &QPushButton::clicked, this, &MarkerPresetsDialog::addPreset);
    connect(btnRemove, &QPushButton::clicked, this, &MarkerPresetsDialog::removePreset);
    connect(btnUp, &QPushButton::clicked, this, [this]() { movePreset(-1); });
    connect(btnDown, &QPushButton::clicked, this, [this]() { movePreset(1); });
    connect(btnScenario, &QPushButton::clicked, this, &MarkerPresetsDialog::addFromScenario);
    connect(table, &QTableWidget::itemSelectionChanged, this, &MarkerPresetsDialog::updateButtons);
    //keySequenceChanged also comes from the clear button (editingFinished doesn't)
    connect(editNext, &QKeySequenceEdit::keySequenceChanged, this, [this]() { keyChanged(-1, editNext); });
    connect(editUndo, &QKeySequenceEdit::keySequenceChanged, this, [this]() { keyChanged(-2, editUndo); });

    fillTable();
    resize(560, 420);
}

void MarkerPresetsDialog::fillTable(int select)
{
    filling = true;
    //Qt deletes replaced cell widgets later, from the event loop: hidden now, so they're never drawn out of place
    for (int row = 0; row < table->rowCount(); row++)
        if (QWidget *old = table->cellWidget(row, 1)) old->hide();
    table->setRowCount(0);
    table->setRowCount(int(edited.presets.count()));
    for (int row = 0; row < edited.presets.count(); row++)
    {
        table->setItem(row, 0, new QTableWidgetItem(edited.presets[row].text));
        QKeySequenceEdit *edit = keyEdit(table);
        edit->setKeySequence(edited.presets[row].key);
        connect(edit, &QKeySequenceEdit::keySequenceChanged, this, [this, row, edit]() { keyChanged(row, edit); });
        table->setCellWidget(row, 1, edit);
    }
    filling = false;
    if (select >= 0 && select < table->rowCount()) table->selectRow(select);
    updateButtons();
}

//the texts as typed in the table, into edited
void MarkerPresetsDialog::readTexts()
{
    for (int row = 0; row < table->rowCount() && row < edited.presets.count(); row++)
        if (table->item(row, 0)) edited.presets[row].text = table->item(row, 0)->text().trimmed();
}

MarkerPresets MarkerPresetsDialog::presets() const
{
    MarkerPresets result = edited;
    for (int row = 0; row < table->rowCount() && row < result.presets.count(); row++)
        if (table->item(row, 0)) result.presets[row].text = table->item(row, 0)->text().trimmed();
    //a preset with no text adds nothing
    result.presets.erase(std::remove_if(result.presets.begin(), result.presets.end(),
                                        [](const MarkerPreset &preset) { return preset.text.isEmpty(); }),
                         result.presets.end());
    return result;
}

QString MarkerPresetsDialog::lastKeyMessage() const
{
    return lblMessage->text();
}

bool MarkerPresetsDialog::setKey(int row, const QKeySequence &key)
{
    QKeySequenceEdit *edit = row == -1 ? editNext : row == -2 ? editUndo
                           : qobject_cast<QKeySequenceEdit *>(table->cellWidget(row, 1));
    if (!edit) return false;
    edit->setKeySequence(key);
    keyChanged(row, edit);
    return edit->keySequence() == key;
}

void MarkerPresetsDialog::keyChanged(int row, QKeySequenceEdit *edit)
{
    if (filling) return;
    QKeySequence key = edit->keySequence();
    if (key.count() > 1) key = QKeySequence(key[0]); //one key press is enough
    QKeySequence &stored = row == -1 ? edited.nextKey : row == -2 ? edited.undoKey : edited.presets[row].key;
    if (key == stored && key == edit->keySequence()) return;

    QString reason;
    ShortcutCheck::Verdict verdict = ShortcutCheck::check(key, reason);
    if (verdict != ShortcutCheck::Refused)
    {
        readTexts();
        const QString user = edited.keyUser(key, row, row == -1, row == -2);
        if (!user.isEmpty())
        {
            verdict = ShortcutCheck::Refused;
            reason = user;
        }
    }
    const QString name = key.toString(QKeySequence::NativeText);
    if (verdict == ShortcutCheck::Refused)
    {
        lblMessage->setStyleSheet("color: #b00020");
        lblMessage->setText(tr("%1 is already used for %2. Pick another key.").arg(name, reason));
        edit->blockSignals(true);
        edit->setKeySequence(stored);
        edit->blockSignals(false);
        return;
    }
    stored = key;
    edit->blockSignals(true);
    edit->setKeySequence(key);
    edit->blockSignals(false);
    if (verdict == ShortcutCheck::Warn)
    {
        lblMessage->setStyleSheet("color: #8a5a00");
        lblMessage->setText(tr("%1 is set, but it's often used for %2; pick another if that matters.").arg(name, reason));
    }
    else
    {
        lblMessage->setStyleSheet(QString());
        lblMessage->clear();
    }
}

void MarkerPresetsDialog::addPreset()
{
    readTexts();
    MarkerPreset preset;
    preset.text = tr("Marker %1").arg(edited.presets.count() + 1);
    edited.presets.append(preset);
    fillTable(int(edited.presets.count()) - 1);
    table->setCurrentCell(int(edited.presets.count()) - 1, 0);
    table->editItem(table->item(int(edited.presets.count()) - 1, 0));
}

void MarkerPresetsDialog::removePreset()
{
    const int row = table->currentRow();
    if (row < 0 || row >= edited.presets.count()) return;
    readTexts();
    edited.presets.removeAt(row);
    fillTable(qMin(row, int(edited.presets.count()) - 1));
}

void MarkerPresetsDialog::movePreset(int by)
{
    const int row = table->currentRow();
    const int to = row + by;
    if (row < 0 || to < 0 || to >= edited.presets.count()) return;
    readTexts();
    edited.presets.swapItemsAt(row, to);
    fillTable(to);
}

void MarkerPresetsDialog::addFromScenario()
{
    readTexts();
    edited.addScenarioSteps(scenario);
    fillTable(int(edited.presets.count()) - 1);
}

void MarkerPresetsDialog::updateButtons()
{
    const int row = table->currentRow();
    const bool selected = row >= 0 && !table->selectedItems().isEmpty();
    btnRemove->setEnabled(selected);
    btnUp->setEnabled(selected && row > 0);
    btnDown->setEnabled(selected && row < table->rowCount() - 1);
}
