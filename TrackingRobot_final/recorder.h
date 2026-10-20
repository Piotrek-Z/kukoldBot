#pragma once
#include <QObject>
#include <QTimer>
#include <QFile>
#include <QTextStream>
#include <QStringList>

class RobotClient;

// Nagrywarka RZECZYWISTYCH stawów robota (ramiona + tułów) z tematu /joint_states.
// Co interwał dopisuje wiersz do pliku CSV (separator ';' - otwiera się wprost w Excelu).
// Sterowanie: komendy czatu ("nagrywaj", "stop nagrywania") albo przyciski w GUI.
class JointRecorder : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool recording READ isRecording NOTIFY recordingChanged)
    Q_PROPERTY(QString filePath READ filePath NOTIFY recordingChanged)
    Q_PROPERTY(int sampleCount READ sampleCount NOTIFY sampleCountChanged)

public:
    explicit JointRecorder(RobotClient *robot, QObject *parent = nullptr);
    ~JointRecorder();

    bool isRecording() const { return m_recording; }
    QString filePath() const { return m_filePath; }
    int sampleCount() const { return m_sampleCount; }

    Q_INVOKABLE void startRecording(double intervalSec = 1.0);
    Q_INVOKABLE void stopRecording();
    Q_INVOKABLE void openRecording(); // otwiera plik w domyślnym programie (np. Excel)

signals:
    void recordingChanged(bool recording);
    void sampleCountChanged(int count);
    void message(const QString &text); // komunikaty zwrotne -> czat (system)

private slots:
    void onTimeout();

private:
    RobotClient *m_robot;
    QTimer m_timer;
    QFile m_file;
    QTextStream m_stream;
    bool m_recording = false;
    QString m_filePath;          // ostatnio zapisany plik (po stopRecording)
    QStringList m_headerNames;   // nazwy stawów z pierwszego odebranego /joint_states
    int m_sampleCount = 0;
};
