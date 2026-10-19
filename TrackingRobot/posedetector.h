#pragma once
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <vector>
#include <string>

struct KeyPoint3D {
    float x{0.0f};          // Metry lub piksele
    float y{0.0f};          // Metry lub piksele
    float z{0.0f};          // Rzeczywista głębia Z w metrach!
    float visibility{0.0f}; // Pewność detekcji
};

struct PersonPose3D {
    std::vector<KeyPoint3D> keypoints; // 33 punkty MediaPipe
    std::vector<KeyPoint3D> worldLandmarks; // Rzeczywiste metry w przestrzeni 3D
    float score{0.0f};

    // Kąty dla robota:
    double rightShoulderYaw{0.0};   // Oś 1: Sięganie w przód (0° - 80°)
    double rightShoulderPitch{0.0}; // Oś 2: Unoszenie barku (0° - 160°)
    double rightElbowAngle{0.0};    // Oś 4: Zgięcie łokcia (0° - 130°)
    float wristRelX{0.0f}; // w metrach (prawo / lewo)
    float wristRelY{0.0f}; // w metrach (góra / dół)
    float wristRelZ{0.0f}; // w metrach (przód / tył)
};

class PoseDetector {
public:
    PoseDetector();
    bool loadModel(const std::string &modelPath);
    std::vector<PersonPose3D> processFrame(cv::Mat &frame);
    void drawSkeleton(cv::Mat &frame, const PersonPose3D &pose);

private:
    cv::dnn::Net m_net;
    int m_inputWidth{256};  // TU 640
    int m_inputHeight{256}; //I TU 640
    float m_confThreshold{0.50f};

    float calculateAngle3D(const KeyPoint3D &a, const KeyPoint3D &b, const KeyPoint3D &c);
};