#include "robotclient.h"
#include <QDebug>

RobotClient::RobotClient(QObject *parent) : QObject(parent) {
    connect(&m_webSocket, &QWebSocket::connected, this, &RobotClient::onConnected);
    connect(&m_webSocket, &QWebSocket::disconnected, this, &RobotClient::onDisconnected);
    connect(&m_webSocket, &QWebSocket::errorOccurred, this, &RobotClient::onError);
}

RobotClient::~RobotClient() {
    disconnectFromRobot();
}

void RobotClient::connectToRobot(const QString &ip, int port) {
    QString url = QString("ws://%1:%2").arg(ip).arg(port);
    m_statusText = "Łączenie...";
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

    // W ROS 2 podajemy typ bez dodatkowego /msg/:
    advertiseTopic("/teleop/joint_commands", "std_msgs/Float64MultiArray");

    // Od razu wysyłamy zerową ramkę inicjalizacyjną, aby temat pojawił się w ROS 2
    sendJointAngles(0.0, 0.0, 0.0, 0.0);
}

void RobotClient::onDisconnected() {
    m_connected = false;
    m_statusText = "ROZŁĄCZONY";
    emit connectionChanged(false);
    emit statusChanged(m_statusText);
    qDebug() << "[ROS] Rozłączono z robotem.";
}

void RobotClient::onError(QAbstractSocket::SocketError error) {
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

void RobotClient::sendJointAngles(double rightElbow, double leftElbow, double headYaw, double torsoRoll) {
    if (!m_connected) return;

    // Kompletna struktura std_msgs/Float64MultiArray dla ROS 2
    QJsonObject msg;

    // Wymagany przez ROS 2 pusty layout
    QJsonObject layout;
    layout["dim"] = QJsonArray();
    layout["data_offset"] = 0;
    msg["layout"] = layout;

    // Właściwe dane kątów
    QJsonArray dataArray;
    dataArray.append(rightElbow);
    dataArray.append(leftElbow);
    dataArray.append(headYaw);
    dataArray.append(torsoRoll);
    msg["data"] = dataArray;

    publishJson("/teleop/joint_commands", msg);
}
void RobotClient::emergencyStop() {
    qWarning() << "[ROS] !!! WYWOŁANO STOP AWARYJNY !!!";
    if (m_connected) {
        // Publikacja komendy zatrzymania
        QJsonObject stopMsg;
        QJsonArray zeros;
        zeros.append(0.0);
        zeros.append(0.0);
        zeros.append(0.0);
        zeros.append(0.0);
        stopMsg["data"] = zeros;
        publishJson("/teleop/joint_commands", stopMsg);
    }
}

void RobotClient::publishJson(const QString &topic, const QJsonObject &msg) {
    QJsonObject pub;
    pub["op"] = "publish";
    pub["topic"] = topic;
    pub["msg"] = msg;

    m_webSocket.sendTextMessage(QString::fromUtf8(QJsonDocument(pub).toJson(QJsonDocument::Compact)));
}