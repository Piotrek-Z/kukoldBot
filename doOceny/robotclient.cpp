#include "robotclient.h"
#include "appconfig.h"
#include <QDebug>

RobotClient::RobotClient(QObject *parent) : QObject(parent) {
    connect(&m_webSocket, &QWebSocket::connected, this, &RobotClient::onConnected);
    connect(&m_webSocket, &QWebSocket::disconnected, this, &RobotClient::onDisconnected);
    connect(&m_webSocket, &QWebSocket::errorOccurred, this, &RobotClient::onError);
    m_sendTimer.start();
}

RobotClient::~RobotClient() {
    disconnectFromRobot();
}

void RobotClient::connectToRobot(const QString &ip, int port) {
    QString url = QString("ws://%1:%2").arg(ip).arg(port);
    m_statusText = "Łączenie z robotem...";
    emit statusChanged(m_statusText);
    m_webSocket.open(QUrl(url));
}

void RobotClient::disconnectFromRobot() {
    m_webSocket.close();
}

void RobotClient::onConnected() {
    m_connected = true;
    m_statusText = "POŁĄCZONY";
    emit connectionChanged(true);
    emit statusChanged(m_statusText);
    qDebug() << "[ROS] Połączono z robotem przez WebSocket!";

    // Rozgłaszamy oba tematy:
    advertiseTopic(AppConfig::instance().topicJointCommands, "std_msgs/Float64MultiArray");
    advertiseTopic(AppConfig::instance().topicCartesianCommands, "geometry_msgs/Point");
}

void RobotClient::sendCartesianTarget(double dx, double dy, double dz) {
    if (!m_connected) return;
    if (m_sendTimer.elapsed() < 40) return;
    m_sendTimer.restart();

    // Zaokrąglenie do 3 miejsc po przecinku (dokładność 1 mm):
    QJsonObject msg;
    msg["x"] = std::round(dx * 1000.0) / 1000.0; // Przód/tył
    msg["y"] = std::round(dy * 1000.0) / 1000.0; // Lewo/prawo
    msg["z"] = std::round(dz * 1000.0) / 1000.0; // Góra/dół
    publishJson(AppConfig::instance().topicCartesianCommands, msg);
}


void RobotClient::onDisconnected() {
    m_connected = false;
    m_statusText = "ROZŁĄCZONY";
    emit connectionChanged(false);
    emit statusChanged(m_statusText);
    qDebug() << "[ROS] Rozłączono z robotem.";
}

void RobotClient::onError(QAbstractSocket::SocketError error) {
    Q_UNUSED(error);
    m_statusText = "BŁĄD POŁĄCZENIA";
    emit statusChanged(m_statusText);
    qWarning() << "[ROS] Błąd WebSocket:" << m_webSocket.errorString();
}

void RobotClient::advertiseTopic(const QString &topic, const QString &type) {
    QJsonObject adv;
    adv["op"] = "advertise";
    adv["topic"] = topic;
    adv["type"] = type;
    m_webSocket.sendTextMessage(QString::fromUtf8(QJsonDocument(adv).toJson(QJsonDocument::Compact)));
}

void RobotClient::sendJointAngles(double shoulderYaw, double shoulderPitch, double elbowAngle, double headYaw, double torsoRoll) {
    if (!m_connected) return;

    // Dławik 25 Hz (dokładnie co 40 ms)
    if (m_sendTimer.elapsed() < 40) {
        return;
    }
    m_sendTimer.restart();

    // Czyste zaokrąglenie stopni do 1 miejsca po przecinku:
    double yawDeg   = std::clamp(std::round(shoulderYaw * 10.0) / 10.0, 0.0, 85.0);
    double pitchDeg = std::clamp(std::round(shoulderPitch * 10.0) / 10.0, 0.0, 160.0);
    double elbowDeg = std::clamp(std::round(elbowAngle * 10.0) / 10.0, 0.0, 130.0);

    QJsonObject msg;
    QJsonObject layout;
    layout["dim"] = QJsonArray();
    layout["data_offset"] = 0;
    msg["layout"] = layout;

    QJsonArray dataArray;
    dataArray.append(yawDeg);   // data[0] -> Sięganie (0° - 85°)
    dataArray.append(pitchDeg); // data[1] -> Unoszenie (0° - 160°)
    dataArray.append(elbowDeg); // data[2] -> Łokieć (0° - 130°)
    dataArray.append(0.0);
    dataArray.append(0.0);
    msg["data"] = dataArray;

    publishJson("/teleop/joint_commands", msg);
}
void RobotClient::emergencyStop() {
    qWarning() << "[ROS] !!! WYWOŁANO STOP AWARYJNY !!!";
    if (m_connected) {
        QJsonObject stopMsg;
        QJsonArray zeros;
        zeros.append(0.0);
        zeros.append(0.0);
        zeros.append(0.0);
        zeros.append(0.0);
        zeros.append(0.0);
        stopMsg["data"] = zeros;
        publishJson(AppConfig::instance().topicJointCommands, stopMsg);
    }
}

void RobotClient::publishJson(const QString &topic, const QJsonObject &msg) {
    QJsonObject pub;
    pub["op"] = "publish";
    pub["topic"] = topic;
    pub["msg"] = msg;

    m_webSocket.sendTextMessage(QString::fromUtf8(QJsonDocument(pub).toJson(QJsonDocument::Compact)));
}