#include "desktoppet.h"

#include <QGuiApplication>
#include <QPixmap>
#include <QScreen>
#include <QString>
#include <QStringList>
#include <QVBoxLayout>

namespace
{
constexpr int kPetSize = 256;
constexpr int kWindowWidth = 360;
constexpr int kWindowHeight = 420;
constexpr int kAnimationFps = 15;
}

DesktopPet::DesktopPet(QWidget *parent)
    : QWidget(parent)
    , m_imageLabel(new QLabel(this))
    , m_statusLabel(new QLabel(this))
    , m_currentFrameIndex(0)
{
    setWindowTitle(QStringLiteral("QtCatPet Demo"));
    setFixedSize(kWindowWidth, kWindowHeight);
    setStyleSheet(QStringLiteral(
        "QWidget { background-color: #f6f1e8; }"
        "QLabel#titleLabel { color: #3b3128; font-size: 22px; font-weight: 700; }"
        "QLabel#imageLabel { background-color: #ffffff; border: 2px solid #d7c7b2; }"
        "QLabel#statusLabel { color: #6c5c4f; font-size: 14px; }"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(18);

    auto *titleLabel = new QLabel(QStringLiteral("QtCatPet Demo"), this);
    titleLabel->setObjectName(QStringLiteral("titleLabel"));
    titleLabel->setAlignment(Qt::AlignCenter);

    m_imageLabel->setObjectName(QStringLiteral("imageLabel"));
    m_imageLabel->setFixedSize(kPetSize, kPetSize);
    m_imageLabel->setAlignment(Qt::AlignCenter);

    m_statusLabel->setObjectName(QStringLiteral("statusLabel"));
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setWordWrap(true);

    layout->addWidget(titleLabel);
    layout->addWidget(m_imageLabel, 0, Qt::AlignCenter);
    layout->addWidget(m_statusLabel);

    m_animationTimer.setInterval(1000 / kAnimationFps);
    connect(&m_animationTimer, &QTimer::timeout, this, &DesktopPet::showNextFrame);

    loadFrames();
    showIdleFrame();
    updateStatus();
    centerOnScreen();
    startAnimation();
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

void DesktopPet::showIdleFrame()
{
    if (!m_idleFrame.isNull()) {
        m_imageLabel->setPixmap(m_idleFrame);
    } else {
        m_imageLabel->setText(QStringLiteral("静止图加载失败"));
    }
}

void DesktopPet::showNextFrame()
{
    if (m_animationFrames.isEmpty()) {
        return;
    }

    m_imageLabel->setPixmap(m_animationFrames.at(m_currentFrameIndex));
    m_currentFrameIndex = (m_currentFrameIndex + 1) % m_animationFrames.size();
}

void DesktopPet::updateStatus()
{
    const QString idleStatus = m_idleFrame.isNull()
        ? QStringLiteral("静止图：加载失败")
        : QStringLiteral("静止图：已加载 %1x%2").arg(m_idleFrame.width()).arg(m_idleFrame.height());
    const QString frameStatus = QStringLiteral("旋转帧：%1 张").arg(m_animationFrames.size());
    m_statusLabel->setText(idleStatus + QStringLiteral("\n") + frameStatus);
}

void DesktopPet::centerOnScreen()
{
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) {
        return;
    }

    const QRect available = screen->availableGeometry();
    move(available.center().x() - width() / 2,
         available.center().y() - height() / 2);
}
