#include "VoiceRecorder.h"

#include <QAudioDevice>
#include <QAudioSource>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QMediaDevices>
#include <QUuid>
#include <QtEndian>

#include <cmath>

namespace stacks {

VoiceRecorder::VoiceRecorder(QObject *parent)
    : QObject(parent)
{
    m_format.setSampleRate(16000);
    m_format.setChannelCount(1);
    m_format.setSampleFormat(QAudioFormat::Int16);
}

VoiceRecorder::~VoiceRecorder()
{
    cancel();
}

bool VoiceRecorder::start(const QString &directory, QString *error)
{
    if (m_recording)
        return true;

    m_directory = directory;
    QDir().mkpath(m_directory);

    const QAudioDevice device = QMediaDevices::defaultAudioInput();
    if (device.isNull()) {
        if (error)
            *error = QStringLiteral("No microphone is available on this machine.");
        emit errorOccurred(QStringLiteral("No microphone is available on this machine."));
        return false;
    }

    QAudioFormat format = m_format;
    if (!device.isFormatSupported(format)) {
        QAudioFormat preferred = device.preferredFormat();
        preferred.setSampleFormat(QAudioFormat::Int16);
        preferred.setChannelCount(1);
        if (preferred.sampleRate() <= 0)
            preferred.setSampleRate(16000);
        format = preferred;
    }
    m_format = format;

    m_source = new QAudioSource(device, m_format, this);
    m_source->setBufferSize(16384);
    m_io = m_source->start();
    if (!m_io) {
        if (error)
            *error = QStringLiteral("Could not start audio capture.");
        emit errorOccurred(QStringLiteral("Could not start audio capture."));
        delete m_source;
        m_source = nullptr;
        return false;
    }

    m_pcm.clear();
    m_timer.start();
    m_recording = true;
    connect(m_io, &QIODevice::readyRead, this, &VoiceRecorder::consumeAudio, Qt::UniqueConnection);

    emit recordingChanged(true);
    return true;
}

void VoiceRecorder::consumeAudio()
{
    if (!m_io)
        return;
    const QByteArray chunk = m_io->readAll();
    if (chunk.isEmpty())
        return;
    m_pcm.append(chunk);

    // Compute a quiet, honest peak level from the newest samples.
    const int sampleCount = chunk.size() / 2;
    if (sampleCount > 0) {
        const qint16 *samples = reinterpret_cast<const qint16 *>(chunk.constData());
        double peak = 0.0;
        const int stride = std::max(1, sampleCount / 256);
        for (int i = 0; i < sampleCount; i += stride)
            peak = std::max(peak, std::abs(static_cast<double>(samples[i])) / 32768.0);
        emit levelChanged(peak);
    }
}

QString VoiceRecorder::stop()
{
    if (!m_recording)
        return m_lastPath;

    m_recording = false;
    emit recordingChanged(false);

    // stop() invalidates the QIODevice returned by QAudioSource::start().
    if (m_io) {
        m_pcm.append(m_io->readAll());
        m_io = nullptr;
    }
    if (m_source)
        m_source->stop();
    if (m_source) {
        m_source->deleteLater();
        m_source = nullptr;
    }

    if (m_pcm.isEmpty()) {
        emit errorOccurred(QStringLiteral("The recording was empty."));
        return {};
    }

    const QString path = QDir(m_directory).filePath(
        QStringLiteral("voice-%1.wav").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));

    if (!writeWav(path, m_pcm, m_format)) {
        emit errorOccurred(QStringLiteral("Could not write the recording."));
        return {};
    }

    m_lastPath = path;
    return path;
}

void VoiceRecorder::cancel()
{
    if (!m_recording)
        return;
    m_recording = false;
    if (m_source)
        m_source->stop();
    m_io = nullptr;
    if (m_source) {
        m_source->deleteLater();
        m_source = nullptr;
    }
    m_pcm.clear();
    emit recordingChanged(false);
}

int VoiceRecorder::durationMs() const
{
    if (m_format.sampleRate() <= 0 || m_format.channelCount() <= 0)
        return 0;
    const int bytesPerFrame = m_format.bytesPerSample() * m_format.channelCount();
    if (bytesPerFrame <= 0)
        return 0;
    return static_cast<int>((static_cast<qint64>(m_pcm.size()) / bytesPerFrame) * 1000 / m_format.sampleRate());
}

bool VoiceRecorder::writeWav(const QString &path, const QByteArray &pcm, const QAudioFormat &format)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;

    const quint16 channels = static_cast<quint16>(format.channelCount());
    const quint32 sampleRate = static_cast<quint32>(format.sampleRate());
    const quint16 bitsPerSample = static_cast<quint16>(format.bytesPerSample() * 8);
    const quint32 byteRate = sampleRate * channels * bitsPerSample / 8;
    const quint16 blockAlign = static_cast<quint16>(channels * bitsPerSample / 8);
    const quint32 dataSize = static_cast<quint32>(pcm.size());

    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);
    out.writeRawData("RIFF", 4);
    out << quint32(36 + dataSize);
    out.writeRawData("WAVE", 4);
    out.writeRawData("fmt ", 4);
    out << quint32(16);
    out << quint16(1); // PCM
    out << channels;
    out << sampleRate;
    out << byteRate;
    out << blockAlign;
    out << bitsPerSample;
    out.writeRawData("data", 4);
    out << dataSize;
    out.writeRawData(pcm.constData(), pcm.size());
    file.close();
    return true;
}

} // namespace stacks
