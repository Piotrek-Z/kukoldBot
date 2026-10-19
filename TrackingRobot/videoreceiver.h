#pragma once
#include <QObject>
#include <QThread>
#include <QImage>
#include <QElapsedTimer>
#include <opencv2/opencv.hpp>
#include "posedetector.h"
#include "one_euro_filter.h"
#include "appconfig.h"

class VideoWorker : public QObject {
    Q_OBJECT
public:
    explicit VideoWorker(const QString &sourceUrl, QObject *parent = nullptr);
    void stop();

public slots:
    void process();

signals:
    void frameReady(const QImage &image);
    void statusChanged(const QString &status);
    void allAnglesUpdated(double rightYaw, double rightShoulder, double rightElbow, double headYaw, double torsoRoll);
    void cartesianUpdated(double dx, double dy, double dz); // <-- DODAJ TĘ LINIJKĘ
    void finished();

private:
    QString m_sourceUrl;
    bool m_running{false};
    PoseDetector m_poseDetector;

    QElapsedTimer m_filterTimer;
    OneEuroFilter m_rightYawFilter;
    OneEuroFilter m_rightShoulderFilter;
    OneEuroFilter m_rightElbowFilter;
};

class VideoReceiver : public QObject {
    Q_OBJECT
    Q_PROPERTY(double rightShoulderYaw READ rightShoulderYaw NOTIFY anglesChanged)
    Q_PROPERTY(double rightShoulderPitch READ rightShoulderPitch NOTIFY anglesChanged)
    Q_PROPERTY(double rightElbowAngle READ rightElbowAngle NOTIFY anglesChanged)

public:
    explicit VideoReceiver(QObject *parent = nullptr);
    ~VideoReceiver();

    Q_INVOKABLE void startStream(const QString &sourceUrl);
    Q_INVOKABLE void stopStream();

    double rightShoulderYaw() const { return m_rightShoulderYaw; }
    double rightShoulderPitch() const { return m_rightShoulderPitch; }
    double rightElbowAngle() const { return m_rightElbowAngle; }
    double headYaw() const { return m_headYaw; }
    double torsoRoll() const { return m_torsoRoll; }

signals:
    void frameReady(const QImage &image);
    void statusChanged(const QString &status);
    void anglesChanged();
    void allAnglesUpdated(double rightYaw, double rightShoulder, double rightElbow, double headYaw, double torsoRoll);
    void cartesianUpdated(double dx, double dy, double dz);

private slots:
    void onAllAnglesReceived(double rightYaw, double rightShoulder, double rightElbow, double headYaw, double torsoRoll);

private:
    QThread *m_thread{nullptr};
    VideoWorker *m_worker{nullptr};
    double m_rightShoulderYaw{0.0};
    double m_rightShoulderPitch{0.0};
    double m_rightElbowAngle{0.0};
    double m_headYaw{0.0};
    double m_torsoRoll{0.0};
};