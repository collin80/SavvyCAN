#ifndef SHORTCUTCHECK_H
#define SHORTCUTCHECK_H

#include <QKeySequence>
#include <QString>

//Whether a key can be given to something new (the marker presets): what SavvyCAN, Qt and the desktop already use it
//for. See docs/log-notes-roadmap.md section 8.3 for where the list comes from; keep the two in step.
class ShortcutCheck
{
public:
    enum Verdict { Free, Warn, Refused };

    //reason: what uses the key ("Open Log File"), empty when it's free
    static Verdict check(const QKeySequence &key, QString &reason);
};

#endif // SHORTCUTCHECK_H
