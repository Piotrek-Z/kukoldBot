#pragma once

#include <QObject>
#include <QJsonObject>
#include <QTcpSocket>
#include <QTimer>

// ============================================================================
// HeliosLink - klient łącza do core'a Heliosa (TCP, JSON-linie).
//
// Core działa na robocie (domyślnie 192.168.49.97:5123, zmiana przez
// HELIOS_GUI_HOST / HELIOS_GUI_PORT). Klasa sama łączy i ponawia co 2,5 s.
//
// Protokół (druga strona: core/gui_link.*):
//   -> {"type":"chat","text":"...","id":N}   tekst z czatu (id do echa)
//   -> {"type":"set_listening","on":bool}    guzik NASŁUCH
//   <- {"type":"reply"|"heard"|"state"|"log"|"error", ...}
// ============================================================================

class HeliosLink : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool connected READ isConnected NOTIFY connectionChanged)
    Q_PROPERTY(QString earState READ earState NOTIFY earStateChanged)

public:
    explicit HeliosLink(QObject *parent = nullptr);

    bool isConnected() const { return m_connected; }
    QString earState() const { return m_state; }

    // Wysyła tekst do core'a. Zwraca id (do potwierdzenia echa)
    // albo 0, gdy brak połączenia (QML pokazuje wtedy ostrzeżenie).
    Q_INVOKABLE qint64 sendChat(const QString &text);
    Q_INVOKABLE void setListening(bool on);

signals:
    void connectionChanged(bool connected);
    void earStateChanged(const QString &state);
    void replyReceived(const QString &text, qint64 ref);
    void heardReceived(const QString &text, qint64 ref, const QString &source);
    void logReceived(const QString &text);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError error);
    void tryConnect();

private:
    void sendJson(const QJsonObject &obj);
    void setState(const QString &state);
    void handleLine(const QByteArray &line);

    QTcpSocket m_socket;
    QTimer m_reconnect;
    QByteArray m_buffer;
    QString m_host;
    quint16 m_port = 5123;
    QString m_state = QStringLiteral("offline");
    qint64 m_nextId = 1;
    bool m_connected = false;
};
