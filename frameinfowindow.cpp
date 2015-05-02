#include "frameinfowindow.h"
#include "ui_frameinfowindow.h"

FrameInfoWindow::FrameInfoWindow(QList<CANFrame> *frames, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::FrameInfoWindow)
{
    ui->setupUi(this);
    modelFrames = frames;
    refreshIDList();
}

FrameInfoWindow::~FrameInfoWindow()
{
    delete ui;
}

void FrameInfoWindow::updateDetailsWindow(int frameIdx)
{
    int idx, numFrames, targettedID;
    int minLen, maxLen, thisLen;
    int minData[8];
    int maxData[8];
    int dataHistogram[256][8];
    //TreeNode baseNode, dataBase, histBase, numBase;
/*
    if (frameIdx > -1)
    {
        parseFrameCache();
        targettedID = Utility.ParseStringToNum(listFrameIDs.Items[listFrameIDs.SelectedIndex].ToString());
        idx = getIdxForID(targettedID);
        numFrames = frames[idx].Count;

        treeDetails.Nodes.Clear();
        baseNode = treeDetails.Nodes.Add("ID: " + listFrameIDs.Items[listFrameIDs.SelectedIndex].ToString());
        if (frames[idx].ElementAt(0).extended) //if these frames seem to be extended then try for J1939 decoding
        {
            J1939ID jid = new J1939ID();
            jid.src = targettedID & 0xFF;
            jid.priority = targettedID >> 26;
            jid.pgn = (targettedID >> 8) & 0x3FFFF; //18 bits
            jid.pf = (targettedID >> 16) & 0xFF;
            jid.ps = (targettedID >> 8) & 0xFF;

            if (jid.pf > 0xEF)
            {
                jid.isBroadcast = true;
                jid.dest = 0xFFFF;
                baseNode.Nodes.Add("Broadcast frame");
            }
            else
            {
                jid.dest = jid.ps;
                baseNode.Nodes.Add("Destination ID: 0x" + jid.dest.ToString("X2"));
            }
            baseNode.Nodes.Add("SRC: 0x" + jid.src.ToString("X2"));
            baseNode.Nodes.Add("PGN: " + jid.pgn.ToString());
            baseNode.Nodes.Add("PF: 0x" + jid.pf.ToString("X2"));
            baseNode.Nodes.Add("PS: 0x" + jid.ps.ToString("X2"));
        }
        treeDetails.Nodes.Add("# Of Frames: " + numFrames.ToString());
        minLen = 8;
        maxLen = 0;
        for (int i = 0; i < 8; i++)
        {
            minData[i] = 256;
            maxData[i] = -1;
            for (int k = 0; k < 256; k++) dataHistogram[k, i] = 0;
        }
        for (int j = 0; j < numFrames; j++)
        {
            thisLen = frames[idx].ElementAt(j).len;
            if (thisLen > maxLen) maxLen = thisLen;
            if (thisLen < minLen) minLen = thisLen;
            for (int c = 0; c < thisLen; c++)
            {
                byte dat = frames[idx].ElementAt(j).data[c];
                if (minData[c] > dat) minData[c] = dat;
                if (maxData[c] < dat) maxData[c] = dat;
                dataHistogram[dat, c]++; //add one to count for this
            }
        }
        if (minLen < maxLen)
            baseNode = treeDetails.Nodes.Add("Data Length: " + minLen.ToString() + " to " + maxLen.ToString());
        else
            baseNode = treeDetails.Nodes.Add("Data Length: " + minLen.ToString());

        for (int d = 0; d < numGraphs; d++)
        {
            Graphs[d].valueCache = null;
            Graphs[d].ID = 0;
        }

        for (int c = 0; c < maxLen; c++)
        {
            Graphs[6 + c].bias = 0;
            Graphs[6 + c].scale = 1;
            Graphs[6 + c].mask = 0xFF;
            Graphs[6 + c].B1 = c;
            Graphs[6 + c].B2 = c;
            Graphs[6 + c].ID = targettedID;
            Graphs[6 + c].color = theseColors[c];
            Graphs[6 + c].signed = false;
            if (numFrames > 100)
            {
                Graphs[6 + c].stride = (int)((((float)numFrames) / 100.0f) + 0.5f);
            }
            else Graphs[6 + c].stride = 1;

            dataBase = baseNode.Nodes.Add("Data Byte " + c.ToString());
            dataBase.Nodes.Add("Range: " + minData[c] + " to " + maxData[c]);
            histBase = dataBase.Nodes.Add("Histogram");
            for (int d = 0; d < 256; d++)
            {
                if (dataHistogram[d, c] > 0)
                {
                    numBase = histBase.Nodes.Add(d.ToString() + "/0x" + d.ToString("X2") + ": " + dataHistogram[d, c]);
                }
            }
        }

        parseFrameCache();
        for (int c = 0; c < maxLen; c++) setupGraph(6 + c);
        setupGraphs();
    }
    else
    {
    }
*/
}

void FrameInfoWindow::refreshIDList()
{
    int id;
    for (int i = 0; i < modelFrames->count(); i++)
    {
        id = modelFrames->at(i).ID;
        if (!foundID.contains(id))
        {
            foundID.append(id);
            ui->listFrameID->addItem(QString::number(id, 16).toUpper().rightJustified(4,'0'));
        }
    }
    //default is to sort in ascending order
    ui->listFrameID->sortItems();
}
