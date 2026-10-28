#ifndef GITHUBDEVICEFLOW_H
#define GITHUBDEVICEFLOW_H

#include <QObject>
#include <QTimer>
#include <QDateTime>

class QNetworkAccessManager;
class QNetworkReply;

//"Sign in with GitHub" by OAuth device flow
//(https://docs.github.com/apps/oauth-apps/building-oauth-apps/authorizing-oauth-apps#device-flow):
//GitHub gives a short code, the user enters it at github.com/login/device, and this polls until they approve.
//Needs only the OAuth App's client ID, no secret.
class GitHubDeviceFlow : public QObject
{
    Q_OBJECT

public:
    explicit GitHubDeviceFlow(QObject *parent = nullptr);
    ~GitHubDeviceFlow();

    void setWebUrl(const QString &url); //https://github.com
    void start(const QString &clientId, const QString &scope);
    void cancel();
    bool isRunning() const;

signals:
    void codeReady(const QString &userCode, const QString &verificationUrl);
    void signedIn(const QString &token);
    void failed(const QString &error);

private:
    QNetworkAccessManager *manager;
    QNetworkReply *pending;
    QTimer pollTimer;
    QString webUrl;
    QString clientId;
    QString deviceCode;
    int intervalSecs;
    QDateTime expires;

    QNetworkReply *post(const QString &path, const QByteArray &form);
    void poll();
    void fail(const QString &error);
};

#endif // GITHUBDEVICEFLOW_H
