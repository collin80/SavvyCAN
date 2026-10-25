#include <QtTest/QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QUrlQuery>
#include "github/githubaccountdialog.h"
#include "github/githubbrowserdialog.h"
#include "github/githubcache.h"
#include "github/githubclient.h"
#include "github/githubdeviceflow.h"
#include "github/githubsettings.h"

// GitHub repos (phase 1 of docs/github-repos-roadmap.md): the API client and the device flow sign-in against a fake
// GitHub on a local port, the download cache, the settings, and matching file names to file types.

struct FakeRequest
{
    QByteArray method;
    QByteArray target;  //path and query, as sent
    QHash<QByteArray, QByteArray> headers; //lower-case names
    QByteArray body;
};

struct FakeResponse
{
    int status = 200;
    QByteArray body;
    QList<QPair<QByteArray, QByteArray>> headers;
    bool noAnswer = false; //never answer (for requests still running when the client goes)

    void addHeader(const QByteArray &name, const QByteArray &value) { headers.append(qMakePair(name, value)); }
};

//just enough HTTP/1.1 for QNetworkAccessManager: one request per connection, then close
class FakeGitHub : public QObject
{
    Q_OBJECT

public:
    std::function<FakeResponse(const FakeRequest &)> handler;
    QList<FakeRequest> requests;

    FakeGitHub()
    {
        server.listen(QHostAddress::LocalHost);
        connect(&server, &QTcpServer::newConnection, this, [this]()
        {
            while (QTcpSocket *socket = server.nextPendingConnection())
            {
                connect(socket, &QTcpSocket::readyRead, this, [this, socket]() { readFrom(socket); });
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            }
        });
    }
    QString url() const { return QString("http://127.0.0.1:%1").arg(server.serverPort()); }

private:
    QTcpServer server;
    QHash<QTcpSocket *, QByteArray> buffers;

    void readFrom(QTcpSocket *socket)
    {
        QByteArray &buffer = buffers[socket];
        buffer += socket->readAll();
        const int headerEnd = int(buffer.indexOf("\r\n\r\n"));
        if (headerEnd < 0) return;

        FakeRequest request;
        const QList<QByteArray> lines = buffer.left(headerEnd).split('\n');
        const QList<QByteArray> first = lines[0].trimmed().split(' ');
        request.method = first.value(0);
        request.target = first.value(1);
        for (int i = 1; i < lines.count(); i++)
        {
            const int colon = int(lines[i].indexOf(':'));
            if (colon > 0) request.headers[lines[i].left(colon).trimmed().toLower()] = lines[i].mid(colon + 1).trimmed();
        }
        const int length = request.headers.value("content-length", "0").toInt();
        if (buffer.size() < headerEnd + 4 + length) return;
        request.body = buffer.mid(headerEnd + 4, length);
        buffers.remove(socket);
        requests.append(request);

        const FakeResponse response = handler ? handler(request) : FakeResponse { 404, "{}", {}, false };
        if (response.noAnswer) return;
        QByteArray out = "HTTP/1.1 " + QByteArray::number(response.status) + " Status\r\n";
        out += "Content-Length: " + QByteArray::number(response.body.size()) + "\r\nConnection: close\r\n";
        for (const auto &header : response.headers) out += header.first + ": " + header.second + "\r\n";
        out += "\r\n" + response.body;
        socket->write(out);
        socket->disconnectFromHost();
    }
};

static FakeResponse json(const QByteArray &body, int status = 200)
{
    FakeResponse response;
    response.status = status;
    response.body = body;
    response.addHeader("Content-Type", "application/json");
    return response;
}

class TestGitHub : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testParseRepo_data();
    void testParseRepo();
    void testSettings();
    void testCachePaths();
    void testCacheStoreAndOrigin();
    void testMatchesFileType();
    void testGetUser();
    void testNoToken();
    void testGetRepo();
    void testListBranchesPaged();
    void testGetTree();
    void testDownload();
    void testErrors();
    void testUnreachable();
    void testDeleteWhileRunning();
    void testDeviceFlow();
    void testDeviceFlowRefused_data();
    void testDeviceFlowRefused();
    void testBrowserDialog();
    void testAccountDialog();

private:
    QTemporaryDir settingsDir;
};

void TestGitHub::initTestCase()
{
    //settings in a temporary folder, never the user's own
    QVERIFY(settingsDir.isValid());
    QCoreApplication::setOrganizationName("SavvyCAN-test");
    QCoreApplication::setApplicationName("TestGitHub");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir.path());
}

void TestGitHub::testParseRepo_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<bool>("valid");
    QTest::addColumn<QString>("owner");
    QTest::addColumn<QString>("name");

    QTest::newRow("plain") << "tomdebree/savvycan-github-test" << true << "tomdebree" << "savvycan-github-test";
    QTest::newRow("spaces") << "  Tom-evnut/SavvyCAN " << true << "Tom-evnut" << "SavvyCAN";
    QTest::newRow("link") << "https://github.com/Tom-evnut/SavvyCAN" << true << "Tom-evnut" << "SavvyCAN";
    QTest::newRow("link .git") << "https://github.com/Tom-evnut/SavvyCAN.git" << true << "Tom-evnut" << "SavvyCAN";
    QTest::newRow("link to file") << "https://github.com/a/b/blob/main/logs/x.csv" << true << "a" << "b";
    QTest::newRow("no scheme") << "github.com/a/b.c_d" << true << "a" << "b.c_d";
    QTest::newRow("www") << "https://www.github.com/a/b/" << true << "a" << "b";
    QTest::newRow("one part") << "tomdebree" << false << "" << "";
    QTest::newRow("three parts") << "a/b/c" << false << "" << "";
    QTest::newRow("other host") << "https://gitlab.com/a/b" << false << "" << "";
    QTest::newRow("bad owner") << "a_b/c" << false << "" << "";
    QTest::newRow("dots") << "a/.." << false << "" << "";
    QTest::newRow("empty") << "" << false << "" << "";
}

void TestGitHub::testParseRepo()
{
    QFETCH(QString, text);
    QFETCH(bool, valid);
    QFETCH(QString, owner);
    QFETCH(QString, name);

    QString gotOwner, gotName;
    QCOMPARE(GitHubSettings::parseRepo(text, gotOwner, gotName), valid);
    if (valid)
    {
        QCOMPARE(gotOwner, owner);
        QCOMPARE(gotName, name);
    }
}

void TestGitHub::testSettings()
{
    GitHubSettings::clearToken();
    QVERIFY(GitHubSettings::token().isEmpty());
    GitHubSettings::setToken("github_pat_abc123", false);
    QCOMPARE(GitHubSettings::token(), QString("github_pat_abc123"));
    QVERIFY(!GitHubSettings::tokenFromDeviceFlow());
    QVERIFY(!QSettings().value("GitHub/Token").toByteArray().contains("abc123")); //not stored as plain text
    GitHubSettings::setToken("gho_xyz", true);
    QVERIFY(GitHubSettings::tokenFromDeviceFlow());
    GitHubSettings::clearToken();
    QVERIFY(GitHubSettings::token().isEmpty());

    QCOMPARE(GitHubSettings::apiUrl(), QString("https://api.github.com"));
    QCOMPARE(GitHubSettings::webUrl(), QString("https://github.com"));
    GitHubSettings::setClientId("  Iv1.abc ");
    QCOMPARE(GitHubSettings::clientId(), QString("Iv1.abc"));
    GitHubSettings::setClientId("");
    QCOMPARE(GitHubSettings::clientId(), QString());

    GitHubRepo logs;
    logs.owner = "a";
    logs.name = "logs";
    logs.branch = "main";
    logs.dbcs = false;
    GitHubRepo dbcs;
    dbcs.owner = "b";
    dbcs.name = "dbcs";
    dbcs.branch = "feature/x";
    dbcs.logs = false;
    GitHubSettings::setRepos({ logs, dbcs });
    QList<GitHubRepo> read = GitHubSettings::repos();
    QCOMPARE(read.count(), 2);
    QCOMPARE(read[0].fullName(), QString("a/logs"));
    QVERIFY(read[0].logs && !read[0].dbcs);
    QCOMPARE(read[1].branch, QString("feature/x"));
    QVERIFY(!read[1].logs && read[1].dbcs);

    GitHubSettings::setRepos({ dbcs });
    QCOMPARE(GitHubSettings::repos().count(), 1);
    GitHubSettings::setRepos({});
    QVERIFY(GitHubSettings::repos().isEmpty());
}

void TestGitHub::testCachePaths()
{
    GitHubCache::setRootDir("/cache");
    GitHubOrigin origin { "owner", "repo", "main", "logs/2026/run 1.csv", "abc" };
    QCOMPARE(GitHubCache::localPath(origin), QString("/cache/owner/repo/main/logs/2026/run 1.csv"));

    origin.branch = "feature/lin"; //can't become two folders
    QCOMPARE(GitHubCache::localPath(origin), QString("/cache/owner/repo/feature%2Flin/logs/2026/run 1.csv"));

    origin.branch = "main";
    for (const QString &bad : { "../x.csv", "logs/../../x.csv", "/x.csv", "logs//x.csv", "a\\b.csv", "c:x.csv",
                                "x.csv.origin.json" })
    {
        origin.path = bad;
        QVERIFY2(GitHubCache::localPath(origin).isEmpty(), qPrintable(bad));
    }
    origin.path = "x.csv";
    origin.owner = "..";
    QVERIFY(GitHubCache::localPath(origin).isEmpty());
    QVERIFY(GitHubCache::localPath(GitHubOrigin()).isEmpty());
}

void TestGitHub::testCacheStoreAndOrigin()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    GitHubCache::setRootDir(dir.filePath("github"));

    const GitHubOrigin origin { "owner", "repo", "main", "dbc/car.dbc", "0123abcd" };
    QString localFile, error;
    QVERIFY2(GitHubCache::store(origin, "VERSION \"\"\n", localFile, error), qPrintable(error));
    QCOMPARE(localFile, dir.filePath("github/owner/repo/main/dbc/car.dbc"));
    QFile file(localFile);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), QByteArray("VERSION \"\"\n"));
    file.close();

    const GitHubOrigin read = GitHubCache::originOf(localFile);
    QVERIFY(read.isValid());
    QCOMPARE(read.owner, origin.owner);
    QCOMPARE(read.branch, origin.branch);
    QCOMPARE(read.path, origin.path);
    QCOMPARE(read.sha, origin.sha);
    QCOMPARE(read.displayName(), QString("github:owner/repo/dbc/car.dbc"));

    //a copy somewhere else (with its .origin.json) isn't the cached file
    QVERIFY(QFile::copy(localFile, dir.filePath("car.dbc")));
    QVERIFY(QFile::copy(localFile + ".origin.json", dir.filePath("car.dbc.origin.json")));
    QVERIFY(!GitHubCache::originOf(dir.filePath("car.dbc")).isValid());
    QVERIFY(!GitHubCache::originOf(dir.filePath("nothing.dbc")).isValid());

    GitHubOrigin bad = origin;
    bad.path = "../car.dbc";
    QVERIFY(!GitHubCache::store(bad, "x", localFile, error));
    QVERIFY(!error.isEmpty());

    QVERIFY(GitHubCache::clear());
    QVERIFY(!QFile::exists(dir.filePath("github")));
    QVERIFY(QFile::exists(dir.filePath("car.dbc")));
}

void TestGitHub::testMatchesFileType()
{
    QVERIFY(GitHubBrowserDialog::matchesFileType("x.csv", "GVRET Logs (*.csv *.CSV)"));
    QVERIFY(GitHubBrowserDialog::matchesFileType("X.Csv", "IXXAT CSV export (*.csv)")); //any case
    QVERIFY(!GitHubBrowserDialog::matchesFileType("x.csv.bak", "GVRET Logs (*.csv *.CSV)"));
    QVERIFY(!GitHubBrowserDialog::matchesFileType("x.txt", "GVRET Logs (*.csv *.CSV)"));
    QVERIFY(GitHubBrowserDialog::matchesFileType("README", "Autodetect File Type (*.*)"));
    QVERIFY(GitHubBrowserDialog::matchesFileType("x.pcap", "Wireshark SocketCAN (*.pcap *.PCAP")); //no ')'
    QVERIFY(GitHubBrowserDialog::matchesFileType("anything", "No patterns"));
    QVERIFY(GitHubBrowserDialog::matchesFileType("car.dbc", "DBC File (*.dbc)"));
}

void TestGitHub::testGetUser()
{
    FakeGitHub github;
    github.handler = [](const FakeRequest &) { return json(R"({"login":"tester","id":1})"); };
    GitHubClient client;
    client.setApiUrl(github.url() + "/");
    client.setToken(" gho_secret ");

    bool done = false;
    QString login, error;
    client.getUser([&](const QString &l, const QString &e) { login = l; error = e; done = true; });
    QTRY_VERIFY(done);
    QCOMPARE(error, QString());
    QCOMPARE(login, QString("tester"));

    const FakeRequest &request = github.requests.constFirst();
    QCOMPARE(request.method, QByteArray("GET"));
    QCOMPARE(request.target, QByteArray("/user"));
    QCOMPARE(request.headers.value("authorization"), QByteArray("Bearer gho_secret"));
    QCOMPARE(request.headers.value("accept"), QByteArray("application/vnd.github+json"));
    QCOMPARE(request.headers.value("x-github-api-version"), QByteArray("2022-11-28"));
    QCOMPARE(request.headers.value("user-agent"), QByteArray("SavvyCAN"));
}

void TestGitHub::testNoToken()
{
    FakeGitHub github;
    github.handler = [](const FakeRequest &) { return json(R"({"default_branch":"main"})"); };
    GitHubClient client;
    client.setApiUrl(github.url());

    bool done = false;
    client.getRepo("a", "b", [&](const QString &, const QString &) { done = true; });
    QTRY_VERIFY(done);
    QVERIFY(!github.requests.constFirst().headers.contains("authorization"));
}

void TestGitHub::testGetRepo()
{
    FakeGitHub github;
    github.handler = [](const FakeRequest &request)
    {
        if (request.target == "/repos/Tom-evnut/SavvyCAN") return json(R"({"default_branch":"fork-ci"})");
        if (request.target == "/repos/a/empty") return json(R"({"default_branch":null})");
        return json(R"({"message":"Not Found"})", 404);
    };
    GitHubClient client;
    client.setApiUrl(github.url());

    int done = 0;
    QString branch, error, emptyError;
    client.getRepo("Tom-evnut", "SavvyCAN", [&](const QString &b, const QString &e) { branch = b; error = e; done++; });
    client.getRepo("a", "empty", [&](const QString &, const QString &e) { emptyError = e; done++; });
    QTRY_COMPARE(done, 2);
    QCOMPARE(error, QString());
    QCOMPARE(branch, QString("fork-ci"));
    QVERIFY(!emptyError.isEmpty());
}

void TestGitHub::testListBranchesPaged()
{
    FakeGitHub github;
    github.handler = [](const FakeRequest &request)
    {
        if (request.target == "/repos/o/r/branches?per_page=100&page=1")
        {
            FakeResponse response = json(R"([{"name":"main","commit":{"sha":"c1"}},{"name":"feature/lin","commit":{"sha":"c2"}}])");
            response.addHeader("Link", "<https://api.github.com/repositories/1/branches?per_page=100&page=2>; rel=\"next\", "
                                              "<https://api.github.com/repositories/1/branches?per_page=100&page=2>; rel=\"last\"");
            return response;
        }
        if (request.target == "/repos/o/r/branches?per_page=100&page=2")
            return json(R"([{"name":"old","commit":{"sha":"c3"}}])");
        return json("{}", 404);
    };
    GitHubClient client;
    client.setApiUrl(github.url());

    bool done = false;
    QList<GitHubBranch> branches;
    QString error;
    client.listBranches("o", "r", [&](const QList<GitHubBranch> &b, const QString &e) { branches = b; error = e; done = true; });
    QTRY_VERIFY(done);
    QCOMPARE(error, QString());
    QCOMPARE(branches.count(), 3);
    QCOMPARE(branches[1].name, QString("feature/lin"));
    QCOMPARE(branches[1].commitSha, QString("c2"));
    QCOMPARE(branches[2].name, QString("old"));
    QCOMPARE(github.requests.count(), 2);
}

void TestGitHub::testGetTree()
{
    FakeGitHub github;
    github.handler = [](const FakeRequest &request)
    {
        if (request.target != "/repos/o/r/git/trees/c1?recursive=1") return json("{}", 404);
        return json(R"({"sha":"t1","truncated":true,"tree":[
            {"path":"logs","type":"tree","sha":"t2"},
            {"path":"logs/run 1.csv","type":"blob","sha":"b1","size":1234},
            {"path":"dbc/car.dbc","type":"blob","sha":"b2","size":99},
            {"path":"vendor/lib","type":"commit","sha":"s1"}]})");
    };
    GitHubClient client;
    client.setApiUrl(github.url());

    bool done = false, truncated = false;
    QList<GitHubEntry> entries;
    QString error;
    client.getTree("o", "r", "c1", [&](const QList<GitHubEntry> &list, bool t, const QString &e)
    {
        entries = list;
        truncated = t;
        error = e;
        done = true;
    });
    QTRY_VERIFY(done);
    QCOMPARE(error, QString());
    QVERIFY(truncated);
    QCOMPARE(entries.count(), 3); //not the submodule
    QVERIFY(entries[0].isFolder);
    QCOMPARE(entries[1].path, QString("logs/run 1.csv"));
    QCOMPARE(entries[1].sha, QString("b1"));
    QCOMPARE(entries[1].size, qint64(1234));
    QVERIFY(!entries[1].isFolder);
}

void TestGitHub::testDownload()
{
    QByteArray content;
    for (int i = 0; i < 20000; i++) content += "1,0x100,false,Rx,0,8,01,02,03,04,05,06,07,08,\n";

    FakeGitHub github;
    github.handler = [&content](const FakeRequest &request)
    {
        if (request.target != "/repos/o/r/contents/logs/run%201%20%23a.csv?ref=feature%2Flin") return json("{}", 404);
        FakeResponse response;
        response.body = content;
        return response;
    };
    GitHubClient client;
    client.setApiUrl(github.url());

    bool done = false;
    QByteArray data;
    QString error;
    QNetworkReply *reply = client.download("o", "r", "feature/lin", "logs/run 1 #a.csv",
                                           [&](const QByteArray &d, const QString &e) { data = d; error = e; done = true; });
    QVERIFY(reply);
    QTRY_VERIFY(done);
    QCOMPARE(error, QString());
    QCOMPARE(data.size(), content.size());
    QCOMPARE(data, content);
    QCOMPARE(github.requests.constFirst().headers.value("accept"), QByteArray("application/vnd.github.raw"));
}

void TestGitHub::testErrors()
{
    FakeGitHub github;
    github.handler = [](const FakeRequest &request)
    {
        if (request.target == "/repos/o/unauthorized") return json(R"({"message":"Bad credentials"})", 401);
        if (request.target == "/repos/o/missing") return json(R"({"message":"Not Found"})", 404);
        if (request.target == "/repos/o/limited")
        {
            FakeResponse response = json(R"({"message":"API rate limit exceeded"})", 403);
            response.addHeader("X-RateLimit-Remaining", "0");
            response.addHeader("X-RateLimit-Reset", "1761400000");
            return response;
        }
        if (request.target == "/repos/o/forbidden") return json(R"({"message":"Resource not accessible"})", 403);
        return json("oops", 500);
    };
    GitHubClient client;
    client.setApiUrl(github.url());

    QHash<QString, QString> errors;
    for (const QString &repo : { "unauthorized", "missing", "limited", "forbidden", "broken" })
        client.getRepo("o", repo, [&errors, repo](const QString &, const QString &e) { errors[repo] = e; });
    QTRY_COMPARE(errors.count(), 5);

    QVERIFY2(errors["unauthorized"].contains("sign in again"), qPrintable(errors["unauthorized"]));
    QVERIFY2(errors["missing"].contains("no access"), qPrintable(errors["missing"]));
    QVERIFY2(errors["limited"].contains("limit is used up until " +
                                        QDateTime::fromSecsSinceEpoch(1761400000).toString("HH:mm")),
             qPrintable(errors["limited"]));
    QVERIFY2(errors["forbidden"].contains("403: Resource not accessible"), qPrintable(errors["forbidden"]));
    QVERIFY2(errors["broken"].contains("500"), qPrintable(errors["broken"]));
}

void TestGitHub::testUnreachable()
{
    //a port nothing listens on
    QTcpServer closed;
    QVERIFY(closed.listen(QHostAddress::LocalHost));
    const quint16 port = closed.serverPort();
    closed.close();

    GitHubClient client;
    client.setApiUrl(QString("http://127.0.0.1:%1").arg(port));
    bool done = false;
    QString error;
    client.getUser([&](const QString &, const QString &e) { error = e; done = true; });
    QTRY_VERIFY(done);
    QVERIFY2(error.startsWith("Couldn't reach GitHub"), qPrintable(error));
}

void TestGitHub::testDeleteWhileRunning()
{
    FakeGitHub github;
    github.handler = [](const FakeRequest &) { FakeResponse response; response.noAnswer = true; return response; };
    GitHubClient *client = new GitHubClient;
    client->setApiUrl(github.url());

    bool called = false;
    client->getUser([&](const QString &, const QString &) { called = true; });
    QTRY_COMPARE(github.requests.count(), 1);
    delete client;
    QTest::qWait(50);
    QVERIFY(!called);
}

void TestGitHub::testDeviceFlow()
{
    int polls = 0;
    FakeGitHub github;
    github.handler = [&polls](const FakeRequest &request)
    {
        if (request.target == "/login/device/code")
            return json(R"({"device_code":"dev123","user_code":"ABCD-1234","verification_uri":"https://github.com/login/device",)"
                        R"("expires_in":900,"interval":0})");
        if (request.target == "/login/oauth/access_token")
        {
            //GitHub answers 200 with an error while waiting
            polls++;
            if (polls == 1) return json(R"({"error":"authorization_pending"})");
            if (polls == 2) return json(R"({"error":"slow_down","interval":0})");
            return json(R"({"access_token":"gho_token","token_type":"bearer","scope":"repo"})");
        }
        return json("{}", 404);
    };

    GitHubDeviceFlow flow;
    flow.setWebUrl(github.url());
    QSignalSpy codeSpy(&flow, &GitHubDeviceFlow::codeReady);
    QSignalSpy signedInSpy(&flow, &GitHubDeviceFlow::signedIn);
    QSignalSpy failedSpy(&flow, &GitHubDeviceFlow::failed);

    flow.start("Iv1.client", "repo");
    QVERIFY(flow.isRunning());
    QTRY_COMPARE(signedInSpy.count(), 1);
    QCOMPARE(failedSpy.count(), 0);
    QCOMPARE(codeSpy.count(), 1);
    QCOMPARE(codeSpy[0][0].toString(), QString("ABCD-1234"));
    QCOMPARE(codeSpy[0][1].toString(), QString("https://github.com/login/device"));
    QCOMPARE(signedInSpy[0][0].toString(), QString("gho_token"));
    QCOMPARE(polls, 3);
    QVERIFY(!flow.isRunning());

    const QUrlQuery codeForm(QString::fromUtf8(github.requests[0].body));
    QCOMPARE(github.requests[0].method, QByteArray("POST"));
    QCOMPARE(codeForm.queryItemValue("client_id"), QString("Iv1.client"));
    QCOMPARE(codeForm.queryItemValue("scope"), QString("repo"));
    QCOMPARE(github.requests[0].headers.value("accept"), QByteArray("application/json"));
    const QUrlQuery pollForm(QString::fromUtf8(github.requests[1].body));
    QCOMPARE(pollForm.queryItemValue("device_code"), QString("dev123"));
    QCOMPARE(pollForm.queryItemValue("grant_type", QUrl::FullyDecoded),
             QString("urn:ietf:params:oauth:grant-type:device_code"));
}

void TestGitHub::testDeviceFlowRefused_data()
{
    QTest::addColumn<QByteArray>("codeAnswer");
    QTest::addColumn<QByteArray>("pollAnswer");
    QTest::addColumn<QString>("message");

    const QByteArray code = R"({"device_code":"d","user_code":"U","verification_uri":"v","expires_in":900,"interval":0})";
    QTest::newRow("denied") << code << QByteArray(R"({"error":"access_denied"})") << "turned down";
    QTest::newRow("expired") << code << QByteArray(R"({"error":"expired_token"})") << "expired";
    QTest::newRow("other") << code << QByteArray(R"({"error":"incorrect_client_credentials","error_description":"The client_id is not valid."})")
                           << "The client_id is not valid.";
    QTest::newRow("no device flow") << QByteArray(R"({"error":"device_flow_disabled","error_description":"Device Flow must be explicitly enabled for this App"})")
                                    << QByteArray() << "Device Flow must be explicitly enabled";
}

void TestGitHub::testDeviceFlowRefused()
{
    QFETCH(QByteArray, codeAnswer);
    QFETCH(QByteArray, pollAnswer);
    QFETCH(QString, message);

    FakeGitHub github;
    github.handler = [&](const FakeRequest &request)
    {
        return json(request.target == "/login/device/code" ? codeAnswer : pollAnswer);
    };
    GitHubDeviceFlow flow;
    flow.setWebUrl(github.url());
    QSignalSpy signedInSpy(&flow, &GitHubDeviceFlow::signedIn);
    QSignalSpy failedSpy(&flow, &GitHubDeviceFlow::failed);

    flow.start("Iv1.client", "public_repo");
    QTRY_COMPARE(failedSpy.count(), 1);
    QVERIFY2(failedSpy[0][0].toString().contains(message), qPrintable(failedSpy[0][0].toString()));
    QCOMPARE(signedInSpy.count(), 0);
    QVERIFY(!flow.isRunning());
}

//the files shown in a dialog's tree, folders left out
static QStringList shownFiles(QTreeWidget *tree)
{
    QStringList files;
    for (QTreeWidgetItemIterator it(tree); *it; ++it)
        if ((*it)->data(0, Qt::UserRole).toInt() >= 0) files.append((*it)->text(0));
    files.sort();
    return files;
}

static QTreeWidgetItem *findItem(QTreeWidget *tree, const QString &name)
{
    for (QTreeWidgetItemIterator it(tree); *it; ++it)
        if ((*it)->text(0) == name) return *it;
    return nullptr;
}

//with SAVVYCAN_SCREENSHOTS set to a folder, the dialogs are saved there as pictures
static void screenshot(QWidget *widget, const QString &name)
{
    const QString dir = qEnvironmentVariable("SAVVYCAN_SCREENSHOTS");
    if (!dir.isEmpty()) widget->grab().save(dir + "/" + name + ".png");
}

static FakeResponse fakeRepo(const FakeRequest &request)
{
    if (request.target == "/user") return json(R"({"login":"tester"})");
    if (request.target == "/repos/o/r/branches?per_page=100&page=1")
        return json(R"([{"name":"main","commit":{"sha":"c1"}},{"name":"dev","commit":{"sha":"c2"}}])");
    if (request.target == "/repos/o/r/git/trees/c1?recursive=1")
        return json(R"({"truncated":false,"tree":[
            {"path":"logs","type":"tree","sha":"t1"},
            {"path":"logs/run.csv","type":"blob","sha":"b1","size":2048},
            {"path":"logs/notes.txt","type":"blob","sha":"b2","size":10},
            {"path":"dbc","type":"tree","sha":"t2"},
            {"path":"dbc/car.dbc","type":"blob","sha":"b3","size":500}]})");
    if (request.target == "/repos/o/r/git/trees/c2?recursive=1") return json(R"({"truncated":false,"tree":[]})");
    if (request.target == "/repos/o/r/contents/logs/run.csv?ref=c1")
    {
        FakeResponse response;
        response.body = "Time Stamp,ID,Extended,Dir,Bus,LEN,D1\n";
        return response;
    }
    return json(R"({"message":"Not Found"})", 404);
}

void TestGitHub::testBrowserDialog()
{
    FakeGitHub github;
    github.handler = fakeRepo;
    QTemporaryDir cache;
    QVERIFY(cache.isValid());
    GitHubCache::setRootDir(cache.path());
    QSettings().setValue("GitHub/ApiUrl", github.url());
    GitHubSettings::setToken("gho_token", true);
    GitHubRepo repo;
    repo.owner = "o";
    repo.name = "r";
    repo.branch = "main";
    repo.dbcs = false;
    GitHubRepo dbcOnly;
    dbcOnly.owner = "o";
    dbcOnly.name = "dbc-only";
    dbcOnly.logs = false;
    GitHubSettings::setRepos({ repo, dbcOnly });

    GitHubBrowserDialog dialog(GitHubBrowserDialog::Logs, { "Autodetect File Type (*.*)", "GVRET Logs (*.csv *.CSV)" });
    dialog.show();
    QTreeWidget *tree = dialog.findChild<QTreeWidget *>("tree");
    QComboBox *comboRepo = dialog.findChild<QComboBox *>("comboRepo");
    QComboBox *comboBranch = dialog.findChild<QComboBox *>("comboBranch");
    QComboBox *comboType = dialog.findChild<QComboBox *>("comboType");
    QLineEdit *editSearch = dialog.findChild<QLineEdit *>("editSearch");
    QPushButton *btnOpen = dialog.findChild<QPushButton *>("btnOpen");
    QLabel *lblStatus = dialog.findChild<QLabel *>("lblStatus");
    QVERIFY(tree && comboRepo && comboBranch && comboType && editSearch && btnOpen && lblStatus);

    QCOMPARE(comboRepo->count(), 1); //only the logs repo
    QTRY_COMPARE(shownFiles(tree), QStringList({ "car.dbc", "notes.txt", "run.csv" }));
    QCOMPARE(comboBranch->count(), 2);
    QCOMPARE(comboBranch->currentText(), QString("main"));
    QVERIFY(!btnOpen->isEnabled());
    screenshot(&dialog, "github-browser");

    comboType->setCurrentIndex(1);
    QCOMPARE(shownFiles(tree), QStringList({ "run.csv" }));
    comboType->setCurrentIndex(0);
    editSearch->setText("NOTES");
    QCOMPARE(shownFiles(tree), QStringList({ "notes.txt" }));
    editSearch->setText("logs/");
    QCOMPARE(shownFiles(tree), QStringList({ "notes.txt", "run.csv" }));

    //another branch, then back
    comboBranch->setCurrentIndex(1);
    QTRY_COMPARE(lblStatus->text(), QString("This branch has no files."));
    QVERIFY(shownFiles(tree).isEmpty());
    QCOMPARE(GitHubSettings::repos()[0].branch, QString("dev")); //remembered
    comboBranch->setCurrentIndex(0);
    QTRY_COMPARE(shownFiles(tree).count(), 2);

    findItem(tree, "logs")->setSelected(true);
    QVERIFY(!btnOpen->isEnabled()); //a folder
    tree->clearSelection();
    findItem(tree, "run.csv")->setSelected(true);
    QVERIFY(btnOpen->isEnabled());
    btnOpen->click();

    QCOMPARE(dialog.result(), int(QDialog::Accepted));
    QCOMPARE(dialog.fileTypeIndex(), 0);
    QCOMPARE(dialog.localFile(), cache.path() + "/o/r/main/logs/run.csv");
    QCOMPARE(dialog.origin().sha, QString("b1"));
    QCOMPARE(dialog.origin().displayName(), QString("github:o/r/logs/run.csv"));
    QFile file(dialog.localFile());
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), QByteArray("Time Stamp,ID,Extended,Dir,Bus,LEN,D1\n"));
    QVERIFY(github.requests.last().headers.value("authorization") == "Bearer gho_token");

    GitHubSettings::clearToken();
    GitHubSettings::setRepos({});
    QSettings().remove("GitHub");
}

void TestGitHub::testAccountDialog()
{
    FakeGitHub github;
    github.handler = fakeRepo;
    QSettings().setValue("GitHub/ApiUrl", github.url());
    GitHubSettings::setToken("gho_token", true);

    GitHubAccountDialog dialog;
    dialog.show();
    QLabel *lblStatus = dialog.findChild<QLabel *>("lblStatus");
    QVERIFY(lblStatus);
    QTRY_COMPARE(lblStatus->text(), QString("Signed in as tester (Sign in with GitHub)."));
    screenshot(&dialog, "github-account");

    GitHubSettings::clearToken();
    QSettings().remove("GitHub");
}

QTEST_MAIN(TestGitHub)
#include "test_github.moc"
