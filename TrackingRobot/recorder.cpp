#include "recorder.h"
#include "robotclient.h"
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QDesktopServices>
#include <QUrl>
#include <QDebug>

JointRecorder::JointRecorder(RobotClient *robot, QObject *parent)
    : QObject(parent), m_robot(robot) {
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &JointRecorder::onTimeout);
}

JointRecorder::~JointRecorder() {
    m_timer.stop();
    if (m_file.isOpen()) {
        m_stream.flush();
        m_file.close();
    }
}

void JointRecorder::startRecording(double intervalSec) {
    if (m_recording) {
        emit message(QString("Nagrywanie: już nagrywam (%1 próbek).").arg(m_sampleCount));
        return;
    }
    if (!m_robot || !m_robot->isConnected()) {
        emit message("Nagrywanie: brak połączenia z robotem - najpierw POŁĄCZ.");
        return;
    }
    if (intervalSec < 0.2) intervalSec = 0.2;
    if (intervalSec > 60.0) intervalSec = 60.0;

    QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                  + "/HeliosNagrania";
    if (!QDir().mkpath(dir)) { // awaryjnie katalog tymczasowy
        dir = QDir::tempPath() + "/HeliosNagrania";
        QDir().mkpath(dir);
    }
    QString name = "nagranie_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".csv";
    m_file.setFileName(dir + "/" + name);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit message("Nagrywanie: nie mogę utworzyć pliku w " + QDir::toNativeSeparators(dir));
        return;
    }
    m_stream.setDevice(&m_file);
    m_headerNames.clear();
    m_sampleCount = 0;
    emit sampleCountChanged(0);
    m_recording = true;
    emit recordingChanged(true);
    m_timer.start(int(intervalSec * 1000.0));

    QString info = QString("Nagrywanie stawów robota: co %1 s -> %2")
                       .arg(intervalSec, 0, 'g', 3).arg(name);
    if (!m_robot->hasJointState())
        info += " (czekam na dane z /joint_states...)";
    emit message(info);
    qInfo() << "[REC]" << info << m_file.fileName();
}

void JointRecorder::stopRecording() {
    if (!m_recording) {
        emit message("Nagrywanie: nic nie jest teraz nagrywane.");
        return;
    }
    m_timer.stop();
    m_stream.flush();
    m_file.close();
    m_filePath = m_file.fileName();
    m_recording = false;
    emit recordingChanged(false);
    emit message(QString("Nagrywanie zakończone: %1 próbek -> %2")
                     .arg(m_sampleCount).arg(QDir::toNativeSeparators(m_filePath)));
    qInfo() << "[REC] Zapisano" << m_sampleCount << "próbek do" << m_filePath;
}

void JointRecorder::openRecording() {
    QString path = m_recording ? m_file.fileName() : m_filePath;
    if (path.isEmpty()) {
        emit message("Nagrywanie: brak pliku - najpierw coś nagraj.");
        return;
    }
    if (m_recording) m_stream.flush(); // żeby było widać próbki sprzed otwarcia
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path)))
        emit message("Nie mogę otworzyć pliku: " + QDir::toNativeSeparators(path));
}

void JointRecorder::onTimeout() {
    if (!m_recording || !m_robot) return;
    if (!m_robot->hasJointState()) return; // robot (jeszcze) nie publikuje /joint_states

    QStringList names = m_robot->jointNames();
    QVector<double> pos = m_robot->jointPositions();

    if (m_headerNames.isEmpty()) { // nagłówek z nazw stawów z pierwszej próbki
        m_headerNames = names;
        m_stream << "timestamp_iso;timestamp_ms";
        for (const QString &n : m_headerNames) m_stream << ";" << n;
        m_stream << "\n";
    }
    m_stream << QDateTime::currentDateTime().toString(Qt::ISODateWithMs)
             << ";" << QDateTime::currentMSecsSinceEpoch();
    int n = qMin(m_headerNames.size(), pos.size());
    for (int i = 0; i < n; ++i) m_stream << ";" << QString::number(pos[i], 'f', 6);
    for (int i = n; i < m_headerNames.size(); ++i) m_stream << ";"; // gdyby robot coś uciął
    m_stream << "\n";
    m_stream.flush();

    ++m_sampleCount;
    emit sampleCountChanged(m_sampleCount);
}
