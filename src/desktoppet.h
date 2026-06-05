#ifndef DESKTOPPET_H
#define DESKTOPPET_H

#include <QList>
#include <QLabel>
#include <QPoint>
#include <QPixmap>
#include <QTimer>
#include <QWidget>

class QEvent;
class QMouseEvent;

class DesktopPet : public QWidget
{
    Q_OBJECT

public:
    explicit DesktopPet(QWidget *parent = nullptr);

protected:
    void enterEvent(QEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    void loadFrames();
    void startAnimation();
    void stopAnimation();
    void showIdleFrame();
    void showNextFrame();
    void centerOnScreen();
    
    QPoint m_dragPosition;

    QLabel *m_petLabel;
    QPixmap m_idleFrame;
    QList<QPixmap> m_animationFrames;
    QTimer m_animationTimer;
    int m_currentFrameIndex;
};

#endif
