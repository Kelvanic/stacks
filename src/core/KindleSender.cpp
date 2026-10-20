#include "KindleSender.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QMimeDatabase>
#include <QRegularExpression>
#include <QSslSocket>
#include <QUrl>
#include <QUuid>

#include <algorithm>

namespace stacks {
namespace {

QString sanitizeHeader(QString value)
{
    value.replace(QLatin1Char('\r'), QLatin1Char(' '));
    value.replace(QLatin1Char('\n'), QLatin1Char(' '));
    return value.trimmed();
}

QByteArray base64Lines(const QByteArray &bytes)
{
    QByteArray lines;
    const QByteArray encoded = bytes.toBase64();
    for (qsizetype i = 0; i < encoded.size(); i += 76)
        lines += encoded.mid(i, 76) + "\r\n";
    return lines;
}

QString megabytes(qint64 bytes)
{
    return QString::number(double(bytes) / (1024.0 * 1024.0), 'f', 1);
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
    const QFileInfo attachment(filePath);
    if (!attachment.isFile()) {
        finish(false, QStringLiteral("The attachment could not be read."));
        return;
    }
    if (attachment.size() > kMaxAttachmentBytes) {
        finish(false, QStringLiteral("Amazon accepts files up to %1 MB by email; this one is %2 MB.")
                          .arg(kMaxAttachmentBytes / (1024 * 1024))
                          .arg(megabytes(attachment.size())));
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
    qint64 sizeLimit = 0;
    for (const QString &line : m_replyLines) {
        const QString capability = line.mid(4).toUpper();
        const QStringList words = capability.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (capability.startsWith(QLatin1String("AUTH ")) && words.contains(QStringLiteral("PLAIN")))
            supportsPlain = true;
        if (words.size() == 2 && words.first() == QLatin1String("SIZE"))
            sizeLimit = words.at(1).toLongLong();
    }
    if (!supportsPlain) {
        fail(QStringLiteral("The mail server does not support AUTH PLAIN over TLS."));
        return;
    }
    // Refuse before uploading tens of megabytes the server would reject.
    if (sizeLimit > 0 && m_message.size() > sizeLimit) {
        fail(QStringLiteral("The mail server accepts messages up to %1 MB; this one is %2 MB once encoded.")
                 .arg(megabytes(sizeLimit), megabytes(m_message.size())));
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
        // Every line is a header, a boundary or base64, so none starts with
        // '.' and no dot-stuffing is needed. The socket keeps its own copy.
        m_state = State::Body;
        m_socket->write(m_message);
        m_socket->write(".\r\n");
        m_message = QByteArray();
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
    m_message = QByteArray();
    m_settings.password.clear();
    emit finished(ok, message);
}

// Builds a 7-bit message: UTF-8 only appears base64- or percent-encoded, so
// any SMTP server accepts it. The attachment streams in chunks straight into
// its base64 lines instead of being held in memory several times over.
QByteArray KindleSender::buildMessage(const QString &filePath, const QString &title, const QString &author) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    const QString name = sanitizeHeader(QFileInfo(filePath).fileName());
    QByteArray asciiName;
    for (const QChar c : name)
        asciiName += (c.unicode() >= 0x20 && c.unicode() < 0x7f && c != QLatin1Char('"') && c != QLatin1Char('\\'))
            ? char(c.unicode()) : '_';
    const QByteArray boundary = "----Stacks-" + QUuid::createUuid().toByteArray(QUuid::WithoutBraces);
    const QByteArray mime = QMimeDatabase().mimeTypeForFile(filePath).name().toLatin1();
    const QByteArray date = QLocale::c().toString(QDateTime::currentDateTimeUtc(),
                                                  QStringLiteral("ddd, dd MMM yyyy HH:mm:ss")).toLatin1() + " +0000";

    QByteArray message;
    message.reserve(qsizetype(file.size() * 4 / 3 + file.size() / 38 + 4096));
    message += "From: <" + m_settings.fromAddress.toUtf8() + ">\r\n"
               "To: <" + m_settings.kindleAddress.toUtf8() + ">\r\n"
               "Subject: =?UTF-8?B?" + sanitizeHeader(title).toUtf8().toBase64() + "?=\r\n"
               "Date: " + date + "\r\n"
               "Message-ID: <" + QUuid::createUuid().toByteArray(QUuid::WithoutBraces) + "@stacks.local>\r\n"
               "MIME-Version: 1.0\r\n"
               "Content-Type: multipart/mixed; boundary=\"" + boundary + "\"\r\n\r\n";
    message += "--" + boundary + "\r\n"
               "Content-Type: text/plain; charset=utf-8\r\n"
               "Content-Transfer-Encoding: base64\r\n\r\n"
               + base64Lines(QStringLiteral("Converted with Stacks.\r\n%1 — %2\r\n")
                                 .arg(sanitizeHeader(title), sanitizeHeader(author)).toUtf8())
               + "\r\n";
    message += "--" + boundary + "\r\n"
               "Content-Type: " + mime + "; name=\"" + asciiName + "\"\r\n"
               "Content-Transfer-Encoding: base64\r\n"
               "Content-Disposition: attachment; filename=\"" + asciiName + "\"; filename*=UTF-8''"
               + QUrl::toPercentEncoding(name) + "\r\n\r\n";

    // Whole 57-byte groups encode to unpadded 76-character lines.
    constexpr qint64 kChunk = 57 * 1024;
    while (!file.atEnd()) {
        const QByteArray encoded = file.read(kChunk).toBase64();
        if (encoded.isEmpty())
            break;
        for (qsizetype i = 0; i < encoded.size(); i += 76) {
            message.append(encoded.constData() + i, std::min<qsizetype>(76, encoded.size() - i));
            message += "\r\n";
        }
    }
    if (file.error() != QFile::NoError)
        return {};
    message += "\r\n--" + boundary + "--\r\n";
    return message;
}

} // namespace stacks
