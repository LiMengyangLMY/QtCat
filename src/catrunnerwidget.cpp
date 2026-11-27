#include "catrunnerwidget.h"

#include <QFont>
#include <QImage>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRandomGenerator>
#include <QSettings>
#include <QtMath>

namespace
{
constexpr int kTickIntervalMs = 16;   // ~60 FPS
constexpr int kGroundY = 158;         // 地面基准线 Y
constexpr int kCatX = 28;             // 小猫水平位置
constexpr int kCatSize = 56;          // 原图平滑缩小尺寸
constexpr double kGravity = 0.50;
constexpr double kJumpVel1 = -9.4;    // 一段跳初速度
constexpr double kJumpVel2 = -8.5;    // 二段跳初速度
constexpr int kMaxFishCharge = 5;     // 5 条蓝色鱼干充满无敌
constexpr int kInvincibleDurationTicks = 188;    // ~3.0 秒总无敌时长 (188 * 16ms ≈ 3.0s)
constexpr int kInvincibleSlowdownTicks = 63;     // 最后 ~1.0 秒恢复常速阶段 (63 * 16ms ≈ 1.0s)
constexpr int kInvincibleEaseTicks = 20;         // 进入最后 1 秒时平滑降速过渡帧数 (~0.32s)
constexpr double kInvincibleDashSpeedMul = 1.55; // 无敌冲刺最高速度倍率

double invincibleSpeedMultiplier(int remainingTicks)
{
    if (remainingTicks <= 0) {
        return 1.0;
    }
    if (remainingTicks > kInvincibleSlowdownTicks) {
        return kInvincibleDashSpeedMul;
    }
    const int steadyNormalTicks = kInvincibleSlowdownTicks - kInvincibleEaseTicks;
    if (remainingTicks <= steadyNormalTicks) {
        return 1.0;
    }
    const double t = static_cast<double>(remainingTicks - steadyNormalTicks) / kInvincibleEaseTicks;
    const double smooth = t * t * (3.0 - 2.0 * t);
    return 1.0 + (kInvincibleDashSpeedMul - 1.0) * smooth;
}
}

CatRunnerWidget::CatRunnerWidget(const QPixmap &idleCat,
                                 const QList<QPixmap> &rotateFrames,
                                 QWidget *parent)
    : QWidget(parent)
    , m_state(State::Ready)
    , m_catY(0.0)
    , m_catVy(0.0)
    , m_jumpsUsed(0)
    , m_isSpinning(false)
    , m_spinFrameIdx(0)
    , m_tickCount(0)
    , m_score(0)
    , m_highScore(0)
    , m_fishCharge(0)
    , m_invincibleTicks(0)
    , m_groundScroll(0.0)
    , m_nextSpawnDist(120.0)
    , m_draggingWindow(false)
{
    setFixedSize(kGameWidth, kGameHeight);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);

    // 猫咪保持原图高清质感平滑缩小
    if (!idleCat.isNull()) {
        m_catIdle = idleCat.scaled(kCatSize, kCatSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    for (const QPixmap &f : rotateFrames) {
        if (!f.isNull()) {
            m_catRotate.append(f.scaled(kCatSize, kCatSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    }

    // 像素风道具使用 FastTransformation 保留锐利方块像素边缘
    QPixmap rawCactus(QStringLiteral(":/game/cactus.png"));
    QPixmap rawCluster(QStringLiteral(":/game/cactus_cluster.png"));
    QPixmap rawCucumber(QStringLiteral(":/game/cucumber.png"));
    QPixmap rawFish(QStringLiteral(":/game/blue_fish.png"));

    m_pixCactus = rawCactus.scaled(34, 38, Qt::KeepAspectRatio, Qt::FastTransformation);
    m_pixCactusCluster = rawCluster.scaled(44, 38, Qt::KeepAspectRatio, Qt::FastTransformation);
    m_pixCucumber = rawCucumber.scaled(44, 22, Qt::KeepAspectRatio, Qt::FastTransformation);
    m_pixBlueFish = rawFish.scaled(34, 21, Qt::KeepAspectRatio, Qt::FastTransformation);
    m_pixBlueFishMini = rawFish.scaled(18, 11, Qt::KeepAspectRatio, Qt::FastTransformation);

    // 制作未点亮的半透明暗色小鱼干槽位图标
    QImage dimImg = m_pixBlueFishMini.toImage().convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < dimImg.height(); ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(dimImg.scanLine(y));
        for (int x = 0; x < dimImg.width(); ++x) {
            int a = qAlpha(line[x]);
            if (a > 0) {
                line[x] = qRgba(120, 135, 155, a * 95 / 255);
            }
        }
    }
    m_pixBlueFishDim = QPixmap::fromImage(dimImg);

    loadHighScore();

    m_tickTimer.setInterval(kTickIntervalMs);
    connect(&m_tickTimer, &QTimer::timeout, this, &CatRunnerWidget::onGameTick);
}

void CatRunnerWidget::loadHighScore()
{
    QSettings settings(QStringLiteral("QtCat"), QStringLiteral("QtCatPet"));
    m_highScore = settings.value(QStringLiteral("runnerHighScore"), 0).toInt();
}

void CatRunnerWidget::saveHighScore()
{
    QSettings settings(QStringLiteral("QtCat"), QStringLiteral("QtCatPet"));
    settings.setValue(QStringLiteral("runnerHighScore"), m_highScore);
}

void CatRunnerWidget::activateGame()
{
    setFocus(Qt::OtherFocusReason);
    if (m_state == State::Running) {
        if (!m_tickTimer.isActive()) {
            m_tickTimer.start();
        }
    }
    update();
}

void CatRunnerWidget::freezeAndPause()
{
    if (m_state == State::Running) {
        m_state = State::Paused;
    }
    m_tickTimer.stop();
    update();
}

void CatRunnerWidget::resetNewRun(bool startImmediately)
{
    m_tickTimer.stop();
    m_catY = 0.0;
    m_catVy = 0.0;
    m_jumpsUsed = 0;
    m_isSpinning = false;
    m_spinFrameIdx = 0;
    m_tickCount = 0;
    m_score = 0;
    m_fishCharge = 0;
    m_invincibleTicks = 0;
    m_groundScroll = 0.0;
    m_nextSpawnDist = 90.0;
    m_entities.clear();
    m_popups.clear();

    if (startImmediately) {
        m_state = State::Running;
        m_tickTimer.start();
    } else {
        m_state = State::Ready;
    }
    update();
}

double CatRunnerWidget::currentBaseSpeed() const
{
    double spd = 4.3 + (m_score * 0.0032);
    return qMin(9.2, spd);
}

void CatRunnerWidget::triggerJumpOrStart()
{
    if (m_state == State::Ready) {
        resetNewRun(true);
        m_catVy = kJumpVel1;
        m_jumpsUsed = 1;
        return;
    }

    if (m_state == State::Paused) {
        m_state = State::Running;
        m_tickTimer.start();
        update();
        return;
    }

    if (m_state == State::GameOver) {
        resetNewRun(true);
        return;
    }

    // Running: 一段跳与二段跳
    if (m_jumpsUsed == 0) {
        m_catVy = kJumpVel1;
        m_jumpsUsed = 1;
        m_isSpinning = false;
    } else if (m_jumpsUsed == 1) {
        m_catVy = kJumpVel2;
        m_jumpsUsed = 2;
        // 二段跳触发高速旋转动画
        m_isSpinning = true;
        m_spinFrameIdx = 0;
    }
}

void CatRunnerWidget::spawnNextGroup()
{
    auto *rng = QRandomGenerator::global();
    const double spawnX = kGameWidth + 16.0;

    // 决定本次障碍物类型：单仙人掌 / 双仙人掌 / 地面黄瓜 / 半空飞来黄瓜
    int roll = rng->bounded(100);
    Entity obs;
    obs.x = spawnX;

    if (m_score >= 35 && roll < 26) {
        // 偶尔半空飞来的像素黄瓜（分中低空需跳过 / 高空可从下方钻过两种高度）
        obs.kind = EntityKind::CucumberAir;
        obs.w = m_pixCucumber.width();
        obs.h = m_pixCucumber.height();
        bool highAir = (rng->bounded(2) == 0);
        obs.baseY = highAir ? (kGroundY - 88.0) : (kGroundY - 54.0);
        obs.y = obs.baseY;
    } else if (roll < 56) {
        obs.kind = EntityKind::CactusSingle;
        obs.w = m_pixCactus.width();
        obs.h = m_pixCactus.height();
        obs.baseY = kGroundY - obs.h + 2;
        obs.y = obs.baseY;
    } else if (roll < 80) {
        obs.kind = EntityKind::CactusCluster;
        obs.w = m_pixCactusCluster.width();
        obs.h = m_pixCactusCluster.height();
        obs.baseY = kGroundY - obs.h + 2;
        obs.y = obs.baseY;
    } else {
        obs.kind = EntityKind::CucumberGround;
        obs.w = m_pixCucumber.width();
        obs.h = m_pixCucumber.height();
        obs.baseY = kGroundY - obs.h + 1;
        obs.y = obs.baseY;
    }
    m_entities.append(obs);

    // 约 48% 概率伴随生成蓝色小鱼干（或在障碍物上方诱导跳跃，或在安全间隙）
    if (rng->bounded(100) < 48) {
        Entity fish;
        fish.kind = EntityKind::BlueFish;
        fish.w = m_pixBlueFish.width();
        fish.h = m_pixBlueFish.height();

        if (obs.kind != EntityKind::CucumberAir && rng->bounded(2) == 0) {
            // 悬停在地面障碍物正上方，鼓励跳起或二段跳吃鱼
            fish.x = spawnX + 4.0;
            fish.baseY = (rng->bounded(3) == 0) ? (kGroundY - 106.0) : (kGroundY - 76.0);
        } else {
            // 位于障碍物后方安全区
            fish.x = spawnX + obs.w + rng->bounded(48, 95);
            fish.baseY = (rng->bounded(2) == 0) ? (kGroundY - 32.0) : (kGroundY - 68.0);
        }
        fish.y = fish.baseY;
        m_entities.append(fish);
    }

    // 下一次生成间距随速度适度拉宽，保证 100% 可躲避
    double minGap = 155.0 + currentBaseSpeed() * 14.0;
    int extraGap = rng->bounded(0, 95);
    m_nextSpawnDist = minGap + extraGap;
}

void CatRunnerWidget::onGameTick()
{
    if (m_state != State::Running) {
        return;
    }

    ++m_tickCount;

    const int remainingInvincible = m_invincibleTicks;
    const bool invincible = (remainingInvincible > 0);
    const double speedMul = invincibleSpeedMultiplier(remainingInvincible);
    if (m_invincibleTicks > 0) {
        --m_invincibleTicks;
    }

    const double speed = currentBaseSpeed() * speedMul;
    m_groundScroll += speed;

    // 距离计分（随实际加速状态同步频率）
    if (m_tickCount % ((speedMul > 1.2) ? 4 : 6) == 0) {
        ++m_score;
        if (m_score > m_highScore) {
            m_highScore = m_score;
        }
    }

    // 小猫纵向跳跃物理
    if (m_jumpsUsed > 0 || m_catY < 0.0) {
        m_catY += m_catVy;
        m_catVy += kGravity;
        if (m_catY >= 0.0) {
            m_catY = 0.0;
            m_catVy = 0.0;
            m_jumpsUsed = 0;
            m_isSpinning = false;
        }
    }

    // 旋转帧推进（二段跳或无敌冲刺状态）
    if ((m_isSpinning || invincible) && !m_catRotate.isEmpty()) {
        if (m_tickCount % 2 == 0) {
            m_spinFrameIdx = (m_spinFrameIdx + 1) % m_catRotate.size();
        }
    }

    // 生成新障碍物与鱼干
    m_nextSpawnDist -= speed;
    if (m_nextSpawnDist <= 0.0) {
        spawnNextGroup();
    }

    // 小猫有效碰撞盒（内缩留出宽容度，避免胡须/透明边误判）
    const double catTop = (kGroundY - kCatSize + 4) + m_catY;
    const QRectF catHitbox(kCatX + 14, catTop + 12, kCatSize - 26, kCatSize - 18);

    // 更新所有实体与碰撞检测
    for (int i = m_entities.size() - 1; i >= 0; --i) {
        Entity &e = m_entities[i];

        if (e.knocked) {
            e.x += e.vx;
            e.y += e.vy;
            e.vy += 0.45;
            e.angle += 18.0;
            if (e.x > kGameWidth + 60 || e.y > kGameHeight + 60) {
                m_entities.removeAt(i);
            }
            continue;
        }

        double entitySpeed = (e.kind == EntityKind::CucumberAir) ? (speed * 1.15) : speed;
        e.x -= entitySpeed;

        if (e.kind == EntityKind::CucumberAir) {
            e.y = e.baseY + qSin((m_tickCount + e.x) * 0.08) * 4.5;
        } else if (e.kind == EntityKind::BlueFish) {
            e.y = e.baseY + qSin((m_tickCount * 0.12) + i) * 2.5;
        }

        if (e.x + e.w < -20) {
            m_entities.removeAt(i);
            continue;
        }

        QRectF itemHitbox(e.x + 4, e.y + 4, qMax(8, e.w - 8), qMax(8, e.h - 8));
        if (!catHitbox.intersects(itemHitbox)) {
            continue;
        }

        if (e.kind == EntityKind::BlueFish) {
            // 吃到蓝色小鱼干！
            m_score += 15;
            if (m_score > m_highScore) {
                m_highScore = m_score;
            }

            if (m_invincibleTicks <= 0) {
                ++m_fishCharge;
                if (m_fishCharge >= kMaxFishCharge) {
                    m_fishCharge = 0;
                    m_invincibleTicks = kInvincibleDurationTicks;
                    FloatingPopup dashPop;
                    dashPop.text = QStringLiteral("⚡ 无敌旋转冲刺!");
                    dashPop.x = kCatX + 10;
                    dashPop.y = catTop - 6;
                    dashPop.ticksLeft = 48;
                    dashPop.highlight = true;
                    m_popups.append(dashPop);
                } else {
                    FloatingPopup pop;
                    pop.text = QStringLiteral("+15");
                    pop.x = e.x;
                    pop.y = e.y;
                    pop.ticksLeft = 30;
                    m_popups.append(pop);
                }
            } else {
                FloatingPopup pop;
                pop.text = QStringLiteral("+15");
                pop.x = e.x;
                pop.y = e.y;
                pop.ticksLeft = 28;
                m_popups.append(pop);
            }

            m_entities.removeAt(i);
            continue;
        }

        // 障碍物碰撞
        if (invincible) {
            // 无敌冲刺：直接撞飞障碍物并加分！
            e.knocked = true;
            e.vx = 9.5;
            e.vy = -6.8;
            m_score += 20;
            if (m_score > m_highScore) {
                m_highScore = m_score;
            }
            FloatingPopup pop;
            pop.text = QStringLiteral("撞飞 +20!");
            pop.x = e.x;
            pop.y = e.y - 4;
            pop.ticksLeft = 32;
            pop.highlight = true;
            m_popups.append(pop);
        } else {
            // Game Over
            m_state = State::GameOver;
            m_tickTimer.stop();
            saveHighScore();
            break;
        }
    }

    // 更新飘字特效
    for (int i = m_popups.size() - 1; i >= 0; --i) {
        m_popups[i].y -= 0.7;
        if (--m_popups[i].ticksLeft <= 0) {
            m_popups.removeAt(i);
        }
    }

    update();
}

void CatRunnerWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    // 关键细节：填充 alpha=1 的极微透明层（肉眼 100% 全透明不可见），
    // 确保 Windows 分层透明窗口在跑道空白处单击/右键时不会穿透到底层桌面软件！
    p.fillRect(rect(), QColor(0, 0, 0, 1));

    // 1. 绘制像素风跑道虚线地面与碎石点缀
    const int scrollInt = static_cast<int>(m_groundScroll);
    p.setPen(Qt::NoPen);
    // 双色像素地面线，确保在纯白和纯黑桌面壁纸下都清晰低调
    p.setBrush(QColor(30, 41, 59, 150));
    p.drawRect(8, kGroundY, kGameWidth - 16, 2);
    p.setBrush(QColor(226, 232, 240, 130));
    for (int x = 8 - (scrollInt % 16); x < kGameWidth - 8; x += 16) {
        if (x >= 8) {
            p.drawRect(x, kGroundY - 1, 9, 1);
        }
    }
    // 像素小碎石颗粒
    p.setBrush(QColor(100, 116, 139, 135));
    for (int i = 0; i < 14; ++i) {
        int px = kGameWidth - ((scrollInt + i * 37) % (kGameWidth - 16)) - 8;
        int py = kGroundY + 4 + ((i * 13) % 7);
        p.drawRect(px, py, (i % 2 == 0) ? 3 : 2, 2);
    }

    // 2. 绘制障碍物与蓝色小鱼干
    for (const Entity &e : m_entities) {
        const QPixmap *pix = nullptr;
        switch (e.kind) {
        case EntityKind::CactusSingle:
            pix = &m_pixCactus;
            break;
        case EntityKind::CactusCluster:
            pix = &m_pixCactusCluster;
            break;
        case EntityKind::CucumberGround:
        case EntityKind::CucumberAir:
            pix = &m_pixCucumber;
            break;
        case EntityKind::BlueFish:
            pix = &m_pixBlueFish;
            break;
        }
        if (!pix || pix->isNull()) continue;

        if (e.knocked) {
            p.save();
            p.translate(e.x + e.w * 0.5, e.y + e.h * 0.5);
            p.rotate(e.angle);
            p.drawPixmap(QPointF(-e.w * 0.5, -e.h * 0.5), *pix);
            p.restore();
        } else {
            p.drawPixmap(QPointF(e.x, e.y), *pix);
        }
    }

    // 3. 绘制小猫主角
    const bool invincible = (m_invincibleTicks > 0);
    double runBob = (m_state == State::Running && m_jumpsUsed == 0)
                        ? qSin(m_groundScroll * 0.20) * 2.0
                        : 0.0;
    const double catDrawY = (kGroundY - kCatSize + 4) + m_catY + runBob;

    // 无敌冲刺时的蓝色像素流光护盾与残影
    if (invincible) {
        p.save();
        p.setPen(QPen(QColor(56, 189, 248, 190), 2, Qt::DotLine));
        p.setBrush(QColor(37, 99, 235, 45));
        p.drawEllipse(QRectF(kCatX - 2, catDrawY - 2, kCatSize + 4, kCatSize + 4));
        p.restore();
    }

    const QPixmap *catFrame = &m_catIdle;
    if ((m_isSpinning || invincible) && !m_catRotate.isEmpty()) {
        catFrame = &m_catRotate.at(m_spinFrameIdx % m_catRotate.size());
    }
    if (!catFrame->isNull()) {
        p.drawPixmap(QPointF(kCatX, catDrawY), *catFrame);
    }

    // 4. 顶部 HUD：左侧 5 条蓝色鱼干充能槽（或无敌倒计时条），右侧 HI 最高分与当前分
    p.save();
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(15, 23, 42, 115));
    p.drawRoundedRect(QRectF(8, 6, 116, 20), 5, 5);

    if (invincible) {
        double ratio = static_cast<double>(m_invincibleTicks) / kInvincibleDurationTicks;
        p.setBrush(QColor(56, 189, 248, 220));
        p.drawRoundedRect(QRectF(12, 11, 108.0 * ratio, 10), 3, 3);
    } else {
        for (int i = 0; i < kMaxFishCharge; ++i) {
            const QPixmap &ico = (i < m_fishCharge) ? m_pixBlueFishMini : m_pixBlueFishDim;
            p.drawPixmap(12 + i * 21, 10, ico);
        }
    }

    // 右上角分数胶囊
    p.setBrush(QColor(15, 23, 42, 115));
    p.drawRoundedRect(QRectF(kGameWidth - 154, 6, 146, 20), 5, 5);
    QFont hudFont(QStringLiteral("Consolas"), 9, QFont::Bold);
    p.setFont(hudFont);
    p.setPen(QColor(241, 245, 249, 235));
    QString scoreStr = QStringLiteral("HI %1  %2")
                           .arg(m_highScore, 5, 10, QLatin1Char('0'))
                           .arg(m_score, 5, 10, QLatin1Char('0'));
    p.drawText(QRectF(kGameWidth - 152, 6, 142, 20), Qt::AlignCenter, scoreStr);
    p.restore();

    // 5. 飘字特效 (+15 / 无敌冲刺 / 撞飞+20)
    QFont popFont(QStringLiteral("Microsoft YaHei"), 8, QFont::Bold);
    p.setFont(popFont);
    for (const FloatingPopup &pop : m_popups) {
        int alpha = qBound(0, pop.ticksLeft * 8, 255);
        p.setPen(QColor(15, 23, 42, alpha));
        p.drawText(QPointF(pop.x + 1, pop.y + 1), pop.text);
        p.setPen(pop.highlight ? QColor(56, 189, 248, alpha) : QColor(186, 230, 253, alpha));
        p.drawText(QPointF(pop.x, pop.y), pop.text);
    }

    // 6. 状态横幅提示 (Ready / Paused / GameOver)
    if (m_state != State::Running) {
        QString banner;
        if (m_state == State::Ready) {
            banner = QStringLiteral("🐾 空格/左键 跳跃与二段跳（Esc / 鼠标中键 秒藏）");
        } else if (m_state == State::Paused) {
            banner = QStringLiteral("⏸ 已冻结存档 — 按 空格 或 单击左键 继续当前局");
        } else if (m_state == State::GameOver) {
            banner = QStringLiteral("💥 撞到啦！得分 %1 — 按 空格 或 单击左键 再来一局").arg(m_score);
        }

        p.save();
        QFont bannerFont(QStringLiteral("Microsoft YaHei"), 8, QFont::Bold);
        p.setFont(bannerFont);
        QRectF pill(54, 62, kGameWidth - 108, 28);
        p.setPen(QPen(QColor(56, 189, 248, 120), 1));
        p.setBrush(QColor(15, 23, 42, 165));
        p.drawRoundedRect(pill, 6, 6);
        p.setPen(QColor(248, 250, 252));
        p.drawText(pill, Qt::AlignCenter, banner);
        p.restore();
    }
}

void CatRunnerWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        freezeAndPause();
        emit bossKeyRequested();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Space || event->key() == Qt::Key_Up || event->key() == Qt::Key_W) {
        if (!event->isAutoRepeat()) {
            triggerJumpOrStart();
        }
        event->accept();
        return;
    }

    QWidget::keyPressEvent(event);
}

void CatRunnerWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton) {
        // 游戏模式下用鼠标中键触发老板键秒藏，避免与左键连点二段跳冲突
        freezeAndPause();
        emit bossKeyRequested();
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        setFocus(Qt::MouseFocusReason);
        m_pressGlobalPos = event->globalPos();
        m_dragOffset = event->globalPos() - window()->frameGeometry().topLeft();
        m_draggingWindow = false;
        triggerJumpOrStart();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void CatRunnerWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton) {
        if ((event->globalPos() - m_pressGlobalPos).manhattanLength() > 6) {
            m_draggingWindow = true;
        }
        if (m_draggingWindow) {
            window()->move(event->globalPos() - m_dragOffset);
        }
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void CatRunnerWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_draggingWindow = false;
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void CatRunnerWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // Qt 在快速连点第二下时发送 DoubleClick 而非 Press，这里直接转发为二段跳！
        triggerJumpOrStart();
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}
