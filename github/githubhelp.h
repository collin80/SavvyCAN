#ifndef GITHUBHELP_H
#define GITHUBHELP_H

#include <QString>
#include <functional>

//Help (F1, and the Help buttons) in the GitHub windows opens help/github.md. MainWindow points this at SavvyCAN's help
//window; left empty (the unit tests), help does nothing.
inline std::function<void(const QString &page)> &gitHubHelp()
{
    static std::function<void(const QString &page)> show;
    return show;
}

inline void showGitHubHelp()
{
    if (gitHubHelp()) gitHubHelp()("github.md");
}

#endif // GITHUBHELP_H
