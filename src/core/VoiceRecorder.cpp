#include "VoiceRecorder.h"

#include <QAudioDevice>
#include <QAudioSource>
#include <QDataStream>
#include <QDir>
#include <QMediaDevices>
#include <QUuid>

#include <algorithm>
#include <cmath>
#include <limits>

namespace stacks {

namespace {
constexpr qint64 kWavHeaderBytes = 44;
} // namespace

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

QByteArray VoiceRecorder::wavHeader(const QAudioFormat &format, quint32 dataBytes)
{
    const quint16 channels = static_cast<quint16>(format.channelCount());
    const quint32 sampleRate = static_cast<quint32>(format.sampleRate());
    const quint16 bitsPerSample = static_cast<quint16>(format.bytesPerSample() * 8);
    const quint32 byteRate = sampleRate * channels * bitsPerSample / 8;
    const quint16 blockAlign = static_cast<quint16>(channels * bitsPerSample / 8);

    QByteArray header;
    QDataStream out(&header, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);
    out.writeRawData("RIFF", 4);
    out << quint32(36 + dataBytes);
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
    out << dataBytes;
    return header;
}

bool VoiceRecorder::start(const QString &directory, QString *error)
{
    if (m_recording)
        return true;

    const auto fail = [&](const QString &message) {
        if (error)
            *error = message;
        emit errorOccurred(message);
        return false;
    };

    m_directory = directory;
    QDir().mkpath(m_directory);

    const QAudioDevice device = QMediaDevices::defaultAudioInput();
    if (device.isNull())
        return fail(QStringLiteral("No microphone is available on this machine."));

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

    const qint64 bytesPerFrame = qint64(m_format.bytesPerSample()) * m_format.channelCount();
    if (bytesPerFrame <= 0 || m_format.sampleRate() <= 0)
        return fail(QStringLiteral("The microphone reported an unusable audio format."));
    const qint64 byDuration = kMaxDurationMs * m_format.sampleRate() / 1000 * bytesPerFrame;
    const qint64 byWavLimit = qint64(std::numeric_limits<quint32>::max()) - 36;
    m_maxDataBytes = std::min(byDuration, byWavLimit) / bytesPerFrame * bytesPerFrame;

    m_file.setFileName(QDir(m_directory).filePath(
        QStringLiteral("voice-%1.wav.part").arg(QUuid::createUuid().toString(QUuid::WithoutBraces))));
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || m_file.write(wavHeader(m_format, 0)) != kWavHeaderBytes) {
        discardFile();
        return fail(QStringLiteral("Could not create the recording file."));
    }
    m_dataBytes = 0;

    m_source = new QAudioSource(device, m_format, this);
    m_source->setBufferSize(16384);
    m_io = m_source->start();
    if (!m_io) {
        releaseSource();
        discardFile();
        return fail(QStringLiteral("Could not start audio capture."));
    }

    connect(m_io, &QIODevice::readyRead, this, &VoiceRecorder::consumeAudio);
    connect(m_source, &QAudioSource::stateChanged, this, [this](QAudio::State state) {
        if (m_recording && m_source && state == QAudio::StoppedState
            && m_source->error() != QAudio::NoError)
            endCapture(QStringLiteral("The microphone stopped. The recording so far was kept."));
    });

    m_recording = true;
    m_endPending = false;
    emit recordingChanged(true);
    return true;
}

bool VoiceRecorder::writePcm(const QByteArray &pcm)
{
    if (pcm.isEmpty() || !m_file.isOpen())
        return true;
    const qint64 room = m_maxDataBytes - m_dataBytes;
    const QByteArray accepted = pcm.size() > room ? pcm.left(room) : pcm;
    if (!accepted.isEmpty()) {
        if (m_file.write(accepted) != accepted.size()) {
            endCapture(QStringLiteral("The disk is full or unavailable. The recording so far was kept."));
            return false;
        }
        m_dataBytes += accepted.size();
    }
    if (m_dataBytes >= m_maxDataBytes) {
        endCapture(QStringLiteral("Voice notes are limited to %1 hours. This one was saved.")
                       .arg(kMaxDurationMs / 3600000));
        return false;
    }
    return true;
}

void VoiceRecorder::consumeAudio()
{
    if (!m_io || !m_recording)
        return;
    const QByteArray chunk = m_io->readAll();
    if (chunk.isEmpty())
        return;

    // A quiet, honest peak level from the newest samples.
    const qsizetype sampleCount = chunk.size() / 2;
    if (sampleCount > 0) {
        const qint16 *samples = reinterpret_cast<const qint16 *>(chunk.constData());
        double peak = 0.0;
        const qsizetype stride = std::max<qsizetype>(1, sampleCount / 256);
        for (qsizetype i = 0; i < sampleCount; i += stride)
            peak = std::max(peak, std::abs(static_cast<double>(samples[i])) / 32768.0);
        emit levelChanged(peak);
    }
    writePcm(chunk);
}

void VoiceRecorder::endCapture(const QString &reason)
{
    // Announced after the current audio callback returns, so the owner can
    // stop and tear down the source without re-entering it.
    if (m_endPending)
        return;
    m_endPending = true;
    QMetaObject::invokeMethod(this, [this, reason]() {
        if (m_recording)
            emit captureEnded(reason);
        m_endPending = false;
    }, Qt::QueuedConnection);
}

QString VoiceRecorder::stop()
{
    if (!m_recording)
        return m_lastPath;

    m_recording = false;
    emit recordingChanged(false);

    // stop() invalidates the QIODevice returned by QAudioSource::start(), so
    // drain it first.
    if (m_io)
        writePcm(m_io->readAll());
    releaseSource();

    if (m_dataBytes <= 0) {
        discardFile();
        emit errorOccurred(QStringLiteral("The recording was empty."));
        return {};
    }

    const QString partPath = m_file.fileName();
    const QString path = partPath.chopped(int(sizeof(".part") - 1));
    const bool written = m_file.seek(0)
        && m_file.write(wavHeader(m_format, quint32(m_dataBytes))) == kWavHeaderBytes
        && m_file.flush();
    m_file.close();
    if (!written || !QFile::rename(partPath, path)) {
        QFile::remove(partPath);
        m_dataBytes = 0;
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
    releaseSource();
    discardFile();
    emit recordingChanged(false);
}

void VoiceRecorder::releaseSource()
{
    if (m_io)
        disconnect(m_io, nullptr, this, nullptr);
    m_io = nullptr;
    if (m_source) {
        disconnect(m_source, nullptr, this, nullptr);
        m_source->stop();
        m_source->deleteLater();
        m_source = nullptr;
    }
}

void VoiceRecorder::discardFile()
{
    if (m_file.isOpen())
        m_file.close();
    if (!m_file.fileName().isEmpty())
        QFile::remove(m_file.fileName());
    m_dataBytes = 0;
}

int VoiceRecorder::durationMs() const
{
    const qint64 bytesPerFrame = qint64(m_format.bytesPerSample()) * m_format.channelCount();
    if (m_format.sampleRate() <= 0 || bytesPerFrame <= 0)
        return 0;
    return static_cast<int>((m_dataBytes / bytesPerFrame) * 1000 / m_format.sampleRate());
}

} // namespace stacks
