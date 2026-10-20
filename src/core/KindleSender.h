#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

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
    };

    explicit KindleSender(QObject *parent = nullptr);
    ~KindleSender() override;

    bool isBusy() const { return m_busy; }
    void send(const Settings &settings, const QString &filePath,
              const QString &title, const QString &author);

signals:
    void progress(const QString &message);
    void finished(bool ok, const QString &message);

private:
    struct Step {
        QStringList commands;   // one per round trip
        QStringList expected;   // expected reply code for each command
        bool payloadAfter = false; // the body follows the last command
    };

    void start();
    void onConnected();
    void onReadyRead();
    void onSocketError();
    void fail(const QString &message);
    void finish(bool ok, const QString &message);
    void writeLine(const QString &line);
    void sendNextCommand();
    QString buildMessage(const QString &filePath, const QString &title, const QString &author) const;

    QSslSocket *m_socket = nullptr;
    Settings m_settings;
    QString m_filePath;
    QString m_title;
    QString m_author;
    QVector<Step> m_steps;
    int m_step = 0;
    int m_command = 0;
    bool m_busy = false;
    bool m_encrypted = false;
    bool m_expectBody = false;
};

} // namespace stacks
