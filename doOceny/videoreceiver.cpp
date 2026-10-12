#include "videoreceiver.h"
#include <QDebug>

VideoWorker::VideoWorker(const QString &sourceUrl, QObject *parent)
    : QObject(parent), m_sourceUrl(sourceUrl) {
    auto &cfg = AppConfig::instance();

    // Oś 1 (Sięganie w przód): mocniejsze tłumienie w spoczynku, aby nie drżała
    m_rightYawFilter.setParameters(0.7, 0.004, 0.8);

    // Oś 2 (Unoszenie barku): naturalna dynamika
    m_rightShoulderFilter.setParameters(1.0, 0.007, 0.8);

    // Oś 4 (Zgięcie łokcia): bardzo szybka reakcja na zginanie przedramienia
    m_rightElbowFilter.setParameters(1.2, 0.008, 0.8);
}

void VideoWorker::stop() {
    m_running = false;
}

void VideoWorker::process() {
    m_running = true;
    m_filterTimer.start();

    auto &cfg = AppConfig::instance();
    emit statusChanged("Ładowanie modelu AI...");

    if (!m_poseDetector.loadModel(cfg.modelPath.toStdString())) {
        qWarning() << "BŁĄD: Nie udało się wczytać modelu z:" << cfg.modelPath;
    }

    emit statusChanged("Łączenie ze źródłem: " + m_sourceUrl);

    cv::VideoCapture cap;
    bool isNumeric;
    int camIndex = m_sourceUrl.toInt(&isNumeric);
    if (isNumeric) {
        cap.open(camIndex);
    } else {
        cap.open(m_sourceUrl.toStdString());
    }

    if (!cap.isOpened()) {
        emit statusChanged("BŁĄD: Nie można otworzyć strumienia: " + m_sourceUrl);
        emit finished();
        return;
    }

    emit statusChanged("Połączono. Śledzenie aktywne.");

    cv::Mat frame;
    double lastValidYaw = 0.0;
    double lastValidPitch = 0.0;
    double lastValidElbow = 0.0;
    qint64 lastDetectionTime = 0;

    while (m_running) {
        if (!cap.read(frame) || frame.empty()) {
            QThread::msleep(10);
            continue;
        }

        auto poses = m_poseDetector.processFrame(frame);
        double timestampSec = m_filterTimer.elapsed() / 1000.0;
        qint64 nowMs = m_filterTimer.elapsed();

        if (!poses.empty()) {
            const auto &p = poses[0];
            lastDetectionTime = nowMs;

            lastValidYaw     = p.rightShoulderYaw;
            lastValidPitch   = p.rightShoulderPitch;
            lastValidElbow   = p.rightElbowAngle;

            double filteredYaw   = lastValidYaw;
            double filteredPitch = lastValidPitch;
            double filteredElbow = lastValidElbow;

            if (cfg.filterEnabled) {
                filteredYaw   = m_rightYawFilter.filter(lastValidYaw, timestampSec);
                filteredPitch = m_rightShoulderFilter.filter(lastValidPitch, timestampSec);
                filteredElbow = m_rightElbowFilter.filter(lastValidElbow, timestampSec);
            }

            emit allAnglesUpdated(filteredYaw, filteredPitch, filteredElbow, 0.0, 0.0);

            // TUTAJ NADAJEMY WSPÓŁRZĘDNE KARTEZJAŃSKIE 3D:
            emit cartesianUpdated(p.wristRelX, p.wristRelY, p.wristRelZ);
        }
        else {
            // Histereza utraty widzenia: jeśli zgubiono klatkę na krócej niż 300 ms,
            // podtrzymujemy ostatnią znaną pozycję, zapobiegając szarpnięciom robota!
            if (nowMs - lastDetectionTime < 300 && lastDetectionTime > 0) {
                double filteredYaw   = m_rightYawFilter.filter(lastValidYaw, timestampSec);
                double filteredPitch = m_rightShoulderFilter.filter(lastValidPitch, timestampSec);
                double filteredElbow = m_rightElbowFilter.filter(lastValidElbow, timestampSec);

                emit allAnglesUpdated(filteredYaw, filteredPitch, filteredElbow, 0.0, 0.0);
            }
        }

        // Konwersja BGR -> RGB do podglądu QML
        cv::Mat rgbFrame;
        cv::cvtColor(frame, rgbFrame, cv::COLOR_BGR2RGB);

        QImage qimg(rgbFrame.data,
                    rgbFrame.cols,
                    rgbFrame.rows,
                    static_cast<int>(rgbFrame.step),
                    QImage::Format_RGB888);

        emit frameReady(qimg.copy());
    }
    cap.release();
    emit statusChanged("Zatrzymano.");
    emit finished();
}

VideoReceiver::VideoReceiver(QObject *parent) : QObject(parent) {}

VideoReceiver::~VideoReceiver() {
    stopStream();
}

void VideoReceiver::startStream(const QString &sourceUrl) {
    stopStream();

    m_thread = new QThread(this);
    m_worker = new VideoWorker(sourceUrl);
    m_worker->moveToThread(m_thread);

    connect(m_thread, &QThread::started, m_worker, &VideoWorker::process);
    connect(m_worker, &VideoWorker::frameReady, this, &VideoReceiver::frameReady);
    connect(m_worker, &VideoWorker::statusChanged, this, &VideoReceiver::statusChanged);
    connect(m_worker, &VideoWorker::allAnglesUpdated, this, &VideoReceiver::onAllAnglesReceived);
    connect(m_worker, &VideoWorker::allAnglesUpdated, this, &VideoReceiver::allAnglesUpdated);
    connect(m_worker, &VideoWorker::cartesianUpdated, this, &VideoReceiver::cartesianUpdated);
    connect(m_worker, &VideoWorker::finished, m_thread, &QThread::quit);
    connect(m_worker, &VideoWorker::finished, m_worker, &QObject::deleteLater);
    connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);

    m_thread->start();
}

void VideoReceiver::stopStream() {
    if (m_worker) m_worker->stop();
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
        m_thread = nullptr;
        m_worker = nullptr;
    }
}

void VideoReceiver::onAllAnglesReceived(double rightYaw, double rightShoulder, double rightElbow, double headYaw, double torsoRoll) {
    m_rightShoulderYaw = rightYaw;
    m_rightShoulderPitch = rightShoulder;
    m_rightElbowAngle = rightElbow;
    emit anglesChanged();
    emit allAnglesUpdated(rightYaw, rightShoulder, rightElbow, headYaw, torsoRoll);
}