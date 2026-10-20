#pragma once

#include <QObject>
#include <QWebSocket>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QElapsedTimer>
#include <QStringList>
#include <QVector>

class RobotClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isConnected READ isConnected NOTIFY connectionChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
    Q_PROPERTY(bool hasJointState READ hasJointState NOTIFY jointStateChanged)
    Q_PROPERTY(int jointCount READ jointCount NOTIFY jointStateChanged)

public:
    explicit RobotClient(QObject *parent = nullptr);
    ~RobotClient();

    bool isConnected() const { return m_connected; }
    QString statusText() const { return m_statusText; }

    // Ostatni znany stan stawów ROBOTA z /joint_states (ramiona + tułów)
    bool hasJointState() const { return !m_jointNames.isEmpty() && !m_jointPositions.isEmpty(); }
    int jointCount() const { return m_jointNames.size(); }
    QStringList jointNames() const { return m_jointNames; }
    QVector<double> jointPositions() const { return m_jointPositions; }

    Q_INVOKABLE void connectToRobot(const QString &ip = "192.168.49.97", int port = 9090);
    Q_INVOKABLE void disconnectFromRobot();
    Q_INVOKABLE void emergencyStop();

public slots:
    // Wysyła wygładzone kąty człowieka do robota
    void sendJointAngles(double shoulderYaw, double shoulderPitch, double elbowAngle, double headYaw, double torsoRoll);
    void sendCartesianTarget(double dx, double dy, double dz);

signals:
    void connectionChanged(bool connected);
    void statusChanged(const QString &status);
    void jointStateChanged();

private slots:
    void onConnected();
    void onDisconnected();
    void onError(QAbstractSocket::SocketError error);
    void onTextMessage(const QString &message);

private:
    QWebSocket m_webSocket;

    QElapsedTimer m_sendTimer;
    bool m_connected = false;
    QString m_statusText = "ROZŁĄCZONY";

    QStringList m_jointNames;
    QVector<double> m_jointPositions;
    qint64 m_jointStateMs = 0;

    void advertiseTopic(const QString &topic, const QString &type);
    void subscribeTopic(const QString &topic, const QString &type);
    void publishJson(const QString &topic, const QJsonObject &msg);
};
