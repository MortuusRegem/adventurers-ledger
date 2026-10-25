#pragma once
#include <QtNetwork>
#include <QtWidgets>
#include <functional>

// Public release metadata only. Character data never enters a network request.
namespace LedgerUpdater
{
inline QString version()
{
    return "1.1.1";
}
inline QString repo()
{
    return "MortuusRegem/adventurers-ledger";
}
inline QString installerName(const QString &releaseVersion)
{
#ifdef Q_OS_MACOS
    const QString arch = QSysInfo::buildCpuArchitecture() == "arm64" ? "arm64" : "x86_64";
    return "Adventurers-Ledger-" + releaseVersion + "-macOS-" + arch + ".dmg";
#else
    return "Adventurers-Ledger-Setup-" + releaseVersion + ".exe";
#endif
}
struct Release
{
    QString version, hash;
    QUrl url;
    qint64 size = 0;
};
inline bool parseRelease(const QJsonObject &obj, Release &out)
{
    const QString tag = obj["tag_name"].toString();
    if (obj["draft"].toBool() || obj["prerelease"].toBool() ||
        !QRegularExpression("^v[0-9]+\\.[0-9]+\\.[0-9]+$").match(tag).hasMatch())
        return false;
    out.version = tag.mid(1);
    const QString name = installerName(out.version);
    for (auto entry : obj["assets"].toArray())
    {
        auto a = entry.toObject();
        if (a["name"].toString() != name)
            continue;
        out.url = QUrl(a["browser_download_url"].toString());
        out.size = a["size"].toInteger();
        const QString digest = a["digest"].toString();
        if (!QRegularExpression("^sha256:[a-fA-F0-9]{64}$").match(digest).hasMatch())
            return false;
        out.hash = digest.mid(7).toLower();
        return out.url.toString() ==
                   "https://github.com/" + repo() + "/releases/download/" + tag + "/" + name &&
               out.size > 0 && out.size <= 128 * 1024 * 1024;
    }
    return false;
}
inline bool fetch(QWidget *parent, QUrl url, QIODevice &output, qint64 limit, QString label,
                  QString &error, int &status)
{
    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "AdventurersLedger/1.1.1");
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(30000);
    auto *reply = manager.get(request);
    QProgressDialog progress(label, "Cancel", 0, 0, parent);
    progress.setWindowTitle("Adventurer's Ledger updates");
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setMinimumDuration(0);
    QEventLoop loop;
    QTimer deadline;
    deadline.setSingleShot(true);
    QObject::connect(&deadline, &QTimer::timeout, reply,
                     [&]
                     {
                         error = "The update request timed out.";
                         reply->abort();
                     });
    QObject::connect(&progress, &QProgressDialog::canceled, reply,
                     [&]
                     {
                         error = "Canceled.";
                         reply->abort();
                     });
    qint64 received = 0;
    auto read = [&]
    {
        const auto bytes = reply->readAll();
        received += bytes.size();
        if (received > limit)
        {
            error = "Update download exceeded its expected size.";
            reply->abort();
            return;
        }
        if (output.write(bytes) != bytes.size())
        {
            error = "Could not write the update download.";
            reply->abort();
        }
    };
    QObject::connect(reply, &QNetworkReply::readyRead, &loop, read);
    QObject::connect(reply, &QNetworkReply::downloadProgress, &progress,
                     [&](qint64 done, qint64 total)
                     {
                         if (total > 0 && total <= limit)
                         {
                             progress.setMaximum(100);
                             progress.setValue(int(done * 100 / total));
                         }
                     });
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    deadline.start(300000);
    if (!reply->isFinished())
        loop.exec();
    read();
    status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (error.isEmpty() && (reply->error() != QNetworkReply::NoError || status != 200))
        error = "Could not retrieve the update. Check your internet connection and try again.\n" +
                reply->errorString();
    return error.isEmpty();
}
inline bool backup(const QString &directory, QString &destination)
{
    destination = QFileInfo(directory).absolutePath() + "/update-backups/" +
                  QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz");
    if (!QDir().mkpath(destination))
        return false;
    QDir source(directory);
    QDirIterator files(directory, QDir::Files | QDir::Hidden | QDir::NoSymLinks,
                       QDirIterator::Subdirectories);
    while (files.hasNext())
    {
        QString from = files.next(), to = destination + "/" + source.relativeFilePath(from);
        if (!QDir().mkpath(QFileInfo(to).absolutePath()) || !QFile::copy(from, to))
            return false;
    }
    return true;
}
inline bool selfTest()
{
    const QString name = installerName("1.2.0");
    QJsonObject asset{{"name", name},
                      {"size", 1234},
                      {"digest", "sha256:" + QString(64, 'a')},
                      {"browser_download_url",
                       "https://github.com/" + repo() + "/releases/download/v1.2.0/" + name}};
    QJsonObject obj{{"tag_name", "v1.2.0"}, {"assets", QJsonArray{asset}}};
    Release release;
    if (!parseRelease(obj, release) || release.version != "1.2.0" || release.size != 1234)
        return false;
    for (auto key : {"prerelease", "draft"})
    {
        auto bad = obj;
        bad[key] = true;
        if (parseRelease(bad, release))
            return false;
    }
    for (auto url : {"http://github.com/unsafe.exe", "https://example.com/update.exe"})
    {
        auto bad = obj, a = asset;
        a["browser_download_url"] = url;
        bad["assets"] = QJsonArray{a};
        if (parseRelease(bad, release))
            return false;
    }
    for (auto digest : {"", "sha256:bad", "sha1:123"})
    {
        auto bad = obj, a = asset;
        a["digest"] = digest;
        bad["assets"] = QJsonArray{a};
        if (parseRelease(bad, release))
            return false;
    }
    QTemporaryDir dir;
    const QString source = dir.path() + "/characters";
    QDir().mkpath(source);
    QFile original(source + "/test.json");
    if (!original.open(QIODevice::WriteOnly) || original.write("test character") < 0)
        return false;
    original.close();
    QString backupPath;
    if (!backup(source, backupPath))
        return false;
    QFile copied(backupPath + "/test.json");
    if (!copied.open(QIODevice::ReadOnly) || copied.readAll() != "test character")
        return false;
    for (int scenario = 0; scenario < 3; ++scenario)
    {
        QTcpServer server;
        if (!server.listen(QHostAddress::LocalHost))
            return false;
        QObject::connect(
            &server, &QTcpServer::newConnection, &server,
            [&]
            {
                auto *socket = server.nextPendingConnection();
                QObject::connect(
                    socket, &QTcpSocket::readyRead, socket,
                    [=]
                    {
                        socket->readAll();
                        socket->write(scenario == 2 ? "HTTP/1.1 404 Not Found\r\nContent-Length: "
                                                      "3\r\nConnection: close\r\n\r\nbad"
                                                    : "HTTP/1.1 200 OK\r\nContent-Length: "
                                                      "5\r\nConnection: close\r\n\r\nhello");
                        socket->disconnectFromHost();
                    });
            });
        QByteArray data;
        QBuffer buffer(&data);
        buffer.open(QIODevice::WriteOnly);
        QString error;
        int status;
        bool okay = fetch(nullptr, QUrl("http://127.0.0.1:" + QString::number(server.serverPort())),
                          buffer, scenario == 1 ? 2 : 100, "Testing updater", error, status);
        if (okay != (scenario == 0) || (okay && data != "hello"))
            return false;
    }
    return true;
}
inline void check(QWidget *parent, const std::function<bool()> &save, const QString &directory)
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    QString error;
    int status = 0;
    if (!fetch(parent, QUrl("https://api.github.com/repos/" + repo() + "/releases/latest"), buffer,
               2 * 1024 * 1024, "Checking for updates…", error, status))
    {
        if (error != "Canceled.")
            QMessageBox::information(
                parent, "Updates",
                status == 404
                    ? "No published update is available yet. Installed version: " + version()
                    : error);
        return;
    }
    Release release;
    if (!parseRelease(QJsonDocument::fromJson(bytes).object(), release))
    {
        QMessageBox::warning(
            parent, "Updates",
            "This release has no valid installer for this computer and SHA-256 checksum. "
            "Please try again after the release is complete.");
        return;
    }
    if (QVersionNumber::fromString(release.version) <= QVersionNumber::fromString(version()))
    {
        QMessageBox::information(parent, "Updates",
                                 "You're up to date. Installed version: " + version());
        return;
    }
    if (QMessageBox::question(
            parent, "Update available",
            "Version " + release.version + " is available (installed: " + version() +
                ").\n\nDownload the update? Your characters stay on this computer.") !=
        QMessageBox::Yes)
        return;
    const QString cache = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) +
                          "/updates/" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!QDir().mkpath(cache))
    {
        QMessageBox::warning(parent, "Updates", "Could not create the update download folder.");
        return;
    }
    const QString path = cache + "/" + installerName(release.version);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        QMessageBox::warning(parent, "Updates", "Could not save the installer.");
        return;
    }
    error.clear();
    if (!fetch(parent, release.url, file, release.size, "Downloading update…", error, status))
    {
        file.cancelWriting();
        if (error != "Canceled.")
            QMessageBox::warning(parent, "Updates", error);
        return;
    }
    if (!file.commit())
    {
        QMessageBox::warning(parent, "Updates", "Could not finish saving the installer.");
        return;
    }
    QFile downloaded(path);
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!downloaded.open(QIODevice::ReadOnly) || downloaded.size() != release.size ||
        !hash.addData(&downloaded) || hash.result().toHex() != release.hash.toLatin1())
    {
        downloaded.close();
        QFile::remove(path);
        QMessageBox::warning(parent, "Updates",
                             "Installer verification failed. Nothing was installed.");
        return;
    }
    downloaded.close();
    if (QMessageBox::question(parent, "Install update",
                              "The installer is verified. Save your character, back up your "
                              "collection, and close the app to install now?") != QMessageBox::Yes)
        return;
    if (!save())
    {
        QMessageBox::warning(parent, "Updates",
                             "Your character could not be saved. The update was stopped.");
        return;
    }
    QString backupPath;
    if (!backup(directory, backupPath))
    {
        QMessageBox::warning(parent, "Updates",
                             "Could not back up all characters. The update was stopped.");
        return;
    }
#ifdef Q_OS_MACOS
    QMessageBox::information(parent, "Install Mac update",
                             "The disk image will open and this app will close. Drag Adventurers "
                             "Ledger into Applications and choose Replace. Your characters are "
                             "stored separately and have been backed up.\n\nIf macOS blocks the "
                             "new app, open System Settings > Privacy & Security > Open Anyway.");
    if (!QProcess::startDetached("/usr/bin/open", {path}))
#else
    if (!QProcess::startDetached(path,
                                 {"/DIR=" + QCoreApplication::applicationDirPath(), "/NORESTART"}))
#endif
    {
        QMessageBox::warning(parent, "Updates",
                             "Could not open the installer. Your app is still open.");
        return;
    }
    parent->close();
}
} // namespace LedgerUpdater
