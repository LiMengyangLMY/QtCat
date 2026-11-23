#ifndef NOVELMANAGER_H
#define NOVELMANAGER_H

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QSettings>
#include <QString>

struct BookItem
{
    QString filePath; // 优先存相对路径 "books/xxx.ext"，兼容绝对路径
    QString fileName;
    double progressPct = 0.0; // 0.00 ~ 100.00
    int scrollPosition = 0;
    int totalScroll = 1;
    QDateTime lastReadTime;

    double computedProgressPct() const
    {
        if (totalScroll > 0 && scrollPosition >= 0) {
            double pct = (static_cast<double>(scrollPosition) / totalScroll) * 100.0;
            return qBound(0.0, pct, 100.0);
        }
        return qBound(0.0, progressPct, 100.0);
    }
};

class NovelManager : public QObject
{
    Q_OBJECT

public:
    explicit NovelManager(QObject *parent = nullptr);

    // 文件读取与自动编码/格式检测
    static QString readNovelText(const QString &filePath, const QString &codecName = QString());
    static QString detectEncoding(const QString &filePath);
    static QString previewText(const QString &filePath, const QString &codecName = QString(), int maxChars = 200);
    static QString formatDisplayName(const QString &filePath);

    // 内置书库与 CSV 路径
    static QString booksDirPath();
    static QString bookshelfCsvPath();
    static QString resolveBookPath(const QString &storedOrAbsPath);
    static bool isInsideBooksDir(const QString &absPath);

    // 导入到内置书库（返回相对路径 "books/..."，自动去重）
    QString importBookToLibrary(const QString &sourcePath);
    // 从书架移除并清理内置书库副本
    bool removeBook(const QString &storedPath);

    void flushBookshelf(bool force = false);

    // 历史书架
    QList<BookItem> recentBooks() const;
    BookItem findBook(const QString &storedOrAbsPath) const;
    void addOrUpdateBook(const QString &storedPath, int scrollPos = 0, int totalScroll = 1);
    void updateProgress(const QString &storedPath, int scrollPos, int totalScroll);
    BookItem currentBook() const;

    // 个性化设置（颜色、字号、小说区显隐）
    QString textColor() const;
    void setTextColor(const QString &colorHex);

    int fontSize() const;
    void setFontSize(int pt);

    bool isNovelVisible() const;
    void setNovelVisible(bool visible);

signals:
    void styleChanged();
    void bookRemoved(const QString &storedPath);

private:
    void loadSettings();
    void savePreferences();
    bool loadBookshelfCsv();
    void migrateLegacyBookshelfFromSettings(QSettings &settings);
    void migrateExternalBooksToLibrary();

    static QString computeFileHash(const QString &absPath);
    static QString toStoredPath(const QString &absPath);
    static QString escapeCsvField(const QString &field);
    static QStringList parseCsvLine(const QString &line);

    QList<BookItem> m_recentBooks;
    QString m_currentFilePath;
    QString m_textColor;
    int m_fontSize;
    bool m_novelVisible;
    bool m_dirty;
};

#endif // NOVELMANAGER_H
