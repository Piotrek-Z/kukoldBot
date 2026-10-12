#pragma once

#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <vector>

struct KeyPoint {
    float x;
    float y;
    float confidence;
};

struct PersonPose {
    std::vector<KeyPoint> keypoints;
    float boxScore = 0.0f;
    float rightElbowAngle = -1.0f;
    float leftElbowAngle = -1.0f;
    float torsoPitch = 0.0f;  // Pochylenie tułowia przód/tył (-45° do +45°)
    float torsoRoll = 0.0f;   // Przechylenie tułowia na boki
    float headYaw = 0.0f;     // Obrót głowy w lewo/prawo
};
class PoseDetector {
public:
    PoseDetector();
    bool loadModel(const std::string &modelPath);
    std::vector<PersonPose> processFrame(cv::Mat &frame);

private:
    cv::dnn::Net m_net;
    const int m_inputWidth = 640;
    const int m_inputHeight = 640;
    const float m_confThreshold = 0.40f;
    const float m_kptThreshold = 0.45f;

    float calculateAngle(const KeyPoint &a, const KeyPoint &b, const KeyPoint &c);
    void drawSkeleton(cv::Mat &frame, const PersonPose &pose);
};