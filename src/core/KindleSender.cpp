#include "KindleSender.h"

#include <QFile>
#include <QFileInfo>
#include <QSslSocket>
#include <QTextStream>

namespace stacks {

namespace {

QString base64Chunked(const QByteArray &data)
{
    const QByteArray encoded = data.toBase64();
    QString out;
    for (int i = 0; i < encoded.size(); i += 76)
        out += QString::fromLatin1(encoded.mid(i, 76)) + QLatin1Char('\n');
    return out;
}

QString sanitizeHeader(const QString &value)
{
    QString out = value;
    out.replace(QLatin1Char('\r'), QLatin1Char(' '));
    out.replace(QLatin1Char('\n'), QLatin1Char(' '));
    return out.trimmed();
}

} // namespace

KindleSender::KindleSender(QObject *parent)
    : QObject(parent)
{
}

KindleSender::~KindleSender() = default;

void KindleSender::send(const Settings &settings, const QString &filePath,
                        const QString &title, const QString &author)
{
    if (m_busy) {
        finish(false, QStringLiteral("A send is already in progress."));
        return;
    }
    if (!QFileInfo::exists(filePath)) {
        finish(false, QStringLiteral("The converted file is missing."));
        return;
    }
    if (settings.kindleAddress.trimmed().isEmpty()) {
        finish(false, QStringLiteral("Add your @kindle.com address first."));
        return;
    }
    if (settings.smtpHost.trimmed().isEmpty() || settings.user.trimmed().isEmpty()) {
        finish(false, QStringLiteral("Add your sending email and SMTP details first."));
        return;
    }

    m_settings = settings;
    m_filePath = filePath;
    m_title = title;
    m_author = author;
    m_busy = true;
    m_step = 0;
    m_command = 0;
    m_encrypted = false;

    m_steps.clear();

    Step hello;
    hello.commands = QStringList{QStringLiteral("EHLO stacks.local")};
    hello.expected = QStringList{QStringLiteral("250")};
    m_steps.append(hello);

    if (settings.smtpPort == 465) {
        m_encrypted = true;
    } else {
        Step startTls;
        startTls.commands = QStringList{QStringLiteral("STARTTLS")};
        startTls.expected = QStringList{QStringLiteral("220")};
        m_steps.append(startTls);

        Step helloAgain;
        helloAgain.commands = QStringList{QStringLiteral("EHLO stacks.local")};
        helloAgain.expected = QStringList{QStringLiteral("250")};
        m_steps.append(helloAgain);
    }

    Step auth;
    const QString plain = QString::fromLatin1(
        QByteArray("\0" + settings.user.toUtf8() + "\0" + settings.password.toUtf8()).toBase64());
    auth.commands = QStringList{QStringLiteral("AUTH PLAIN %1").arg(plain)};
    auth.expected = QStringList{QStringLiteral("235")};
    m_steps.append(auth);

    Step from;
    from.commands = QStringList{QStringLiteral("MAIL FROM:<%1>").arg(settings.fromAddress.trimmed())};
    from.expected = QStringList{QStringLiteral("250")};
    m_steps.append(from);

    Step to;
    to.commands = QStringList{QStringLiteral("RCPT TO:<%1>").arg(settings.kindleAddress.trimmed())};
    to.expected = QStringList{QStringLiteral("250")};
    m_steps.append(to);

    Step data;
    data.commands = QStringList{QStringLiteral("DATA")};
    data.expected = QStringList{QStringLiteral("354")};
    data.payloadAfter = true;
    m_steps.append(data);

    Step quit;
    quit.commands = QStringList{QStringLiteral("QUIT")};
    quit.expected = QStringList{QStringLiteral("221")};
    m_steps.append(quit);

    start();
}

void KindleSender::start()
{
    delete m_socket;
    m_socket = new QSslSocket(this);
    m_socket->setPeerVerifyMode(QSslSocket::VerifyNone);

    connect(m_socket, &QSslSocket::connected, this, &KindleSender::onConnected);
    connect(m_socket, &QSslSocket::readyRead, this, &KindleSender::onReadyRead);
    connect(m_socket, &QSslSocket::encrypted, this, [this]() {
        m_encrypted = true;
        onReadyRead();
    });
    connect(m_socket, &QSslSocket::errorOccurred, this, [this](QSslSocket::SocketError) {
        onSocketError();
    });

    emit progress(QStringLiteral("Connecting to %1…").arg(m_settings.smtpHost));
    if (m_settings.smtpPort == 465)
        m_socket->connectToHostEncrypted(m_settings.smtpHost, quint16(m_settings.smtpPort));
    else
        m_socket->connectToHost(m_settings.smtpHost, quint16(m_settings.smtpPort));
}

void KindleSender::onConnected()
{
    emit progress(QStringLiteral("Connected. Handshaking…"));
}

void KindleSender::writeLine(const QString &line)
{
    if (!m_socket)
        return;
    const QByteArray data = line.toUtf8() + "\r\n";
    m_socket->write(data);
}

void KindleSender::sendNextCommand()
{
    if (m_step >= m_steps.size()) {
        finish(true, QStringLiteral("Sent to Kindle."));
        return;
    }
    const Step &step = m_steps.at(m_step);
    if (m_command < step.commands.size())
        writeLine(step.commands.at(m_command));
}

void KindleSender::onReadyRead()
{
    if (!m_socket || m_step >= m_steps.size())
        return;

    while (m_socket->canReadLine()) {
        const QString line = QString::fromUtf8(m_socket->readLine()).trimmed();
        if (line.isEmpty())
            continue;

        const Step &step = m_steps.at(m_step);
        const QString expected = m_command < step.expected.size() ? step.expected.at(m_command) : QString();

        if (m_expectBody) {
            // The body was accepted; the server confirms with 250.
            m_expectBody = false;
            if (!line.startsWith(QLatin1String("250"))) {
                fail(QStringLiteral("The mail server rejected the attachment: %1").arg(line));
                return;
            }
            m_step += 1;
            m_command = 0;
            sendNextCommand();
            return;
        }

        if (line.startsWith(expected)) {
            if (step.payloadAfter && m_command == step.commands.size() - 1) {
                emit progress(QStringLiteral("Sending the file…"));
                const QString message = buildMessage(m_filePath, m_title, m_author);
                writeLine(message);
                m_socket->write("\r\n.\r\n");
                m_expectBody = true;
                return;
            }
            m_command += 1;
            if (m_command >= step.commands.size()) {
                m_step += 1;
                m_command = 0;
            }
            sendNextCommand();
            return;
        }

        if (line.startsWith(QLatin1Char('2')) || line.startsWith(QLatin1Char('3')))
            continue;

        if (line.startsWith(QLatin1String("454"))) {
            fail(QStringLiteral("Your Kindle address rejected the file. Check it in Amazon’s Personal Document Settings."));
            return;
        }
        if (line.startsWith(QLatin1String("535"))
            || line.startsWith(QLatin1String("530"))
            || line.startsWith(QLatin1String("550"))) {
            fail(QStringLiteral("The mail server refused the login. Check your email, app password and SMTP host."));
            return;
        }
        fail(QStringLiteral("The mail server said: %1").arg(line));
        return;
    }
}

void KindleSender::onSocketError()
{
    if (!m_busy)
        return;
    fail(QStringLiteral("Mail error: %1").arg(m_socket ? m_socket->errorString() : QStringLiteral("unknown")));
}

void KindleSender::fail(const QString &message)
{
    finish(false, message);
}

void KindleSender::finish(bool ok, const QString &message)
{
    m_busy = false;
    if (m_socket) {
        m_socket->abort();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    emit finished(ok, message);
}

QString KindleSender::buildMessage(const QString &filePath, const QString &title,
                                   const QString &author) const
{
    QFile file(filePath);
    QByteArray payload;
    if (file.open(QIODevice::ReadOnly))
        payload = file.readAll();
    file.close();

    const QString name = QFileInfo(filePath).fileName();
    const QString boundary = QStringLiteral("----=_StacksKindle");

    QString message;
    message += QStringLiteral("From: %1\r\n").arg(sanitizeHeader(m_settings.fromAddress));
    message += QStringLiteral("To: %1\r\n").arg(sanitizeHeader(m_settings.kindleAddress));
    message += QStringLiteral("Subject: %1\r\n").arg(sanitizeHeader(title));
    message += QStringLiteral("MIME-Version: 1.0\r\n");
    message += QStringLiteral("Content-Type: multipart/mixed; boundary=\"%1\"\r\n\r\n").arg(boundary);

    message += QStringLiteral("--%1\r\n").arg(boundary);
    message += QStringLiteral("Content-Type: text/plain; charset=utf-8\r\n\r\n");
    message += QStringLiteral("Converted with Stacks.\r\n");
    if (!author.isEmpty())
        message += QStringLiteral("%1 — %2\r\n").arg(title, author);
    message += QStringLiteral("\r\n");

    message += QStringLiteral("--%1\r\n").arg(boundary);
    message += QStringLiteral("Content-Type: application/epub+zip; name=\"%1\"\r\n").arg(name);
    message += QStringLiteral("Content-Transfer-Encoding: base64\r\n");
    message += QStringLiteral("Content-Disposition: attachment; filename=\"%1\"\r\n\r\n").arg(name);
    message += base64Chunked(payload);
    message += QStringLiteral("\r\n--%1--\r\n").arg(boundary);

    return message;
}

} // namespace stacks
