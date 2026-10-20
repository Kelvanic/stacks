#include "KindleSender.h"

#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QRegularExpression>
#include <QSslSocket>
#include <QUuid>

namespace stacks {
namespace {

QString sanitizeHeader(QString value)
{
    value.replace(QLatin1Char('\r'), QLatin1Char(' '));
    value.replace(QLatin1Char('\n'), QLatin1Char(' '));
    return value.trimmed();
}

bool validAddress(const QString &address)
{
    static const QRegularExpression email(QStringLiteral("^[^\\s<>@]+@[^\\s<>@]+$"));
    return email.match(address).hasMatch();
}

} // namespace

KindleSender::KindleSender(QObject *parent) : QObject(parent)
{
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, [this]() {
        finish(m_deliveryAccepted, m_deliveryAccepted ? QStringLiteral("Sent to Kindle.")
                                                     : QStringLiteral("The mail server timed out."));
    });
}

KindleSender::~KindleSender() = default;

void KindleSender::send(const Settings &settings, const QString &filePath,
                        const QString &title, const QString &author)
{
    if (m_busy)
        return; // Never abort an active transfer because of a duplicate request.
    m_settings = settings;
    m_settings.fromAddress = settings.fromAddress.trimmed();
    m_settings.kindleAddress = settings.kindleAddress.trimmed();
    if (!validAddress(m_settings.fromAddress) || !validAddress(m_settings.kindleAddress)) {
        finish(false, QStringLiteral("Add valid sending and Kindle email addresses first."));
        return;
    }
    if (settings.smtpHost.trimmed().isEmpty() || settings.user.isEmpty()
        || settings.password.isEmpty() || settings.smtpPort < 1 || settings.smtpPort > 65535) {
        finish(false, QStringLiteral("Add your SMTP host, port, user and app password first."));
        return;
    }
    m_message = buildMessage(filePath, title, author);
    if (m_message.isEmpty()) {
        finish(false, QStringLiteral("The attachment could not be read."));
        return;
    }
    m_busy = true;
    m_deliveryAccepted = false;
    m_state = State::Greeting;
    m_replyCode = 0;
    m_replyLines.clear();
    start();
}

void KindleSender::start()
{
    m_socket = new QSslSocket(this);
    m_socket->setPeerVerifyMode(QSslSocket::VerifyPeer);
    m_socket->setReadBufferSize(65536);
    connect(m_socket, &QSslSocket::connected, this, &KindleSender::onConnected);
    connect(m_socket, &QSslSocket::readyRead, this, &KindleSender::onReadyRead);
    connect(m_socket, &QSslSocket::encrypted, this, [this]() {
        if (m_state == State::TlsHandshake)
            command(State::SecureEhlo, QStringLiteral("EHLO stacks.local"));
        else
            onReadyRead(); // Implicit TLS: now wait for the server greeting.
    });
    connect(m_socket, &QSslSocket::sslErrors, this, [this](const QList<QSslError> &) {
        fail(QStringLiteral("The mail server's TLS certificate could not be verified."));
    });
    connect(m_socket, &QSslSocket::errorOccurred, this, [this](QSslSocket::SocketError) {
        onSocketError();
    });
    connect(m_socket, &QSslSocket::bytesWritten, this, [this](qint64) {
        if (m_state == State::Body)
            m_timeout.start(qMax(1, m_settings.timeoutMs));
    });
    connect(m_socket, &QSslSocket::disconnected, this, [this]() {
        if (m_busy)
            finish(m_deliveryAccepted, m_deliveryAccepted ? QStringLiteral("Sent to Kindle.")
                                                         : QStringLiteral("The mail server disconnected."));
    });
    m_timeout.start(qMax(1, m_settings.timeoutMs));
    emit progress(QStringLiteral("Connecting to %1…").arg(m_settings.smtpHost));
    if (m_settings.implicitTls || m_settings.smtpPort == 465)
        m_socket->connectToHostEncrypted(m_settings.smtpHost, quint16(m_settings.smtpPort));
    else
        m_socket->connectToHost(m_settings.smtpHost, quint16(m_settings.smtpPort));
}

void KindleSender::onConnected()
{
    emit progress(QStringLiteral("Connected. Waiting for the mail server…"));
}

void KindleSender::writeLine(const QString &line)
{
    m_timeout.start(qMax(1, m_settings.timeoutMs));
    m_socket->write(line.toUtf8() + "\r\n");
}

void KindleSender::command(State state, const QString &line)
{
    m_state = state;
    writeLine(line);
}

void KindleSender::onReadyRead()
{
    if (!m_busy || !m_socket || m_state == State::TlsHandshake)
        return;
    if ((m_settings.implicitTls || m_settings.smtpPort == 465) && !m_socket->isEncrypted())
        return;
    if (m_socket->bytesAvailable() > 8192 && !m_socket->canReadLine()) {
        fail(QStringLiteral("The mail server returned an oversized reply."));
        return;
    }
    while (m_busy && m_socket && m_socket->canReadLine()) {
        const QByteArray raw = m_socket->readLine(8193);
        if (raw.size() > 8192 || !raw.endsWith('\n')) {
            fail(QStringLiteral("The mail server returned an oversized reply."));
            return;
        }
        const QString line = QString::fromUtf8(raw).trimmed();
        bool ok = false;
        const int code = line.left(3).toInt(&ok);
        if (!ok || line.size() < 3 || code < 200 || code > 599
            || (line.size() > 3 && line.at(3) != QLatin1Char('-') && line.at(3) != QLatin1Char(' '))
            || (m_replyCode != 0 && code != m_replyCode) || m_replyLines.size() >= 100) {
            fail(QStringLiteral("The mail server returned an invalid reply."));
            return;
        }
        m_timeout.start(qMax(1, m_settings.timeoutMs));
        m_replyCode = code;
        m_replyLines.append(line);
        if (line.size() > 3 && line.at(3) == QLatin1Char('-'))
            continue; // Wait for the final line of a multiline response.
        handleReply(code);
        m_replyCode = 0;
        m_replyLines.clear();
        if (m_state == State::TlsHandshake)
            return;
    }
}

void KindleSender::authenticate()
{
    if (!m_socket->isEncrypted()) {
        fail(QStringLiteral("Refusing to send credentials without verified TLS."));
        return;
    }
    bool supportsPlain = false;
    for (const QString &line : m_replyLines) {
        const QString capability = line.mid(4).toUpper();
        if (capability.startsWith(QLatin1String("AUTH "))
            && capability.split(QLatin1Char(' '), Qt::SkipEmptyParts).contains(QStringLiteral("PLAIN")))
            supportsPlain = true;
    }
    if (!supportsPlain) {
        fail(QStringLiteral("The mail server does not support AUTH PLAIN over TLS."));
        return;
    }
    QByteArray credentials(1, '\0');
    credentials += m_settings.user.toUtf8();
    credentials += '\0';
    credentials += m_settings.password.toUtf8();
    command(State::Auth, QStringLiteral("AUTH PLAIN %1").arg(QString::fromLatin1(credentials.toBase64())));
}

void KindleSender::handleReply(int code)
{
    int expected = 250;
    if (m_state == State::Greeting || m_state == State::StartTls) expected = 220;
    if (m_state == State::Auth) expected = 235;
    if (m_state == State::Data) expected = 354;
    if (m_state == State::Quit) expected = 221;
    if (code != expected && !(m_state == State::Recipient && code == 251)) {
        fail(QStringLiteral("The mail server rejected the request (SMTP %1).").arg(code));
        return;
    }
    switch (m_state) {
    case State::Greeting:
        command(State::Ehlo, QStringLiteral("EHLO stacks.local"));
        break;
    case State::Ehlo:
        if (m_socket->isEncrypted()) {
            authenticate();
        } else {
            bool startTls = false;
            for (const QString &line : m_replyLines)
                startTls |= line.mid(4).trimmed().compare(QStringLiteral("STARTTLS"), Qt::CaseInsensitive) == 0;
            if (!startTls) {
                fail(QStringLiteral("The mail server does not offer STARTTLS. No credentials were sent."));
                return;
            }
            command(State::StartTls, QStringLiteral("STARTTLS"));
        }
        break;
    case State::StartTls:
        m_state = State::TlsHandshake;
        m_socket->startClientEncryption();
        break;
    case State::SecureEhlo:
        authenticate();
        break;
    case State::Auth:
        command(State::MailFrom, QStringLiteral("MAIL FROM:<%1>").arg(m_settings.fromAddress));
        break;
    case State::MailFrom:
        command(State::Recipient, QStringLiteral("RCPT TO:<%1>").arg(m_settings.kindleAddress));
        break;
    case State::Recipient:
        command(State::Data, QStringLiteral("DATA"));
        break;
    case State::Data: {
        emit progress(QStringLiteral("Sending the file…"));
        QString message = m_message;
        message.replace(QStringLiteral("\r\n."), QStringLiteral("\r\n.."));
        if (message.startsWith(QLatin1Char('.')))
            message.prepend(QLatin1Char('.'));
        m_state = State::Body;
        m_socket->write(message.toUtf8() + ".\r\n");
        break;
    }
    case State::Body:
        m_deliveryAccepted = true;
        command(State::Quit, QStringLiteral("QUIT"));
        break;
    case State::Quit:
        finish(true, QStringLiteral("Sent to Kindle."));
        break;
    default:
        fail(QStringLiteral("Unexpected SMTP reply."));
    }
}

void KindleSender::onSocketError()
{
    if (m_busy)
        finish(m_deliveryAccepted, m_deliveryAccepted ? QStringLiteral("Sent to Kindle.")
               : QStringLiteral("Mail error: %1").arg(m_socket->errorString()));
}

void KindleSender::fail(const QString &message) { finish(false, message); }

void KindleSender::finish(bool ok, const QString &message)
{
    m_busy = false;
    m_state = State::Idle;
    m_timeout.stop();
    if (m_socket) {
        QSslSocket *socket = m_socket;
        m_socket = nullptr;
        socket->disconnect(this);
        socket->abort();
        socket->deleteLater();
    }
    m_message.clear();
    m_settings.password.clear();
    emit finished(ok, message);
}

QString KindleSender::buildMessage(const QString &filePath, const QString &title, const QString &author) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const QByteArray payload = file.readAll();
    if (file.error() != QFile::NoError)
        return {};
    QString name = sanitizeHeader(QFileInfo(filePath).fileName());
    name.replace(QLatin1Char('"'), QLatin1Char('_'));
    name.replace(QLatin1Char('\\'), QLatin1Char('_'));
    const QString boundary = QStringLiteral("----Stacks-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    QString message = QStringLiteral("From: %1\r\nTo: %2\r\nSubject: =?UTF-8?B?%3?=\r\n"
                                     "MIME-Version: 1.0\r\nContent-Type: multipart/mixed; boundary=\"%4\"\r\n\r\n")
                          .arg(m_settings.fromAddress, m_settings.kindleAddress,
                               QString::fromLatin1(sanitizeHeader(title).toUtf8().toBase64()), boundary);
    message += QStringLiteral("--%1\r\nContent-Type: text/plain; charset=utf-8\r\n\r\nConverted with Stacks.\r\n%2 — %3\r\n\r\n")
                   .arg(boundary, sanitizeHeader(title), sanitizeHeader(author));
    const QString mime = QMimeDatabase().mimeTypeForFile(filePath).name();
    message += QStringLiteral("--%1\r\nContent-Type: %2; name=\"%3\"\r\nContent-Transfer-Encoding: base64\r\n"
                              "Content-Disposition: attachment; filename=\"%3\"\r\n\r\n").arg(boundary, mime, name);
    const QByteArray encoded = payload.toBase64();
    for (qsizetype i = 0; i < encoded.size(); i += 76)
        message += QString::fromLatin1(encoded.mid(i, 76)) + QStringLiteral("\r\n");
    return message + QStringLiteral("\r\n--%1--\r\n").arg(boundary);
}

} // namespace stacks
