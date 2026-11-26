#include "desktoppet.h"
#include "bookdialog.h"
#include "catrunnerwidget.h"
#include "ebookparser.h"
#include "novelmanager.h"

#include <QAction>
#include <QColorDialog>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFileInfo>
#include <QGuiApplication>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScreen>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QWheelEvent>

namespace
{
constexpr int kPetWidth = 256;
constexpr int kPetHeight = 256;
constexpr int kNovelHeight = 128; // 猫高度的 50%
constexpr int kControlWidth = 24; // 右侧前进/后退按钮列宽
constexpr int kTotalHeight = kPetHeight + kNovelHeight; // 384
constexpr int kAnimationFps = 15;
constexpr int kScreenMargin = 48;
constexpr int kAutoScrollIntervalMs = 1500; // 自动播放：每行 1.5s
constexpr int kAutoSaveIntervalMs = 5 * 60 * 1000; // 每 5 分钟自动更新 CSV（异常退出兜底）
}

DesktopPet::DesktopPet(QWidget *parent)
    : QWidget(parent)
    , m_hasSavedPetPos(false)
    , m_inGameMode(false)
    , m_petLabel(new QLabel(this))
    , m_novelText(new QPlainTextEdit(this))
    , m_prevLineBtn(new QPushButton(QStringLiteral("▲"), this))
    , m_nextLineBtn(new QPushButton(QStringLiteral("▼"), this))
    , m_runnerWidget(nullptr)
    , m_novelManager(new NovelManager(this))
    , m_currentFrameIndex(0)
    , m_isAutoScrolling(false)
{
    setWindowTitle(QStringLiteral("QtCatPet"));
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setMouseTracking(true);
    setAcceptDrops(true);

    // 1. 猫本体控件 (256x256)
    m_petLabel->setGeometry(0, 0, kPetWidth, kPetHeight);
    m_petLabel->setAlignment(Qt::AlignCenter);
    m_petLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    // 2. 小说阅读文本框 (位于猫本体下方，与右侧前进/后退按钮共同占满 256x128)
    m_novelText->setGeometry(0, kPetHeight, kPetWidth - kControlWidth, kNovelHeight);
    m_novelText->setReadOnly(true);
    m_novelText->setFrameShape(QFrame::NoFrame);
    m_novelText->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_novelText->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_novelText->setTextInteractionFlags(Qt::NoTextInteraction);
    m_novelText->setFocusPolicy(Qt::NoFocus);
    m_novelText->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    // 让鼠标事件透传给 DesktopPet，避免吞掉拖拽、滚轮、双击和右键菜单事件
    m_novelText->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_novelText->verticalScrollBar()->setSingleStep(1);

    // 3. 手动播放：后退（上划一行）与前进（下划一行）两个按钮
    const int btnX = kPetWidth - kControlWidth + 2;
    const int btnW = kControlWidth - 4;
    const int btnH = 44;

    m_prevLineBtn->setGeometry(btnX, kPetHeight + 12, btnW, btnH);
    m_prevLineBtn->setFocusPolicy(Qt::NoFocus);
    m_prevLineBtn->setCursor(Qt::PointingHandCursor);
    m_prevLineBtn->setToolTip(QStringLiteral("后退：上划一行"));
    connect(m_prevLineBtn, &QPushButton::clicked, this, &DesktopPet::stepLineUp);

    m_nextLineBtn->setGeometry(btnX, kPetHeight + kNovelHeight - btnH - 12, btnW, btnH);
    m_nextLineBtn->setFocusPolicy(Qt::NoFocus);
    m_nextLineBtn->setCursor(Qt::PointingHandCursor);
    m_nextLineBtn->setToolTip(QStringLiteral("前进：下划一行"));
    connect(m_nextLineBtn, &QPushButton::clicked, this, &DesktopPet::stepLineDown);

    connect(m_novelText->verticalScrollBar(), &QScrollBar::valueChanged, this, &DesktopPet::onScrollChanged);
    connect(m_novelManager, &NovelManager::styleChanged, this, &DesktopPet::updateNovelStyle);
    connect(m_novelManager, &NovelManager::bookRemoved, this, [this](const QString &removedPath) {
        if (m_loadedFilePath == removedPath) {
            stopAutoScroll();
            m_loadedFilePath.clear();
            BookItem nextBook = m_novelManager->currentBook();
            const QString nextAbs = NovelManager::resolveBookPath(nextBook.filePath);
            if (!nextBook.filePath.isEmpty() && QFile::exists(nextAbs)) {
                loadNovelFile(nextBook.filePath, QStringLiteral("auto"), nextBook.scrollPosition);
            } else {
                m_novelText->setPlainText(QStringLiteral("右键导入小说 或\n拖拽 TXT/EPUB/MOBI 到小猫身上～"));
                updateNovelStyle();
            }
        }
    });

    // 4. 动画与自动滚动计时器
    m_animationTimer.setInterval(1000 / kAnimationFps);
    connect(&m_animationTimer, &QTimer::timeout, this, &DesktopPet::showNextFrame);

    m_autoScrollTimer.setInterval(kAutoScrollIntervalMs);
    connect(&m_autoScrollTimer, &QTimer::timeout, this, &DesktopPet::onAutoScrollTimeout);

    // 5 分钟定时落盘 CSV（作为异常退出的兜底）
    m_autoSaveTimer.setInterval(kAutoSaveIntervalMs);
    connect(&m_autoSaveTimer, &QTimer::timeout, this, [this]() {
        m_novelManager->flushBookshelf(false);
    });
    m_autoSaveTimer.start();

    // 主动退出时立即落盘保存最新阅读进度
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this]() {
        m_novelManager->flushBookshelf(true);
    });

    loadFrames();
    showIdleFrame();

    // 初始化小猫跑酷小游戏面板 (460x180，复用原图与旋转帧)
    m_runnerWidget = new CatRunnerWidget(m_idleFrame, m_animationFrames, this);
    m_runnerWidget->setGeometry(0, 0, CatRunnerWidget::kGameWidth, CatRunnerWidget::kGameHeight);
    m_runnerWidget->setVisible(false);
    connect(m_runnerWidget, &CatRunnerWidget::bossKeyRequested, this, &DesktopPet::exitGameToIdlePet);

    // 5. 初始化窗口几何与样式
    updateNovelStyle();
    updateWindowGeometry();

    // 6. 自动恢复上次阅读的小说
    BookItem lastBook = m_novelManager->currentBook();
    const QString lastAbs = NovelManager::resolveBookPath(lastBook.filePath);
    if (!lastBook.filePath.isEmpty() && QFile::exists(lastAbs)) {
        loadNovelFile(lastBook.filePath, QStringLiteral("auto"), lastBook.scrollPosition);
    } else {
        m_novelText->setPlainText(QStringLiteral("右键导入小说 或\n拖拽 TXT/EPUB/MOBI 到小猫身上～"));
        updateNovelStyle();
    }

    centerOnScreen();
}

DesktopPet::~DesktopPet()
{
    if (m_novelManager) {
        m_novelManager->flushBookshelf(false);
    }
}

void DesktopPet::updateWindowGeometry()
{
    if (m_inGameMode && m_runnerWidget) {
        if (m_isAutoScrolling) {
            stopAutoScroll();
        }
        stopAnimation();
        m_petLabel->setVisible(false);
        m_novelText->setVisible(false);
        m_prevLineBtn->setVisible(false);
        m_nextLineBtn->setVisible(false);
        m_runnerWidget->setVisible(true);
        setFixedSize(CatRunnerWidget::kGameWidth, CatRunnerWidget::kGameHeight);
        update();
        return;
    }

    if (m_runnerWidget) {
        m_runnerWidget->setVisible(false);
    }
    m_petLabel->setVisible(true);

    bool visible = m_novelManager->isNovelVisible();
    if (!visible && m_isAutoScrolling) {
        stopAutoScroll();
    }
    m_novelText->setVisible(visible);
    m_prevLineBtn->setVisible(visible);
    m_nextLineBtn->setVisible(visible);

    if (visible) {
        setFixedSize(kPetWidth, kTotalHeight);
    } else {
        setFixedSize(kPetWidth, kPetHeight);
    }
    update();
}

void DesktopPet::enterGameMode(bool forceNewRun)
{
    if (!m_runnerWidget) {
        return;
    }

    if (!m_inGameMode) {
        m_savedPetPos = pos();
        m_hasSavedPetPos = true;
    }

    stopAutoScroll();
    stopAnimation();
    m_novelManager->flushBookshelf(false);

    m_inGameMode = true;
    updateWindowGeometry();

    // 防止在屏幕右侧展开 460px 时超出屏幕右边界
    if (QScreen *screen = QGuiApplication::screenAt(frameGeometry().center())) {
        const QRect avail = screen->availableGeometry();
        int nx = qBound(avail.left() + 8, x(), avail.right() - width() - 8);
        int ny = qBound(avail.top() + 8, y(), avail.bottom() - height() - 8);
        move(nx, ny);
    }

    if (forceNewRun) {
        m_runnerWidget->resetNewRun(false);
    }
    m_runnerWidget->activateGame();
    activateWindow();
}

void DesktopPet::exitGameToIdlePet()
{
    if (!m_inGameMode) {
        return;
    }

    if (m_runnerWidget) {
        m_runnerWidget->freezeAndPause();
    }
    m_inGameMode = false;
    // 老板键：收起跑道并回到 256x256 纯静止大猫形态
    m_novelManager->setNovelVisible(false);
    updateWindowGeometry();

    if (m_hasSavedPetPos) {
        move(m_savedPetPos);
    }
}

void DesktopPet::exitGameToNovel()
{
    if (m_runnerWidget) {
        m_runnerWidget->freezeAndPause();
    }
    m_inGameMode = false;
    m_novelManager->setNovelVisible(true);
    updateWindowGeometry();

    if (m_hasSavedPetPos) {
        move(m_savedPetPos);
    }
}

void DesktopPet::updateNovelStyle()
{
    QString color = m_novelManager->textColor();
    int size = m_novelManager->fontSize();

    QScrollBar *vbar = m_novelText->verticalScrollBar();
    const int oldPos = vbar->value();
    const int oldMax = vbar->maximum();
    const double savedPct = (oldMax > 0) ? (static_cast<double>(oldPos) / oldMax) : 0.0;
    QSignalBlocker blocker(vbar);

    // 默认字号为 8pt，背景完全透明
    QString qss = QStringLiteral(
        "QPlainTextEdit {"
        "  background: transparent;"
        "  border: none;"
        "  color: %1;"
        "  font-family: 'Microsoft YaHei', 'Segoe UI', sans-serif;"
        "  font-size: %2pt;"
        "  padding: 2px;"
        "}"
    ).arg(color).arg(size);
    m_novelText->setStyleSheet(qss);

    // 按钮采用低调半透明风格，颜色与正文字色同步，悬停时略微高亮
    QString btnQss = QStringLiteral(
        "QPushButton {"
        "  background-color: rgba(128, 128, 128, 35);"
        "  color: %1;"
        "  border: none;"
        "  border-radius: 4px;"
        "  font-size: 9px;"
        "  padding: 0px;"
        "}"
        "QPushButton:hover {"
        "  background-color: rgba(128, 128, 128, 95);"
        "}"
        "QPushButton:pressed {"
        "  background-color: rgba(128, 128, 128, 150);"
        "}"
    ).arg(color);
    m_prevLineBtn->setStyleSheet(btnQss);
    m_nextLineBtn->setStyleSheet(btnQss);

    // 紧凑行间距
    QTextCursor cursor = m_novelText->textCursor();
    cursor.select(QTextCursor::Document);
    QTextBlockFormat fmt;
    fmt.setLineHeight(102, QTextBlockFormat::ProportionalHeight);
    fmt.setBottomMargin(1);
    cursor.setBlockFormat(fmt);

    const int newMax = vbar->maximum();
    int restoredPos = (oldMax > 0 && newMax > 0 && newMax != oldMax)
                          ? qBound(0, qRound(savedPct * newMax), newMax)
                          : qBound(vbar->minimum(), oldPos, newMax);
    vbar->setValue(restoredPos);

    if (!m_loadedFilePath.isEmpty() && newMax > 0) {
        m_novelManager->updateProgress(m_loadedFilePath, restoredPos, newMax);
    }

    updateWindowGeometry();
}

void DesktopPet::toggleNovelVisibility()
{
    bool nextVisible = !m_novelManager->isNovelVisible();
    if (!nextVisible) {
        // 隐藏文字时立即停止自动播放，并顺手落盘保存当前进度
        stopAutoScroll();
        m_novelManager->flushBookshelf(false);
    }
    m_novelManager->setNovelVisible(nextVisible);
}

void DesktopPet::toggleAutoScroll()
{
    if (m_isAutoScrolling) {
        stopAutoScroll();
    } else {
        startAutoScroll();
    }
}

void DesktopPet::startAutoScroll()
{
    if (!m_novelManager->isNovelVisible()) {
        m_novelManager->setNovelVisible(true);
    }

    QScrollBar *vbar = m_novelText->verticalScrollBar();
    if (vbar->maximum() <= 0 || vbar->value() >= vbar->maximum()) {
        // 已经在末尾或无需滚动时不启动空转
        stopAutoScroll();
        return;
    }

    m_isAutoScrolling = true;
    m_autoScrollTimer.start(kAutoScrollIntervalMs); // 每行 1.5 秒
    setToolTip(QStringLiteral("▶ 自动播放中 (每行1.5秒，空格/中键暂停)"));
}

void DesktopPet::stopAutoScroll()
{
    m_isAutoScrolling = false;
    m_autoScrollTimer.stop();
    setToolTip(QString());
}

void DesktopPet::onAutoScrollTimeout()
{
    if (!m_novelManager->isNovelVisible()) {
        stopAutoScroll();
        return;
    }

    QScrollBar *vbar = m_novelText->verticalScrollBar();
    if (vbar->value() < vbar->maximum()) {
        vbar->setValue(vbar->value() + 1); // 严格每次下划 1 行
    } else {
        stopAutoScroll();
    }
}

void DesktopPet::scrollByLines(int delta)
{
    if (!m_novelManager->isNovelVisible()) {
        return;
    }

    QScrollBar *vbar = m_novelText->verticalScrollBar();
    int target = qBound(vbar->minimum(), vbar->value() + delta, vbar->maximum());
    vbar->setValue(target);
}

void DesktopPet::stepLineUp()
{
    scrollByLines(-1);
    if (m_isAutoScrolling) {
        // 手动调整后重置 1.5s 计时，避免立刻连跳
        m_autoScrollTimer.start(kAutoScrollIntervalMs);
    }
}

void DesktopPet::stepLineDown()
{
    scrollByLines(1);
    if (m_isAutoScrolling) {
        m_autoScrollTimer.start(kAutoScrollIntervalMs);
    }
}

void DesktopPet::openBookDialog()
{
    BookDialog dlg(m_novelManager, this);
    connect(&dlg, &BookDialog::novelSelected, this, &DesktopPet::loadNovelFile);
    dlg.exec();
}

void DesktopPet::jumpToProgressPercent()
{
    if (m_loadedFilePath.isEmpty()) {
        return;
    }

    QScrollBar *vbar = m_novelText->verticalScrollBar();
    const int maxVal = vbar->maximum();
    if (maxVal <= 0) {
        return;
    }

    const double currentPct = (static_cast<double>(vbar->value()) / maxVal) * 100.0;
    bool ok = false;
    double targetPct = QInputDialog::getDouble(
        this,
        QStringLiteral("跳转到指定进度"),
        QStringLiteral("请输入绝对阅读位置 (0.0 ~ 100.0 %)："),
        currentPct,
        0.0,
        100.0,
        1,
        &ok
    );

    if (!ok) {
        return;
    }

    int targetPos = qBound(0, qRound((targetPct / 100.0) * maxVal), maxVal);
    vbar->setValue(targetPos);
    m_novelManager->updateProgress(m_loadedFilePath, targetPos, maxVal);
    m_novelManager->flushBookshelf(true);

    if (m_isAutoScrolling) {
        m_autoScrollTimer.start(kAutoScrollIntervalMs);
    }
}

void DesktopPet::loadNovelFile(const QString &filePath, const QString &codecName, int resumePos)
{
    if (filePath.isEmpty()) {
        return;
    }

    // 导入或复用到内置书库 dist/books/，彻底避免原文件移动导致失效
    const QString storedPath = m_novelManager->importBookToLibrary(filePath);
    const QString absPath = NovelManager::resolveBookPath(storedPath);
    if (storedPath.isEmpty() || !QFile::exists(absPath)) {
        return;
    }

    // 切书前先把当前书的最新进度落盘
    if (!m_loadedFilePath.isEmpty() && m_loadedFilePath != storedPath) {
        m_novelManager->flushBookshelf(false);
    }

    const BookItem savedBook = m_novelManager->findBook(storedPath);
    const int savedPos = (resumePos > 0) ? resumePos : savedBook.scrollPosition;
    const int savedTotal = savedBook.totalScroll;
    const double savedPct = savedBook.progressPct;

    m_loadedFilePath = storedPath;
    QString text = NovelManager::readNovelText(absPath, codecName);

    if (text.isEmpty()) {
        m_novelText->setPlainText(QStringLiteral("（文件内容为空）"));
        return;
    }

    auto resolveRestorePos = [savedPos, savedTotal, savedPct](int curMax) -> int {
        if (curMax <= 0) return 0;
        if (savedTotal > 0 && savedTotal == curMax && savedPos > 0) {
            return qBound(0, savedPos, curMax);
        }
        if (savedPct > 0.0) {
            return qBound(0, qRound((savedPct / 100.0) * curMax), curMax);
        }
        return qBound(0, savedPos, curMax);
    };

    QScrollBar *vbar = m_novelText->verticalScrollBar();
    {
        // 加载期间阻塞滚动信号，防止 setPlainText 触发 valueChanged(0) 覆盖已保存的进度
        QSignalBlocker blocker(vbar);
        m_novelText->setPlainText(text);
        updateNovelStyle();

        // 保持 textWidth = -1 让 QPlainTextEdit 按视口全宽自动换行
        vbar->setValue(resolveRestorePos(vbar->maximum()));
    }

    // 若在窗口首次 show() 前调用，布局完成后再同步一次滚动位置与总行数并写入 CSV
    QTimer::singleShot(0, this, [this, storedPath, resolveRestorePos]() {
        if (m_loadedFilePath != storedPath) return;
        QScrollBar *vb = m_novelText->verticalScrollBar();
        int targetPos = resolveRestorePos(vb->maximum());
        if (vb->value() != targetPos && vb->maximum() > 0) {
            QSignalBlocker b(vb);
            vb->setValue(targetPos);
        }
        m_novelManager->addOrUpdateBook(storedPath, vb->value(), vb->maximum());
    });

    // 如果当前隐藏了文本框，自动唤出
    if (!m_novelManager->isNovelVisible()) {
        m_novelManager->setNovelVisible(true);
    }
}

void DesktopPet::onScrollChanged(int value)
{
    if (!m_loadedFilePath.isEmpty()) {
        QScrollBar *vbar = m_novelText->verticalScrollBar();
        m_novelManager->updateProgress(m_loadedFilePath, value, vbar->maximum());
    }
}

void DesktopPet::enterEvent(QEvent *event)
{
    QWidget::enterEvent(event);
    if (!m_inGameMode) {
        startAnimation();
    }
}

void DesktopPet::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
    if (!m_inGameMode) {
        stopAnimation();
    }
}

void DesktopPet::mousePressEvent(QMouseEvent *event)
{
    if (m_inGameMode) {
        QWidget::mousePressEvent(event);
        return;
    }

    // 1. 鼠标中键：一键启停 1.5s/行 自动播放
    if (event->button() == Qt::MiddleButton) {
        toggleAutoScroll();
        event->accept();
        return;
    }

    // 2. 鼠标左键：拖拽窗口
    if (event->button() == Qt::LeftButton) {
        m_dragPosition = event->globalPos() - frameGeometry().topLeft();
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void DesktopPet::mouseMoveEvent(QMouseEvent *event)
{
    if (m_inGameMode) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    // 1. 窗口拖拽
    if (event->buttons() & Qt::LeftButton) {
        move(event->globalPos() - m_dragPosition);
        event->accept();
        return;
    }

    // 2. 动画分区：鼠标仅在猫咪本体区域时触发旋转动画，在小说区域时不干扰
    if (event->pos().y() < kPetHeight) {
        startAnimation();
    } else {
        stopAnimation();
    }

    QWidget::mouseMoveEvent(event);
}

void DesktopPet::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (!m_inGameMode) {
            // 双击老板键（看小说模式）：一键隐蔽/恢复小说文本框
            toggleNovelVisibility();
        }
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void DesktopPet::wheelEvent(QWheelEvent *event)
{
    if (m_inGameMode) {
        event->ignore();
        return;
    }

    if (m_novelManager->isNovelVisible()) {
        int deltaY = event->angleDelta().y();
        if (deltaY != 0) {
            // Ctrl + 滚轮：快速实时调节字号
            if (event->modifiers() & Qt::ControlModifier) {
                int fontDelta = (deltaY > 0) ? 1 : -1;
                m_novelManager->setFontSize(m_novelManager->fontSize() + fontDelta);
                event->accept();
                return;
            }

            int step = (deltaY > 0) ? -1 : 1;
            if (event->modifiers() & Qt::ShiftModifier) {
                step *= 4;
            }
            scrollByLines(step);
            if (m_isAutoScrolling) {
                m_autoScrollTimer.start(kAutoScrollIntervalMs);
            }
        }
        event->accept();
        return;
    }
    QWidget::wheelEvent(event);
}

void DesktopPet::keyPressEvent(QKeyEvent *event)
{
    if (m_inGameMode) {
        if (event->key() == Qt::Key_Escape) {
            exitGameToIdlePet();
            event->accept();
            return;
        }
        QWidget::keyPressEvent(event);
        return;
    }

    // Esc 快捷老板键：隐藏文本框并暂停自动播放
    if (event->key() == Qt::Key_Escape) {
        stopAutoScroll();
        m_novelManager->setNovelVisible(false);
        event->accept();
        return;
    }

    // 空格键：启停自动播放
    if (event->key() == Qt::Key_Space) {
        toggleAutoScroll();
        event->accept();
        return;
    }

    if (m_novelManager->isNovelVisible()) {
        if (event->key() == Qt::Key_Up || event->key() == Qt::Key_K) {
            stepLineUp();
            event->accept();
            return;
        } else if (event->key() == Qt::Key_Down || event->key() == Qt::Key_J) {
            stepLineDown();
            event->accept();
            return;
        } else if (event->key() == Qt::Key_PageUp) {
            scrollByLines(-4);
            event->accept();
            return;
        } else if (event->key() == Qt::Key_PageDown || event->key() == Qt::Key_Return) {
            scrollByLines(4);
            event->accept();
            return;
        }
    }

    QWidget::keyPressEvent(event);
}

void DesktopPet::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        const auto exts = EbookParser::supportedExtensions();
        for (const QUrl &url : event->mimeData()->urls()) {
            QString ext = QFileInfo(url.toLocalFile()).suffix().toLower();
            if (exts.contains(ext)) {
                event->acceptProposedAction();
                return;
            }
        }
    }
    event->ignore();
}

void DesktopPet::dropEvent(QDropEvent *event)
{
    const auto exts = EbookParser::supportedExtensions();
    const QList<QUrl> urls = event->mimeData()->urls();
    for (const QUrl &url : urls) {
        QString path = url.toLocalFile();
        QString ext = QFileInfo(path).suffix().toLower();
        if (exts.contains(ext)) {
            loadNovelFile(path, QStringLiteral("auto"), 0);
            event->acceptProposedAction();
            return;
        }
    }
}

void DesktopPet::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    menu.setStyleSheet(QStringLiteral(
        "QMenu {"
        "  background-color: #2F3136; color: #DCDDDE;"
        "  border: 1px solid #202225; border-radius: 6px; padding: 4px;"
        "  font-family: 'Microsoft YaHei', 'Segoe UI'; font-size: 12px;"
        "}"
        "QMenu::item { padding: 6px 20px; border-radius: 4px; }"
        "QMenu::item:selected { background-color: #5865F2; color: #FFFFFF; }"
        "QMenu::separator { height: 1px; background-color: #40444B; margin: 4px 8px; }"
    ));

    // 若当前处于跑酷小游戏模式，展示精简专属游戏右键菜单
    if (m_inGameMode) {
        QAction *bossAct = menu.addAction(QStringLiteral("🐾  冻结存档并退回静止大猫 (Esc / 鼠标中键)"));
        connect(bossAct, &QAction::triggered, this, &DesktopPet::exitGameToIdlePet);

        QAction *restartAct = menu.addAction(QStringLiteral("🔄  重新开始新的一局"));
        connect(restartAct, &QAction::triggered, this, [this]() {
            if (m_runnerWidget) {
                m_runnerWidget->resetNewRun(true);
            }
        });

        QAction *toNovelAct = menu.addAction(QStringLiteral("📖  暂存游戏并切回摸鱼看小说"));
        connect(toNovelAct, &QAction::triggered, this, &DesktopPet::exitGameToNovel);

        menu.addSeparator();
        QAction *quitAct = menu.addAction(QStringLiteral("❌  退出"));
        connect(quitAct, &QAction::triggered, qApp, &QCoreApplication::quit);
        menu.exec(event->globalPos());
        return;
    }

    // 0. 小猫跑酷小游戏入口（若有冻结局提示继续）
    if (m_runnerWidget && m_runnerWidget->hasFrozenRun()) {
        QAction *resumeGameAct = menu.addAction(QStringLiteral("🎮  继续小猫跑酷 (已冻结存档)"));
        connect(resumeGameAct, &QAction::triggered, this, [this]() {
            enterGameMode(false);
        });
        QAction *newGameAct = menu.addAction(QStringLiteral("🔄  新开一局小猫跑酷"));
        connect(newGameAct, &QAction::triggered, this, [this]() {
            enterGameMode(true);
        });
    } else {
        QAction *gameAct = menu.addAction(QStringLiteral("🎮  小猫跑酷小游戏"));
        connect(gameAct, &QAction::triggered, this, [this]() {
            enterGameMode(false);
        });
    }

    menu.addSeparator();

    // 1. 导入小说 / 摸鱼书架
    QAction *bookAction = menu.addAction(QStringLiteral("📖  导入电子书 / 摸鱼书架..."));
    connect(bookAction, &QAction::triggered, this, &DesktopPet::openBookDialog);

    // 2. 自动播放开关 (每行 1.5s)
    QString autoText = m_isAutoScrolling
                           ? QStringLiteral("⏸  暂停自动播放 (每行1.5秒，空格/中键)")
                           : QStringLiteral("▶  开启自动播放 (每行1.5秒，空格/中键)");
    QAction *autoScrollAct = menu.addAction(autoText);
    connect(autoScrollAct, &QAction::triggered, this, &DesktopPet::toggleAutoScroll);

    // 3. 老板键切换显示
    QString toggleText = m_novelManager->isNovelVisible()
                             ? QStringLiteral("👁️  隐蔽小说框 (Esc / 双击猫咪)")
                             : QStringLiteral("👁️  显示小说框 (双击猫咪)");
    QAction *toggleAction = menu.addAction(toggleText);
    connect(toggleAction, &QAction::triggered, this, &DesktopPet::toggleNovelVisibility);

    menu.addSeparator();

    // 4. 手动输入绝对进度跳转 (x%)
    QScrollBar *vbar = m_novelText->verticalScrollBar();
    double curPct = (vbar->maximum() > 0)
                        ? (static_cast<double>(vbar->value()) / vbar->maximum()) * 100.0
                        : 0.0;
    QAction *jumpAction = menu.addAction(
        QStringLiteral("🎯  跳转到指定进度 (当前 %1%)...").arg(QString::number(curPct, 'f', 1))
    );
    jumpAction->setEnabled(!m_loadedFilePath.isEmpty() && vbar->maximum() > 0);
    connect(jumpAction, &QAction::triggered, this, &DesktopPet::jumpToProgressPercent);

    menu.addSeparator();

    // 5. 文字颜色子菜单 (从白到黑)
    QMenu *colorMenu = menu.addMenu(QStringLiteral("🎨  文字颜色 (白 ~ 黑)"));
    colorMenu->setStyleSheet(menu.styleSheet());

    auto addColorAction = [this, colorMenu](const QString &name, const QString &hex) {
        QAction *act = colorMenu->addAction(name);
        connect(act, &QAction::triggered, this, [this, hex]() {
            m_novelManager->setTextColor(hex);
        });
    };

    addColorAction(QStringLiteral("⚪ 纯白 (#FFFFFF) - 适合暗色壁纸"), QStringLiteral("#FFFFFF"));
    addColorAction(QStringLiteral("🔘 浅灰 (#D0D0D0) - 柔和舒适"), QStringLiteral("#D0D0D0"));
    addColorAction(QStringLiteral("🔘 中灰 (#888888) - 推荐摸鱼档"), QStringLiteral("#888888"));
    addColorAction(QStringLiteral("🔘 深灰 (#444444) - 浅灰壁纸"), QStringLiteral("#444444"));
    addColorAction(QStringLiteral("⚫ 纯黑 (#000000) - 适合亮色壁纸"), QStringLiteral("#000000"));

    colorMenu->addSeparator();
    QAction *customColorAction = colorMenu->addAction(QStringLiteral("🎨 自定义颜色..."));
    connect(customColorAction, &QAction::triggered, this, [this]() {
        QColor cur(m_novelManager->textColor());
        QColor chosen = QColorDialog::getColor(cur, this, QStringLiteral("选择摸鱼文字颜色"));
        if (chosen.isValid()) {
            m_novelManager->setTextColor(chosen.name());
        }
    });

    // 6. 字号调整子菜单（显示当前字号 + 常用档位 + 自定义输入 + Ctrl+滚轮）
    const int curFontSize = m_novelManager->fontSize();
    QMenu *fontMenu = menu.addMenu(QStringLiteral("🔤  字号调整 (当前 %1pt)").arg(curFontSize));
    fontMenu->setStyleSheet(menu.styleSheet());

    QAction *biggerFont = fontMenu->addAction(QStringLiteral("放大字号 (+1pt / Ctrl+滚轮上)"));
    connect(biggerFont, &QAction::triggered, this, [this]() {
        m_novelManager->setFontSize(m_novelManager->fontSize() + 1);
    });

    QAction *smallerFont = fontMenu->addAction(QStringLiteral("缩小字号 (-1pt / Ctrl+滚轮下)"));
    connect(smallerFont, &QAction::triggered, this, [this]() {
        m_novelManager->setFontSize(m_novelManager->fontSize() - 1);
    });

    QAction *customFont = fontMenu->addAction(QStringLiteral("自定义字号 (4 ~ 20pt)..."));
    connect(customFont, &QAction::triggered, this, [this]() {
        bool ok = false;
        int pt = QInputDialog::getInt(
            this,
            QStringLiteral("调整字号"),
            QStringLiteral("请输入字号大小 (4 ~ 20 pt)："),
            m_novelManager->fontSize(),
            4,
            20,
            1,
            &ok
        );
        if (ok) {
            m_novelManager->setFontSize(pt);
        }
    });

    fontMenu->addSeparator();
    const QList<int> presetSizes = {5, 6, 7, 8, 9, 10, 12, 14};
    for (int pt : presetSizes) {
        QString label = (pt == 8)
                            ? QStringLiteral("%1pt (默认)").arg(pt)
                            : QStringLiteral("%1pt").arg(pt);
        QAction *presetAct = fontMenu->addAction(label);
        presetAct->setCheckable(true);
        presetAct->setChecked(curFontSize == pt);
        connect(presetAct, &QAction::triggered, this, [this, pt]() {
            m_novelManager->setFontSize(pt);
        });
    }

    menu.addSeparator();

    // 7. 退出
    QAction *quitAction = menu.addAction(QStringLiteral("❌  退出"));
    connect(quitAction, &QAction::triggered, qApp, &QCoreApplication::quit);

    menu.exec(event->globalPos());
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
