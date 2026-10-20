#include "posedetector.h"
#include <cmath>
#include <algorithm>
#include <qlogging.h>
#include <QDebug>
#include <QString>

PoseDetector::PoseDetector() {}


bool PoseDetector::loadModel(const std::string &modelPath) {
    try {
        m_net = cv::dnn::readNetFromONNX(modelPath);
        m_net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        m_net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);

        if (m_net.empty()) return false;

        // Domyślny rozmiar
        m_inputWidth = 256;
        m_inputHeight = 256;

        // Jeśli w nazwie pliku jest "yolo", ustawiamy 640x640:
        if (modelPath.find("yolo") != std::string::npos) {
            m_inputWidth = 640;
            m_inputHeight = 640;
        }

        // Próba automatycznego odczytu warstwy wejściowej z OpenCV DNN:
        try {
            std::vector<cv::MatShape> inShapes, outShapes;
            m_net.getLayerShapes(cv::MatShape(), 0, 0, inShapes, outShapes);

            if (!inShapes.empty() && inShapes[0].size() == 4) {
                if (inShapes[0][2] > 0) m_inputHeight = inShapes[0][2];
                if (inShapes[0][3] > 0) m_inputWidth  = inShapes[0][3];
            }
        } catch (...) {
            // W razie braku dynamicznych kształtów używamy ustalonego rozmiaru
        }

        qInfo() << "[AI] Model załadowany:" << QString::fromStdString(modelPath)
                << "Rozmiar wejścia sieci:" << m_inputWidth << "x" << m_inputHeight;

        return true;
    } catch (const cv::Exception &e) {
        qWarning() << "[AI] Błąd ładowania modelu:" << e.what();
        return false;
    }
}

float PoseDetector::calculateAngle3D(const KeyPoint3D &a, const KeyPoint3D &b, const KeyPoint3D &c) {
    // Wektor BA (od łokcia B do barku A)
    float v1x = a.x - b.x;
    float v1y = a.y - b.y;
    float v1z = a.z - b.z;

    // Wektor BC (od łokcia B do dłoni C)
    float v2x = c.x - b.x;
    float v2y = c.y - b.y;
    float v2z = c.z - b.z;

    float dot = v1x * v2x + v1y * v2y + v1z * v2z;
    float mag1 = std::sqrt(v1x * v1x + v1y * v1y + v1z * v1z);
    float mag2 = std::sqrt(v2x * v2x + v2y * v2y + v2z * v2z);

    if (mag1 * mag2 < 1e-5f) return 0.0f;

    float cosAngle = std::clamp(dot / (mag1 * mag2), -1.0f, 1.0f);
    return std::acos(cosAngle) * 180.0f / 3.14159265f;
}

std::vector<PersonPose3D> PoseDetector::processFrame(cv::Mat &frame) {
    std::vector<PersonPose3D> poses;
    if (m_net.empty() || frame.empty()) return poses;

    cv::Mat inputBlob;
    cv::dnn::blobFromImage(frame, inputBlob, 1.0 / 255.0, cv::Size(m_inputWidth, m_inputHeight), cv::Scalar(), true, false);
    m_net.setInput(inputBlob);

    std::vector<cv::Mat> outputs;
    m_net.forward(outputs, m_net.getUnconnectedOutLayersNames());
    if (outputs.empty()) return poses;

    cv::Mat output = outputs[0];
    int dimensions = output.size[1];
    int rows = output.size[2];

    output = output.reshape(1, dimensions);
    cv::transpose(output, output);

    float x_factor = (float)frame.cols / m_inputWidth;
    float y_factor = (float)frame.rows / m_inputHeight;

    int bestRow = -1;
    float maxScore = 0.0f;
    float frameCenterX = frame.cols / 2.0f;

    // 1. Sprawdzamy czy w kadrze faktycznie stoi człowiek (odrzucamy pusty pokój):
    for (int i = 0; i < rows; ++i) {
        float score = output.at<float>(i, 4);
        if (score > 0.55f) {
            float boxX = output.at<float>(i, 0) * x_factor;
            float distFromCenter = std::abs(boxX - frameCenterX) / frameCenterX;
            float metric = score * (1.0f - 0.3f * distFromCenter);

            if (metric > maxScore) {
                maxScore = metric;
                bestRow = i;
            }
        }
    }

    // Jeśli nikogo nie ma w kadrze -> zwracamy puste poses (zero duchów i losowych linii)
    if (bestRow == -1) {
        return poses;
    }

    PersonPose3D pose;
    pose.score = maxScore;

    for (int k = 0; k < 17; ++k) {
        int kptIdx = 5 + k * 3;
        float kptX = output.at<float>(bestRow, kptIdx) * x_factor;
        float kptY = output.at<float>(bestRow, kptIdx + 1) * y_factor;
        float kptVis = output.at<float>(bestRow, kptIdx + 2); // pewność widoczności punktu

        // Zgodnie ze strukturą: x, y, z, visibility
        pose.keypoints.push_back({kptX, kptY, 0.0f, kptVis});
    }

    // =========================================================================
    // GEOMETRIA CZŁOWIEKA - ZOPTYMALIZOWANA DLA KAMERY HELIOSA
    // =========================================================================
    if (pose.keypoints[6].visibility > 0.40f && pose.keypoints[8].visibility > 0.40f) {
        const auto &shoulder = pose.keypoints[6];
        const auto &elbow    = pose.keypoints[8];

        // Szerokość klatki piersiowej jako wzorzec skali odległości od kamery:
        float chestWidth = 100.0f;
        if (pose.keypoints[5].visibility > 0.40f) {
            chestWidth = std::abs(shoulder.x - pose.keypoints[5].x);
            if (chestWidth < 30.0f) chestWidth = 100.0f;
        }

        float armDx = elbow.x - shoulder.x;
        float armDy = elbow.y - shoulder.y;
        float armLen = std::hypot(armDx, armDy);

        // --- 1. OŚ 2: UNOSZENIE BARKU (GÓRA / DÓŁ) ---
        if (armLen > 10.0f) {
            float cosAngle = std::clamp(armDy / armLen, -1.0f, 1.0f);
            pose.rightShoulderPitch = std::acos(cosAngle) * 180.0f / 3.14159265f;
        }

        // --- 2. OŚ 1 & OŚ 4: SIĘGANIE W PRZÓD ORAZ ŁOKIEĆ ---
        if (pose.keypoints[10].visibility > 0.40f) {
            const auto &wrist = pose.keypoints[10];
            float fullArmDx = wrist.x - shoulder.x;
            float fullArmDy = wrist.y - shoulder.y;
            float fullArmLen2D = std::hypot(fullArmDx, fullArmDy);

            // Szacowana pełna długość wyprostowanej ręki w pikselach:
            float expectedFullArm = chestWidth * 1.30f;

            // Odchylenie dłoni w bok (prawo/lewo) od osi barku:
            float lateralOffset = std::abs(fullArmDx);

            // Gdy ręka idzie w stronę kamery (Zdjęcie 4), jej rzut 2D drastycznie maleje:
            float compressionRatio = std::clamp(fullArmLen2D / expectedFullArm, 0.20f, 1.0f);





            // =================================================================
            // KINEMATYKA KARTEZJAŃSKA (WEKTOR DŁONI W METRACH 3D)
            // =================================================================
            // Skalujemy odległości pikselowe względem szerokości klatki (ok. 35-40 cm u człowieka):
            float scaleMeters = 0.38f / chestWidth;

            // Współrzędne w układzie robota (X: przód, Y: lewo/prawo, Z: góra/dół):
            // 1. Wysunięcie w przód (X):
            float reachMeters = (pose.rightShoulderYaw / 80.0f) * 0.40f;
            pose.wristRelX = reachMeters; //

            // 2. Przesunięcie w bok (Y): dłoń na zewnątrz od barku
            pose.wristRelY = (wrist.x - shoulder.x) * scaleMeters;

            // 3. Wysokość (Z): dłoń w dół/góra od barku
            pose.wristRelZ = -(wrist.y - shoulder.y) * scaleMeters; // minus, bo w pikselach Y rośnie w dół





            // Sięganie w przód zachodzi, gdy ręka ulega skrótowi perspektywicznemu
            // lub dłoń znajduje się przed klatką piersiową:
            if (compressionRatio < 0.85f && lateralOffset < chestWidth * 0.90f) {
                // Czyste skalowanie wyciągnięcia: od 0° (przy 85% długości) do 80° (przy pełnym wyciągnięciu):
                float reachFactor = (0.85f - compressionRatio) / 0.55f;
                pose.rightShoulderYaw = std::clamp(reachFactor * 80.0f, 0.0f, 80.0f);

                // Jeśli dłoń nie jest uniesiona pod brodę, to łokieć JEST PROSTY:
                if (wrist.y > shoulder.y - 30.0f) {
                    pose.rightElbowAngle = 0.0f;
                } else {
                    // Rzeczywisty biceps przy klatce:
                    float rawAngle = calculateAngle3D(shoulder, elbow, wrist);
                    pose.rightElbowAngle = std::clamp(180.0f - rawAngle, 0.0f, 130.0f);
                }
            } else {
                // Ręka z boku ciała lub w T-Pose (Zdjęcie 2):
                pose.rightShoulderYaw = 0.0f;
                float rawAngle = calculateAngle3D(shoulder, elbow, wrist);
                pose.rightElbowAngle = std::clamp(180.0f - rawAngle, 0.0f, 130.0f);
            }
        }
    }
    drawSkeleton(frame, pose);
    poses.push_back(pose);

    return poses;
}

void PoseDetector::drawSkeleton(cv::Mat &frame, const PersonPose3D &pose) {
    if (pose.keypoints.size() < 17) return;

    if (pose.keypoints[6].visibility > 0.50f && pose.keypoints[8].visibility > 0.50f) {
        cv::line(frame, cv::Point(pose.keypoints[6].x, pose.keypoints[6].y),
                 cv::Point(pose.keypoints[8].x, pose.keypoints[8].y), cv::Scalar(0, 255, 0), 3);
        cv::circle(frame, cv::Point(pose.keypoints[6].x, pose.keypoints[6].y), 6, cv::Scalar(0, 255, 0), -1);
        cv::circle(frame, cv::Point(pose.keypoints[8].x, pose.keypoints[8].y), 6, cv::Scalar(0, 255, 255), -1);
    }

    if (pose.keypoints[8].visibility > 0.50f && pose.keypoints[10].visibility > 0.50f) {
        cv::line(frame, cv::Point(pose.keypoints[8].x, pose.keypoints[8].y),
                 cv::Point(pose.keypoints[10].x, pose.keypoints[10].y), cv::Scalar(0, 255, 0), 3);
        cv::circle(frame, cv::Point(pose.keypoints[10].x, pose.keypoints[10].y), 6, cv::Scalar(0, 0, 255), -1);
    }

    // Telemetria kątów w lewym górnym rogu
    std::string txtS = "Bark:   " + std::to_string((int)pose.rightShoulderPitch) + " deg";
    std::string txtF = "Przod:  " + std::to_string((int)pose.rightShoulderYaw) + " deg";
    std::string txtE = "Lokiec: " + std::to_string((int)pose.rightElbowAngle) + " deg";

    cv::putText(frame, txtS, cv::Point(25, 40), cv::FONT_HERSHEY_SIMPLEX, 0.75, cv::Scalar(0, 255, 0), 2);
    cv::putText(frame, txtF, cv::Point(25, 75), cv::FONT_HERSHEY_SIMPLEX, 0.75, cv::Scalar(0, 255, 255), 2);
    cv::putText(frame, txtE, cv::Point(25, 110), cv::FONT_HERSHEY_SIMPLEX, 0.75, cv::Scalar(0, 200, 255), 2);
}