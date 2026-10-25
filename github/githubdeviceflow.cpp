#include "githubdeviceflow.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

GitHubDeviceFlow::GitHubDeviceFlow(QObject *parent)
    : QObject(parent),
      manager(new QNetworkAccessManager(this)),
      pending(nullptr),
      webUrl("https://github.com"),
      intervalSecs(5)
{
    pollTimer.setSingleShot(true);
    connect(&pollTimer, &QTimer::timeout, this, &GitHubDeviceFlow::poll);
}

GitHubDeviceFlow::~GitHubDeviceFlow()
{
    cancel();
}

void GitHubDeviceFlow::setWebUrl(const QString &url)
{
    webUrl = url;
    while (webUrl.endsWith('/')) webUrl.chop(1);
}

bool GitHubDeviceFlow::isRunning() const
{
    return pending || pollTimer.isActive();
}

QNetworkReply *GitHubDeviceFlow::post(const QString &path, const QByteArray &form)
{
    QNetworkRequest req(QUrl(webUrl + path));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("User-Agent", "SavvyCAN");
    pending = manager->post(req, form);
    return pending;
}

void GitHubDeviceFlow::start(const QString &id, const QString &scope)
{
    cancel();
    clientId = id;

    QUrlQuery form;
    form.addQueryItem("client_id", clientId);
    form.addQueryItem("scope", scope);
    QNetworkReply *reply = post("/login/device/code", form.toString(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        reply->deleteLater();
        pending = nullptr;
        const QJsonObject json = QJsonDocument::fromJson(reply->readAll()).object();
        if (reply->error() != QNetworkReply::NoError || json.contains("error") || !json.contains("device_code"))
        {
            const QString why = json.value("error_description").toString();
            fail(tr("GitHub didn't start the sign-in: %1").arg(why.isEmpty() ? reply->errorString() : why));
            return;
        }
        deviceCode = json.value("device_code").toString();
        intervalSecs = json.value("interval").toInt(5);
        expires = QDateTime::currentDateTimeUtc().addSecs(json.value("expires_in").toInt(900));
        emit codeReady(json.value("user_code").toString(), json.value("verification_uri").toString());
        pollTimer.start(intervalSecs * 1000);
    });
}

void GitHubDeviceFlow::poll()
{
    if (QDateTime::currentDateTimeUtc() > expires)
    {
        fail(tr("The code expired before it was entered: start the sign-in again."));
        return;
    }

    QUrlQuery form;
    form.addQueryItem("client_id", clientId);
    form.addQueryItem("device_code", deviceCode);
    form.addQueryItem("grant_type", "urn:ietf:params:oauth:grant-type:device_code");
    QNetworkReply *reply = post("/login/oauth/access_token", form.toString(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        reply->deleteLater();
        pending = nullptr;
        if (reply->error() != QNetworkReply::NoError)
        {
            fail(tr("Couldn't reach GitHub: %1").arg(reply->errorString()));
            return;
        }
        const QJsonObject json = QJsonDocument::fromJson(reply->readAll()).object();
        const QString error = json.value("error").toString();
        const QString token = json.value("access_token").toString();

        if (!token.isEmpty())
        {
            deviceCode.clear();
            emit signedIn(token);
        }
        else if (error == "authorization_pending") pollTimer.start(intervalSecs * 1000);
        else if (error == "slow_down")
        {
            //GitHub says how long to wait now; the spec says 5 s more otherwise
            intervalSecs = json.contains("interval") ? json.value("interval").toInt() : intervalSecs + 5;
            pollTimer.start(intervalSecs * 1000);
        }
        else if (error == "expired_token") fail(tr("The code expired before it was entered: start the sign-in again."));
        else if (error == "access_denied") fail(tr("The sign-in was turned down on GitHub."));
        else
        {
            const QString why = json.value("error_description").toString();
            fail(tr("GitHub didn't finish the sign-in: %1").arg(why.isEmpty() ? error : why));
        }
    });
}

void GitHubDeviceFlow::cancel()
{
    pollTimer.stop();
    deviceCode.clear();
    if (pending)
    {
        pending->disconnect(this);
        pending->abort();
        pending->deleteLater();
        pending = nullptr;
    }
}

void GitHubDeviceFlow::fail(const QString &error)
{
    pollTimer.stop();
    deviceCode.clear();
    emit failed(error);
}
