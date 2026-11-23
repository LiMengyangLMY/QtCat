#include "novelmanager.h"
#include "ebookparser.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextCodec>

NovelManager::NovelManager(QObject *parent)
    : QObject(parent)
    , m_textColor(QStringLiteral("#D0D0D0"))
    , m_fontSize(8)
    , m_novelVisible(true)
    , m_dirty(false)
{
    loadSettings();
}

QString NovelManager::booksDirPath()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("books"));
}

QString NovelManager::bookshelfCsvPath()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("bookshelf.csv"));
}

QString NovelManager::resolveBookPath(const QString &storedOrAbsPath)
{
    if (storedOrAbsPath.isEmpty()) {
        return QString();
    }
    QFileInfo fi(storedOrAbsPath);
    if (fi.isAbsolute()) {
        return QDir::cleanPath(fi.absoluteFilePath());
    }
    return QDir::cleanPath(QDir(QCoreApplication::applicationDirPath()).filePath(storedOrAbsPath));
}

bool NovelManager::isInsideBooksDir(const QString &absPath)
{
    if (absPath.isEmpty()) {
        return false;
    }
    QString booksDir = QDir::cleanPath(QFileInfo(booksDirPath()).absoluteFilePath());
    QString target = QDir::cleanPath(QFileInfo(absPath).absoluteFilePath());
    if (!booksDir.endsWith(QLatin1Char('/'))) {
        booksDir.append(QLatin1Char('/'));
    }
#ifdef Q_OS_WIN
    return target.startsWith(booksDir, Qt::CaseInsensitive);
#else
    return target.startsWith(booksDir, Qt::CaseSensitive);
#endif
}

QString NovelManager::toStoredPath(const QString &absPath)
{
    QString cleaned = QDir::cleanPath(QFileInfo(absPath).absoluteFilePath());
    if (isInsideBooksDir(cleaned)) {
        QDir appDir(QCoreApplication::applicationDirPath());
        return QDir::cleanPath(appDir.relativeFilePath(cleaned));
    }
    return cleaned;
}

QString NovelManager::computeFileHash(const QString &absPath)
{
    QFile file(absPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        return QString();
    }
    return QString::fromLatin1(hash.result().toHex());
}

QString NovelManager::importBookToLibrary(const QString &sourcePath)
{
    const QString absSrc = resolveBookPath(sourcePath);
    if (absSrc.isEmpty() || !QFile::exists(absSrc)) {
        return QString();
    }

    // 已经在内置书库 books/ 中，直接返回相对路径
    if (isInsideBooksDir(absSrc)) {
        return toStoredPath(absSrc);
    }

    const QString booksDir = booksDirPath();
    QDir().mkpath(booksDir);

    const QString srcHash = computeFileHash(absSrc);
    const qint64 srcSize = QFileInfo(absSrc).size();

    // 1. 内容去重：若书库中已有相同内容的副本，直接复用（移动原文件后重新拖入也能无缝继承旧进度）
    if (!srcHash.isEmpty()) {
        const QFileInfoList existingFiles = QDir(booksDir).entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
        for (const QFileInfo &efi : existingFiles) {
            if (efi.size() == srcSize && computeFileHash(efi.absoluteFilePath()) == srcHash) {
                return toStoredPath(efi.absoluteFilePath());
            }
        }
    }

    // 2. 生成目标文件名，若同名但内容不同则追加 8 位哈希后缀避免覆盖
    QFileInfo srcInfo(absSrc);
    QString baseName = srcInfo.completeBaseName();
    QString ext = srcInfo.suffix();
    QString candidateName = srcInfo.fileName();
    QString targetAbs = QDir(booksDir).filePath(candidateName);

    if (QFile::exists(targetAbs)) {
        QString shortHash = srcHash.isEmpty() ? QString::number(QDateTime::currentMSecsSinceEpoch()) : srcHash.left(8);
        candidateName = ext.isEmpty()
                            ? QStringLiteral("%1_%2").arg(baseName, shortHash)
                            : QStringLiteral("%1_%2.%3").arg(baseName, shortHash, ext);
        targetAbs = QDir(booksDir).filePath(candidateName);
    }

    if (QFile::copy(absSrc, targetAbs)) {
        return toStoredPath(targetAbs);
    }

    // 极端情况拷贝失败时回退使用原路径
    return absSrc;
}

bool NovelManager::removeBook(const QString &storedPath)
{
    int idx = -1;
    for (int i = 0; i < m_recentBooks.size(); ++i) {
        if (m_recentBooks[i].filePath == storedPath) {
            idx = i;
            break;
        }
    }
    if (idx < 0) {
        return false;
    }

    const QString removedPath = m_recentBooks[idx].filePath;
    m_recentBooks.removeAt(idx);

    if (m_currentFilePath == removedPath) {
        m_currentFilePath = m_recentBooks.isEmpty() ? QString() : m_recentBooks.first().filePath;
    }

    // 仅清理内置书库 books/ 下的副本，绝不删除用户原磁盘位置的文件
    const QString absPath = resolveBookPath(removedPath);
    if (isInsideBooksDir(absPath) && QFile::exists(absPath)) {
        QFile::remove(absPath);
    }

    m_dirty = true;
    savePreferences();
    flushBookshelf(true);
    emit bookRemoved(removedPath);
    return true;
}

QString NovelManager::escapeCsvField(const QString &field)
{
    QString escaped = field;
    escaped.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}

QStringList NovelManager::parseCsvLine(const QString &line)
{
    QStringList fields;
    QString current;
    bool inQuotes = false;

    for (int i = 0; i < line.size(); ++i) {
        QChar ch = line.at(i);
        if (inQuotes) {
            if (ch == QLatin1Char('"')) {
                if (i + 1 < line.size() && line.at(i + 1) == QLatin1Char('"')) {
                    current.append(QLatin1Char('"'));
                    ++i;
                } else {
                    inQuotes = false;
                }
            } else {
                current.append(ch);
            }
        } else {
            if (ch == QLatin1Char('"')) {
                inQuotes = true;
            } else if (ch == QLatin1Char(',')) {
                fields.append(current);
                current.clear();
            } else {
                current.append(ch);
            }
        }
    }
    fields.append(current);
    return fields;
}

QString NovelManager::detectEncoding(const QString &filePath)
{
    const QString absPath = resolveBookPath(filePath);
    QFileInfo fi(absPath);
    QString ext = fi.suffix().toLower();
    if (ext == QStringLiteral("epub")) {
        return QStringLiteral("EPUB (UTF-8)");
    } else if (ext == QStringLiteral("mobi") || ext == QStringLiteral("azw") || ext == QStringLiteral("azw3")) {
        return QStringLiteral("MOBI (UTF-8 / Latin1)");
    }

    QFile file(absPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QStringLiteral("UTF-8");
    }

    QByteArray data = file.read(8192);
    file.close();

    QTextCodec::ConverterState state;
    QTextCodec *utf8Codec = QTextCodec::codecForName("UTF-8");
    if (utf8Codec) {
        utf8Codec->toUnicode(data.constData(), data.size(), &state);
        if (state.invalidChars == 0) {
            return QStringLiteral("UTF-8");
        }
    }

    return QStringLiteral("GB18030");
}

QString NovelManager::formatDisplayName(const QString &filePath)
{
    QString ext = QFileInfo(resolveBookPath(filePath)).suffix().toUpper();
    return ext.isEmpty() ? QStringLiteral("TXT") : ext;
}

QString NovelManager::readNovelText(const QString &filePath, const QString &codecName)
{
    const QString absPath = resolveBookPath(filePath);
    EbookContent content = EbookParser::parseFile(absPath, codecName);
    if (content.success) {
        return content.text;
    }
    return content.errorMessage.isEmpty() ? QStringLiteral("（文件内容为空或解析失败）") : content.errorMessage;
}

QString NovelManager::previewText(const QString &filePath, const QString &codecName, int maxChars)
{
    const QString absPath = resolveBookPath(filePath);
    return EbookParser::previewFile(absPath, maxChars, codecName);
}

QList<BookItem> NovelManager::recentBooks() const
{
    return m_recentBooks;
}

BookItem NovelManager::findBook(const QString &storedOrAbsPath) const
{
    const QString resolvedTarget = resolveBookPath(storedOrAbsPath);
    for (const auto &book : m_recentBooks) {
        if (book.filePath == storedOrAbsPath || resolveBookPath(book.filePath) == resolvedTarget) {
            return book;
        }
    }
    return BookItem();
}

void NovelManager::addOrUpdateBook(const QString &storedPath, int scrollPos, int totalScroll)
{
    if (storedPath.isEmpty()) return;

    m_currentFilePath = storedPath;
    QFileInfo fi(resolveBookPath(storedPath));

    int existingIdx = -1;
    for (int i = 0; i < m_recentBooks.size(); ++i) {
        if (m_recentBooks[i].filePath == storedPath) {
            existingIdx = i;
            break;
        }
    }

    BookItem item;
    item.filePath = storedPath;
    item.fileName = fi.fileName();
    item.scrollPosition = qMax(0, scrollPos);
    item.totalScroll = qMax(1, totalScroll);
    item.progressPct = item.computedProgressPct();
    item.lastReadTime = QDateTime::currentDateTime();

    if (existingIdx >= 0) {
        if (scrollPos <= 0 && m_recentBooks[existingIdx].scrollPosition > 0 && totalScroll <= 1) {
            item.scrollPosition = m_recentBooks[existingIdx].scrollPosition;
            item.totalScroll = m_recentBooks[existingIdx].totalScroll;
            item.progressPct = m_recentBooks[existingIdx].progressPct;
        }
        m_recentBooks.removeAt(existingIdx);
    }

    m_recentBooks.prepend(item);

    while (m_recentBooks.size() > 20) {
        m_recentBooks.removeLast();
    }

    m_dirty = true;
    savePreferences();
    flushBookshelf(true);
}

void NovelManager::updateProgress(const QString &storedPath, int scrollPos, int totalScroll)
{
    // 仅更新内存并标记脏位，由 5 分钟兜底定时或主动退出/切书/跳转时落盘 CSV
    for (int i = 0; i < m_recentBooks.size(); ++i) {
        if (m_recentBooks[i].filePath == storedPath) {
            int safePos = qMax(0, scrollPos);
            int safeTotal = qMax(1, totalScroll);
            if (m_recentBooks[i].scrollPosition != safePos || m_recentBooks[i].totalScroll != safeTotal) {
                m_recentBooks[i].scrollPosition = safePos;
                m_recentBooks[i].totalScroll = safeTotal;
                m_recentBooks[i].progressPct = m_recentBooks[i].computedProgressPct();
                m_recentBooks[i].lastReadTime = QDateTime::currentDateTime();
                m_dirty = true;
            }
            break;
        }
    }
}

BookItem NovelManager::currentBook() const
{
    if (!m_currentFilePath.isEmpty()) {
        for (const auto &book : m_recentBooks) {
            if (book.filePath == m_currentFilePath) {
                return book;
            }
        }
    }
    if (!m_recentBooks.isEmpty()) {
        return m_recentBooks.first();
    }
    return BookItem();
}

QString NovelManager::textColor() const
{
    return m_textColor;
}

void NovelManager::setTextColor(const QString &colorHex)
{
    if (m_textColor != colorHex) {
        m_textColor = colorHex;
        savePreferences();
        emit styleChanged();
    }
}

int NovelManager::fontSize() const
{
    return m_fontSize;
}

void NovelManager::setFontSize(int pt)
{
    int clamped = qBound(4, pt, 20);
    if (m_fontSize != clamped) {
        m_fontSize = clamped;
        savePreferences();
        emit styleChanged();
    }
}

bool NovelManager::isNovelVisible() const
{
    return m_novelVisible;
}

void NovelManager::setNovelVisible(bool visible)
{
    if (m_novelVisible != visible) {
        m_novelVisible = visible;
        savePreferences();
        emit styleChanged();
    }
}

bool NovelManager::loadBookshelfCsv()
{
    const QString csvPath = bookshelfCsvPath();
    QFile file(csvPath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QByteArray raw = file.readAll();
    file.close();

    if (raw.startsWith("\xEF\xBB\xBF")) {
        raw.remove(0, 3);
    }

    const QString content = QString::fromUtf8(raw);
    const QStringList lines = content.split(QRegExp(QStringLiteral("\r?\n")), Qt::SkipEmptyParts);

    m_recentBooks.clear();
    for (int i = 0; i < lines.size(); ++i) {
        const QString &line = lines.at(i).trimmed();
        if (line.isEmpty()) continue;
        if (i == 0 && line.startsWith(QStringLiteral("file_path"), Qt::CaseInsensitive)) {
            continue;
        }

        const QStringList cols = parseCsvLine(line);
        if (cols.size() < 6) continue;

        BookItem item;
        item.filePath = cols.at(0);
        item.fileName = cols.at(1);
        item.progressPct = qBound(0.0, cols.at(2).toDouble(), 100.0);
        item.scrollPosition = qMax(0, cols.at(3).toInt());
        item.totalScroll = qMax(1, cols.at(4).toInt());
        item.lastReadTime = QDateTime::fromString(cols.at(5), Qt::ISODate);

        const QString absPath = resolveBookPath(item.filePath);
        if (!absPath.isEmpty() && QFile::exists(absPath)) {
            m_recentBooks.append(item);
        }
    }

    if (!m_recentBooks.isEmpty()) {
        m_currentFilePath = m_recentBooks.first().filePath;
    }
    return true;
}

void NovelManager::migrateExternalBooksToLibrary()
{
    bool changed = false;
    for (int i = 0; i < m_recentBooks.size(); ++i) {
        const QString absPath = resolveBookPath(m_recentBooks[i].filePath);
        if (!absPath.isEmpty() && QFile::exists(absPath) && !isInsideBooksDir(absPath)) {
            const QString imported = importBookToLibrary(absPath);
            if (!imported.isEmpty() && imported != m_recentBooks[i].filePath) {
                if (m_currentFilePath == m_recentBooks[i].filePath) {
                    m_currentFilePath = imported;
                }
                m_recentBooks[i].filePath = imported;
                changed = true;
            }
        }
    }

    if (changed) {
        m_dirty = true;
        savePreferences();
        flushBookshelf(true);
    }
}

void NovelManager::flushBookshelf(bool force)
{
    if (!m_dirty && !force) {
        return;
    }

    const QString csvPath = bookshelfCsvPath();
    const QString tmpPath = csvPath + QStringLiteral(".tmp");

    QFile tmpFile(tmpPath);
    if (!tmpFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }

    tmpFile.write("\xEF\xBB\xBF");

    QByteArray out;
    out.append("file_path,file_name,progress_pct,scroll_pos,total_scroll,last_read_time\r\n");

    for (const auto &b : m_recentBooks) {
        QStringList row;
        row << escapeCsvField(b.filePath)
            << escapeCsvField(b.fileName)
            << QString::number(b.computedProgressPct(), 'f', 2)
            << QString::number(b.scrollPosition)
            << QString::number(b.totalScroll)
            << escapeCsvField(b.lastReadTime.toString(Qt::ISODate));
        out.append(row.join(QLatin1Char(',')).toUtf8());
        out.append("\r\n");
    }

    tmpFile.write(out);
    tmpFile.flush();
    tmpFile.close();

    if (QFile::exists(csvPath)) {
        QFile::remove(csvPath);
    }
    if (QFile::rename(tmpPath, csvPath)) {
        m_dirty = false;
    }
}

void NovelManager::migrateLegacyBookshelfFromSettings(QSettings &settings)
{
    int count = settings.beginReadArray(QStringLiteral("recentBooks"));
    m_recentBooks.clear();
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        BookItem item;
        item.filePath = settings.value(QStringLiteral("filePath")).toString();
        item.fileName = settings.value(QStringLiteral("fileName")).toString();
        item.scrollPosition = settings.value(QStringLiteral("scrollPosition"), 0).toInt();
        item.totalScroll = qMax(1, settings.value(QStringLiteral("totalScroll"), 1).toInt());
        item.progressPct = item.computedProgressPct();
        item.lastReadTime = settings.value(QStringLiteral("lastReadTime")).toDateTime();
        if (!item.lastReadTime.isValid()) {
            item.lastReadTime = QDateTime::currentDateTime();
        }
        if (QFile::exists(resolveBookPath(item.filePath))) {
            m_recentBooks.append(item);
        }
    }
    settings.endArray();

    if (!m_recentBooks.isEmpty()) {
        if (m_currentFilePath.isEmpty()) {
            m_currentFilePath = m_recentBooks.first().filePath;
        }
        flushBookshelf(true);
    }
}

void NovelManager::loadSettings()
{
    QSettings settings(QStringLiteral("QtCat"), QStringLiteral("QtCatPet"));
    m_textColor = settings.value(QStringLiteral("textColor"), QStringLiteral("#D0D0D0")).toString();

    const int fontVersion = settings.value(QStringLiteral("fontSizeSchemaVersion"), 1).toInt();
    if (fontVersion < 4) {
        m_fontSize = 8;
        settings.setValue(QStringLiteral("fontSize"), m_fontSize);
        settings.setValue(QStringLiteral("fontSizeSchemaVersion"), 4);
    } else {
        m_fontSize = qBound(4, settings.value(QStringLiteral("fontSize"), 8).toInt(), 20);
    }

    m_novelVisible = settings.value(QStringLiteral("novelVisible"), true).toBool();
    m_currentFilePath = settings.value(QStringLiteral("currentFilePath"), QString()).toString();

    if (!loadBookshelfCsv()) {
        migrateLegacyBookshelfFromSettings(settings);
    }

    // 自动把旧记录中的外部路径书籍迁入内置书库 dist/books/
    migrateExternalBooksToLibrary();
}

void NovelManager::savePreferences()
{
    QSettings settings(QStringLiteral("QtCat"), QStringLiteral("QtCatPet"));
    settings.setValue(QStringLiteral("textColor"), m_textColor);
    settings.setValue(QStringLiteral("fontSize"), m_fontSize);
    settings.setValue(QStringLiteral("novelVisible"), m_novelVisible);
    settings.setValue(QStringLiteral("currentFilePath"), m_currentFilePath);
}
