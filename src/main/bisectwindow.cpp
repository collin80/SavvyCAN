#include "main/bisectwindow.h"
#include "ui_bisectwindow.h"

#include "main/framefileio.h"
#include "main/helpwindow.h"
#include "main/mainwindow.h"

#include <QDebug>
#include <algorithm>

BisectWindow::BisectWindow(const QVector<CANFrame>* frames, QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::BisectWindow)
{
  ui->setupUi(this);
  setWindowFlags(Qt::Window);

  modelFrames = frames;

  connect(MainWindow::getReference(), SIGNAL(framesUpdated(int)), this, SLOT(updatedFrames(int)));
  connect(ui->btnCalculate, &QAbstractButton::clicked, this, &BisectWindow::handleCalculateButton);
  connect(ui->btnReplaceFrames, &QAbstractButton::clicked, this, &BisectWindow::handleReplaceButton);
  connect(ui->btnSaveFrames, &QAbstractButton::clicked, this, &BisectWindow::handleSaveButton);
  connect(ui->slideFrameNumber, &QSlider::sliderReleased, this, &BisectWindow::updateFrameNumText);
  connect(ui->slidePercentage, &QSlider::sliderReleased, this, &BisectWindow::updatePercentText);
  connect(ui->editFrameNumber, &QLineEdit::editingFinished, this, &BisectWindow::updateFrameNumSlider);
  connect(ui->editPercentage, &QLineEdit::editingFinished, this, &BisectWindow::updatePercentSlider);
  connect(ui->rbFrameNumber, &QRadioButton::toggled, this, &BisectWindow::updateSectionsText);
  connect(ui->rbPercentage, &QRadioButton::toggled, this, &BisectWindow::updateSectionsText);
  connect(ui->rbBusNum, &QRadioButton::toggled, this, &BisectWindow::updateSectionsText);
  connect(ui->rbIDRange, &QRadioButton::toggled, this, &BisectWindow::updateSectionsText);

  installEventFilter(this);

  updateSectionsText();
}

BisectWindow::~BisectWindow()
{
  removeEventFilter(this);
  delete ui;
}

void BisectWindow::showEvent(QShowEvent* event)
{
  QDialog::showEvent(event);
  refreshIDList();
  refreshFrameNumbers();
  updateFrameNumText();
  updatePercentText();
}

bool BisectWindow::eventFilter(QObject* obj, QEvent* event)
{
  if (event->type() == QEvent::KeyRelease)
  {
    QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
    switch (keyEvent->key())
    {
    case Qt::Key_F1:
      HelpWindow::getRef()->showHelp("bisector.md");
      break;
    }
    return true;
  }
  else
  {
    // standard event processing
    return QObject::eventFilter(obj, event);
  }
  return false;
}

void BisectWindow::updateSectionsText()
{
  if (ui->rbBusNum->isChecked())
  {
    ui->rbLowerSection->setText("Only this bus");
    ui->rbUpperSection->setText("Not this bus");
  }
  if (ui->rbFrameNumber->isChecked())
  {
    ui->rbLowerSection->setText("Up to this frame number");
    ui->rbUpperSection->setText("After this frame number");
  }
  if (ui->rbIDRange->isChecked())
  {
    ui->rbLowerSection->setText("Inside the ID range");
    ui->rbUpperSection->setText("Outside the ID range");
  }
  if (ui->rbPercentage->isChecked())
  {
    ui->rbLowerSection->setText("Up to this percentage into the file");
    ui->rbUpperSection->setText("After this percentage into the file");
  }
}

void BisectWindow::refreshIDList()
{
  int id;
  foundID.clear();
  ui->cbIDLower->clear();
  ui->cbIDUpper->clear();
  for (int i = 0; i < modelFrames->count(); i++)
  {
    id = modelFrames->at(i).frameId();
    if (!foundID.contains(id))
    {
      foundID.append(id);
    }
  }

  std::sort(foundID.begin(), foundID.end());

  foreach (int id, foundID)
  {
    ui->cbIDLower->addItem(Utility::formatCANID(id));
    ui->cbIDUpper->addItem(Utility::formatCANID(id));
  }
}

void BisectWindow::refreshFrameNumbers()
{
  ui->labelMainListNum->setText(QString::number(modelFrames->count()));
  ui->labelSplitNum->setText(QString::number(splitFrames.count()));
  ui->slideFrameNumber->setMaximum(modelFrames->count());
}

void BisectWindow::updatedFrames(int numFrames)
{
  if (numFrames == -1)  // all frames deleted
  {
    refreshFrameNumbers();
  }
  else if (numFrames == -2)  // all new set of frames. Reset
  {
    refreshFrameNumbers();
    refreshIDList();
  }
  else  // just got some new frames. See if they are relevant.
  {
  }
}

void BisectWindow::handleCalculateButton()
{
  splitFrames.clear();
  bool saveLower = ui->rbLowerSection->isChecked();
  int targetFrameNum = 0;
  if (ui->rbFrameNumber->isChecked() || ui->rbPercentage->isChecked())
  {
    if (ui->rbFrameNumber->isChecked())
      targetFrameNum = ui->slideFrameNumber->value();
    else
      targetFrameNum = modelFrames->count() * (ui->slidePercentage->value() / 10000.0);
    qDebug() << "Target frame num " << targetFrameNum;
    if (saveLower)
    {
      for (int i = 0; i < targetFrameNum; i++)
        splitFrames.append(modelFrames->at(i));
    }
    else
    {
      for (int i = targetFrameNum; i < modelFrames->count(); i++)
        splitFrames.append(modelFrames->at(i));
    }
  }
  else if (ui->rbIDRange->isChecked())
  {
    uint32_t lowerID = Utility::ParseStringToNum2(ui->cbIDLower->currentText());
    uint32_t upperID = Utility::ParseStringToNum2(ui->cbIDUpper->currentText());
    for (int i = 0; i < modelFrames->count(); i++)
    {
      if (modelFrames->at(i).frameId() >= lowerID && modelFrames->at(i).frameId() <= upperID)
      {
        if (saveLower)
          splitFrames.append(modelFrames->at(i));
      }
      else
      {
        if (!saveLower)
          splitFrames.append(modelFrames->at(i));
      }
    }
  }
  else if (ui->rbBusNum->isChecked())
  {
    int targetBus = Utility::ParseStringToNum(ui->editBusNum->text());
    for (int i = 0; i < modelFrames->count(); i++)
    {
      if (modelFrames->at(i).bus == targetBus)
      {
        if (saveLower)
          splitFrames.append(modelFrames->at(i));
      }
      else
      {
        if (!saveLower)
          splitFrames.append(modelFrames->at(i));
      }
    }
  }
  refreshFrameNumbers();
}

void BisectWindow::handleReplaceButton()
{
  CANFrameModel* model;
  model = MainWindow::getReference()->getCANFrameModel();
  model->clearFrames();
  model->insertFrames(splitFrames);
  refreshFrameNumbers();
  refreshIDList();
}

void BisectWindow::handleSaveButton()
{
  QMessageBox msg;
  QString filename;
  if (FrameFileIO::saveFrameFile(filename, &splitFrames))
  {
    msg.setText(tr("Successfully saved file"));
  }
  else
  {
    msg.setText(tr("Error while attempting to save."));
  }
  msg.exec();
}

void BisectWindow::updateFrameNumSlider()
{
  ui->slideFrameNumber->setValue(ui->editFrameNumber->text().toInt());
}

void BisectWindow::updatePercentSlider()
{
  ui->slidePercentage->setValue(ui->editPercentage->text().toFloat() * 100);
}

void BisectWindow::updateFrameNumText()
{
  ui->editFrameNumber->setText(QString::number(ui->slideFrameNumber->value()));
}

void BisectWindow::updatePercentText()
{
  ui->editPercentage->setText(QString::number(ui->slidePercentage->value() / 100.0f));
}
