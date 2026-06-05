#include "desktoppet.h"

#include <QEvent>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QPixmap>
#include <QScreen>

namespace
{
constexpr int kPetSize = 256;
constexpr int kAnimationFps = 15;
constexpr int kScreenMargin = 48;
}

DesktopPet::DesktopPet(QWidget *parent)
    : QWidget(parent)
    , m_petLabel(new QLabel(this))
    , m_currentFrameIndex(0)
{
    setWindowTitle(QStringLiteral("QtCatPet"));
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setMouseTracking(true);
    setFixedSize(kPetSize, kPetSize);

    m_petLabel->setGeometry(rect());
    m_petLabel->setAlignment(Qt::AlignCenter);
    m_petLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

    m_animationTimer.setInterval(1000 / kAnimationFps);
    connect(&m_animationTimer, &QTimer::timeout, this, &DesktopPet::showNextFrame);

    loadFrames();
    showIdleFrame();
    centerOnScreen();
}

void DesktopPet::enterEvent(QEvent *event)
{
    QWidget::enterEvent(event);
    startAnimation();
}

void DesktopPet::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
    stopAnimation();
}

void DesktopPet::loadFrames()
{
    m_idleFrame.load(QStringLiteral(":/pet/idle/idle.png"));

    for (int index = 1; index < 100; ++index) {
        QPixmap frame;
        frame.load(QStringLiteral(":/pet/rotate/frame_%1.png")
                       .arg(index, 2, 10, QLatin1Char('0')));
        if (frame.isNull()) {
            break;
        }

        m_animationFrames.append(frame);
    }
}

void DesktopPet::startAnimation()
{
    if (m_animationFrames.isEmpty()) {
        return;
    }

    if (!m_animationTimer.isActive()) {
        m_currentFrameIndex = 0;
        m_animationTimer.start();
        showNextFrame();
    }
}

void DesktopPet::stopAnimation()
{
    m_animationTimer.stop();
    m_currentFrameIndex = 0;
    showIdleFrame();
}

void DesktopPet::showIdleFrame()
{
    if (!m_idleFrame.isNull()) {
        m_petLabel->setPixmap(m_idleFrame);
    } else {
        m_petLabel->setText(QStringLiteral("idle 加载失败"));
    }
}

void DesktopPet::showNextFrame()
{
    if (m_animationFrames.isEmpty()) {
        return;
    }

    m_petLabel->setPixmap(m_animationFrames.at(m_currentFrameIndex));
    m_currentFrameIndex = (m_currentFrameIndex + 1) % m_animationFrames.size();
}

void DesktopPet::centerOnScreen()
{
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) {
        return;
    }

    const QRect available = screen->availableGeometry();
    move(available.right() - width() - kScreenMargin,
         available.bottom() - height() - kScreenMargin);
}

void DesktopPet::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragPosition = event->globalPos() - frameGeometry().topLeft();
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void DesktopPet::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton) {
        move(event->globalPos() - m_dragPosition);
        event->accept();
        return;
    }

    QWidget::mouseMoveEvent(event);
}
