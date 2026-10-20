#include "core/KindleSender.h"

#include <QFile>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslKey>
#include <QSslSocket>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

using namespace stacks;

// Test-only localhost key/certificate, never used by the application.
class SmtpServer : public QTcpServer
{
public:
    bool implicitTls = false;
    bool offersTls = true;
    bool silent = false;
    bool rejectBody = false;
    bool authenticatedEncrypted = false;
    QByteArray credentials;
    QByteArray message;
    QStringList commands;
    QSslCertificate certificate;
    QSslKey key;

    SmtpServer()
    {
        QFile cert(QStringLiteral(STACKS_FIXTURE_DIR "/localhost-cert.pem"));
        QFile pem(QStringLiteral(STACKS_FIXTURE_DIR "/localhost-key.pem"));
        if (cert.open(QIODevice::ReadOnly)) certificate = QSslCertificate(cert.readAll());
        if (pem.open(QIODevice::ReadOnly)) key = QSslKey(pem.readAll(), QSsl::Rsa);
    }

protected:
    void incomingConnection(qintptr descriptor) override
    {
        auto *socket = new QSslSocket(this);
        socket->setSocketDescriptor(descriptor);
        socket->setLocalCertificate(certificate);
        socket->setPrivateKey(key);
        socket->setPeerVerifyMode(QSslSocket::VerifyNone); // Server does not request client certificates.
        auto inBody = std::make_shared<bool>(false);
        connect(socket, &QSslSocket::readyRead, this, [this, socket, inBody]() {
            while (socket->canReadLine()) {
                const QByteArray raw = socket->readLine();
                const QString line = QString::fromUtf8(raw).trimmed();
                if (*inBody) {
                    if (raw == ".\r\n") {
                        *inBody = false;
                        socket->write(rejectBody ? "550 rejected\r\n" : "250 queued\r\n");
                    } else {
                        message += raw;
                    }
                    continue;
                }
                commands.append(line);
                if (line.startsWith(QLatin1String("EHLO"))) {
                    if (socket->isEncrypted()) {
                        socket->write("250-localhost\r\n250-AUTH PLAIN LOGIN\r\n");
                        // Deliberately fragment a multiline reply across readyRead events.
                        QTimer::singleShot(5, socket, [socket]() { socket->write("250 SIZE 50000000\r\n"); });
                    } else {
                        socket->write(offersTls ? "250-localhost\r\n250-STARTTLS\r\n250 SIZE 50000000\r\n"
                                               : "250-localhost\r\n250 AUTH PLAIN\r\n");
                    }
                } else if (line == QLatin1String("STARTTLS")) {
                    socket->write("220 ready for TLS\r\n");
                    socket->flush();
                    socket->startServerEncryption();
                } else if (line.startsWith(QLatin1String("AUTH PLAIN "))) {
                    credentials = QByteArray::fromBase64(raw.mid(11).trimmed());
                    authenticatedEncrypted = socket->isEncrypted();
                    socket->write("235 authenticated\r\n");
                } else if (line.startsWith(QLatin1String("MAIL FROM:")) || line.startsWith(QLatin1String("RCPT TO:"))) {
                    socket->write("250 OK\r\n");
                } else if (line == QLatin1String("DATA")) {
                    *inBody = true;
                    socket->write("354 send data\r\n");
                } else if (line == QLatin1String("QUIT")) {
                    socket->write("221 bye\r\n");
                    socket->disconnectFromHost();
                }
            }
        });
        if (silent) return;
        if (implicitTls) {
            connect(socket, &QSslSocket::encrypted, socket, [socket]() { socket->write("220 localhost\r\n"); });
            socket->startServerEncryption();
        } else {
            socket->write("220 localhost\r\n");
        }
    }
};

class DefaultTlsConfig
{
    QSslConfiguration original = QSslConfiguration::defaultConfiguration();
public:
    explicit DefaultTlsConfig(const QSslCertificate &certificate)
    {
        auto configuration = original;
        configuration.addCaCertificate(certificate);
        QSslConfiguration::setDefaultConfiguration(configuration);
    }
    ~DefaultTlsConfig() { QSslConfiguration::setDefaultConfiguration(original); }
};

class TestSmtp : public QObject
{
    Q_OBJECT
private slots:
    void verifiedDelivery_data();
    void verifiedDelivery();
    void refusesUntrustedCertificate();
    void refusesMissingStartTls();
    void timesOutAndCanRetry();
};

static KindleSender::Settings settingsFor(const SmtpServer &server)
{
    KindleSender::Settings settings;
    settings.smtpHost = QStringLiteral("localhost");
    settings.smtpPort = server.serverPort();
    settings.implicitTls = server.implicitTls;
    settings.fromAddress = QStringLiteral("sender@example.com");
    settings.kindleAddress = QStringLiteral("reader@kindle.com");
    settings.user = QStringLiteral("test-user");
    settings.password = QStringLiteral("test-password");
    settings.timeoutMs = 3000;
    return settings;
}

static QString attachment(const QTemporaryDir &dir)
{
    const QString path = dir.filePath(QStringLiteral("document.txt"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return {};
    file.write("Attachment contents");
    return path;
}

void TestSmtp::verifiedDelivery_data()
{
    QTest::addColumn<bool>("implicit");
    QTest::newRow("STARTTLS") << false;
    QTest::newRow("implicit TLS") << true;
}

void TestSmtp::verifiedDelivery()
{
    QFETCH(bool, implicit);
    SmtpServer server;
    QVERIFY(!server.certificate.isNull() && !server.key.isNull());
    server.implicitTls = implicit;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    DefaultTlsConfig trusted(server.certificate);
    QTemporaryDir dir;
    KindleSender sender;
    QSignalSpy finished(&sender, &KindleSender::finished);
    sender.send(settingsFor(server), attachment(dir), QStringLiteral("A title"), QStringLiteral("An author"));
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 10000);
    QVERIFY2(finished.first().at(0).toBool(), qPrintable(finished.first().at(1).toString()));
    QVERIFY(server.authenticatedEncrypted);
    QCOMPARE(server.credentials, QByteArray("\0test-user\0test-password", 24));
    QCOMPARE(server.commands.count(QStringLiteral("EHLO stacks.local")), implicit ? 1 : 2);
    QCOMPARE(server.commands.contains(QStringLiteral("STARTTLS")), !implicit);
    QVERIFY(server.message.contains("Content-Type: text/plain; name=\"document.txt\""));
    QVERIFY(server.message.contains(QByteArray("Attachment contents").toBase64()));
    QVERIFY(!sender.isBusy());
}

void TestSmtp::refusesUntrustedCertificate()
{
    SmtpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    QTemporaryDir dir;
    KindleSender sender;
    QSignalSpy finished(&sender, &KindleSender::finished);
    sender.send(settingsFor(server), attachment(dir), QStringLiteral("Title"), QString());
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 10000);
    QVERIFY(!finished.first().at(0).toBool());
    QVERIFY(server.credentials.isEmpty());
    QVERIFY(server.message.isEmpty());
}

void TestSmtp::refusesMissingStartTls()
{
    SmtpServer server;
    server.offersTls = false;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    QTemporaryDir dir;
    KindleSender sender;
    QSignalSpy finished(&sender, &KindleSender::finished);
    sender.send(settingsFor(server), attachment(dir), QStringLiteral("Title"), QString());
    QTRY_COMPARE(finished.size(), 1);
    QVERIFY(!finished.first().at(0).toBool());
    QVERIFY(server.credentials.isEmpty());
    QCOMPARE(server.commands, QStringList{QStringLiteral("EHLO stacks.local")});
}

void TestSmtp::timesOutAndCanRetry()
{
    SmtpServer server;
    server.silent = true;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    DefaultTlsConfig trusted(server.certificate);
    QTemporaryDir dir;
    const QString path = attachment(dir);
    KindleSender sender;
    QSignalSpy finished(&sender, &KindleSender::finished);
    auto settings = settingsFor(server);
    settings.timeoutMs = 50;
    sender.send(settings, path, QStringLiteral("Title"), QString());
    QTRY_COMPARE(finished.size(), 1);
    QVERIFY(!finished.first().at(0).toBool());
    QVERIFY(!sender.isBusy());
    server.silent = false;
    server.rejectBody = true;
    settings.timeoutMs = 3000;
    sender.send(settings, path, QStringLiteral("Title"), QString());
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 2, 10000);
    QVERIFY(!finished.at(1).at(0).toBool());
    server.rejectBody = false;
    sender.send(settings, path, QStringLiteral("Title"), QString());
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 3, 10000);
    QVERIFY2(finished.at(2).at(0).toBool(), qPrintable(finished.at(2).at(1).toString()));
}

QTEST_GUILESS_MAIN(TestSmtp)
#include "tst_smtp.moc"
