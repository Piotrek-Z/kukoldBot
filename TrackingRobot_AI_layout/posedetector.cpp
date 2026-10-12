#include "posedetector.h"
#include <cmath>
#include <algorithm>
#include <iostream>

const std::vector<std::pair<int, int>> SKELETON_PAIRS = {
    // Głowa: trójkąt orientacji głowy (Uszy - Nos)
    {3, 4},   // Lewe ucho - Prawe ucho (oś obrotu)
    {0, 3},   // Nos - Lewe ucho
    {0, 4},   // Nos - Prawe ucho

    // Ramię lewe
    {5, 7},   // Lewy bark - Lewy łokieć
    {7, 9},   // Lewy łokieć - Lewy nadgarstek

    // Ramię prawe
    {6, 8},   // Prawy bark - Prawy łokieć
    {8, 10},  // Prawy łokieć - Prawy nadgarstek

    // Tułów (obręcz barkowa, boki, pas biodrowy)
    {5, 6},   // Lewy bark - Prawy bark (szerokość klatki)
    {5, 11},  // Lewy bark - Lewe biodro
    {6, 12},  // Prawy bark - Prawe biodro
    {11, 12}  // Lewe biodro - Prawe biodro
};

const std::vector<int> ESSENTIAL_JOINTS = {0, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};

PoseDetector::PoseDetector() {}

bool PoseDetector::loadModel(const std::string &modelPath) {
    try {
        m_net = cv::dnn::readNetFromONNX(modelPath);
        m_net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        m_net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        if (m_net.empty()) {
            std::cerr << "[PoseDetector] Net is empty after readNetFromONNX: " << modelPath << std::endl;
            return false;
        }
        return true;
    } catch (const cv::Exception &e) {
        std::cerr << "[PoseDetector] Failed to load ONNX model: " << modelPath << "\n"
                  << e.what() << std::endl;
        return false;
    }
}

float PoseDetector::calculateAngle(const KeyPoint &a, const KeyPoint &b, const KeyPoint &c) {
    // Kąt w stawie B między wektorami BA i BC
    float v1x = a.x - b.x;
    float v1y = a.y - b.y;
    float v2x = c.x - b.x;
    float v2y = c.y - b.y;

    float dot = v1x * v2x + v1y * v2y;
    float mag1 = std::sqrt(v1x * v1x + v1y * v1y);
    float mag2 = std::sqrt(v2x * v2x + v2y * v2y);

    if (mag1 * mag2 < 1e-4) return 0.0f;

    float cosAngle = dot / (mag1 * mag2);
    cosAngle = std::max(-1.0f, std::min(1.0f, cosAngle));
    return std::acos(cosAngle) * 180.0f / 3.14159265f;
}

std::vector<PersonPose> PoseDetector::processFrame(cv::Mat &frame) {
    std::vector<PersonPose> poses;
    if (m_net.empty() || frame.empty()) return poses;

    cv::Mat blob;
    cv::dnn::blobFromImage(frame, blob, 1.0 / 255.0, cv::Size(m_inputWidth, m_inputHeight), cv::Scalar(), true, false);
    m_net.setInput(blob);

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

    float maxScore = 0.0f;
    int bestRow = -1;

    for (int i = 0; i < rows; ++i) {
        float score = output.at<float>(i, 4);
        if (score > m_confThreshold && score > maxScore) {
            maxScore = score;
            bestRow = i;
        }
    }

    if (bestRow != -1) {
        PersonPose pose;
        pose.boxScore = maxScore;

        for (int k = 0; k < 17; ++k) {
            int kptIdx = 5 + k * 3;
            float kptX = output.at<float>(bestRow, kptIdx) * x_factor;
            float kptY = output.at<float>(bestRow, kptIdx + 1) * y_factor;
            float kptConf = output.at<float>(bestRow, kptIdx + 2);

            pose.keypoints.push_back({kptX, kptY, kptConf});
        }

        // Kąt prawego łokcia
        if (pose.keypoints[6].confidence > m_kptThreshold &&
            pose.keypoints[8].confidence > m_kptThreshold &&
            pose.keypoints[10].confidence > m_kptThreshold) {
            pose.rightElbowAngle = calculateAngle(pose.keypoints[6], pose.keypoints[8], pose.keypoints[10]);
        }

        // Kąt lewego łokcia
        if (pose.keypoints[5].confidence > m_kptThreshold &&
            pose.keypoints[7].confidence > m_kptThreshold &&
            pose.keypoints[9].confidence > m_kptThreshold) {
            pose.leftElbowAngle = calculateAngle(pose.keypoints[5], pose.keypoints[7], pose.keypoints[9]);
        }

        // Pochylenie tułowia (Roll)
        if (pose.keypoints[5].confidence > m_kptThreshold && pose.keypoints[6].confidence > m_kptThreshold &&
            pose.keypoints[11].confidence > m_kptThreshold && pose.keypoints[12].confidence > m_kptThreshold) {
            float midShoulderX = (pose.keypoints[5].x + pose.keypoints[6].x) / 2.0f;
            float midShoulderY = (pose.keypoints[5].y + pose.keypoints[6].y) / 2.0f;
            float midHipX = (pose.keypoints[11].x + pose.keypoints[12].x) / 2.0f;
            float midHipY = (pose.keypoints[11].y + pose.keypoints[12].y) / 2.0f;

            float dx = midShoulderX - midHipX;
            float dy = midHipY - midShoulderY;
            pose.torsoRoll = std::atan2(dx, dy) * 180.0f / 3.14159265f;
        }

        // Obrót głowy lewo/prawo (Yaw)
        if (pose.keypoints[0].confidence > m_kptThreshold &&
            pose.keypoints[3].confidence > m_kptThreshold && pose.keypoints[4].confidence > m_kptThreshold) {
            float earDist = pose.keypoints[4].x - pose.keypoints[3].x;
            if (std::abs(earDist) > 5.0f) {
                float midEarX = (pose.keypoints[3].x + pose.keypoints[4].x) / 2.0f;
                pose.headYaw = (pose.keypoints[0].x - midEarX) / (earDist / 2.0f) * 45.0f;
            }
        }

        drawSkeleton(frame, pose);
        poses.push_back(pose);
    }

    return poses;
}

void PoseDetector::drawSkeleton(cv::Mat &frame, const PersonPose &pose) {
    for (const auto &pair : SKELETON_PAIRS) {
        const auto &p1 = pose.keypoints[pair.first];
        const auto &p2 = pose.keypoints[pair.second];

        if (p1.confidence > m_kptThreshold && p2.confidence > m_kptThreshold) {
            // Głowa w kolorze błękitnym, ciało w kolorze zielonym
            cv::Scalar color = (pair.first <= 4 && pair.second <= 4) ? cv::Scalar(255, 200, 0) : cv::Scalar(0, 255, 0);
            cv::line(frame, cv::Point(p1.x, p1.y), cv::Point(p2.x, p2.y), color, 2, cv::LINE_AA);
        }
    }

    for (int idx : ESSENTIAL_JOINTS) {
        const auto &kpt = pose.keypoints[idx];
        if (kpt.confidence > m_kptThreshold) {
            cv::circle(frame, cv::Point(kpt.x, kpt.y), 5, cv::Scalar(0, 0, 255), -1, cv::LINE_AA);
            cv::circle(frame, cv::Point(kpt.x, kpt.y), 7, cv::Scalar(255, 255, 255), 1, cv::LINE_AA);
        }
    }

    if (pose.rightElbowAngle > 0) {
        const auto &elbow = pose.keypoints[8];
        std::string txt = "R: " + std::to_string((int)pose.rightElbowAngle) + " deg";
        cv::putText(frame, txt, cv::Point(elbow.x + 10, elbow.y - 10),
                    cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 255), 2, cv::LINE_AA);
    }

    if (pose.leftElbowAngle > 0) {
        const auto &elbow = pose.keypoints[7];
        std::string txt = "L: " + std::to_string((int)pose.leftElbowAngle) + " deg";
        cv::putText(frame, txt, cv::Point(elbow.x - 80, elbow.y - 10),
                    cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 255), 2, cv::LINE_AA);
    }
}