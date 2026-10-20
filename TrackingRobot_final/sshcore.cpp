#include "sshcore.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>

SshCore::SshCore(QObject *parent) : QObject(parent) {
    m_host = QString::fromUtf8(qgetenv("HELIOS_SSH_HOST"));
    if (m_host.isEmpty()) {
        m_host = QStringLiteral("192.168.49.97");
    }
    m_user = QString::fromUtf8(qgetenv("HELIOS_SSH_USER"));
    if (m_user.isEmpty()) {
        m_user = QStringLiteral("std");
    }
    m_key = QString::fromUtf8(qgetenv("HELIOS_SSH_KEY"));
    if (m_key.isEmpty()) {
        // Instalator kładzie klucz obok .exe (plik helios_key).
        const QString bundledKey = QCoreApplication::applicationDirPath() + QStringLiteral("/helios_key");
        if (QFile::exists(bundledKey)) {
            m_key = bundledKey;
        }
    }
    m_script = QString::fromUtf8(qgetenv("HELIOS_SSH_SCRIPT"));
    if (m_script.isEmpty()) {
        m_script = QStringLiteral("/home/std/bin/helios_core.sh");
    }
    m_sshBin = QString::fromUtf8(qgetenv("HELIOS_SSH_BIN"));
    m_ssh = resolveSsh();
    qDebug() << "[SshCore] ssh:" << (m_ssh.isEmpty() ? QStringLiteral("(NIE ZNALEZIONO)") : m_ssh);

    connect(&m_cmd, &QProcess::finished, this, &SshCore::onCmdFinished);
    connect(&m_cmd, &QProcess::errorOccurred, this, &SshCore::onCmdError);
    connect(&m_log, &QProcess::readyReadStandardOutput, this, &SshCore::onLogReady);
    connect(&m_log, &QProcess::finished, this, &SshCore::onLogFinished);
    // "autopowtarzanie": co 100 ms wysyłamy wszystkie trzymane klawisze
connect(&m_hold, &QTimer::timeout, this, [this] {
    if (m_drive.state() == QProcess::Running)
        for (char c : m_held) m_drive.write(QByteArray(1, c));
});
// sesja padła (WiFi?) -> zgaś tryb
connect(&m_drive, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        this, [this]{ m_held.clear(); m_hold.stop();
                      emit notice(QStringLiteral("remote control rozłączony")); });

    m_poll.setInterval(15000);
    connect(&m_poll, &QTimer::timeout, this, &SshCore::refreshStatus);

    qDebug() << "[SshCore] cel:" << sshTarget() << "skrypt:" << m_script;
    refreshStatus();
    m_poll.start();
}

void SshCore::startCore() {
    runCmd(Cmd::Start, m_script + QStringLiteral(" start"), false);
}

void SshCore::stopCore() {
    runCmd(Cmd::Stop, m_script + QStringLiteral(" stop"), false);
}

void SshCore::refreshStatus() {
    runCmd(Cmd::Status, m_script + QStringLiteral(" status"), true);
}

// Nie polegamy na samym PATH: na Windows apka GUI często widzi inny PATH
// niż terminal (msys64/Git). Szukamy po kolei, pierwsze trafienie wygrywa.
QString SshCore::resolveSsh() const {
    if (!m_sshBin.isEmpty()) {
        return m_sshBin;
    }
    // ssh.exe z instalatora (podkatalog ssh obok .exe) ma pierwszeństwo.
    const QString bundledSsh = QCoreApplication::applicationDirPath() + QStringLiteral("/ssh/ssh.exe");
    if (QFile::exists(bundledSsh)) {
        return bundledSsh;
    }
    const QString inPath = QStandardPaths::findExecutable(QStringLiteral("ssh"));
    if (!inPath.isEmpty()) {
        return inPath;
    }
    const QStringList candidates = {
        QStringLiteral("C:/Windows/System32/OpenSSH/ssh.exe"),
        QStringLiteral("C:/Windows/Sysnative/OpenSSH/ssh.exe"),
        QStringLiteral("C:/msys64/usr/bin/ssh.exe"),
        QStringLiteral("C:/Program Files/Git/usr/bin/ssh.exe"),
    };
    for (const QString &c : candidates) {
        if (QFile::exists(c)) {
            return c;
        }
    }
    return QString();
}

QStringList SshCore::baseArgs() const {
    QStringList args;
    // BatchMode: nigdy nie pytaj o hasło (padnij od razu z czytelnym błędem).
    // accept-new: pierwsze połączenie dopisuje klucz hosta bez pytania.
    args << QStringLiteral("-o") << QStringLiteral("BatchMode=yes")
         << QStringLiteral("-o") << QStringLiteral("ConnectTimeout=8")
         << QStringLiteral("-o") << QStringLiteral("StrictHostKeyChecking=accept-new");
    if (!m_key.isEmpty()) {
        args << QStringLiteral("-i") << m_key;
    }
    args << sshTarget();
    return args;
}

void SshCore::runCmd(Cmd cmd, const QString &remote, bool quiet) {
    if (m_cmd.state() != QProcess::NotRunning) {
        if (!quiet) {
            setError(QString());
            setError(QStringLiteral("Poprzednia komenda SSH jeszcze działa - poczekaj chwilę."));
        }
        return;
    }
    if (m_ssh.isEmpty()) {
        if (!quiet) {
            setError(QString());
            setError(QStringLiteral("Nie znaleziono programu ssh (ani w PATH, ani w System32/OpenSSH, msys64, Git). Doinstaluj OpenSSH albo ustaw HELIOS_SSH_BIN."));
        }
        return;
    }
    m_pending = cmd;
    m_forceError = !quiet;
    // Natychmiastowy odzew na klik, zanim ssh w ogóle odpowie (timeout to 8 s).
    if (cmd == Cmd::Start) {
        emit notice(QStringLiteral("wysyłam START do %1…").arg(sshTarget()));
    } else if (cmd == Cmd::Stop) {
        emit notice(QStringLiteral("wysyłam STOP do %1…").arg(sshTarget()));
    }
    if (cmd == Cmd::Start) {
        setState(QStringLiteral("starting"));
    } else if (cmd == Cmd::Stop) {
        setState(QStringLiteral("stopping"));
    }
    QStringList args = baseArgs();
    args << remote;
    m_cmd.start(m_ssh, args);
}

void SshCore::setState(const QString &state) {
    if (m_state == state) {
        return;
    }
    m_state = state;
    emit coreStateChanged(state);
}

void SshCore::setError(const QString &error) {
    if (m_lastError == error) {
        return;
    }
    m_lastError = error;
    emit lastErrorChanged(error);
}

void SshCore::onCmdFinished(int exitCode, QProcess::ExitStatus status) {
    Q_UNUSED(status)
    const QString out = QString::fromUtf8(m_cmd.readAllStandardOutput()).trimmed();
    const QString err = QString::fromUtf8(m_cmd.readAllStandardError()).trimmed();
    const Cmd cmd = m_pending;
    m_pending = Cmd::None;
    // Klikany błąd pokazujemy ZAWSZE (obejście tłumienia identycznych komunikatów).
    if (m_forceError) {
        m_forceError = false;
        setError(QString());
    }

    if (exitCode != 0) {
        // Powód błędu: skrypt zgłasza FAILED na stdout, samo ssh na stderr.
        QString msg;
        if (!err.isEmpty()) {
            msg = err.split(QLatin1Char('\n')).first();
        } else if (!out.isEmpty()) {
            msg = out.split(QLatin1Char('\n')).first();
        } else {
            msg = QStringLiteral("ssh wróciło z kodem %1").arg(exitCode);
        }
        if (msg.size() > 200) {
            msg = msg.left(200) + QStringLiteral("…");
        }
        setError(msg);
        setState(QStringLiteral("unknown"));
        return;
    }

    if (cmd == Cmd::Status) {
        const QString first = out.split(QLatin1Char('\n')).first().section(QLatin1Char(' '), 0, 0);
        if (first == QStringLiteral("running")) {
            setError(QString());
            setState(QStringLiteral("running"));
        } else if (first == QStringLiteral("stopped")) {
            setError(QString());
            setState(QStringLiteral("stopped"));
        } else if (first == QStringLiteral("starting")) {
            setError(QString());
            setState(QStringLiteral("starting"));
        } else {
            setError(QStringLiteral("Niezrozumiały status: ") + out.left(100));
            setState(QStringLiteral("unknown"));
        }
    } else if (cmd == Cmd::Start) {
        if (out.contains(QStringLiteral("STARTED"))) {
            setError(QString());
            setState(QStringLiteral("starting"));
            emit notice(QStringLiteral("core wstał (%1), sprawdzam status…").arg(out.split(QLatin1Char('\n')).first()));
            // Core wstaje kilkanaście sekund (modele) - odśwież status za chwilę.
            QTimer::singleShot(4000, this, &SshCore::refreshStatus);
            QTimer::singleShot(12000, this, &SshCore::refreshStatus);
        } else {
            setError(!out.isEmpty() ? out.left(200) : QStringLiteral("Start nie powiódł się."));
            setState(QStringLiteral("unknown"));
        }
    } else if (cmd == Cmd::Stop) {
        if (out.contains(QStringLiteral("STOPPED")) && !out.contains(QLatin1Char('?'))) {
            setError(QString());
            setState(QStringLiteral("stopped"));
            emit notice(QStringLiteral("core zatrzymany."));
        } else {
            setError(!out.isEmpty() ? out.left(200) : QStringLiteral("Stop nie powiódł się."));
            setState(QStringLiteral("unknown"));
        }
    }
}

void SshCore::onCmdError(QProcess::ProcessError error) {
    if (error == QProcess::FailedToStart) {
        m_pending = Cmd::None;
        m_forceError = false;
        setError(QString());
        setError(QStringLiteral("Nie można uruchomić ssh (%1) - sprawdź HELIOS_SSH_BIN.").arg(m_ssh.isEmpty() ? QStringLiteral("?") : m_ssh));
        setState(QStringLiteral("unknown"));
    }
    // Pozostałe błędy i tak kończą się onCmdFinished z kodem - tam je opisujemy.
}

void SshCore::setLogStreaming(bool on) {
    if (on && !m_streaming) {
        if (m_ssh.isEmpty()) {
            emit logReceived(QStringLiteral("SSH: nie znaleziono programu ssh (doinstaluj OpenSSH albo ustaw HELIOS_SSH_BIN)."));
            return;
        }
        m_streaming = true;
        m_logBuffer.clear();
        QStringList args = baseArgs();
        args << (m_script + QStringLiteral(" log"));
        m_log.start(m_ssh, args);
    } else if (!on && m_streaming) {
        m_streaming = false;
        m_log.terminate();
        if (!m_log.waitForFinished(2000)) {
            m_log.kill();
        }
    }
}

void SshCore::onLogReady() {
    m_logBuffer.append(m_log.readAllStandardOutput());
    if (m_logBuffer.size() > 65536) {
        m_logBuffer.clear();  // śmieć bez nowej linii - wywalamy, nie zrywamy
        return;
    }
    int pos = 0;
    while ((pos = m_logBuffer.indexOf('\n')) >= 0) {
        const QByteArray raw = m_logBuffer.left(pos);
        m_logBuffer.remove(0, pos + 1);
        QString line = QString::fromUtf8(raw);
        if (line.endsWith(QLatin1Char('\r'))) {
            line.chop(1);
        }
        emit logReceived(line);
    }
}

void SshCore::onLogFinished(int exitCode, QProcess::ExitStatus status) {
    Q_UNUSED(exitCode)
    Q_UNUSED(status)
    if (m_streaming) {
        // Padło samo (np. WiFi) - nie ukrywamy tego przed użytkownikiem.
        m_streaming = false;
        emit logReceived(QStringLiteral("--- strumień logu przerwany (zamknij i otwórz LOG) ---"));
    }
}


void SshCore::driveStart() {
    if (m_drive.state() != QProcess::NotRunning) return;
    if (m_ssh.isEmpty()) { setError(QStringLiteral("Brak ssh.")); return; }
    const QString dir = QDir::cleanPath(
        QFileInfo(m_script).path() + "/../rokae_ws/src/rokae_ros2-main/helios_amr");
    QStringList args = baseArgs();
    args << (QStringLiteral("cd '%1' && bash baza.sh clear && bash baza.sh wasd").arg(dir));
    qDebug() << "[drive]" << m_ssh << args;
    m_drive.start(m_ssh, args);
}

void SshCore::driveKey(QChar k) {
    if (m_drive.state() != QProcess::Running) driveStart();
    if (m_drive.state() != QProcess::Running) return;
    m_holdKey = k.toLatin1();
    m_drive.write(QByteArray(1, m_holdKey));
    m_hold.start(100);   // odświeża "okno trzymania" co 100 ms (zamiast autopowtarzania terminala)
}

void SshCore::driveEnd() {
    m_hold.stop(); m_holdKey = 0;
    if (m_drive.state() == QProcess::Running) {
        m_drive.write("x");
        if (!m_drive.waitForFinished(2000)) m_drive.kill();  // wasd oddaje sterowanie przy 'x'
    }
}

void SshCore::remoteToggle() {
    
    if (m_drive.state() == QProcess::Running) {
        driveEnd();   // 'x' -> amr_wasd wychodzi grzecznie i oddaje sterowanie
        isRemoteOn = false;
        emit notice(QStringLiteral("remote control OFF"));
    } else {
        driveStart(); // cd <dir> && bash baza.sh wasd --speed 150 --turn 200 --hold-ms 600
        isRemoteOn = true;
        emit notice(QStringLiteral("remote control ON - WASD/strzalki, SPACJA = stop"));
    }

}

void SshCore::drivePress(QChar k) {
    if (m_drive.state() != QProcess::Running) return;  // tylko gdy sesja włączona
    const char c = k.toLatin1();
    if (!m_held.contains(c)) {
        m_held.insert(c);
        m_drive.write(QByteArray(1, c));
        if (m_held.size() == 1) m_hold.start(100);   // brak autopowtarzania terminala
    }
}

void SshCore::driveRelease(QChar k) {
    m_held.remove(k.toLatin1());
    if (m_held.isEmpty()) m_hold.stop();
    // amr_wasd sam wyśle zera + stop po wygaśnięciu hold-ms (~600 ms)
}

void SshCore::driveStop() {   // SPACJA - panic frame z panelu (jeśli cap/panel_estop.hex istnieje)
    if (m_drive.state() == QProcess::Running) m_drive.write(" ");
}

bool SshCore::remoteOn(){
    return isRemoteOn;
}
