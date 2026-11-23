#include "bookdialog.h"
#include "ebookparser.h"
#include "novelmanager.h"

#include <QComboBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

BookDialog::BookDialog(NovelManager *manager, QWidget *parent)
    : QDialog(parent)
    , m_manager(manager)
    , m_dropAreaLabel(nullptr)
    , m_recentList(nullptr)
    , m_deleteBtn(nullptr)
    , m_encodingCombo(nullptr)
    , m_previewEdit(nullptr)
    , m_confirmBtn(nullptr)
    , m_chosenEncoding(QStringLiteral("auto"))
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground);
    setAcceptDrops(true);
    setFixedSize(450, 500);

    setupUi();
    refreshRecentList();
}

void BookDialog::setupUi()
{
    // 整体卡片容器，深色磨砂与圆角边框
    auto *container = new QWidget(this);
    container->setGeometry(rect());
    container->setObjectName(QStringLiteral("dialogCard"));
    container->setStyleSheet(QStringLiteral(
        "#dialogCard {"
        "  background-color: #23272A;"
        "  border: 1px solid #36393F;"
        "  border-radius: 10px;"
        "}"
        "QLabel { color: #DCDDDE; font-family: 'Segoe UI', 'Microsoft YaHei'; font-size: 12px; }"
        "QPushButton {"
        "  background-color: #5865F2; color: #FFFFFF; border: none; border-radius: 4px;"
        "  padding: 6px 14px; font-weight: bold; font-size: 12px;"
        "}"
        "QPushButton:hover { background-color: #4752C4; }"
        "QPushButton#closeBtn { background-color: transparent; color: #B9BBBE; font-size: 14px; padding: 2px 6px; }"
        "QPushButton#closeBtn:hover { color: #ED4245; }"
        "QListWidget {"
        "  background-color: #2F3136; border: 1px solid #202225; border-radius: 4px;"
        "  color: #DCDDDE; padding: 4px;"
        "}"
        "QListWidget::item { padding: 4px 6px; border-radius: 3px; }"
        "QListWidget::item:hover { background-color: #393C43; }"
        "QListWidget::item:selected { background-color: #5865F2; color: #FFFFFF; }"
        "QComboBox {"
        "  background-color: #2F3136; border: 1px solid #202225; border-radius: 4px;"
        "  color: #DCDDDE; padding: 4px 8px;"
        "}"
        "QTextEdit {"
        "  background-color: #2F3136; border: 1px solid #202225; border-radius: 4px;"
        "  color: #B9BBBE; font-size: 11px; padding: 6px;"
        "}"
    ));

    auto *mainLayout = new QVBoxLayout(container);
    mainLayout->setContentsMargins(14, 12, 14, 14);
    mainLayout->setSpacing(10);

    // 1. 顶部标题栏
    auto *titleLayout = new QHBoxLayout();
    auto *iconLabel = new QLabel(QStringLiteral("📖  摸鱼书架 (TXT / EPUB / MOBI / AZW3)"), container);
    iconLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 13px; color: #FFFFFF;"));
    auto *closeBtn = new QPushButton(QStringLiteral("✕"), container);
    closeBtn->setObjectName(QStringLiteral("closeBtn"));
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    titleLayout->addWidget(iconLabel);
    titleLayout->addStretch();
    titleLayout->addWidget(closeBtn);
    mainLayout->addLayout(titleLayout);

    // 2. 文件拖拽/选择区域
    m_dropAreaLabel = new QLabel(
        QStringLiteral("📂  点击浏览电子书 (TXT / EPUB / MOBI / AZW3)\n或将电子书文件直接拖拽到此处"),
        container
    );
    m_dropAreaLabel->setAlignment(Qt::AlignCenter);
    m_dropAreaLabel->setCursor(Qt::PointingHandCursor);
    m_dropAreaLabel->setStyleSheet(QStringLiteral(
        "QLabel {"
        "  border: 1px dashed #4F545C; border-radius: 6px;"
        "  background-color: #2F3136; color: #96989D; padding: 12px;"
        "  font-size: 12px; line-height: 1.4;"
        "}"
        "QLabel:hover { border-color: #5865F2; color: #DCDDDE; background-color: #36393F; }"
    ));
    mainLayout->addWidget(m_dropAreaLabel);

    // 3. 历史书架列表 + 删除按钮
    auto *recentHeader = new QHBoxLayout();
    auto *recentTitle = new QLabel(QStringLiteral("📚 内置书库 (双击直接开读)："), container);
    recentTitle->setStyleSheet(QStringLiteral("font-weight: bold; color: #B9BBBE;"));

    m_deleteBtn = new QPushButton(QStringLiteral("🗑️ 删除选中"), container);
    m_deleteBtn->setEnabled(false);
    m_deleteBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #4F545C; color: #DCDDDE; padding: 3px 8px; font-size: 11px; }"
        "QPushButton:hover:enabled { background-color: #ED4245; color: #FFFFFF; }"
        "QPushButton:disabled { background-color: #2F3136; color: #666A70; }"
    ));
    connect(m_deleteBtn, &QPushButton::clicked, this, &BookDialog::onDeleteRecentClicked);

    recentHeader->addWidget(recentTitle);
    recentHeader->addStretch();
    recentHeader->addWidget(m_deleteBtn);
    mainLayout->addLayout(recentHeader);

    m_recentList = new QListWidget(container);
    m_recentList->setFixedHeight(110);
    connect(m_recentList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *) {
        onRecentBookDoubleClicked(m_recentList->currentRow());
    });
    connect(m_recentList, &QListWidget::itemSelectionChanged, this, &BookDialog::onRecentBookSelectionChanged);
    mainLayout->addWidget(m_recentList);

    // 4. 编码检测与首段防乱码预览
    auto *encLayout = new QHBoxLayout();
    auto *encLabel = new QLabel(QStringLiteral("⚙️ 编码识别："), container);
    m_encodingCombo = new QComboBox(container);
    m_encodingCombo->addItem(QStringLiteral("自动检测 (推荐)"), QStringLiteral("auto"));
    m_encodingCombo->addItem(QStringLiteral("UTF-8"), QStringLiteral("UTF-8"));
    m_encodingCombo->addItem(QStringLiteral("GB18030 / GBK"), QStringLiteral("GB18030"));
    connect(m_encodingCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BookDialog::onEncodingChanged);

    encLayout->addWidget(encLabel);
    encLayout->addWidget(m_encodingCombo);
    encLayout->addStretch();
    mainLayout->addLayout(encLayout);

    m_previewEdit = new QTextEdit(container);
    m_previewEdit->setReadOnly(true);
    m_previewEdit->setFixedHeight(75);
    m_previewEdit->setPlaceholderText(QStringLiteral("（选择文件后在此预览前数行文字，确保无乱码）"));
    mainLayout->addWidget(m_previewEdit);

    // 5. 底部按钮
    auto *btnLayout = new QHBoxLayout();
    m_confirmBtn = new QPushButton(QStringLiteral("✔ 确认导入并开读"), container);
    m_confirmBtn->setEnabled(false);
    connect(m_confirmBtn, &QPushButton::clicked, this, &BookDialog::onConfirmClicked);

    auto *cancelBtn = new QPushButton(QStringLiteral("取消"), container);
    cancelBtn->setStyleSheet(QStringLiteral("background-color: #4F545C;"));
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    btnLayout->addStretch();
    btnLayout->addWidget(cancelBtn);
    btnLayout->addWidget(m_confirmBtn);
    mainLayout->addLayout(btnLayout);
}

void BookDialog::refreshRecentList()
{
    m_recentList->clear();
    const auto books = m_manager->recentBooks();
    for (const auto &b : books) {
        QString ext = NovelManager::formatDisplayName(b.filePath);
        QString text = QStringLiteral("[%1] %2  (已读 %3%)")
                           .arg(ext)
                           .arg(b.fileName)
                           .arg(QString::number(b.computedProgressPct(), 'f', 1));
        auto *item = new QListWidgetItem(text, m_recentList);
        item->setData(Qt::UserRole, b.filePath);
        item->setData(Qt::UserRole + 1, b.scrollPosition);
    }
}

void BookDialog::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_dropAreaLabel && m_dropAreaLabel->geometry().contains(event->pos())) {
            onBrowseClicked();
            event->accept();
            return;
        }
        m_dragPos = event->globalPos() - frameGeometry().topLeft();
        event->accept();
        return;
    }
    QDialog::mousePressEvent(event);
}

void BookDialog::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton && !m_dragPos.isNull()) {
        move(event->globalPos() - m_dragPos);
        event->accept();
        return;
    }
    QDialog::mouseMoveEvent(event);
}

void BookDialog::dragEnterEvent(QDragEnterEvent *event)
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

void BookDialog::dropEvent(QDropEvent *event)
{
    const auto exts = EbookParser::supportedExtensions();
    const QList<QUrl> urls = event->mimeData()->urls();
    for (const QUrl &url : urls) {
        QString path = url.toLocalFile();
        QString ext = QFileInfo(path).suffix().toLower();
        if (exts.contains(ext)) {
            selectFile(path, 0);
            event->acceptProposedAction();
            return;
        }
    }
}

void BookDialog::onBrowseClicked()
{
    QString file = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("选择电子书文件"),
        QString(),
        EbookParser::supportedFilter()
    );

    if (!file.isEmpty()) {
        selectFile(file, 0);
    }
}

void BookDialog::onRecentBookSelectionChanged()
{
    QListWidgetItem *cur = m_recentList->currentItem();
    if (m_deleteBtn) {
        m_deleteBtn->setEnabled(cur != nullptr);
    }
    if (cur) {
        QString path = cur->data(Qt::UserRole).toString();
        int pos = cur->data(Qt::UserRole + 1).toInt();
        selectFile(path, pos);
    }
}

void BookDialog::onDeleteRecentClicked()
{
    QListWidgetItem *cur = m_recentList->currentItem();
    if (!cur) return;

    const QString storedPath = cur->data(Qt::UserRole).toString();
    const QString bookLabel = cur->text();
    auto reply = QMessageBox::question(
        this,
        QStringLiteral("从书架删除"),
        QStringLiteral("确定从内置书库删除该书籍及进度记录吗？\n\n%1\n\n（不会删除您原磁盘位置的文件）").arg(bookLabel),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (reply != QMessageBox::Yes) {
        return;
    }

    if (m_manager->removeBook(storedPath)) {
        if (m_chosenPath == storedPath) {
            m_chosenPath.clear();
            m_previewEdit->clear();
            m_confirmBtn->setEnabled(false);
            m_dropAreaLabel->setText(QStringLiteral("📂  点击浏览电子书 (TXT / EPUB / MOBI / AZW3)\n或将电子书文件直接拖拽到此处"));
        }
        refreshRecentList();
        m_deleteBtn->setEnabled(false);
    }
}

void BookDialog::onRecentBookDoubleClicked(int row)
{
    Q_UNUSED(row);
    onConfirmClicked();
}

void BookDialog::onEncodingChanged(int index)
{
    m_chosenEncoding = m_encodingCombo->itemData(index).toString();
    updatePreview();
}

void BookDialog::selectFile(const QString &path, int resumePos)
{
    m_chosenPath = path;
    m_resumePos = resumePos;

    QString detected = NovelManager::detectEncoding(path);
    QString ext = NovelManager::formatDisplayName(path);

    m_dropAreaLabel->setText(QStringLiteral("📄 已选择 [%1]: %2\n编码: %3")
                                 .arg(ext)
                                 .arg(QFileInfo(path).fileName())
                                 .arg(detected));
    m_dropAreaLabel->setStyleSheet(QStringLiteral(
        "QLabel {"
        "  border: 1px solid #5865F2; border-radius: 6px;"
        "  background-color: #2F3136; color: #FFFFFF; padding: 10px;"
        "  font-size: 12px; font-weight: bold; line-height: 1.4;"
        "}"
    ));

    updatePreview();
    m_confirmBtn->setEnabled(true);
}

void BookDialog::updatePreview()
{
    if (m_chosenPath.isEmpty()) {
        m_previewEdit->clear();
        return;
    }

    QString preview = NovelManager::previewText(m_chosenPath, m_chosenEncoding, 180);
    m_previewEdit->setPlainText(preview);
}

void BookDialog::onConfirmClicked()
{
    if (!m_chosenPath.isEmpty()) {
        emit novelSelected(m_chosenPath, m_chosenEncoding, m_resumePos);
        accept();
    }
}
