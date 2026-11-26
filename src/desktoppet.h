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
class QContextMenuEvent;
class QWheelEvent;
class QKeyEvent;
class QDragEnterEvent;
class QDropEvent;
class QPlainTextEdit;
class QPushButton;
class NovelManager;
class CatRunnerWidget;

class DesktopPet : public QWidget
{
    Q_OBJECT

public:
    explicit DesktopPet(QWidget *parent = nullptr);
    ~DesktopPet() override;

protected:
    void enterEvent(QEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void openBookDialog();
    void toggleNovelVisibility();
    void loadNovelFile(const QString &filePath, const QString &codecName = QString(), int resumePos = 0);
    void updateNovelStyle();
    void onScrollChanged(int value);
    void onAutoScrollTimeout();
    void toggleAutoScroll();
    void stepLineUp();
    void stepLineDown();
    void jumpToProgressPercent();
    void enterGameMode(bool forceNewRun = false);
    void exitGameToIdlePet();
    void exitGameToNovel();

private:
    void loadFrames();
    void startAnimation();
    void stopAnimation();
    void showIdleFrame();
    void showNextFrame();
    void centerOnScreen();
    void updateWindowGeometry();
    void startAutoScroll();
    void stopAutoScroll();
    void scrollByLines(int delta);

    QPoint m_dragPosition;
    QPoint m_savedPetPos;
    bool m_hasSavedPetPos;
    bool m_inGameMode;

    QLabel *m_petLabel;
    QPlainTextEdit *m_novelText;
    QPushButton *m_prevLineBtn;
    QPushButton *m_nextLineBtn;
    CatRunnerWidget *m_runnerWidget;
    NovelManager *m_novelManager;

    QPixmap m_idleFrame;
    QList<QPixmap> m_animationFrames;
    QTimer m_animationTimer;
    int m_currentFrameIndex;

    QTimer m_autoScrollTimer;
    bool m_isAutoScrolling;

    QTimer m_autoSaveTimer;

    QString m_loadedFilePath;
};

#endif // DESKTOPPET_H
