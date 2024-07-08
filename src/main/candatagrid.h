#ifndef CANDATAGRID_H
#define CANDATAGRID_H

#include <QWidget>

namespace Ui
{
class CANDataGrid;
}

/*
 * CANDataGrid is a custom QT widget that proves that it's possible to shoehorn 12x too much crap into one widget. The
 * purported goal of the widget was to show an 8x8 grid of bits that show whether each bit in a given message is set or
 * not. It allows one to quickly visualize the bits within a CAN frame's data bytes. From there it evolved to also show
 * whether a given bit was freshly set recently, freshly unset recently, or has stayed set or unset for a while. It can
 * also gray out bytes that don't exist in the current CAN frame. This is handled by the calling code.
 *
 * After that functionality was added so it could also emit a signal when it is clicked. The signal will tell you which
 * "bit" cell was clicked.
 *
 * Then, support was added for modifying the text color of each cell. This can be used for whatever the calling code
 * wants. An example is FlowView that uses all of the above functionality plus this functionality in order to show which
 * bits are set as triggers for stopping the flowview playback.
 *
 * Now the control also tracks which signal is using which bit. The graphical representation does exist now but some
 * tweaking is probably still needed. Also, it seems like it is necessary to allow for a variety of modes.
 *
 * And now, CAN-FD added as the cherry on top! It's a mess but really, all this functionality is handy to have.
 */

enum GridTextState
{
  NORMAL,
  BOLD_BLUE,
  INVERT
};

enum GridMode
{
  CHANGED_BITS,  // traditional view, bunch of bits we color to show what's set and how the bits have changed over time
  SIGNAL_VIEW,   // special view for DBC window where we draw the signals in the bits they take up
  HEAT_VIEW      // another special mode where each bit is given a heat level and we color each bit based on that. Use
             // refData for storage of those values
};

extern QVector<QColor> signalColors;

class CANDataGrid : public QWidget
{
  Q_OBJECT

public:
  explicit CANDataGrid(QWidget* parent = 0);
  ~CANDataGrid();
  void paintEvent(QPaintEvent* event) override;
  void setReference(unsigned char*, bool);
  void updateData(unsigned char*, bool);
  void setUsed(unsigned char*, bool);
  void setHeat(unsigned char*);
  void saveImage(QString filename, int width, int height);
  void setCellTextState(int bitPos, GridTextState state);
  GridTextState getCellTextState(int bitPos);
  void setUsedSignalNum(int bit, int signal);
  void setSignalNames(int sigIdx, const QString sigName);
  void clearSignalNames();
  int getUsedSignalNum(int bit);
  GridMode getMode();
  void setMode(GridMode mode);
  void setBytesToDraw(int num);

protected:
  void mousePressEvent(QMouseEvent* event) Q_DECL_OVERRIDE;

signals:
  void gridClicked(int bitClicked);
  void gridRightClicked(int bitClicked);

private:
  Ui::CANDataGrid* ui;
  int bytesToDraw;
  unsigned char refData[64];
  unsigned char data[64];
  unsigned char usedData[64];
  unsigned char heatData[512];
  int usedSignalNum[512];  // so we can specify which signal claims this bit
  QVector<QString> signalNames;
  GridTextState textStates[64][8];  // first dimension is bytes, second is bits
  QPoint upperLeft, gridSize;
  GridMode gridMode;
  QBrush blackBrush, whiteBrush, redBrush, greenBrush, grayBrush;
  QBrush greenHashBrush, blackHashBrush;
  QPainter* painter;
  QRect viewport;
  int xSpan;
  int ySpan;
  int xSector;
  int ySector;
  double bigTextSize, smallTextSize, sigNameTextSize;
  int xOffset;
  int yOffset;
  int farX, farY, nearX, nearY;
  int neededXDivisions;
  int neededYDivisions;
  QFont mainFont;
  QFont smallFont;
  QFont boldFont;
  QFont sigNameFont;
  QFontMetrics* smallMetric;
  QFontMetrics* largeMetric;
  QColor fire[256];

  void paintGridCells();
  void paintCommonBeginning();
  void paintCommonEnding();
  int gridToBitPosition(int x, int y);
  QPoint getGridPointFromBitPosition(int bitPos);
  int getSignalRowRun(int sigNum, int startBit);
};

#endif  // CANDATAGRID_H
