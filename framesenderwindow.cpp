#include "framesenderwindow.h"
#include "ui_framesenderwindow.h"
#include "utility.h"

FrameSenderWindow::FrameSenderWindow(QVector<CANFrame> *frames, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::FrameSenderWindow)
{
    ui->setupUi(this);

    modelFrames = frames;

    intervalTimer = new QTimer();
    intervalTimer->setInterval(1);

    QStringList headers;
    headers << "En" << "Bus" << "ID" << "Len" << "Data" << "Trigger" << "Modifications" << "Count";
    ui->tableSender->setColumnCount(8);
    ui->tableSender->setColumnWidth(0, 40);
    ui->tableSender->setColumnWidth(1, 40);
    ui->tableSender->setColumnWidth(2, 40);
    ui->tableSender->setColumnWidth(3, 40);
    ui->tableSender->setColumnWidth(4, 200);
    ui->tableSender->setColumnWidth(5, 200);
    ui->tableSender->setColumnWidth(6, 200);
    ui->tableSender->setColumnWidth(7, 80);
    ui->tableSender->setHorizontalHeaderLabels(headers);
    ui->tableSender->insertRow(0);

    connect(ui->tableSender, SIGNAL(cellChanged(int,int)), this, SLOT(onCellChanged(int,int)));

}

FrameSenderWindow::~FrameSenderWindow()
{
    delete ui;

    intervalTimer->stop();
    delete intervalTimer;
}

void FrameSenderWindow::onCellChanged(int row, int col)
{
    if (row == ui->tableSender->rowCount() - 1)
    {
        ui->tableSender->insertRow(ui->tableSender->rowCount());
    }
}

/// <summary>
/// Called every millisecond to set the system update figures and send frames if necessary.
/// </summary>
/// <param name="sender"></param>
/// <param name="timerEventArgs"></param>
void FrameSenderWindow::handleTick()
{
    FrameSendData sendData;
    Trigger trigger;
    Modifier modifier;
    for (int i = 0; i < sendingData.count(); i++)
    {
        sendData = sendingData.at(i);
        if (!sendData.enabled) continue; //abort any processing on this if it is not enabled.
        if (sendData.triggers.count() == 0) return;
        for (int j = 0; j < sendData.triggers.count(); j++)
        {
            trigger = sendData.triggers.at(j);
            if (trigger.currCount >= trigger.maxCount) continue; //don't process if we've sent max frames we were supposed to
            if (!trigger.readyCount) continue; //don't tick if not ready to tick
            //is it time to fire?
            if (++trigger.msCounter >= trigger.milliseconds)
            {
                trigger.msCounter = 0;
                sendData.count++;
                trigger.currCount++;
                doModifiers(i);
                //updateGridRow(i);
                //parent.SendCANFrame(sendingData[i], sendingData[i].bus);
                if (trigger.ID > 0) trigger.readyCount = false; //reset flag if this is a timed ID trigger
            }
        }
    }
}

/// <summary>
/// given an index into the sendingData list we run the modifiers that it has set up
/// </summary>
/// <param name="idx">The index into the sendingData list</param>
void FrameSenderWindow::doModifiers(int idx)
{
    int shadowReg = 0; //shadow register we use to accumulate results
    int first=0, second=0;

    FrameSendData sendData = sendingData.at(idx);
    Modifier mod;
    ModifierOp op;

    if (sendData.modifiers.count() == 0) return; //if no modifiers just leave right now

    for (int i = 0; i < sendData.modifiers.count(); i++)
    {
        mod = sendData.modifiers.at(i);
        for (int j = 0; j < mod.operations.count(); j++)
        {
            op = mod.operations.at(j);
            if (op.first.ID == -1)
            {
                first = shadowReg;
            }
            else first = fetchOperand(idx, op.first);
            second = fetchOperand(idx, op.second);
            switch (op.operation)
            {
                case ADDITION:
                    shadowReg = first + second;
                    break;
                case AND:
                    shadowReg = first & second;
                    break;
                case DIVISION:
                    shadowReg = first / second;
                    break;
                case MULTIPLICATION:
                    shadowReg = first * second;
                    break;
                case OR:
                    shadowReg = first | second;
                    break;
                case SUBTRACTION:
                    shadowReg = first - second;
                    break;
                case XOR:
                    shadowReg = first ^ second;
                    break;
            }
        }
        //Finally, drop the result into the proper data byte
        mod.destByte = (unsigned char) shadowReg;
    }
}

int FrameSenderWindow::fetchOperand(int idx, ModifierOperand op)
{
    CANFrame *tempFrame = NULL;
    if (op.ID == 0) //numeric constant
    {
        return op.databyte;
    }
    else if (op.ID == -2)
    {
        return sendingData.at(idx).data[op.databyte];
    }
    else //look up external data byte
    {
        tempFrame = lookupFrame(op.ID, op.bus);
        if (tempFrame != NULL)
        {
            return tempFrame->data[op.databyte];
        }
        else return 0;
    }
}

/// <summary>
/// Try to find the most recent frame given the input criteria
/// </summary>
/// <param name="ID">The ID to find</param>
/// <param name="bus">Which bus to look on (-1 if you don't care)</param>
/// <returns></returns>
CANFrame* FrameSenderWindow::lookupFrame(int ID, int bus)
{
    CANFrame frame;
    for (int a = 0; a < frameCache.count(); a++)
    {
        frame = frameCache.at(a);
        if ((ID == frame.ID) && ((bus == frame.bus) || bus == -1))
        {
            return &frame;
        }
    }
    return NULL;
}

/// <summary>
/// Process a single line from the dataGrid. Right now it seems to not trigger at all after the first adding of the code but that seems to maybe
/// be because whichever field you where just in will show up as nothing to the code.
/// </summary>
/// <param name="line"></param>
void FrameSenderWindow::processModifierText(int line)
{
    QString modString;
    int numOps;

    //Example line:
    //d0 = D0 + 1,d1 = id:0x200:d3 + id:0x200:d4 AND 0xF0
    //This is certainly much harder to parse than the trigger definitions.
    //the left side of the = has to be D0 to D7. After that there is a string of
    //data that for ease of parsing will require spaces between tokens
    modString = ui->tableSender->item(line, 6)->text().toUpper();
    if (modString != "")
    {
        QStringList mods = modString.split(',');
        sendingData[line].modifiers.clear();
        sendingData[line].modifiers.reserve(mods.length());
        for (int i = 0; i < mods.length(); i++)
        {
            Modifier thisMod;
            thisMod.destByte = 0;
            //now split by space to extract tokens
            QStringList modToks = mods[i].split(' ');
            if (modToks.length() >= 5) //any valid modifier that this code can process has at least 5 tokens (D0 = D0 + 1)
            {
                //valid token assignment will have a data byte as the first token and = as the second
                if (modToks[0].length() == 2 && modToks[0].startsWith('D'))
                {
                    numOps = ((modToks.length() - 5) / 2) + 1;
                    thisMod.operations.clear();
                    thisMod.operations.reserve(numOps);
                    thisMod.destByte = modToks[0].right(modToks[0].length() - 1).toInt();
                    //Now start at token 2 and extract all operations. All ops past the first one
                    //use the implicit shadow register as the first operand.The contents of the shadow
                    //register are what is copied to the destination byte at the end.
                    //each op is of the form <first> <op> <second>. first and second could have more subtokens
                    int currToken = 2;
                    QStringList firstToks, secondToks;
                    for (int j = 0; j < numOps; j++)
                    {
                        ModifierOp thisOp;
                        if (j == 0)
                        {
                            firstToks = modToks[currToken++].toUpper().split(':');
                            parseOperandString(firstToks, thisOp.first);
                        }
                        else
                        {
                            thisOp.first.ID = -1;
                        }
                        thisOp.operation = parseOperation(modToks[currToken++].toUpper());
                        secondToks = modToks[currToken++].toUpper().split(':');
                        thisOp.second.bus = sendingData[line].bus;
                        thisOp.second.ID = sendingData[line].ID;
                        parseOperandString(secondToks, thisOp.second);
                        thisMod.operations.append(thisOp);
                    }
                }
            }
            else
            {
               QStringList firstToks;
               numOps = 1;
               thisMod.destByte = modToks[0].right(modToks[0].length() - 1).toInt();
               ModifierOp thisOp;
               thisOp.operation = ADDITION;
               thisOp.second.ID = 0;
               thisOp.second.databyte = 0;
               firstToks = modToks[2].toUpper().split(':');
               parseOperandString(firstToks, thisOp.first);
               thisMod.operations.append(thisOp);
            }

            sendingData[line].modifiers.append(thisMod);
        }
    }
    //there is no else for the modifiers. We'll accept there not being any
}

void FrameSenderWindow::processTriggerText(int line)
{
    QString trigger;

    //Example line:
    //id=0x200 5ms 10x bus0,1000ms
    //trigger has two levels of syntactic parsing. First you split by comma to get each
    //actual trigger. Then you split by spaces to get the tokens within each trigger
    trigger = ui->tableSender->item(line, 5)->text().toUpper();
    if (trigger != "")
    {
        QStringList triggers = trigger.split(',');
        sendingData[line].triggers.clear();
        sendingData[line].triggers.reserve(triggers.length());
        for (int k = 0; k < triggers.length(); k++)
        {
            Trigger thisTrigger;
            //start out by setting defaults - should be moved to constructor for class Trigger.
            thisTrigger.bus = -1; //-1 means we don't care which
            thisTrigger.ID = -1; //the rest of these being -1 means nothing has changed it
            thisTrigger.maxCount = -1;
            thisTrigger.milliseconds = -1;
            thisTrigger.currCount = 0;
            thisTrigger.msCounter = 0;
            thisTrigger.readyCount = true;

            QStringList trigToks = triggers[k].split(' ');
            for (int x = 0; x < trigToks.length(); x++)
            {
                QString tok = trigToks.at(x);
                if (tok.left(3) == "ID=")
                {
                    thisTrigger.ID = Utility::ParseStringToNum(tok.right(tok.length() - 3));
                    if (thisTrigger.maxCount == -1) thisTrigger.maxCount = 10000000;

                    if (thisTrigger.milliseconds == -1) thisTrigger.milliseconds = 0; //by default don't count, just send it upon trigger
                    thisTrigger.readyCount = false; //won't try counting until trigger hits
                }
                else if (tok.endsWith("MS"))
                {
                    thisTrigger.milliseconds = Utility::ParseStringToNum(tok.left(tok.length()-2));
                    if (thisTrigger.maxCount == -1) thisTrigger.maxCount = 10000000;
                    if (thisTrigger.ID == -1) thisTrigger.ID = 0;
                }
                else if (tok.endsWith("X"))
                {
                    thisTrigger.maxCount = Utility::ParseStringToNum(tok.left(tok.length() - 1));
                    if (thisTrigger.ID == -1) thisTrigger.ID = 0;
                    if (thisTrigger.milliseconds == -1) thisTrigger.milliseconds = 10;
                }
                else if (tok.startsWith("BUS"))
                {
                    thisTrigger.bus = Utility::ParseStringToNum(tok.right(tok.length() - 3));
                }
            }
            //now, find anything that wasn't set and set it to defaults
            if (thisTrigger.maxCount == -1) thisTrigger.maxCount = 10000000;
            if (thisTrigger.milliseconds == -1) thisTrigger.milliseconds = 100;
            if (thisTrigger.ID == -1) thisTrigger.ID = 0;
            sendingData[line].triggers.append(thisTrigger);
        }
    }
    else //setup a default single shot trigger
    {
        Trigger thisTrigger;
        thisTrigger.bus = -1;
        thisTrigger.ID = 0;
        thisTrigger.maxCount = 1;
        thisTrigger.milliseconds = 10;
        sendingData[line].triggers.append(thisTrigger);
    }
}

//Turn a set of tokens into an operand
void FrameSenderWindow::parseOperandString(QStringList tokens, ModifierOperand &operand)
{
    //example string -> bus:0:id:200:d3

    operand.bus = -1;
    operand.ID = -2;
    operand.databyte = 0;

    for (int i = 0; i < tokens.length(); i++)
    {
        if (tokens[i] == "BUS")
        {
            operand.bus = Utility::ParseStringToNum(tokens[++i]);
        }
        else if (tokens[i] == "ID")
        {
            operand.ID = Utility::ParseStringToNum(tokens[++i]);
        }
        else if (tokens[i].length() == 2 && tokens[i].startsWith("D"))
        {
            operand.databyte = Utility::ParseStringToNum(tokens[i].right(tokens[i].length() - 1));
        }
        else
        {
            operand.databyte = Utility::ParseStringToNum(tokens[i]);
            operand.ID = 0; //special ID to show this is a number not a look up.
        }
    }
}

ModifierOperationType FrameSenderWindow::parseOperation(QString op)
{
    if (op == "+") return ADDITION;
    if (op == "-") return SUBTRACTION;
    if (op == "*") return MULTIPLICATION;
    if (op == "/") return DIVISION;
    if (op == "&") return AND;
    if (op == "AND") return AND;
    if (op == "|") return OR;
    if (op == "OR") return OR;
    if (op == "^") return XOR;
    if (op == "XOR") return XOR;
    return ADDITION;
}
//The below is c# code from the old project. It won't run as-is and must be ported to C++/Qt

/*
/// <summary>
/// Updaet the DataGridView with the newest info from sendingData
/// </summary>
/// <param name="idx"></param>
private void updateGridRow(int idx)
{
    FrameSendData temp = sendingData[idx];
    int gridLine = idx;
    StringBuilder dataString = new StringBuilder();
    dataGridView1.Rows[gridLine].Cells[7].Value = temp.count.ToString();
    for (int i = 0; i < temp.len; i++)
    {
        dataString.Append("0x");
        dataString.Append(temp.data[i].ToString("X2"));
        dataString.Append(" ");
    }
    dataGridView1.Rows[gridLine].Cells[4].Value = dataString.ToString();
}

public delegate void GotCANDelegate(CANFrame frame);
/// <summary>
/// Event callback for reception of canbus frames
/// </summary>
/// <param name="frame">The frame that came in</param>
public void GotCANFrame(CANFrame frame)
{
    if (this.InvokeRequired)
    {
        this.Invoke(new GotCANDelegate(GotCANFrame), frame);
    }
    else
    {
        //search list of frames and add if no frame with this ID is found
        //or update if one was found.
        bool found = false;
        for (int a = 0; a < frames.Count; a++)
        {
            if ((frame.ID == frames[a].ID) && (frame.bus == frames[a].bus))
            {
                found = true;
                frames[a].len = frame.len;
                frames[a].data = frame.data;
            }
        }
        if (!found)
        {
            frames.Add(frame);
        }

        //now that frame cache has been updated, try to see if this incoming frame
        //satisfies any triggers

        for (int b = 0; b < sendingData.Count; b++)
        {
            if (sendingData[b].triggers == null) continue;
            for (int c = 0; c < sendingData[b].triggers.Length; c++)
            {
                Trigger thisTrigger = sendingData[b].triggers[c];
                if (thisTrigger.ID > 0)
                {
                    if ((thisTrigger.bus == frame.bus) || (thisTrigger.bus == -1))
                    {
                        if (thisTrigger.ID == frame.ID)
                        {
                            //seems to match this trigger.
                            if (thisTrigger.currCount < thisTrigger.maxCount)
                            {
                                if (thisTrigger.milliseconds == 0) //don't want to time the frame, send it right away
                                {
                                    sendingData[b].triggers[c].currCount++;
                                    sendingData[b].count++;
                                    doModifiers(b);
                                    updateGridRow(b);
                                    parent.SendCANFrame(sendingData[b], sendingData[b].bus);
                                }
                                else //instead of immediate sending we start the timer
                                {
                                    sendingData[b].triggers[c].readyCount = true;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

private void dataGridView1_RowsRemoved(object sender, DataGridViewRowsRemovedEventArgs e)
{
    int rec = e.RowIndex;
    if (rec != -1) sendingData.RemoveAt(rec);
}

private void processCellChange(int line, int col)
{
    FrameSendData tempData;
    int tempVal;

    //if this is a new line then create the base object for the line
    if ((line >= sendingData.Count) || (sendingData[line] == null))
    {
        tempData = new FrameSendData();
        sendingData.Add(tempData);
    }

    sendingData[line].count = 0;

    switch (col)
    {
        case 0: //Enable check box
            if (dataGridView1.Rows[line].Cells[0].Value != null)
            {
                sendingData[line].enabled = (bool)dataGridView1.Rows[line].Cells[0].Value;
            }
            else sendingData[line].enabled = false;
            break;
        case 1: //Bus designation
            tempVal = Utility.ParseStringToNum((string)dataGridView1.Rows[line].Cells[1].Value);
            if (tempVal < 0) tempVal = 0;
            if (tempVal > 1) tempVal = 1;
            sendingData[line].bus = tempVal;
            break;
        case 2: //ID field
            tempVal = Utility.ParseStringToNum((string)dataGridView1.Rows[line].Cells[2].Value);
            if (tempVal < 1) tempVal = 1;
            if (tempVal > 0x7FFFFFFF) tempVal = 0x7FFFFFFF;
            sendingData[line].ID = tempVal;
            if (sendingData[line].ID <= 0x7FF) sendingData[line].extended = false;
            else sendingData[line].extended = true;
            break;
        case 3: //length field
            tempVal = Utility.ParseStringToNum((string)dataGridView1.Rows[line].Cells[3].Value);
            if (tempVal < 0) tempVal = 0;
            if (tempVal > 8) tempVal = 8;
            sendingData[line].len = tempVal;
            break;
        case 4: //Data bytes
            for (int i = 0; i < 8; i++) sendingData[line].data[i] = 0;

            string[] tokens = ((string)dataGridView1.Rows[line].Cells[4].Value).Split(' ');
            for (int j = 0; j < tokens.Length; j++)
            {
                sendingData[line].data[j] = (byte)Utility.ParseStringToNum(tokens[j]);
            }
            break;
        case 5: //triggers
            processTriggerText(line);
            break;
        case 6: //modifiers
            processModifierText(line);
            break;
    }
}

private void dataGridView1_CellEndEdit(object sender, DataGridViewCellEventArgs e)
{
    processCellChange(e.RowIndex, e.ColumnIndex);
}

private void loadFromFileToolStripMenuItem_Click(object sender, EventArgs e)
{
    DialogResult res;
    //First zap all existing data. Ask first. Might not need to actually ask here since you can abort the file open
    //and be OK that way too.
    if (sendingData.Count > 0)
    {
        res = MessageBox.Show("Proceding will erase the current\ncontents of the grid.", "Warning", MessageBoxButtons.YesNo);
        if (res == DialogResult.No) return;
    }
    res = openFileDialog1.ShowDialog();
    if (res == DialogResult.OK)
    {
        sendingData.Clear();
        dataGridView1.Rows.Clear();
        Stream inputFile = openFileDialog1.OpenFile();
        StreamReader inputReader = new StreamReader(inputFile);
        string line;
        int row;
        try
        {
            while (!inputReader.EndOfStream)
            {
                line = inputReader.ReadLine();
                string[] tokens = line.Split('#');
                row = dataGridView1.Rows.Add();
                if (tokens[0] == "T")
                {
                    dataGridView1.Rows[row].Cells[0].Value = true;
                }
                else dataGridView1.Rows[row].Cells[0].Value = false;
                for (int j = 1; j < 7; j++)
                {
                    dataGridView1.Rows[row].Cells[j].Value = tokens[j];
                }
                for (int k = 0; k < 7; k++) processCellChange(row, k);
            }
        }
        catch (Exception ee)
        {
            Debug.Print(ee.ToString());
        }
    }
}

private void saveGridToolStripMenuItem_Click(object sender, EventArgs e)
{
    StringBuilder buildStr = new StringBuilder();
    DialogResult res;
    res = saveFileDialog1.ShowDialog();
    if (res == DialogResult.OK)
    {
        Stream outputFile = saveFileDialog1.OpenFile();
        StreamWriter outputWriter = new StreamWriter(outputFile);
        try
        {
            for (int i = 0; i < sendingData.Count; i++)
            {
                buildStr.Clear();
                if (dataGridView1.Rows[i].Cells[0].Value != null)
                {
                    if ((bool)dataGridView1.Rows[i].Cells[0].Value == true)
                    {
                        buildStr.Append("T");
                    }
                    else buildStr.Append("F");
                }
                else buildStr.Append("F");
                buildStr.Append("#");
                for (int j = 1; j < 7; j++)
                {
                    buildStr.Append(dataGridView1.Rows[i].Cells[j].Value);
                    buildStr.Append("#");
                }
                outputWriter.WriteLine(buildStr.ToString());
            }
        }
        catch (Exception ee)
        {
            Debug.Print(ee.ToString());
        }
        outputWriter.Flush();
        outputWriter.Close();
    }
}

//Note for the below three functions that they all handle the fact that a data view grid will always have an extra row that is blank
//and available for a new record. Because of this the grid count is always one too high.
private void button1_Click(object sender, EventArgs e) //enable all lines in the grid
{
    for (int i = 0; i < dataGridView1.Rows.Count - 1; i++)
    {
        dataGridView1.Rows[i].Cells[0].Value = true;
        sendingData[i].enabled = true;
    }
}

private void button2_Click(object sender, EventArgs e) //disable all lines in the grid
{
    for (int i = 0; i < dataGridView1.Rows.Count - 1; i++)
    {
        dataGridView1.Rows[i].Cells[0].Value = false;
        sendingData[i].enabled = false;
    }
}

private void button3_Click(object sender, EventArgs e) //delete all entries in the grid and data struct
{
    if (dataGridView1.Rows.Count == 1) return;
    for (int i = dataGridView1.Rows.Count - 2; i >= 0; i--)
    {
        sendingData[i].enabled = false;
        sendingData.RemoveAt(i);
        dataGridView1.Rows.RemoveAt(i);
    }
}

*/
