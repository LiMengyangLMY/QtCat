#include "ebookparser.h"

#include <QtGui/private/qzipreader_p.h>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QTextCodec>
#include <QTextDocument>
#include <QXmlStreamReader>
#include <QtEndian>

QStringList EbookParser::supportedExtensions()
{
    return {
        QStringLiteral("txt"),
        QStringLiteral("epub"),
        QStringLiteral("mobi"),
        QStringLiteral("azw"),
        QStringLiteral("azw3")
    };
}

QString EbookParser::supportedFilter()
{
    return QStringLiteral(
        "常用电子书文件 (*.txt *.epub *.mobi *.azw *.azw3);;"
        "文本小说 (*.txt);;"
        "EPUB 电子书 (*.epub);;"
        "Kindle 电子书 (*.mobi *.azw *.azw3);;"
        "所有文件 (*.*)"
    );
}

QString EbookParser::htmlToPlainText(const QString &html)
{
    if (html.isEmpty()) {
        return QString();
    }

    QTextDocument doc;
    doc.setHtml(html);
    QString plain = doc.toPlainText();

    // 规整多余连续空行
    static QRegularExpression multiLines(QStringLiteral("\n{3,}"));
    plain.replace(multiLines, QStringLiteral("\n\n"));
    return plain.trimmed();
}

EbookContent EbookParser::parseFile(const QString &filePath, const QString &codecOverride)
{
    QFileInfo fi(filePath);
    QString ext = fi.suffix().toLower();

    if (ext == QStringLiteral("epub")) {
        return parseEpub(filePath);
    } else if (ext == QStringLiteral("mobi") || ext == QStringLiteral("azw") || ext == QStringLiteral("azw3")) {
        return parseMobi(filePath);
    } else {
        return parseTxt(filePath, codecOverride);
    }
}

QString EbookParser::previewFile(const QString &filePath, int maxChars, const QString &codecOverride)
{
    EbookContent content = parseFile(filePath, codecOverride);
    if (!content.success) {
        return content.errorMessage.isEmpty() ? QStringLiteral("（解析文件失败）") : content.errorMessage;
    }

    QString text = content.text.trimmed();
    if (text.length() > maxChars) {
        text = text.left(maxChars) + QStringLiteral("...");
    }
    return text;
}

EbookContent EbookParser::parseTxt(const QString &filePath, const QString &codecOverride)
{
    EbookContent result;
    result.formatName = QStringLiteral("TXT");
    result.title = QFileInfo(filePath).completeBaseName();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        result.errorMessage = QStringLiteral("无法打开文件: %1").arg(file.errorString());
        return result;
    }

    QByteArray data = file.readAll();
    file.close();

    if (data.isEmpty()) {
        result.errorMessage = QStringLiteral("文件内容为空");
        return result;
    }

    // 手动指定编码优先
    if (!codecOverride.isEmpty() && codecOverride != QStringLiteral("auto")) {
        QTextCodec *codec = QTextCodec::codecForName(codecOverride.toUtf8());
        if (codec) {
            result.text = codec->toUnicode(data);
            result.success = true;
            return result;
        }
    }

    // 智能嗅探：先尝试 UTF-8
    QTextCodec::ConverterState state;
    QTextCodec *utf8Codec = QTextCodec::codecForName("UTF-8");
    if (utf8Codec) {
        QString text = utf8Codec->toUnicode(data.constData(), data.size(), &state);
        if (state.invalidChars == 0) {
            result.text = text;
            result.success = true;
            return result;
        }
    }

    // 回退到 GB18030 / GBK
    QTextCodec *gbkCodec = QTextCodec::codecForName("GB18030");
    if (!gbkCodec) {
        gbkCodec = QTextCodec::codecForName("GBK");
    }
    if (gbkCodec) {
        result.text = gbkCodec->toUnicode(data);
        result.success = true;
        return result;
    }

    result.text = QString::fromUtf8(data);
    result.success = true;
    return result;
}

EbookContent EbookParser::parseEpub(const QString &filePath)
{
    EbookContent result;
    result.formatName = QStringLiteral("EPUB");
    result.title = QFileInfo(filePath).completeBaseName();

    QZipReader zip(filePath);
    if (!zip.isReadable()) {
        result.errorMessage = QStringLiteral("无法作为 ZIP 解压打开 EPUB 文件");
        return result;
    }

    // 1. 读取 META-INF/container.xml 找到 OPF 文件路径
    QString opfPath;
    QByteArray containerData = zip.fileData(QStringLiteral("META-INF/container.xml"));
    if (!containerData.isEmpty()) {
        QXmlStreamReader xml(containerData);
        while (!xml.atEnd() && !xml.hasError()) {
            xml.readNext();
            if (xml.isStartElement() && xml.name() == QStringLiteral("rootfile")) {
                opfPath = xml.attributes().value(QStringLiteral("full-path")).toString();
                break;
            }
        }
    }

    // 回退查找任意 .opf 文件
    if (opfPath.isEmpty()) {
        const auto files = zip.fileInfoList();
        for (const auto &info : files) {
            if (info.filePath.endsWith(QStringLiteral(".opf"), Qt::CaseInsensitive)) {
                opfPath = info.filePath;
                break;
            }
        }
    }

    if (opfPath.isEmpty()) {
        result.errorMessage = QStringLiteral("EPUB 文件损坏或缺少 package.opf 描述文件");
        return result;
    }

    // 2. 解析 OPF
    QByteArray opfData = zip.fileData(opfPath);
    if (opfData.isEmpty()) {
        result.errorMessage = QStringLiteral("无法读取 OPF 配置文件");
        return result;
    }

    QString opfDir;
    int lastSlash = opfPath.lastIndexOf(QLatin1Char('/'));
    if (lastSlash >= 0) {
        opfDir = opfPath.left(lastSlash);
    }

    QHash<QString, QString> manifestItems; // id -> href
    QStringList spineItemRefs;
    QString bookTitle;

    QXmlStreamReader xml(opfData);
    bool inSpine = false;
    bool inManifest = false;

    while (!xml.atEnd() && !xml.hasError()) {
        xml.readNext();
        if (xml.isStartElement()) {
            QString tag = xml.name().toString().toLower();
            if (tag == QStringLiteral("title")) {
                if (bookTitle.isEmpty()) {
                    bookTitle = xml.readElementText().trimmed();
                }
            } else if (tag == QStringLiteral("manifest")) {
                inManifest = true;
            } else if (tag == QStringLiteral("spine")) {
                inSpine = true;
            } else if (inManifest && tag == QStringLiteral("item")) {
                QString id = xml.attributes().value(QStringLiteral("id")).toString();
                QString href = xml.attributes().value(QStringLiteral("href")).toString();
                if (!id.isEmpty() && !href.isEmpty()) {
                    manifestItems.insert(id, href);
                }
            } else if (inSpine && tag == QStringLiteral("itemref")) {
                QString idref = xml.attributes().value(QStringLiteral("idref")).toString();
                if (!idref.isEmpty()) {
                    spineItemRefs.append(idref);
                }
            }
        } else if (xml.isEndElement()) {
            QString tag = xml.name().toString().toLower();
            if (tag == QStringLiteral("manifest")) inManifest = false;
            else if (tag == QStringLiteral("spine")) inSpine = false;
        }
    }

    if (!bookTitle.isEmpty()) {
        result.title = bookTitle;
    }

    // 3. 按阅读顺序拼接章节正文
    QString fullContent;
    for (const QString &idref : spineItemRefs) {
        QString href = manifestItems.value(idref);
        if (href.isEmpty()) continue;

        // 处理相对路径
        QString fullPath = opfDir.isEmpty() ? href : (opfDir + QLatin1Char('/') + href);
        // 清理 URL 锚点
        int hashIdx = fullPath.indexOf(QLatin1Char('#'));
        if (hashIdx >= 0) {
            fullPath = fullPath.left(hashIdx);
        }

        QByteArray htmlBytes = zip.fileData(fullPath);
        if (htmlBytes.isEmpty()) continue;

        QString chapterHtml = QString::fromUtf8(htmlBytes);
        QString chapterText = htmlToPlainText(chapterHtml);

        if (!chapterText.isEmpty()) {
            fullContent += chapterText + QStringLiteral("\n\n");
        }
    }

    if (fullContent.isEmpty()) {
        result.errorMessage = QStringLiteral("未能从该 EPUB 中提取到有效章节文本");
        return result;
    }

    result.text = fullContent.trimmed();
    result.success = true;
    return result;
}

QByteArray EbookParser::decompressPalmDoc(const QByteArray &data)
{
    QByteArray out;
    const unsigned char *p = reinterpret_cast<const unsigned char *>(data.constData());
    int len = data.size();
    int i = 0;

    out.reserve(len * 2);

    while (i < len) {
        unsigned char c = p[i++];
        if (c >= 1 && c <= 8) {
            // 复制接下来的 c 个原样字节
            while (c-- > 0 && i < len) {
                out.append(static_cast<char>(p[i++]));
            }
        } else if (c <= 0x7F) {
            // 普通原样字符
            out.append(static_cast<char>(c));
        } else if (c >= 0xC0) {
            // 空格 + (c ^ 0x80)
            out.append(' ');
            out.append(static_cast<char>(c ^ 0x80));
        } else {
            // 0x80 .. 0xBF: 距离与长度回溯
            if (i >= len) break;
            unsigned char c2 = p[i++];
            int distance = (((c & 0x3F) << 5) | (c2 >> 3));
            int length = (c2 & 0x07) + 3;
            int start = out.size() - distance;
            if (start >= 0) {
                for (int k = 0; k < length; ++k) {
                    out.append(out.at(start + k));
                }
            }
        }
    }

    return out;
}

EbookContent EbookParser::parseMobi(const QString &filePath)
{
    EbookContent result;
    result.formatName = QStringLiteral("MOBI");
    result.title = QFileInfo(filePath).completeBaseName();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        result.errorMessage = QStringLiteral("无法打开文件: %1").arg(file.errorString());
        return result;
    }

    QByteArray data = file.readAll();
    file.close();

    if (data.size() < 78) {
        result.errorMessage = QStringLiteral("文件过小，不是有效的 MOBI/PalmDOC 格式");
        return result;
    }

    // 1. 读取 Palm 数据库头
    quint16 numRecords = qFromBigEndian<quint16>(reinterpret_cast<const uchar *>(data.constData() + 76));
    if (numRecords < 1 || data.size() < 78 + numRecords * 8) {
        result.errorMessage = QStringLiteral("无效的 MOBI Record 记录索引");
        return result;
    }

    QVector<quint32> recordOffsets;
    recordOffsets.reserve(numRecords);
    for (int i = 0; i < numRecords; ++i) {
        quint32 offset = qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(data.constData() + 78 + i * 8));
        recordOffsets.append(offset);
    }

    // 2. 读取 Record 0 (PalmDOC Header & MOBI Header)
    quint32 rec0Offset = recordOffsets.at(0);
    quint32 rec0End = (numRecords > 1) ? recordOffsets.at(1) : static_cast<quint32>(data.size());
    if (rec0Offset >= static_cast<quint32>(data.size()) || rec0End <= rec0Offset || (rec0End - rec0Offset) < 16) {
        result.errorMessage = QStringLiteral("损坏的 MOBI 头信息 (Record 0)");
        return result;
    }

    const char *rec0 = data.constData() + rec0Offset;
    quint16 compression = qFromBigEndian<quint16>(reinterpret_cast<const uchar *>(rec0 + 0));
    quint16 textRecordCount = qFromBigEndian<quint16>(reinterpret_cast<const uchar *>(rec0 + 8));
    quint32 encryptionType = qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(rec0 + 12));

    if (encryptionType != 0) {
        result.errorMessage = QStringLiteral("该 Kindle 电子书含有 DRM 加密保护，无法直接解密");
        return result;
    }

    if (compression != 1 && compression != 2) {
        result.errorMessage = QStringLiteral("该 MOBI 使用了暂不支持的高级压缩算法 (code %1)").arg(compression);
        return result;
    }

    // 读取 MOBI 编码信息与尾部额外字节标志 (extra_flags)
    bool isUtf8 = true;
    quint16 extraFlags = 0;
    const quint32 rec0Len = rec0End - rec0Offset;
    if (rec0Len >= 32 && memcmp(rec0 + 16, "MOBI", 4) == 0) {
        quint32 mobiHdrLen = qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(rec0 + 20));
        quint32 encodingVal = qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(rec0 + 28));
        if (encodingVal == 1252) {
            isUtf8 = false;
        }
        // extra_flags 位于 Record 0 偏移 242 处（当 MOBI 头长度 >= 228 且 Record 0 足够长时有效）
        if (mobiHdrLen >= 228 && rec0Len >= 244) {
            extraFlags = qFromBigEndian<quint16>(reinterpret_cast<const uchar *>(rec0 + 242));
        }
    }

    // 剥离 MOBI/AZW3 每个文本 Record 末尾的附加校验/重叠字节，防止解压产生 \0 截断正文
    auto stripTrailingData = [](QByteArray chunk, quint16 flags) -> QByteArray {
        if (flags == 0 || chunk.isEmpty()) {
            return chunk;
        }
        for (int bit = 15; bit >= 1; --bit) {
            if (flags & (1u << bit)) {
                int val = 0;
                int shift = 0;
                int idx = chunk.size() - 1;
                while (idx >= 0) {
                    unsigned char byte = static_cast<unsigned char>(chunk.at(idx));
                    val |= (byte & 0x7F) << shift;
                    shift += 7;
                    --idx;
                    if (byte & 0x80) {
                        break;
                    }
                }
                if (val > 0 && val <= chunk.size()) {
                    chunk.chop(val);
                }
            }
        }
        if ((flags & 1u) && !chunk.isEmpty()) {
            int sz = (static_cast<unsigned char>(chunk.at(chunk.size() - 1)) & 3) + 1;
            if (sz > 0 && sz <= chunk.size()) {
                chunk.chop(sz);
            }
        }
        return chunk;
    };

    // 3. 读取并解压文本 Records (从 Record 1 到 Record textRecordCount)
    int maxTextRecords = qMin(static_cast<int>(textRecordCount), numRecords - 1);
    QByteArray rawTextBytes;

    for (int i = 1; i <= maxTextRecords; ++i) {
        quint32 start = recordOffsets.at(i);
        quint32 end = (i + 1 < numRecords) ? recordOffsets.at(i + 1) : static_cast<quint32>(data.size());
        if (start >= static_cast<quint32>(data.size()) || end <= start) continue;

        QByteArray chunk = stripTrailingData(data.mid(start, end - start), extraFlags);
        if (compression == 1) {
            rawTextBytes.append(chunk);
        } else if (compression == 2) {
            rawTextBytes.append(decompressPalmDoc(chunk));
        }
    }

    // 安全兜底：移除可能残留的 NUL 字节，避免 QString/QTextDocument 提前截断
    rawTextBytes.replace('\0', QByteArray());

    if (rawTextBytes.isEmpty()) {
        result.errorMessage = QStringLiteral("未能从 MOBI 文件中解压出正文内容");
        return result;
    }

    // 4. 解码文本并滤除 HTML 标签
    QString htmlContent;
    if (isUtf8) {
        htmlContent = QString::fromUtf8(rawTextBytes);
    } else {
        htmlContent = QString::fromLatin1(rawTextBytes);
    }

    QString plain = htmlToPlainText(htmlContent);
    if (plain.isEmpty()) {
        result.errorMessage = QStringLiteral("MOBI 解压文本为空");
        return result;
    }

    result.text = plain;
    result.success = true;
    return result;
}
