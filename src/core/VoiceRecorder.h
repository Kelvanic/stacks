#pragma once

#include <QAudioFormat>
#include <QByteArray>
#include <QElapsedTimer>
#include <QObject>
#include <QString>

QT_BEGIN_NAMESPACE
class QAudioSource;
class QIODevice;
QT_END_NAMESPACE

namespace stacks {

class VoiceRecorder : public QObject
{
    Q_OBJECT
public:
    explicit VoiceRecorder(QObject *parent = nullptr);
    ~VoiceRecorder() override;

    bool start(const QString &directory, QString *error = nullptr);
    QString stop();
    void cancel();

    bool isRecording() const { return m_recording; }
    int durationMs() const;
    QString lastPath() const { return m_lastPath; }

signals:
    void recordingChanged(bool recording);
    void levelChanged(double level);
    void errorOccurred(const QString &message);

private:
    void consumeAudio();
    static bool writeWav(const QString &path, const QByteArray &pcm, const QAudioFormat &format);

    QAudioSource *m_source = nullptr;
    QIODevice *m_io = nullptr;
    QByteArray m_pcm;
    QAudioFormat m_format;
    QElapsedTimer m_timer;
    QString m_directory;
    QString m_lastPath;
    bool m_recording = false;
};

} // namespace stacks
