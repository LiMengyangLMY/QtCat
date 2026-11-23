#ifndef BOOKDIALOG_H
#define BOOKDIALOG_H

#include <QDialog>
#include <QString>

class NovelManager;
class QLabel;
class QListWidget;
class QComboBox;
class QTextEdit;
class QPushButton;

class BookDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BookDialog(NovelManager *manager, QWidget *parent = nullptr);

    QString selectedFilePath() const { return m_chosenPath; }
    QString selectedEncoding() const { return m_chosenEncoding; }
    int resumeScrollPos() const { return m_resumePos; }

signals:
    void novelSelected(const QString &filePath, const QString &encoding, int resumePos);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private slots:
    void onBrowseClicked();
    void onRecentBookDoubleClicked(int row);
    void onRecentBookSelectionChanged();
    void onDeleteRecentClicked();
    void onEncodingChanged(int index);
    void onConfirmClicked();

private:
    void setupUi();
    void refreshRecentList();
    void updatePreview();
    void selectFile(const QString &path, int resumePos = 0);

    NovelManager *m_manager;

    QLabel *m_dropAreaLabel;
    QListWidget *m_recentList;
    QPushButton *m_deleteBtn;
    QComboBox *m_encodingCombo;
    QTextEdit *m_previewEdit;
    QPushButton *m_confirmBtn;

    QString m_chosenPath;
    QString m_chosenEncoding;
    int m_resumePos = 0;

    QPoint m_dragPos;
};

#endif // BOOKDIALOG_H
