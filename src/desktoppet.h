#ifndef DESKTOPPET_H
#define DESKTOPPET_H

#include <QList>
#include <QLabel>
#include <QPixmap>
#include <QTimer>
#include <QWidget>

class DesktopPet : public QWidget
{
    Q_OBJECT

public:
    explicit DesktopPet(QWidget *parent = nullptr);

private:
    void loadFrames();
    void startAnimation();
    void showIdleFrame();
    void showNextFrame();
    void updateStatus();
    void centerOnScreen();

    QLabel *m_imageLabel;
    QLabel *m_statusLabel;
    QPixmap m_idleFrame;
    QList<QPixmap> m_animationFrames;
    QTimer m_animationTimer;
    int m_currentFrameIndex;
};

#endif
