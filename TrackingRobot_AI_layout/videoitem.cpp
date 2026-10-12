#include "videoitem.h"

VideoItem::VideoItem(QQuickItem *parent) : QQuickPaintedItem(parent) {
    setAntialiasing(true);
}

void VideoItem::onNewFrame(const QImage &image) {
    m_currentFrame = image;
    update();
}

void VideoItem::clear() {
    m_currentFrame = QImage(); // Zerujemy obraz
    update();                  // Przerysujemy komponent (będzie czarny/pusty)
}

void VideoItem::paint(QPainter *painter) {
    if (m_currentFrame.isNull()) {
        // Jeśli obraz jest pusty, nic nie malujemy (zostanie tło rodzica z QML)
        return;
    }

    QRectF targetRect = boundingRect();
    QImage scaled = m_currentFrame.scaled(targetRect.size().toSize(),
                                          Qt::KeepAspectRatio,
                                          Qt::SmoothTransformation);

    float x = (targetRect.width() - scaled.width()) / 2.0f;
    float y = (targetRect.height() - scaled.height()) / 2.0f;
    painter->drawImage(QPointF(x, y), scaled);
}