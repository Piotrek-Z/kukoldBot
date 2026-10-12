#pragma once

#include <QObject>
#include <QImage>
#include <QThread>
#include <atomic>
#include <opencv2/opencv.hpp>
#include "posedetector.h"
#include "motionfilter.h"

class VideoWorker : public QObject {
    Q_OBJECT
public:
    explicit VideoWorker(const QString &sourceUrl, QObject *parent = nullptr);
    void stop();

signals:
    void frameReady(const QImage &image);
    void statusChanged(const QString &status);
    void allAnglesUpdated(double rightElbow, double leftElbow, double headYaw, double torsoRoll);
    void finished();

public slots:
    void process();

private:
    QString m_sourceUrl;
    std::atomic<bool> m_running{false};
    PoseDetector m_poseDetector;

    // Filtry wygładzające drgania
    MotionFilter m_rightElbowFilter{0.25f, 0.8f};
    MotionFilter m_leftElbowFilter{0.25f, 0.8f};
    MotionFilter m_headYawFilter{0.20f, 0.5f};
    MotionFilter m_torsoRollFilter{0.15f, 0.5f};
};

class VideoReceiver : public QObject {
    Q_OBJECT
    Q_PROPERTY(double rightElbowAngle READ rightElbowAngle NOTIFY anglesChanged)
    Q_PROPERTY(double leftElbowAngle READ leftElbowAngle NOTIFY anglesChanged)
    Q_PROPERTY(double headYaw READ headYaw NOTIFY anglesChanged)
    Q_PROPERTY(double torsoRoll READ torsoRoll NOTIFY anglesChanged)
    // Dodajemy nową właściwość do śledzenia stanu strumienia
    Q_PROPERTY(bool isStreaming READ isStreaming NOTIFY isStreamingChanged)

public:
    explicit VideoReceiver(QObject *parent = nullptr);
    ~VideoReceiver();

    double rightElbowAngle() const { return m_rightElbowAngle; }
    double leftElbowAngle() const { return m_leftElbowAngle; }
    double headYaw() const { return m_headYaw; }
    double torsoRoll() const { return m_torsoRoll; }
    bool isStreaming() const { return m_isStreaming; } // Getter

    // Dodajemy Q_INVOKABLE, aby móc wywołać te metody z QML
    Q_INVOKABLE void startStream(const QString &sourceUrl);
    Q_INVOKABLE void stopStream();

signals:
    void frameReady(const QImage &image);
    void statusChanged(const QString &status);
    void anglesChanged();
    void isStreamingChanged(bool isStreaming); // Sygnał dla QML

private slots:
    void onAllAnglesReceived(double rightElbow, double leftElbow, double headYaw, double torsoRoll);

private:
    QThread *m_thread = nullptr;
    VideoWorker *m_worker = nullptr;

    double m_rightElbowAngle = 0.0;
    double m_leftElbowAngle = 0.0;
    double m_headYaw = 0.0;
    double m_torsoRoll = 0.0;
    bool m_isStreaming = false; // Zmienna stanu
};
