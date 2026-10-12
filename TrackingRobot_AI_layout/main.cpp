#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "videoreceiver.h"
#include "videoitem.h"
#include "robotclient.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;

    VideoReceiver videoReceiver;
    RobotClient robotClient;

    engine.rootContext()->setContextProperty("videoReceiver", &videoReceiver);
    engine.rootContext()->setContextProperty("robotClient", &robotClient);

    qmlRegisterType<VideoItem>("TrackingRobot", 1, 0, "VideoItem");

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    engine.loadFromModule("TrackingRobot", "Main");

    QObject *rootObject = engine.rootObjects().first();
    VideoItem *videoItem = rootObject->findChild<VideoItem*>("cameraDisplay");
    if (videoItem) {
        QObject::connect(&videoReceiver, &VideoReceiver::frameReady,
                         videoItem, &VideoItem::onNewFrame);
    }

    QObject::connect(&videoReceiver, &VideoReceiver::anglesChanged, [&]() {
        robotClient.sendJointAngles(
            videoReceiver.rightElbowAngle(),
            videoReceiver.leftElbowAngle(),
            videoReceiver.headYaw(),
            videoReceiver.torsoRoll()
            );
    });

    // videoReceiver.startStream("http://10.111.169.242:8080/stream?topic=/camera/color/image_raw"); // dla kamery z robota

     videoReceiver.startStream("0"); //dla innej kamery np. z lapota

    // polaczenie z websocket
    robotClient.connectToRobot("10.111.169.242", 9090);

    return app.exec();
}