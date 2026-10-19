#pragma once

#include <QObject>
#include <QProcess>
#include <QTimer>

// ============================================================================
// SshCore - zdalne sterowanie corem Heliosa na robocie przez SSH.
//
// Woła systemowe `ssh` (jest na Linuksie i Windows 10+) jako podproces -
// bez nowych bibliotek. Po stronie robota cała logika siedzi w skrypcie
// helios_core.sh (start|stop|status|log), tu tylko transport i parsowanie.
//
// Konfiguracja (zmienne środowiskowe po stronie laptopa):
//   HELIOS_SSH_HOST=192.168.49.97  HELIOS_SSH_USER=std
//   HELIOS_SSH_KEY=(puste = domyślne klucze/agenta)
//   HELIOS_SSH_SCRIPT=/home/std/bin/helios_core.sh
//   HELIOS_SSH_BIN=(puste = szukaj ssh: PATH, System32/OpenSSH, msys64, Git)
//
// Wymaga jednorazowo klucza SSH bez hasła (ssh-copy-id).
// coreState: unknown|running|stopped|starting|stopping
// ============================================================================

class SshCore : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString coreState READ coreState NOTIFY coreStateChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QString sshTarget READ sshTarget CONSTANT)

public:
    explicit SshCore(QObject *parent = nullptr);

    QString coreState() const { return m_state; }
    QString lastError() const { return m_lastError; }
    QString sshTarget() const { return m_user + QStringLiteral("@") + m_host; }

    Q_INVOKABLE void startCore();
    Q_INVOKABLE void stopCore();
    Q_INVOKABLE void refreshStatus();
    Q_INVOKABLE void setLogStreaming(bool on);

signals:
    void coreStateChanged(const QString &state);
    void lastErrorChanged(const QString &error);
    void logReceived(const QString &line);
    void notice(const QString &text); // info o klikach START/STOP - ZAWSZE na czat

private slots:
    void onCmdFinished(int exitCode, QProcess::ExitStatus status);
    void onCmdError(QProcess::ProcessError error);
    void onLogReady();
    void onLogFinished(int exitCode, QProcess::ExitStatus status);

private:
    enum class Cmd { None, Start, Stop, Status };
    void runCmd(Cmd cmd, const QString &remote, bool quiet);
    void setState(const QString &state);
    void setError(const QString &error);
    QStringList baseArgs() const;
    QString resolveSsh() const;

    QProcess m_cmd;
    QProcess m_log;
    QTimer m_poll;
    QByteArray m_logBuffer;
    QString m_host;
    QString m_user;
    QString m_key;
    QString m_script;
    QString m_sshBin; // jawnie wskazany program ssh (HELIOS_SSH_BIN, może być puste)
    QString m_ssh;    // wynik resolveSsh() - pełna ścieżka albo "ssh" z PATH
    QString m_state = QStringLiteral("unknown");
    QString m_lastError;
    Cmd m_pending = Cmd::None;
    bool m_streaming = false;
    bool m_forceError = false; // klik użytkownika: pokaż błąd nawet jak identyczny
};
