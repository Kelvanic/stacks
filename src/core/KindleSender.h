#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

class QSslSocket;

namespace stacks {

// Amazon's "Send to Kindle for Email" service: you attach the file to your
// personal @kindle.com address. This speaks just enough SMTP to do that,
// including STARTTLS and base64 attachments.
class KindleSender : public QObject
{
    Q_OBJECT

public:
    struct Settings {
        QString fromAddress;
        QString smtpHost = QStringLiteral("smtp.gmail.com");
        int smtpPort = 587;
        QString user;
        QString password;
        QString kindleAddress;
        bool implicitTls = false; // Also enabled automatically for port 465.
        int timeoutMs = 30000;
    };

    // Amazon's limit for documents sent by email.
    static constexpr qint64 kMaxAttachmentBytes = 50LL * 1024 * 1024;

    explicit KindleSender(QObject *parent = nullptr);
    ~KindleSender() override;

    bool isBusy() const { return m_busy; }
    void send(const Settings &settings, const QString &filePath,
              const QString &title, const QString &author);

signals:
    void progress(const QString &message);
    void finished(bool ok, const QString &message);

private:
    enum class State { Idle, Greeting, Ehlo, StartTls, TlsHandshake, SecureEhlo,
                       Auth, MailFrom, Recipient, Data, Body, Quit };

    void start();
    void onConnected();
    void onReadyRead();
    void handleReply(int code);
    void authenticate();
    void onSocketError();
    void fail(const QString &message);
    void finish(bool ok, const QString &message);
    void writeLine(const QString &line);
    void command(State state, const QString &line);
    QByteArray buildMessage(const QString &filePath, const QString &title, const QString &author) const;

    QSslSocket *m_socket = nullptr;
    Settings m_settings;
    State m_state = State::Idle;
    QTimer m_timeout;
    QStringList m_replyLines;
    int m_replyCode = 0;
    QByteArray m_message;
    bool m_busy = false;
    bool m_deliveryAccepted = false;
};

} // namespace stacks
