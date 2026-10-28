#include <QtTest/QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QUrlQuery>
#include <QMap>
#include <QSet>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include "github/githubaccountdialog.h"
#include "github/githubbrowserdialog.h"
#include "github/githubhelp.h"
#include <QDialogButtonBox>
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
    void testBrowserNotFoundHints();
    void testAccountDialog();
    void testExtensionFor();
    void testClientSave();
    void testSaveNewFile();
    void testSaveBackAndConflicts();
    void testSaveNewBranchAndPullRequest();
    void testCreateRepo();
    void testSaveNameFollowsType();
    void testHelp();
    void testAppInstallCheck();
    void testAppInstallCheckNewRepo();
    void testCompanionOpen();
    void testCompanionSave();

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
    //a new user starts with Tom-evnut's public EV CAN logs repo
    QSettings().remove("GitHub");
    QList<GitHubRepo> defaults = GitHubSettings::repos();
    QCOMPARE(defaults.count(), 1);
    QCOMPARE(defaults[0].fullName(), QString("Tom-evnut/EV-CANlogs"));
    QCOMPARE(defaults[0].branch, QString("main"));

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
    //the SavvyCAN GitHub App unless the user names their own
    QCOMPARE(GitHubSettings::clientId(), QString("Iv23lihZx7wcgpoRCn04"));
    QVERIFY(GitHubSettings::ownClientId().isEmpty());
    QCOMPARE(GitHubSettings::appInstallUrl(), QString("https://github.com/apps/savvycan/installations/new"));
    GitHubSettings::setOwnClientId("  Ov23liOwnApp ");
    QCOMPARE(GitHubSettings::clientId(), QString("Ov23liOwnApp"));
    QCOMPARE(GitHubSettings::ownClientId(), QString("Ov23liOwnApp"));
    QVERIFY(GitHubSettings::appInstallUrl().isEmpty());
    GitHubSettings::setOwnClientId("Iv23lihZx7wcgpoRCn04"); //the SavvyCAN app typed in: not "own"
    QVERIFY(GitHubSettings::ownClientId().isEmpty());
    QVERIFY(!QSettings().contains("GitHub/ClientId"));
    GitHubSettings::setOwnClientId("");
    QCOMPARE(GitHubSettings::clientId(), QString("Iv23lihZx7wcgpoRCn04"));
    QVERIFY(GitHubSettings::isGitHubAppClientId("Iv1.abc"));
    QVERIFY(GitHubSettings::isGitHubAppClientId("Iv23lihZx7wcgpoRCn04"));
    QVERIFY(!GitHubSettings::isGitHubAppClientId("Ov23liOwnApp"));

    GitHubRepo first;
    first.owner = "a";
    first.name = "car-data";
    first.branch = "main";
    GitHubRepo second;
    second.owner = "b";
    second.name = "bench";
    second.branch = "feature/x";
    GitHubSettings::setRepos({ first, second });
    QList<GitHubRepo> read = GitHubSettings::repos();
    QCOMPARE(read.count(), 2);
    QCOMPARE(read[0].fullName(), QString("a/car-data"));
    QCOMPARE(read[1].branch, QString("feature/x"));

    GitHubSettings::setRepos({ second });
    QCOMPARE(GitHubSettings::repos().count(), 1);
    GitHubSettings::setRepos({});
    QVERIFY(GitHubSettings::repos().isEmpty()); //emptied by the user: stays empty

    //the first version kept a repo for logs, DBC files or both: such repos are now in the one list, and a list
    //saved before the default repo existed is kept as it is
    QSettings().remove("GitHub");
    {
        QSettings settings;
        settings.beginWriteArray("GitHub/Repos", 1);
        settings.setArrayIndex(0);
        settings.setValue("Owner", "c");
        settings.setValue("Name", "dbc-only");
        settings.setValue("Branch", "main");
        settings.setValue("Logs", false);
        settings.setValue("DBCs", true);
        settings.endArray();
    }
    read = GitHubSettings::repos();
    QCOMPARE(read.count(), 1);
    QCOMPARE(read[0].fullName(), QString("c/dbc-only"));
    GitHubSettings::setRepos(read);
    QVERIFY(!QSettings().contains("GitHub/Repos/1/Logs")); //not written back
    GitHubSettings::setRepos({});
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
    GitHubRepo other;
    other.owner = "o";
    other.name = "other";
    other.branch = "main";
    GitHubSettings::setRepos({ repo, other });

    GitHubBrowserDialog dialog(GitHubBrowserDialog::Logs, GitHubBrowserDialog::Open, { "Autodetect File Type (*.*)", "GVRET Logs (*.csv *.CSV)" });
    dialog.show();
    QTreeWidget *tree = dialog.findChild<QTreeWidget *>("tree");
    QComboBox *comboRepo = dialog.findChild<QComboBox *>("comboRepo");
    QComboBox *comboBranch = dialog.findChild<QComboBox *>("comboBranch");
    QComboBox *comboType = dialog.findChild<QComboBox *>("comboType");
    QLineEdit *editSearch = dialog.findChild<QLineEdit *>("editSearch");
    QPushButton *btnOpen = dialog.findChild<QPushButton *>("btnOpen");
    QLabel *lblStatus = dialog.findChild<QLabel *>("lblStatus");
    QVERIFY(tree && comboRepo && comboBranch && comboType && editSearch && btnOpen && lblStatus);

    QCOMPARE(comboRepo->count(), 2);
    QCOMPARE(comboRepo->currentText(), QString("o/r"));
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

    //loading DBC files lists the same repos
    GitHubBrowserDialog dbcDialog(GitHubBrowserDialog::DBCs, GitHubBrowserDialog::Open, { "DBC File (*.dbc)" });
    QComboBox *dbcRepos = dbcDialog.findChild<QComboBox *>("comboRepo");
    QVERIFY(dbcRepos);
    QCOMPARE(dbcRepos->count(), 2);
    QCOMPARE(dbcRepos->itemText(0), QString("o/r"));
    QCOMPARE(dbcRepos->itemText(1), QString("o/other"));
    QTRY_COMPARE(shownFiles(dbcDialog.findChild<QTreeWidget *>("tree")), QStringList({ "car.dbc" }));

    GitHubSettings::clearToken();
    GitHubSettings::setRepos({});
    QSettings().remove("GitHub");
}

void TestGitHub::testBrowserNotFoundHints()
{
    FakeGitHub github;
    github.handler = [](const FakeRequest &) { return json(R"({"message":"Not Found"})", 404); };
    QSettings().setValue("GitHub/ApiUrl", github.url());
    GitHubRepo repo;
    repo.owner = "o";
    repo.name = "private";
    repo.branch = "main";
    GitHubSettings::setRepos({ repo });

    {
        GitHubSettings::clearToken();
        GitHubBrowserDialog dialog(GitHubBrowserDialog::Logs, GitHubBrowserDialog::Open, { "Autodetect File Type (*.*)" });
        QLabel *lblStatus = dialog.findChild<QLabel *>("lblStatus");
        QCOMPARE(dialog.findChild<QPushButton *>("btnAccount")->text(), QString("Sign In..."));
        QTRY_VERIFY(lblStatus->text().contains("sign in first"));
    }
    {
        GitHubSettings::setToken("gho_token", true);
        GitHubBrowserDialog dialog(GitHubBrowserDialog::Logs, GitHubBrowserDialog::Open, { "Autodetect File Type (*.*)" });
        QLabel *lblStatus = dialog.findChild<QLabel *>("lblStatus");
        QCOMPARE(dialog.findChild<QPushButton *>("btnAccount")->text(), QString("GitHub Account..."));
        QTRY_VERIFY(lblStatus->text().contains("https://github.com/apps/savvycan/installations/new"));
        QCOMPARE(lblStatus->textFormat(), Qt::RichText);
    }
    {
        GitHubSettings::setToken("github_pat_token", false); //a token's repos are chosen on GitHub: no app link
        GitHubBrowserDialog dialog(GitHubBrowserDialog::Logs, GitHubBrowserDialog::Open, { "Autodetect File Type (*.*)" });
        QLabel *lblStatus = dialog.findChild<QLabel *>("lblStatus");
        QTRY_VERIFY(lblStatus->text().contains("no access"));
        QVERIFY(!lblStatus->text().contains("installations"));
    }

    GitHubSettings::clearToken();
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

    //the SavvyCAN app: no client ID to enter, a button to choose its repos
    QWidget *installRow = dialog.findChild<QWidget *>("installRow");
    QCheckBox *cbOwnApp = dialog.findChild<QCheckBox *>("cbOwnApp");
    QWidget *ownAppBox = dialog.findChild<QWidget *>("ownAppBox");
    QLineEdit *editClientId = dialog.findChild<QLineEdit *>("editClientId");
    QCheckBox *cbPrivateRepos = dialog.findChild<QCheckBox *>("cbPrivateRepos");
    QVERIFY(installRow && cbOwnApp && ownAppBox && editClientId && cbPrivateRepos);
    QVERIFY(installRow->isVisible());
    QVERIFY(!cbOwnApp->isChecked());
    QVERIFY(!ownAppBox->isVisible());

    //one's own OAuth App: client ID and the private repos choice; a GitHub App has no scopes to choose
    cbOwnApp->setChecked(true);
    QVERIFY(ownAppBox->isVisible());
    editClientId->setText("Ov23liOwnApp");
    QCOMPARE(GitHubSettings::clientId(), QString("Ov23liOwnApp"));
    QVERIFY(cbPrivateRepos->isVisible());
    QVERIFY(!installRow->isVisible());
    screenshot(&dialog, "github-account-own-app");
    editClientId->setText("Iv23liOwnGitHubApp");
    QVERIFY(!cbPrivateRepos->isVisible());
    cbOwnApp->setChecked(false);
    QCOMPARE(GitHubSettings::clientId(), QString("Iv23lihZx7wcgpoRCn04"));
    QVERIFY(installRow->isVisible());

    GitHubSettings::clearToken();
    QSettings().remove("GitHub");
}

//A GitHub that remembers: repos with branches, each branch with its files and a commit SHA that changes on every save.
//Enough of the REST API for saving, conflicts, branches, pull requests and creating repos.
class FakeRepos
{
public:
    struct Branch { QString commit; QMap<QString, QPair<QString, QByteArray>> files; }; //path: (blob sha, content)
    QMap<QString, QMap<QString, Branch>> repos; //"owner/name": branch name: branch
    QList<QJsonObject> pulls;
    int counter = 0;
    bool hasInstallations = false;                  //without: /user/installations is "not found"
    QJsonArray installations;
    QMap<qint64, QStringList> installationRepos;

    void install(qint64 id, const QString &account, bool allRepos, const QStringList &repos = QStringList())
    {
        hasInstallations = true;
        installations.append(QJsonObject { { "id", double(id) }, { "account", QJsonObject { { "login", account } } },
                                           { "repository_selection", allRepos ? "all" : "selected" } });
        installationRepos[id] = repos;
    }

    void addFile(const QString &repo, const QString &branch, const QString &path, const QByteArray &content)
    {
        Branch &b = repos[repo][branch];
        b.files[path] = qMakePair(QString("blob%1").arg(++counter), content);
        b.commit = QString("commit%1").arg(++counter);
    }

    FakeResponse handle(const FakeRequest &request)
    {
        const QString target = QString::fromUtf8(request.target);
        const QString path = QUrl::fromPercentEncoding(target.section('?', 0, 0).toUtf8());
        const QUrlQuery query(target.section('?', 1));
        const QJsonObject body = QJsonDocument::fromJson(request.body).object();

        if (path == "/user") return json(R"({"login":"tester"})");
        if (path == "/user/orgs") return json(R"([{"login":"org1"}])");
        if (path == "/user/installations")
        {
            if (!hasInstallations) return json(R"({"message":"Not Found"})", 404);
            return json(QJsonDocument(QJsonObject { { "total_count", installations.count() },
                                                    { "installations", installations } }).toJson());
        }
        if (path.startsWith("/user/installations/") && path.endsWith("/repositories"))
        {
            QJsonArray list;
            for (const QString &repo : installationRepos.value(path.section('/', 3, 3).toLongLong()))
                list.append(QJsonObject { { "full_name", repo } });
            return json(QJsonDocument(QJsonObject { { "total_count", list.count() }, { "repositories", list } }).toJson());
        }
        if (request.method == "POST" && (path == "/user/repos" || path == "/orgs/org1/repos"))
        {
            const QString name = body.value("name").toString();
            if (name == "taken")
                return json(R"({"message":"Repository creation failed.","errors":[{"resource":"Repository","code":"custom","field":"name","message":"name already exists on this account"}]})", 422);
            if (name == "forbidden") return json(R"({"message":"Resource not accessible by integration"})", 403);
            const QString owner = path == "/user/repos" ? "tester" : "org1";
            addFile(owner + "/" + name, "main", "README.md", "# " + name.toUtf8());
            lastCreated = body;
            return json(QString(R"({"owner":{"login":"%1"},"name":"%2","default_branch":"main"})").arg(owner, name).toUtf8(), 201);
        }

        //repo calls: /repos/<owner>/<name>/...
        const QStringList parts = path.split('/');
        if (parts.count() < 4 || parts[1] != "repos") return json(R"({"message":"Not Found"})", 404);
        const QString repoName = parts[2] + "/" + parts[3];
        if (!repos.contains(repoName)) return json(R"({"message":"Not Found"})", 404);
        QMap<QString, Branch> &branches = repos[repoName];
        const QString rest = parts.mid(4).join('/');

        if (rest == "branches")
        {
            QJsonArray array;
            for (auto it = branches.constBegin(); it != branches.constEnd(); ++it)
                array.append(QJsonObject { { "name", it.key() }, { "commit", QJsonObject { { "sha", it.value().commit } } } });
            return json(QJsonDocument(array).toJson());
        }
        if (rest.startsWith("git/trees/"))
        {
            const QString commit = rest.mid(10);
            for (const Branch &b : std::as_const(branches))
            {
                if (b.commit != commit) continue;
                QJsonArray tree;
                QSet<QString> folders;
                for (auto it = b.files.constBegin(); it != b.files.constEnd(); ++it)
                {
                    tree.append(QJsonObject { { "path", it.key() }, { "type", "blob" }, { "sha", it.value().first },
                                              { "size", int(it.value().second.size()) } });
                    QString folder = it.key().section('/', 0, -2);
                    while (!folder.isEmpty() && !folders.contains(folder))
                    {
                        folders.insert(folder);
                        tree.append(QJsonObject { { "path", folder }, { "type", "tree" }, { "sha", "t" + folder } });
                        folder = folder.section('/', 0, -2);
                    }
                }
                return json(QJsonDocument(QJsonObject { { "tree", tree }, { "truncated", false } }).toJson());
            }
            return json(R"({"message":"Not Found"})", 404);
        }
        if (rest.startsWith("contents/"))
        {
            const QString file = rest.mid(9);
            if (request.method == "GET")
            {
                const QString ref = query.queryItemValue("ref", QUrl::FullyDecoded);
                for (auto it = branches.constBegin(); it != branches.constEnd(); ++it)
                {
                    if (it.key() != ref && it.value().commit != ref) continue;
                    if (!it.value().files.contains(file)) break;
                    const auto &f = it.value().files[file];
                    if (request.headers.value("accept") == "application/vnd.github.raw") return FakeResponse { 200, f.second, {}, false };
                    return json(QJsonDocument(QJsonObject { { "sha", f.first }, { "path", file } }).toJson());
                }
                return json(R"({"message":"Not Found"})", 404);
            }
            if (request.method == "PUT")
            {
                const QString branchName = body.value("branch").toString();
                if (!branches.contains(branchName)) return json(R"({"message":"Branch not found"})", 404);
                if (file.contains("fail-me")) return json(R"({"message":"Server Error"})", 500);
                Branch &b = branches[branchName];
                const QString sha = body.value("sha").toString();
                if (b.files.contains(file) && sha.isEmpty())
                    return json(R"({"message":"Invalid request.\n\n\"sha\" wasn't supplied."})", 422);
                if (b.files.contains(file) && sha != b.files[file].first)
                    return json(QString(R"({"message":"%1 does not match %2"})").arg(file, sha).toUtf8(), 409);
                if (!b.files.contains(file) && !sha.isEmpty())
                    return json(QString(R"({"message":"%1 does not match"})").arg(file).toUtf8(), 409);
                const bool created = !b.files.contains(file);
                addFile(repoName, branchName, file, QByteArray::fromBase64(body.value("content").toString().toLatin1()));
                lastMessage = body.value("message").toString();
                return json(QJsonDocument(QJsonObject {
                    { "content", QJsonObject { { "sha", b.files[file].first } } },
                    { "commit", QJsonObject { { "html_url", "https://github.com/" + repoName + "/commit/" + b.commit } } }
                }).toJson(), created ? 201 : 200);
            }
        }
        if (rest == "git/refs" && request.method == "POST")
        {
            const QString name = body.value("ref").toString().mid(11);
            if (branches.contains(name)) return json(R"({"message":"Reference already exists"})", 422);
            for (const Branch &b : std::as_const(branches))
            {
                if (b.commit != body.value("sha").toString()) continue;
                branches[name] = b;
                return json(R"({"ref":"refs/heads/x"})", 201);
            }
            return json(R"({"message":"Object does not exist"})", 422);
        }
        if (rest.startsWith("git/refs/heads/") && request.method == "DELETE")
        {
            branches.remove(rest.mid(15));
            return FakeResponse { 204, QByteArray(), {}, false };
        }
        if (rest == "pulls" && request.method == "POST")
        {
            pulls.append(body);
            return json(QString(R"({"html_url":"https://github.com/%1/pull/%2"})").arg(repoName).arg(pulls.count()).toUtf8(), 201);
        }
        return json(R"({"message":"Not Found"})", 404);
    }

    QJsonObject lastCreated;
    QString lastMessage;
};

//a GitHub with o/r: main has logs/run.csv and dbc/car.dbc
struct SaveFixture
{
    FakeGitHub github;
    FakeRepos repos;
    QTemporaryDir cache;
    QStringList asked; //what the dialog asked, in order
    QList<QMessageBox::StandardButton> answers; //what to answer, in order (Ok when they run out)

    SaveFixture()
    {
        repos.addFile("o/r", "main", "logs/run.csv", "old log");
        repos.addFile("o/r", "main", "dbc/car.dbc", "VERSION \"\"");
        github.handler = [this](const FakeRequest &request) { return repos.handle(request); };
        GitHubCache::setRootDir(cache.path());
        QSettings().setValue("GitHub/ApiUrl", github.url());
        GitHubSettings::setToken("gho_token", true);
        GitHubRepo repo;
        repo.owner = "o";
        repo.name = "r";
        repo.branch = "main";
        GitHubSettings::setRepos({ repo });
    }
    ~SaveFixture()
    {
        GitHubSettings::clearToken();
        QSettings().remove("GitHub");
    }

    void prepare(GitHubBrowserDialog &dialog, const QByteArray &content)
    {
        dialog.ask = [this](QMessageBox::Icon, const QString &text, QMessageBox::StandardButtons,
                            const QMap<QMessageBox::StandardButton, QString> &)
        {
            asked.append(text);
            return answers.isEmpty() ? QMessageBox::Ok : answers.takeFirst();
        };
        dialog.setSaveWriter([content](const QString &localFile, int, QString &)
        {
            QFile file(localFile);
            return file.open(QIODevice::WriteOnly) && file.write(content) == content.size();
        });
    }
};

static const QStringList saveTypes { "GVRET Logs (*.csv *.CSV)", "Candump/Kayak (*.log)" };

void TestGitHub::testExtensionFor()
{
    QCOMPARE(GitHubBrowserDialog::extensionFor("GVRET Logs (*.csv *.CSV)"), QString(".csv"));
    QCOMPARE(GitHubBrowserDialog::extensionFor("DBC File (*.dbc)"), QString(".dbc"));
    QCOMPARE(GitHubBrowserDialog::extensionFor("Autodetect File Type (*.*)"), QString());
    QCOMPARE(GitHubBrowserDialog::extensionFor("No patterns"), QString());
}

void TestGitHub::testClientSave()
{
    FakeGitHub github;
    FakeRepos repos;
    repos.addFile("o/r", "main", "logs/run.csv", "old");
    github.handler = [&repos](const FakeRequest &request) { return repos.handle(request); };
    GitHubClient client;
    client.setApiUrl(github.url());
    client.setToken("gho_token");

    auto put = [&](const QString &path, const QByteArray &content, const QString &sha, bool &conflict, QString &error)
    {
        GitHubSaveResult result;
        bool done = false;
        client.putFile("o", "r", "main", path, content, "a message", sha,
                       [&](const GitHubSaveResult &r, bool c, const QString &e) { result = r; conflict = c; error = e; done = true; });
        QTest::qWaitFor([&done]() { return done; }, 5000);
        return result;
    };

    //a new file
    bool conflict = false;
    QString error;
    GitHubSaveResult result = put("logs/new run.csv", "new content", QString(), conflict, error);
    QCOMPARE(error, QString());
    QVERIFY(!conflict);
    QCOMPARE(result.sha, repos.repos["o/r"]["main"].files["logs/new run.csv"].first);
    QVERIFY(result.commitUrl.startsWith("https://github.com/o/r/commit/"));
    QCOMPARE(repos.repos["o/r"]["main"].files["logs/new run.csv"].second, QByteArray("new content"));
    const QJsonObject sent = QJsonDocument::fromJson(github.requests.last().body).object();
    QCOMPARE(github.requests.last().method, QByteArray("PUT"));
    QCOMPARE(github.requests.last().target, QByteArray("/repos/o/r/contents/logs/new%20run.csv"));
    QCOMPARE(sent.value("message").toString(), QString("a message"));
    QCOMPARE(sent.value("branch").toString(), QString("main"));
    QVERIFY(!sent.contains("sha"));
    QCOMPARE(github.requests.last().headers.value("content-type"), QByteArray("application/json"));

    //an existing file: without its SHA, with a wrong one, then with the right one
    put("logs/run.csv", "x", QString(), conflict, error);
    QVERIFY(conflict);
    put("logs/run.csv", "x", "not-the-sha", conflict, error);
    QVERIFY(conflict);
    QVERIFY(!error.isEmpty());
    const QString current = repos.repos["o/r"]["main"].files["logs/run.csv"].first;
    result = put("logs/run.csv", "replaced", current, conflict, error);
    QVERIFY(!conflict);
    QCOMPARE(error, QString());
    QCOMPARE(repos.repos["o/r"]["main"].files["logs/run.csv"].second, QByteArray("replaced"));

    //an error that isn't a conflict
    put("fail-me.csv", "x", QString(), conflict, error);
    QVERIFY(!conflict);
    QVERIFY(error.contains("500"));

    //the current SHA, or none for a missing file
    QString sha;
    bool done = false;
    client.getFileSha("o", "r", "main", "logs/run.csv", [&](const QString &s, const QString &e) { sha = s; error = e; done = true; });
    QTRY_VERIFY(done);
    QCOMPARE(sha, repos.repos["o/r"]["main"].files["logs/run.csv"].first);
    done = false;
    client.getFileSha("o", "r", "main", "nothing.csv", [&](const QString &s, const QString &e) { sha = s; error = e; done = true; });
    QTRY_VERIFY(done);
    QVERIFY(sha.isEmpty());
    QVERIFY(error.isEmpty());

    //branches and pull requests
    done = false;
    client.createBranch("o", "r", "feature/x", repos.repos["o/r"]["main"].commit, [&](const QString &e) { error = e; done = true; });
    QTRY_VERIFY(done);
    QCOMPARE(error, QString());
    QVERIFY(repos.repos["o/r"].contains("feature/x"));
    QCOMPARE(QJsonDocument::fromJson(github.requests.last().body).object().value("ref").toString(), QString("refs/heads/feature/x"));
    QString url;
    done = false;
    client.createPullRequest("o", "r", "feature/x", "main", "A title", "A body",
                             [&](const QString &u, const QString &e) { url = u; error = e; done = true; });
    QTRY_VERIFY(done);
    QCOMPARE(url, QString("https://github.com/o/r/pull/1"));
    QCOMPARE(repos.pulls[0].value("head").toString(), QString("feature/x"));
    QCOMPARE(repos.pulls[0].value("base").toString(), QString("main"));
    QCOMPARE(repos.pulls[0].value("title").toString(), QString("A title"));
    done = false;
    client.deleteBranch("o", "r", "feature/x", [&](const QString &e) { error = e; done = true; });
    QTRY_VERIFY(done);
    QCOMPARE(error, QString());
    QVERIFY(!repos.repos["o/r"].contains("feature/x"));
    QCOMPARE(github.requests.last().target, QByteArray("/repos/o/r/git/refs/heads/feature/x"));

    //organisations and new repos
    QStringList orgs;
    done = false;
    client.listOrgs([&](const QStringList &o, const QString &e) { orgs = o; error = e; done = true; });
    QTRY_VERIFY(done);
    QCOMPARE(orgs, QStringList { "org1" });

    QString owner, defaultBranch;
    int status = 0;
    done = false;
    client.createRepo(QString(), "logs2", "Some logs", true, [&](const QString &o, const QString &b, const QString &e, int s)
    {
        owner = o; defaultBranch = b; error = e; status = s; done = true;
    });
    QTRY_VERIFY(done);
    QCOMPARE(error, QString());
    QCOMPARE(owner, QString("tester"));
    QCOMPARE(defaultBranch, QString("main"));
    QCOMPARE(github.requests.last().target, QByteArray("/user/repos"));
    QVERIFY(repos.lastCreated.value("private").toBool());
    QVERIFY(repos.lastCreated.value("auto_init").toBool());
    QCOMPARE(repos.lastCreated.value("description").toString(), QString("Some logs"));

    done = false;
    client.createRepo("org1", "shared", QString(), false, [&](const QString &o, const QString &, const QString &e, int)
    {
        owner = o; error = e; done = true;
    });
    QTRY_VERIFY(done);
    QCOMPARE(owner, QString("org1"));
    QCOMPARE(github.requests.last().target, QByteArray("/orgs/org1/repos"));
    QVERIFY(!repos.lastCreated.value("private").toBool());

    done = false;
    client.createRepo(QString(), "taken", QString(), true, [&](const QString &, const QString &, const QString &e, int s)
    {
        error = e; status = s; done = true;
    });
    QTRY_VERIFY(done);
    QCOMPARE(status, 422);
    QVERIFY2(error.contains("name already exists on this account"), qPrintable(error));

    done = false;
    client.createRepo(QString(), "forbidden", QString(), true, [&](const QString &, const QString &, const QString &e, int s)
    {
        error = e; status = s; done = true;
    });
    QTRY_VERIFY(done);
    QCOMPARE(status, 403);
}

void TestGitHub::testSaveNewFile()
{
    SaveFixture f;
    GitHubBrowserDialog dialog(GitHubBrowserDialog::Logs, GitHubBrowserDialog::Save, saveTypes);
    f.prepare(dialog, "Time Stamp,ID\n1,0x100\n");
    dialog.setSuggestion(GitHubOrigin(), "bench run");
    dialog.show();

    QTreeWidget *tree = dialog.findChild<QTreeWidget *>("tree");
    QLineEdit *editFolder = dialog.findChild<QLineEdit *>("editFolder");
    QLineEdit *editName = dialog.findChild<QLineEdit *>("editName");
    QPushButton *btnSave = dialog.findChild<QPushButton *>("btnOpen");
    QTRY_VERIFY(findItem(tree, "logs"));
    QVERIFY(findItem(tree, "dbc")); //every folder, even without files of this type
    QVERIFY(!findItem(tree, "car.dbc"));
    screenshot(&dialog, "github-save");

    tree->setCurrentItem(findItem(tree, "logs"));
    emit tree->itemClicked(findItem(tree, "logs"), 0);
    QCOMPARE(editFolder->text(), QString("logs"));
    QCOMPARE(editName->text(), QString("bench run.csv")); //the file type's extension added
    QVERIFY(btnSave->isEnabled());
    btnSave->click();

    QCOMPARE(dialog.result(), int(QDialog::Accepted));
    const auto &files = f.repos.repos["o/r"]["main"].files;
    QVERIFY(files.contains("logs/bench run.csv")); //the file type's extension added
    QCOMPARE(files["logs/bench run.csv"].second, QByteArray("Time Stamp,ID\n1,0x100\n"));
    QCOMPARE(f.repos.lastMessage, QString("Add bench run.csv from SavvyCAN"));
    QCOMPARE(dialog.origin().path, QString("logs/bench run.csv"));
    QCOMPARE(dialog.origin().sha, files["logs/bench run.csv"].first);
    QVERIFY(dialog.resultUrl().contains("/commit/"));
    QCOMPARE(GitHubCache::originOf(dialog.localFile()).sha, dialog.origin().sha); //cached as saved
    QCOMPARE(f.asked.count(), 1);
    QVERIFY(f.asked[0].startsWith("Saved logs/bench run.csv to o/r on main."));
}

void TestGitHub::testSaveBackAndConflicts()
{
    SaveFixture f;
    const QString loadedSha = f.repos.repos["o/r"]["main"].files["logs/run.csv"].first;
    const GitHubOrigin loaded { "o", "r", "main", "logs/run.csv", loadedSha };

    //saving back what was loaded: no questions, an "Update" commit
    {
        GitHubBrowserDialog dialog(GitHubBrowserDialog::Logs, GitHubBrowserDialog::Save, saveTypes);
        f.prepare(dialog, "version 2");
        dialog.setSuggestion(loaded, QString());
        QCOMPARE(dialog.findChild<QLineEdit *>("editFolder")->text(), QString("logs"));
        QCOMPARE(dialog.findChild<QLineEdit *>("editName")->text(), QString("run.csv"));
        QTRY_VERIFY(findItem(dialog.findChild<QTreeWidget *>("tree"), "run.csv"));
        dialog.findChild<QPushButton *>("btnOpen")->click();
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
        QCOMPARE(f.repos.repos["o/r"]["main"].files["logs/run.csv"].second, QByteArray("version 2"));
        QCOMPARE(f.repos.lastMessage, QString("Update run.csv from SavvyCAN"));
        QCOMPARE(f.asked.count(), 1); //only "Saved"
    }

    //saving the old version again: GitHub has a newer one. "Save Under a New Name" saves nothing yet.
    f.asked.clear();
    {
        GitHubBrowserDialog dialog(GitHubBrowserDialog::Logs, GitHubBrowserDialog::Save, saveTypes);
        f.prepare(dialog, "version 3");
        f.answers = { QMessageBox::No };
        dialog.setSuggestion(loaded, QString());
        QTRY_VERIFY(findItem(dialog.findChild<QTreeWidget *>("tree"), "run.csv"));
        dialog.findChild<QPushButton *>("btnOpen")->click();
        QCOMPARE(f.asked.count(), 1);
        QVERIFY(f.asked[0].contains("isn't the version this one replaces"));
        QVERIFY(dialog.result() != int(QDialog::Accepted));
        QCOMPARE(dialog.findChild<QLineEdit *>("editName")->text(), QString("run (2).csv"));
        QCOMPARE(f.repos.repos["o/r"]["main"].files["logs/run.csv"].second, QByteArray("version 2"));

        //...then saved under that name
        f.asked.clear();
        dialog.findChild<QPushButton *>("btnOpen")->click();
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
        QCOMPARE(f.repos.repos["o/r"]["main"].files["logs/run (2).csv"].second, QByteArray("version 3"));
    }

    //"Overwrite" replaces whatever is there now
    f.asked.clear();
    {
        GitHubBrowserDialog dialog(GitHubBrowserDialog::Logs, GitHubBrowserDialog::Save, saveTypes);
        f.prepare(dialog, "version 4");
        f.answers = { QMessageBox::Yes };
        dialog.setSuggestion(loaded, QString());
        QTRY_VERIFY(findItem(dialog.findChild<QTreeWidget *>("tree"), "run.csv"));
        dialog.findChild<QPushButton *>("btnOpen")->click();
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
        QCOMPARE(f.repos.repos["o/r"]["main"].files["logs/run.csv"].second, QByteArray("version 4"));
        QCOMPARE(f.asked.count(), 2); //the conflict, then "Saved"
    }

    //another file of that name, not the one loaded: asked first; Cancel saves nothing
    f.asked.clear();
    {
        GitHubBrowserDialog dialog(GitHubBrowserDialog::Logs, GitHubBrowserDialog::Save, saveTypes);
        f.prepare(dialog, "other");
        f.answers = { QMessageBox::Cancel };
        dialog.setSuggestion(GitHubOrigin(), "run.csv");
        dialog.findChild<QLineEdit *>("editFolder")->setText("logs");
        QTRY_VERIFY(findItem(dialog.findChild<QTreeWidget *>("tree"), "run.csv"));
        dialog.findChild<QPushButton *>("btnOpen")->click();
        QCOMPARE(f.asked.count(), 1);
        QVERIFY(f.asked[0].contains("is already in o/r on main. Replace it?"));
        QVERIFY(dialog.result() != int(QDialog::Accepted));
        QCOMPARE(f.repos.repos["o/r"]["main"].files["logs/run.csv"].second, QByteArray("version 4"));
    }
}

void TestGitHub::testSaveNewBranchAndPullRequest()
{
    SaveFixture f;
    GitHubBrowserDialog dialog(GitHubBrowserDialog::DBCs, GitHubBrowserDialog::Save, { "DBC File (*.dbc)" });
    f.prepare(dialog, "VERSION \"2\"");
    dialog.setSuggestion(GitHubOrigin { "o", "r", "main", "dbc/car.dbc",
                                        f.repos.repos["o/r"]["main"].files["dbc/car.dbc"].first }, QString());
    QTRY_VERIFY(findItem(dialog.findChild<QTreeWidget *>("tree"), "car.dbc"));

    QCheckBox *cbNewBranch = dialog.findChild<QCheckBox *>("cbNewBranch");
    cbNewBranch->setChecked(true);
    QLineEdit *editBranchName = dialog.findChild<QLineEdit *>("editBranchName");
    QVERIFY(editBranchName->text().startsWith("savvycan/car-"));
    editBranchName->setText("savvycan/car-update");
    dialog.findChild<QLineEdit *>("editPrTitle")->setText("New signals for the car");
    dialog.findChild<QLineEdit *>("editMessage")->setText("Add the charger messages");
    dialog.findChild<QPushButton *>("btnOpen")->click();

    QCOMPARE(dialog.result(), int(QDialog::Accepted));
    QCOMPARE(f.repos.repos["o/r"]["savvycan/car-update"].files["dbc/car.dbc"].second, QByteArray("VERSION \"2\""));
    QCOMPARE(f.repos.repos["o/r"]["main"].files["dbc/car.dbc"].second, QByteArray("VERSION \"\"")); //main untouched
    QCOMPARE(f.repos.lastMessage, QString("Add the charger messages"));
    QCOMPARE(f.repos.pulls.count(), 1);
    QCOMPARE(f.repos.pulls[0].value("head").toString(), QString("savvycan/car-update"));
    QCOMPARE(f.repos.pulls[0].value("base").toString(), QString("main"));
    QCOMPARE(f.repos.pulls[0].value("title").toString(), QString("New signals for the car"));
    QCOMPARE(dialog.resultUrl(), QString("https://github.com/o/r/pull/1"));
    QCOMPARE(dialog.origin().branch, QString("savvycan/car-update"));
    QVERIFY(f.asked.last().contains("opened a pull request into main"));

    //a save that fails takes its new branch away again
    f.asked.clear();
    GitHubBrowserDialog failing(GitHubBrowserDialog::DBCs, GitHubBrowserDialog::Save, { "DBC File (*.dbc)" });
    f.prepare(failing, "x");
    failing.setSuggestion(GitHubOrigin(), "fail-me.dbc");
    QTRY_VERIFY(findItem(failing.findChild<QTreeWidget *>("tree"), "dbc"));
    failing.findChild<QCheckBox *>("cbNewBranch")->setChecked(true);
    failing.findChild<QLineEdit *>("editBranchName")->setText("savvycan/doomed");
    failing.findChild<QPushButton *>("btnOpen")->click();
    QVERIFY(failing.result() != int(QDialog::Accepted));
    QVERIFY(f.asked.last().contains("couldn't be saved"));
    QVERIFY(!f.repos.repos["o/r"].contains("savvycan/doomed"));
}

void TestGitHub::testCreateRepo()
{
    SaveFixture f;
    GitHubBrowserDialog dialog(GitHubBrowserDialog::Logs, GitHubBrowserDialog::Save, saveTypes);
    f.prepare(dialog, "x");
    QPushButton *btnNewRepo = dialog.findChild<QPushButton *>("btnNewRepo");
    QTRY_VERIFY(btnNewRepo->isEnabled()); //once the account is known

    QVERIFY(dialog.createRepoNow("tester", "bench-logs", "Logs from the bench", true));
    QComboBox *comboRepo = dialog.findChild<QComboBox *>("comboRepo");
    QCOMPARE(comboRepo->currentText(), QString("tester/bench-logs"));
    QCOMPARE(GitHubSettings::repos().last().fullName(), QString("tester/bench-logs"));
    QCOMPARE(GitHubSettings::repos().last().branch, QString("main"));
    QTRY_COMPARE(dialog.findChild<QComboBox *>("comboBranch")->currentText(), QString("main"));
    QVERIFY(f.repos.lastCreated.value("private").toBool());

    QVERIFY(dialog.createRepoNow("org1", "team-logs", QString(), true));
    QCOMPARE(comboRepo->currentText(), QString("org1/team-logs"));

    QVERIFY(!dialog.createRepoNow("tester", "taken", QString(), true));
    QVERIFY(f.asked.last().contains("name already exists on this account"));
    QVERIFY(!dialog.createRepoNow("tester", "forbidden", QString(), true));
    QVERIFY(f.asked.last().contains("Administration: read and write"));
    QCOMPARE(GitHubSettings::repos().count(), 3);
}

void TestGitHub::testSaveNameFollowsType()
{
    SaveFixture f;
    GitHubBrowserDialog dialog(GitHubBrowserDialog::Logs, GitHubBrowserDialog::Save, saveTypes);
    f.prepare(dialog, "x");
    QComboBox *comboType = dialog.findChild<QComboBox *>("comboType");
    QLineEdit *editName = dialog.findChild<QLineEdit *>("editName");

    dialog.setSuggestion(GitHubOrigin(), "capture.log"); //picks the type that fits the name
    QCOMPARE(comboType->currentIndex(), 1);
    comboType->setCurrentIndex(0);
    QCOMPARE(editName->text(), QString("capture.csv"));
    dialog.setSuggestion(GitHubOrigin(), "capture.blf"); //no type fits: the name gets the type's extension
    QCOMPARE(editName->text(), QString("capture.csv"));

    //signed out, nothing can be saved
    GitHubSettings::clearToken();
    GitHubBrowserDialog signedOut(GitHubBrowserDialog::Logs, GitHubBrowserDialog::Save, saveTypes);
    f.prepare(signedOut, "x");
    signedOut.setSuggestion(GitHubOrigin(), "a.csv");
    QVERIFY(!signedOut.findChild<QPushButton *>("btnOpen")->isEnabled());
    QCOMPARE(signedOut.findChild<QPushButton *>("btnAccount")->text(), QString("Sign In..."));
}

static int requestsTo(const FakeGitHub &github, const QByteArray &prefix)
{
    int count = 0;
    for (const FakeRequest &request : github.requests)
        if (request.target.startsWith(prefix)) count++;
    return count;
}

//signed in through the SavvyCAN app, saving warns, and asks first, when the app isn't installed on the repo
void TestGitHub::testAppInstallCheck()
{
    //installed on selected repos of o, but not on o/r
    {
        SaveFixture f;
        f.repos.install(7, "o", false, { "o/other" });
        GitHubBrowserDialog dialog(GitHubBrowserDialog::Logs, GitHubBrowserDialog::Save, saveTypes);
        f.prepare(dialog, "x");
        dialog.setSuggestion(GitHubOrigin(), "run.csv");
        QLabel *hint = dialog.findChild<QLabel *>("lblAppHint");
        QTRY_VERIFY(!hint->isHidden());
        QVERIFY(hint->text().contains("isn't installed on o/r"));
        QVERIFY(hint->text().contains("Choose Repos..."));
        screenshot(&dialog, "github-save-not-installed");

        QTRY_VERIFY(findItem(dialog.findChild<QTreeWidget *>("tree"), "logs"));
        f.answers = { QMessageBox::Cancel };
        dialog.findChild<QPushButton *>("btnOpen")->click();
        QCOMPARE(f.asked.count(), 1);
        QVERIFY(f.asked[0].contains("The SavvyCAN app isn't installed on o/r"));
        QVERIFY(!f.repos.repos["o/r"]["main"].files.contains("run.csv"));

        //"Save Anyway" goes ahead (the fake GitHub lets it)
        f.asked.clear();
        f.answers = { QMessageBox::Ignore };
        dialog.findChild<QPushButton *>("btnOpen")->click();
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
        QVERIFY(f.repos.repos["o/r"]["main"].files.contains("run.csv"));
    }

    //installed on o/r itself, or on all of o's repos: nothing said
    for (bool all : { false, true })
    {
        SaveFixture f;
        f.repos.install(7, "someone-else", true);
        f.repos.install(8, "o", all, { "o/r" });
        GitHubBrowserDialog dialog(GitHubBrowserDialog::Logs, GitHubBrowserDialog::Save, saveTypes);
        f.prepare(dialog, "x");
        dialog.setSuggestion(GitHubOrigin(), "run.csv");
        QTRY_VERIFY(findItem(dialog.findChild<QTreeWidget *>("tree"), "logs"));
        QTRY_VERIFY(requestsTo(f.github, "/user/installations") > 0);
        QTest::qWait(100);
        QVERIFY(dialog.findChild<QLabel *>("lblAppHint")->isHidden());
        dialog.findChild<QPushButton *>("btnOpen")->click();
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
        QCOMPARE(f.asked.count(), 1); //only "Saved"
        QCOMPARE(requestsTo(f.github, "/user/installations/7/"), 0); //all repos: no list needed
    }

    //a pasted token chooses its repos on GitHub: no check
    {
        SaveFixture f;
        f.repos.install(7, "o", false);
        GitHubSettings::setToken("github_pat_x", false);
        GitHubBrowserDialog dialog(GitHubBrowserDialog::Logs, GitHubBrowserDialog::Save, saveTypes);
        f.prepare(dialog, "x");
        dialog.setSuggestion(GitHubOrigin(), "run.csv");
        QTRY_VERIFY(findItem(dialog.findChild<QTreeWidget *>("tree"), "logs"));
        dialog.findChild<QPushButton *>("btnOpen")->click();
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
        QCOMPARE(requestsTo(f.github, "/user/installations"), 0);
        QVERIFY(dialog.findChild<QLabel *>("lblAppHint")->isHidden());
    }
}

//New Repo... with the SavvyCAN app needs it on all the owner's repos
void TestGitHub::testAppInstallCheckNewRepo()
{
    SaveFixture f;
    f.repos.install(7, "tester", false, { "tester/old" });
    f.repos.install(8, "org1", true);
    GitHubBrowserDialog dialog(GitHubBrowserDialog::Logs, GitHubBrowserDialog::Save, saveTypes);
    f.prepare(dialog, "x");
    QTRY_VERIFY(dialog.findChild<QPushButton *>("btnNewRepo")->isEnabled());

    f.answers = { QMessageBox::Cancel };
    QVERIFY(!dialog.createRepoNow("tester", "first", QString(), true));
    QCOMPARE(f.asked.count(), 1);
    QVERIFY(f.asked[0].contains("isn't installed on all of tester's repos"));
    QVERIFY(!f.repos.repos.contains("tester/first"));

    f.answers = { QMessageBox::Ignore };
    QVERIFY(dialog.createRepoNow("tester", "second", QString(), true)); //"Create Anyway"
    QVERIFY(f.repos.repos.contains("tester/second"));

    f.asked.clear();
    QVERIFY(dialog.createRepoNow("org1", "third", QString(), true)); //all of org1's repos: no question
    QVERIFY(f.asked.isEmpty());
}

//the Help buttons (and F1) open help/github.md through the hook MainWindow sets
void TestGitHub::testHelp()
{
    QStringList shown;
    gitHubHelp() = [&shown](const QString &page) { shown.append(page); };

    GitHubBrowserDialog browser(GitHubBrowserDialog::Logs, GitHubBrowserDialog::Save, saveTypes);
    browser.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Help)->click();
    GitHubAccountDialog account;
    account.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Help)->click();
    QCOMPARE(shown, QStringList({ "github.md", "github.md" }));

    gitHubHelp() = nullptr;
    showGitHubHelp(); //no hook: nothing happens
    QSettings().remove("GitHub");
}

static QByteArray fileContents(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

//files that go with a log (its notes, <log>.notes.md): listed with it, downloaded with it, and an old cached one dropped
void TestGitHub::testCompanionOpen()
{
    SaveFixture f;
    f.repos.addFile("o/r", "main", "logs/run.csv.notes.md", "# SavvyCAN log notes v1\ntitle: bench\n");
    f.repos.addFile("o/r", "main", "logs/other.csv", "other log");
    f.repos.addFile("o/r", "main", "logs/lone.csv.notes.md", "notes of a log that isn't there");
    //a notes file cached for other.csv earlier, which GitHub doesn't have any more
    QVERIFY(QDir().mkpath(f.cache.path() + "/o/r/main/logs"));
    const QString oldNotes = f.cache.path() + "/o/r/main/logs/other.csv.notes.md";
    {
        QFile old(oldNotes);
        QVERIFY(old.open(QIODevice::WriteOnly));
        old.write("old notes");
    }

    const QStringList openTypes { "Autodetect File Type (*.*)", "GVRET Logs (*.csv *.CSV)" };
    auto openFile = [&](const QString &name, QString &localFile)
    {
        GitHubBrowserDialog dialog(GitHubBrowserDialog::Logs, GitHubBrowserDialog::Open, openTypes);
        dialog.setCompanionSuffixes({ ".notes.md" });
        dialog.ask = [&f](QMessageBox::Icon, const QString &text, QMessageBox::StandardButtons,
                          const QMap<QMessageBox::StandardButton, QString> &)
        {
            f.asked.append(text);
            return QMessageBox::Ok;
        };
        dialog.show();
        QTreeWidget *tree = dialog.findChild<QTreeWidget *>("tree");
        QTRY_VERIFY(findItem(tree, "run.csv"));
        //the notes file goes with its log; one without its log is listed on its own
        QCOMPARE(shownFiles(tree), QStringList({ "car.dbc", "lone.csv.notes.md", "other.csv", "run.csv" }));
        QCOMPARE(findItem(tree, "run.csv")->toolTip(0), QString("With run.csv.notes.md"));
        QVERIFY(findItem(tree, "other.csv")->toolTip(0).isEmpty());
        findItem(tree, name)->setSelected(true);
        dialog.findChild<QPushButton *>("btnOpen")->click();
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
        localFile = dialog.localFile();
    };

    QString localFile;
    openFile("run.csv", localFile);
    QCOMPARE(fileContents(localFile + ".notes.md"), QByteArray("# SavvyCAN log notes v1\ntitle: bench\n"));
    QCOMPARE(GitHubCache::originOf(localFile + ".notes.md").path, QString("logs/run.csv.notes.md"));
    QCOMPARE(GitHubCache::originOf(localFile + ".notes.md").sha, f.repos.repos["o/r"]["main"].files["logs/run.csv.notes.md"].first);

    openFile("other.csv", localFile);
    QCOMPARE(localFile + ".notes.md", oldNotes);
    QVERIFY(!QFile::exists(oldNotes)); //not other.csv's notes any more
    QVERIFY(f.asked.isEmpty());
}

//saving: a notes file the writer wrote next to the log is committed after it, replacing the one there; one it didn't
//write is left on GitHub (and the user told); a writer that returns false without an error saves nothing quietly
void TestGitHub::testCompanionSave()
{
    SaveFixture f;
    int writes = 0;
    bool writeNotes = true;
    bool cancel = false;
    //localFile: the cached copy saved, empty if nothing was
    auto save = [&](bool replaceAnswer, QString &localFile)
    {
        localFile.clear();
        GitHubBrowserDialog dialog(GitHubBrowserDialog::Logs, GitHubBrowserDialog::Save, saveTypes);
        f.prepare(dialog, QByteArray());
        dialog.setCompanionSuffixes({ ".notes.md" });
        dialog.setSaveWriter([&](const QString &localFile, int, QString &)
        {
            if (cancel) return false;
            writes++;
            QFile log(localFile);
            if (!log.open(QIODevice::WriteOnly)) return false;
            log.write("log v" + QByteArray::number(writes));
            if (writeNotes)
            {
                QFile notes(localFile + ".notes.md");
                if (!notes.open(QIODevice::WriteOnly)) return false;
                notes.write("notes v" + QByteArray::number(writes));
            }
            return true;
        });
        dialog.setSuggestion(GitHubOrigin(), "bench.csv");
        dialog.show();
        QTreeWidget *tree = dialog.findChild<QTreeWidget *>("tree");
        QTRY_VERIFY(findItem(tree, "logs"));
        emit tree->itemClicked(findItem(tree, "logs"), 0);
        if (replaceAnswer) f.answers = { QMessageBox::Yes }; //"already in o/r. Replace it?"
        dialog.findChild<QPushButton *>("btnOpen")->click();
        if (dialog.result() == QDialog::Accepted) localFile = dialog.localFile();
    };
    const auto &files = f.repos.repos["o/r"]["main"].files;

    //new log and notes
    QString localFile;
    save(false, localFile);
    QVERIFY(!localFile.isEmpty());
    QCOMPARE(files["logs/bench.csv"].second, QByteArray("log v1"));
    QCOMPARE(files["logs/bench.csv.notes.md"].second, QByteArray("notes v1"));
    QCOMPARE(f.repos.lastMessage, QString("Add bench.csv from SavvyCAN (bench.csv.notes.md)"));
    QCOMPARE(f.asked.count(), 1);
    QVERIFY(f.asked[0].contains("With bench.csv.notes.md."));
    QCOMPARE(fileContents(localFile + ".notes.md"), QByteArray("notes v1")); //cached as saved
    QCOMPARE(GitHubCache::originOf(localFile + ".notes.md").sha, files["logs/bench.csv.notes.md"].first);

    //saved again over both
    f.asked.clear();
    save(true, localFile);
    QVERIFY(!localFile.isEmpty());
    QCOMPARE(files["logs/bench.csv"].second, QByteArray("log v2"));
    QCOMPARE(files["logs/bench.csv.notes.md"].second, QByteArray("notes v2"));
    QVERIFY(f.asked.last().contains("With bench.csv.notes.md."));

    //saved without notes: the notes file on GitHub stays, and the user is told
    f.asked.clear();
    writeNotes = false;
    save(true, localFile);
    QVERIFY(!localFile.isEmpty());
    QCOMPARE(files["logs/bench.csv"].second, QByteArray("log v3"));
    QCOMPARE(files["logs/bench.csv.notes.md"].second, QByteArray("notes v2"));
    QVERIFY(f.asked.last().contains("bench.csv.notes.md on GitHub is from an earlier save"));
    QVERIFY(!QFile::exists(localFile + ".notes.md")); //not cached as this log's

    //the writer cancelled: nothing saved, nothing said
    f.asked.clear();
    cancel = true;
    const int requests = int(f.github.requests.count());
    save(true, localFile);
    QVERIFY(localFile.isEmpty());
    QCOMPARE(files["logs/bench.csv"].second, QByteArray("log v3"));
    QCOMPARE(f.asked.count(), 1); //only "Replace it?", asked before writing
    QVERIFY(f.asked[0].contains("Replace it?"));
    for (int i = requests; i < f.github.requests.count(); i++)
        QVERIFY(f.github.requests[i].method != "PUT");
}

QTEST_MAIN(TestGitHub)
#include "test_github.moc"
