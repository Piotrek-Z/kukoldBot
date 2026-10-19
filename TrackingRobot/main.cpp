#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "videoreceiver.h"
#include "videoitem.h"
#include "robotclient.h"
#include "helioslink.h"
#include "sshcore.h"
#include "recorder.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;

    VideoReceiver videoReceiver;
    RobotClient robotClient;

    engine.rootContext()->setContextProperty("videoReceiver", &videoReceiver);
    engine.rootContext()->setContextProperty("robotClient", &robotClient);

    HeliosLink heliosLink;
    engine.rootContext()->setContextProperty("heliosLink", &heliosLink);

    SshCore sshCore;
    engine.rootContext()->setContextProperty("sshCore", &sshCore);

    JointRecorder jointRecorder(&robotClient);
    engine.rootContext()->setContextProperty("jointRecorder", &jointRecorder);

    qmlRegisterType<VideoItem>("TrackingRobot", 1, 0, "VideoItem");

    // =========================================================================
    // WYBÓR TRYBU STEROWANIA (JOINT ANGLES vs CARTESIAN IK)
    // =========================================================================
    if (AppConfig::instance().controlMode == "cartesian_ik") {
        qInfo() << "[ROBOT] Uruchomiono w trybie: KARTEZJAŃSKIM (Task-Space IK)";
        QObject::connect(&videoReceiver, &VideoReceiver::cartesianUpdated,
                         &robotClient, &RobotClient::sendCartesianTarget);
    } else {
        qInfo() << "[ROBOT] Uruchomiono w trybie: KĄTÓW ZŁĄCZOWYCH (Joint-Space)";
        QObject::connect(&videoReceiver, &VideoReceiver::allAnglesUpdated,
                         &robotClient, &RobotClient::sendJointAngles);
    }

    engine.loadFromModule("TrackingRobot", "Main");

    QObject *rootObject = engine.rootObjects().first();
    VideoItem *videoItem = rootObject->findChild<VideoItem*>("cameraDisplay");
    if (videoItem) {
        QObject::connect(&videoReceiver, &VideoReceiver::frameReady,
                         videoItem, &VideoItem::onNewFrame);
    }

    QObject::connect(&videoReceiver, &VideoReceiver::anglesChanged, [&]() {
        robotClient.sendJointAngles(
            videoReceiver.rightShoulderYaw(),
            videoReceiver.rightShoulderPitch(), // Prawdziwy bark -> data[0]
            videoReceiver.rightElbowAngle(),    // Prawdziwy łokieć -> data[1]
            videoReceiver.headYaw(),
            videoReceiver.torsoRoll()
            );
    });


    // Autostart PRZENIESIONY do QML (Main.setTracking): strumień i połączenie
    // startują dopiero po komendzie śledzenia, żeby apka nie mieliła na starcie.
    // Oryginał:
    // videoReceiver.startStream("http://192.168.49.97:8080/stream?topic=/camera/color/image_raw"); // dla kamery z robota
    // videoReceiver.startStream("0"); //dla innej kamery np. z lapota
    // robotClient.connectToRobot("192.168.49.97", 9090); // polaczenie z websocket

    return app.exec();
}
