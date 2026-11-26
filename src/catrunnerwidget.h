#ifndef CATRUNNERWIDGET_H
#define CATRUNNERWIDGET_H

#include <QList>
#include <QPixmap>
#include <QPoint>
#include <QTimer>
#include <QWidget>

class CatRunnerWidget : public QWidget
{
    Q_OBJECT

public:
    enum class State {
        Ready,
        Running,
        Paused,
        GameOver
    };

    explicit CatRunnerWidget(const QPixmap &idleCat,
                             const QList<QPixmap> &rotateFrames,
                             QWidget *parent = nullptr);

    static constexpr int kGameWidth = 460;
    static constexpr int kGameHeight = 180;

    State state() const { return m_state; }
    bool hasFrozenRun() const { return m_state == State::Paused; }

    void activateGame();     // 进入游戏面板（若有冻结局则保持冻结等待按键继续，否则 Ready）
    void freezeAndPause();   // 老板键冻结当前局存档
    void resetNewRun(bool startImmediately = false);

signals:
    void bossKeyRequested(); // Esc 或双击请求立即退回静止大猫

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private slots:
    void onGameTick();

private:
    enum class EntityKind {
        CactusSingle,
        CactusCluster,
        CucumberGround,
        CucumberAir,
        BlueFish
    };

    struct Entity {
        EntityKind kind;
        double x = 0.0;
        double y = 0.0;
        double baseY = 0.0;
        int w = 32;
        int h = 32;
        bool knocked = false; // 被无敌冲刺撞飞
        double vx = 0.0;
        double vy = 0.0;
        double angle = 0.0;
    };

    struct FloatingPopup {
        QString text;
        double x = 0.0;
        double y = 0.0;
        int ticksLeft = 36;
        bool highlight = false;
    };

    void triggerJumpOrStart();
    void spawnNextGroup();
    void loadHighScore();
    void saveHighScore();
    double currentBaseSpeed() const;

    QPixmap m_catIdle;
    QList<QPixmap> m_catRotate;
    QPixmap m_pixCactus;
    QPixmap m_pixCactusCluster;
    QPixmap m_pixCucumber;
    QPixmap m_pixBlueFish;
    QPixmap m_pixBlueFishMini;
    QPixmap m_pixBlueFishDim;

    QTimer m_tickTimer;
    State m_state;

    // 猫咪物理与动画状态
    double m_catY;       // 相对地面偏移 (<= 0 为空中)
    double m_catVy;
    int m_jumpsUsed;     // 0=地面, 1=一段跳, 2=二段跳
    bool m_isSpinning;
    int m_spinFrameIdx;
    int m_tickCount;

    // 分数、充能与无敌冲刺
    int m_score;
    int m_highScore;
    int m_fishCharge;      // 0..5，满 5 触发 3s 无敌冲刺
    int m_invincibleTicks; // >0 表示处于无敌冲刺 (180 ticks = 3s)
    double m_groundScroll;
    double m_nextSpawnDist;

    QList<Entity> m_entities;
    QList<FloatingPopup> m_popups;

    // 窗口拖拽兼容
    QPoint m_dragOffset;
    QPoint m_pressGlobalPos;
    bool m_draggingWindow;
};

#endif // CATRUNNERWIDGET_H
