#include "helioslink.h"

#include <QJsonDocument>
#include <QDebug>

HeliosLink::HeliosLink(QObject *parent) : QObject(parent) {
    m_host = QString::fromUtf8(qgetenv("HELIOS_GUI_HOST"));
    if (m_host.isEmpty()) {
        m_host = QStringLiteral("192.168.49.97");
    }
    bool ok = false;
    const int port = qgetenv("HELIOS_GUI_PORT").toInt(&ok);
    if (ok && port > 0 && port < 65536) {
        m_port = static_cast<quint16>(port);
    }

    connect(&m_socket, &QTcpSocket::connected, this, &HeliosLink::onConnected);
    connect(&m_socket, &QTcpSocket::disconnected, this, &HeliosLink::onDisconnected);
    connect(&m_socket, &QTcpSocket::readyRead, this, &HeliosLink::onReadyRead);
    connect(&m_socket, &QTcpSocket::errorOccurred, this, &HeliosLink::onSocketError);

    m_reconnect.setInterval(2500);
    connect(&m_reconnect, &QTimer::timeout, this, &HeliosLink::tryConnect);

    qDebug() << "[HeliosLink] core:" << m_host << m_port;
    tryConnect();
}

qint64 HeliosLink::sendChat(const QString &text) {
    if (!m_connected) {
        qWarning() << "[HeliosLink] brak polaczenia - wiadomosc odrzucona";
        return 0;
    }
    const qint64 id = m_nextId++;
    QJsonObject obj;
    obj[QStringLiteral("type")] = QStringLiteral("chat");
    obj[QStringLiteral("text")] = text;
    obj[QStringLiteral("id")] = id;
    sendJson(obj);
    return id;
}

void HeliosLink::setListening(bool on) {
    if (!m_connected) {
        qWarning() << "[HeliosLink] brak polaczenia - setListening odrzucone";
        return;
    }
    QJsonObject obj;
    obj[QStringLiteral("type")] = QStringLiteral("set_listening");
    obj[QStringLiteral("on")] = on;
    sendJson(obj);
}

void HeliosLink::sendJson(const QJsonObject &obj) {
    const QByteArray line = QJsonDocument(obj).toJson(QJsonDocument::Compact) + '\n';
    m_socket.write(line);
}

void HeliosLink::setState(const QString &state) {
    if (m_state == state) {
        return;
    }
    m_state = state;
    emit earStateChanged(state);
}

void HeliosLink::onConnected() {
    m_connected = true;
    m_buffer.clear();
    m_reconnect.stop();
    qDebug() << "[HeliosLink] polaczono z corem";
    emit connectionChanged(true);
}

void HeliosLink::onDisconnected() {
    m_connected = false;
    setState(QStringLiteral("offline"));
    qDebug() << "[HeliosLink] rozlaczono - ponawiam co 2,5 s";
    emit connectionChanged(false);
    if (!m_reconnect.isActive()) {
        m_reconnect.start();
    }
}

void HeliosLink::onSocketError(QAbstractSocket::SocketError error) {
    Q_UNUSED(error)
    // errorOccurred leci też przy nieudanym connect - timer i tak ponawia.
    if (!m_connected && !m_reconnect.isActive()) {
        m_reconnect.start();
    }
}

void HeliosLink::tryConnect() {
    if (m_connected) {
        return;
    }
    if (m_socket.state() == QAbstractSocket::ConnectingState) {
        return;
    }
    m_socket.connectToHost(m_host, m_port);
    if (!m_reconnect.isActive()) {
        m_reconnect.start();
    }
}

void HeliosLink::onReadyRead() {
    m_buffer.append(m_socket.readAll());
    if (m_buffer.size() > 65536) {
        qWarning() << "[HeliosLink] przepelniony bufor - zrywam polaczenie";
        m_socket.disconnectFromHost();
        return;
    }
    int pos = 0;
    while ((pos = m_buffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_buffer.left(pos).trimmed();
        m_buffer.remove(0, pos + 1);
        if (!line.isEmpty()) {
            handleLine(line);
        }
    }
}

void HeliosLink::handleLine(const QByteArray &line) {
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(line, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "[HeliosLink] zle JSON:" << line.left(120);
        return;
    }
    const QJsonObject obj = doc.object();
    const QString type = obj.value(QStringLiteral("type")).toString();
    if (type == QStringLiteral("reply")) {
        emit replyReceived(obj.value(QStringLiteral("text")).toString(),
                           obj.value(QStringLiteral("ref")).toVariant().toLongLong());
    } else if (type == QStringLiteral("heard")) {
        emit heardReceived(obj.value(QStringLiteral("text")).toString(),
                           obj.value(QStringLiteral("ref")).toVariant().toLongLong(),
                           obj.value(QStringLiteral("source")).toString());
    } else if (type == QStringLiteral("state")) {
        setState(obj.value(QStringLiteral("state")).toString());
    } else if (type == QStringLiteral("log")) {
        emit logReceived(obj.value(QStringLiteral("text")).toString());
    } else if (type == QStringLiteral("error")) {
        emit logReceived(QStringLiteral("[core] blad: ") +
                         obj.value(QStringLiteral("code")).toString());
    }
    // hello/pong ignorujemy
}
