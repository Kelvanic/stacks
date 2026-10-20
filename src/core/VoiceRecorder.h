#pragma once

#include <QAudioFormat>
#include <QByteArray>
#include <QFile>
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
    // Audio streams straight to disk, so memory stays flat however long a
    // note runs; this cap only stops a forgotten recording filling the disk.
    static constexpr qint64 kMaxDurationMs = 2LL * 60 * 60 * 1000;

    explicit VoiceRecorder(QObject *parent = nullptr);
    ~VoiceRecorder() override;

    bool start(const QString &directory, QString *error = nullptr);
    QString stop();
    void cancel();

    bool isRecording() const { return m_recording; }
    int durationMs() const;
    QString lastPath() const { return m_lastPath; }

    static QByteArray wavHeader(const QAudioFormat &format, quint32 dataBytes);

signals:
    void recordingChanged(bool recording);
    void levelChanged(double level);
    void errorOccurred(const QString &message);
    // Capture ended on its own (time limit, disk or device failure). The
    // audio written so far is intact; the owner decides whether to keep it.
    void captureEnded(const QString &reason);

private:
    void consumeAudio();
    bool writePcm(const QByteArray &pcm);
    void endCapture(const QString &reason);
    void releaseSource();
    void discardFile();

    QAudioSource *m_source = nullptr;
    QIODevice *m_io = nullptr;
    QFile m_file;
    qint64 m_dataBytes = 0;
    qint64 m_maxDataBytes = 0;
    QAudioFormat m_format;
    QString m_directory;
    QString m_lastPath;
    bool m_recording = false;
    bool m_endPending = false;
};

} // namespace stacks
