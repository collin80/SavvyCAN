#include "shortcutcheck.h"

#include <QCoreApplication>
#include <QList>
#include <QPair>

namespace
{
struct KnownKey
{
    QKeyCombination key;
    const char *use;
};

QString tr(const char *text)
{
    return QCoreApplication::translate("ShortcutCheck", text);
}

//used by SavvyCAN itself, Qt's text fields, or Windows/Qt window handling
const QList<KnownKey> &refusedKeys()
{
    static const QList<KnownKey> keys {
        { QKeyCombination(Qt::ControlModifier, Qt::Key_M), "Add Marker (typed)" },
        { QKeyCombination(Qt::ControlModifier, Qt::Key_O), "Open Log File" },
        { QKeyCombination(Qt::ControlModifier, Qt::Key_C), "copying frames and text" },
        { QKeyCombination(Qt::ControlModifier, Qt::Key_W), "closing the window" },
        { QKeyCombination(Qt::ControlModifier, Qt::Key_F4), "closing the window" },
        { QKeyCombination(Qt::ControlModifier, Qt::Key_Z), "undo (signal editor, text fields)" },
        { QKeyCombination(Qt::ControlModifier, Qt::Key_Y), "redo in text fields" },
        { QKeyCombination(Qt::ControlModifier | Qt::ShiftModifier, Qt::Key_Z), "redo in text fields" },
        { QKeyCombination(Qt::ControlModifier, Qt::Key_A), "selecting all in tables and text fields" },
        { QKeyCombination(Qt::ControlModifier, Qt::Key_V), "pasting" },
        { QKeyCombination(Qt::ControlModifier, Qt::Key_X), "cutting" },
        { QKeyCombination(Qt::ControlModifier, Qt::Key_Tab), "switching tabs" },
        { QKeyCombination(Qt::ControlModifier | Qt::ShiftModifier, Qt::Key_Tab), "switching tabs" },
        { QKeyCombination(Qt::ControlModifier | Qt::ShiftModifier, Qt::Key_Backtab), "switching tabs" },
        { QKeyCombination(Qt::ControlModifier, Qt::Key_Escape), "the Start menu" },
        { QKeyCombination(Qt::ControlModifier | Qt::ShiftModifier, Qt::Key_Escape), "Task Manager" },
        { QKeyCombination(Qt::AltModifier, Qt::Key_F4), "closing the window" },
        { QKeyCombination(Qt::AltModifier, Qt::Key_Tab), "switching windows" },
        { QKeyCombination(Qt::AltModifier, Qt::Key_Escape), "switching windows" },
        { QKeyCombination(Qt::AltModifier, Qt::Key_Space), "the window menu" },
    };
    return keys;
}

//common in other programs, but not used by SavvyCAN: allowed after a warning
const QList<KnownKey> &commonKeys()
{
    static const QList<KnownKey> keys {
        { QKeyCombination(Qt::ControlModifier, Qt::Key_S), "saving, in most programs" },
        { QKeyCombination(Qt::ControlModifier, Qt::Key_N), "a new file, in most programs" },
        { QKeyCombination(Qt::ControlModifier, Qt::Key_P), "printing, in most programs" },
        { QKeyCombination(Qt::ControlModifier, Qt::Key_F), "finding, in most programs" },
        { QKeyCombination(Qt::ControlModifier, Qt::Key_Q), "quitting, on Linux and macOS" },
        { QKeyCombination(Qt::ControlModifier, Qt::Key_H), "replacing or history, in many programs" },
        { QKeyCombination(Qt::ControlModifier, Qt::Key_R), "reloading, in many programs" },
        { QKeyCombination(Qt::NoModifier, Qt::Key_F11), "full screen, in many programs" },
    };
    return keys;
}
}

ShortcutCheck::Verdict ShortcutCheck::check(const QKeySequence &key, QString &reason)
{
    reason.clear();
    if (key.isEmpty()) return Free;
    if (key.count() > 1)
    {
        reason = tr("a key sequence of several presses; use one key (with Ctrl or Alt)");
        return Refused;
    }

    const QKeyCombination combo(key[0].keyboardModifiers() & ~Qt::KeypadModifier, key[0].key());
    const Qt::KeyboardModifiers mods = combo.keyboardModifiers();
    const Qt::Key k = combo.key();

    if (mods & Qt::MetaModifier)
    {
        reason = tr("the desktop (Windows or Super key)");
        return Refused;
    }
    if ((mods & Qt::ControlModifier) && (mods & Qt::AltModifier))
    {
        reason = tr("AltGr on many keyboards (typing characters such as @ and €), and the desktop on Linux");
        return Refused;
    }

    for (const KnownKey &known : refusedKeys())
        if (known.key == combo)
        {
            reason = tr(known.use);
            return Refused;
        }

    if (!(mods & (Qt::ControlModifier | Qt::AltModifier)))
    {
        //function keys alone don't type anything; some are taken
        if (k == Qt::Key_F1) reason = tr("Help, in every SavvyCAN window");
        else if (k >= Qt::Key_F3 && k <= Qt::Key_F7) reason = tr("the DBC editor");
        else if (k == Qt::Key_F10) reason = tr("the menu bar, on Windows");
        else if (k >= Qt::Key_F2 && k <= Qt::Key_F35) reason.clear();
        else reason = tr("typing (and keys such as +, −, R, T, Y and Space in graphs, the flow view and lists)");
        if (!reason.isEmpty()) return Refused;
    }

    for (const KnownKey &known : commonKeys())
        if (known.key == combo)
        {
            reason = tr(known.use);
            return Warn;
        }
    return Free;
}
