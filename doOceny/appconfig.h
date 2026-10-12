#pragma once
#include <QString>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFile>
#include <QDebug>
#include <QCoreApplication>
#include <QDir>

class AppConfig {
public:
    static AppConfig& instance() {
        static AppConfig inst;
        return inst;
    }

    // --- 1. Sieć i Komunikacja ROS 2 ---
    QString robotIp{"192.168.49.97"};
    int robotPort{9090};
    QString topicJointCommands{"/teleop/joint_commands"};
    QString topicCartesianCommands{"/teleop/cartesian_commands"};
    int sendIntervalMs{40}; // 25 Hz

    // --- 2. Kamera i Wideo ---
    QString cameraSource{"http://192.168.49.97:8080/stream?topic=/camera/color/image_raw"};
    QString depthSource{"mediapipe_3d"}; // "mediapipe_3d" LUB "orbbec_depth"
    QString depthCameraUrl{"http://192.168.49.97:8080/stream?topic=/camera/depth/image_raw"};
    int cameraWidth{640};
    int cameraHeight{480};

    // --- 3. Śledzenie AI ---
    QString modelPath{"yolov8n-pose.onnx"};
    QString model3DPath{"mediapipe_pose_3d.onnx"};
    float confThreshold{0.45f};
    float kptThreshold{0.50f};
    bool primaryOperatorLock{true}; // Wybór osoby w centrum stoiska

    // --- 4. Filtracja One-Euro (Zero drżeń / Zero laga) ---
    bool filterEnabled{true};
    double filterMinCutoff{1.0};   // Hz - pasmo w bezruchu (usuwa szum kamery)
    double filterBeta{0.007};      // adaptacja do prędkości (zerowy lag)
    double filterDCutoff{1.0};     // Hz - filtr pochodnej

    // --- 5. Tryb Sterowania ---
    QString controlMode{"joint_angles"}; // "joint_angles" LUB "cartesian_ik"

    bool load(const QString &fileName = "config.json") {
        QString appDir = QCoreApplication::applicationDirPath();
        QString fullPath = appDir + QDir::separator() + fileName;

        QFile file(fullPath);
        // Jeśli nie ma obok .exe, spróbuj w katalogu roboczym
        if (!file.exists()) {
            file.setFileName(fileName);
        }

        if (!file.exists()) {
            qWarning() << "[CONFIG] Plik nie istnieje. Tworzenie domyślnego w:" << fullPath;
            return save(fullPath);
        }

        if (!file.open(QIODevice::ReadOnly)) {
            qWarning() << "[CONFIG] Nie można otworzyć:" << file.fileName();
            return false;
        }

        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        file.close();
        if (doc.isNull() || !doc.isObject()) return false;

        QJsonObject root = doc.object();

        if (root.contains("network")) {
            auto net = root["network"].toObject();
            robotIp = net.value("robot_ip").toString(robotIp);
            robotPort = net.value("robot_port").toInt(robotPort);
            topicJointCommands = net.value("topic_joint_commands").toString(topicJointCommands);
            topicCartesianCommands = net.value("topic_cartesian_commands").toString(topicCartesianCommands);
            sendIntervalMs = net.value("send_interval_ms").toInt(sendIntervalMs);
        }

        if (root.contains("camera")) {
            auto cam = root["camera"].toObject();
            cameraSource = cam.value("source").toString(cameraSource);
            depthSource = cam.value("depth_source").toString(depthSource);
            depthCameraUrl = cam.value("depth_camera_url").toString(depthCameraUrl);
            cameraWidth = cam.value("width").toInt(cameraWidth);
            cameraHeight = cam.value("height").toInt(cameraHeight);
        }

        if (root.contains("ai_tracking")) {
            auto ai = root["ai_tracking"].toObject();
            modelPath = ai.value("model_path").toString(modelPath);
            model3DPath = ai.value("model_3d_path").toString(model3DPath);
            confThreshold = static_cast<float>(ai.value("confidence_threshold").toDouble(confThreshold));
            kptThreshold = static_cast<float>(ai.value("keypoint_threshold").toDouble(kptThreshold));
            primaryOperatorLock = ai.value("primary_operator_lock").toBool(primaryOperatorLock);
        }

        if (root.contains("filtering_one_euro")) {
            auto flt = root["filtering_one_euro"].toObject();
            filterEnabled = flt.value("enabled").toBool(filterEnabled);
            filterMinCutoff = flt.value("min_cutoff").toDouble(filterMinCutoff);
            filterBeta = flt.value("beta").toDouble(filterBeta);
            filterDCutoff = flt.value("d_cutoff").toDouble(filterDCutoff);
        }

        if (root.contains("control_mode")) {
            auto ctrl = root["control_mode"].toObject();
            controlMode = ctrl.value("mode").toString(controlMode);
        }

        qInfo() << "[CONFIG] Pomyślnie załadowano konfigurację. Robot IP:" << robotIp
                << "Tryb głębi:" << depthSource;
        return true;
    }

    bool save(const QString &fullPath) {
        QJsonObject root;

        QJsonObject net;
        net["robot_ip"] = robotIp;
        net["robot_port"] = robotPort;
        net["topic_joint_commands"] = topicJointCommands;
        net["topic_cartesian_commands"] = topicCartesianCommands;
        net["send_interval_ms"] = sendIntervalMs;
        root["network"] = net;

        QJsonObject cam;
        cam["source"] = cameraSource;
        cam["depth_source"] = depthSource;
        cam["depth_camera_url"] = depthCameraUrl;
        cam["width"] = cameraWidth;
        cam["height"] = cameraHeight;
        root["camera"] = cam;

        QJsonObject ai;
        ai["model_path"] = modelPath;
        ai["model_3d_path"] = model3DPath;
        ai["confidence_threshold"] = confThreshold;
        ai["keypoint_threshold"] = kptThreshold;
        ai["primary_operator_lock"] = primaryOperatorLock;
        root["ai_tracking"] = ai;

        QJsonObject flt;
        flt["enabled"] = filterEnabled;
        flt["min_cutoff"] = filterMinCutoff;
        flt["beta"] = filterBeta;
        flt["d_cutoff"] = filterDCutoff;
        root["filtering_one_euro"] = flt;

        QJsonObject ctrl;
        ctrl["mode"] = controlMode;
        root["control_mode"] = ctrl;

        QFile file(fullPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        file.close();
        return true;
    }

private:
    AppConfig() {}
};