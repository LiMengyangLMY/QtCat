#ifndef EBOOKPARSER_H
#define EBOOKPARSER_H

#include <QString>
#include <QStringList>

struct EbookContent
{
    QString title;
    QString text;
    QString formatName; // "TXT", "EPUB", "MOBI", "AZW3"
    QString errorMessage;
    bool success = false;
};

class EbookParser
{
public:
    static EbookContent parseFile(const QString &filePath, const QString &codecOverride = QString());
    static QString previewFile(const QString &filePath, int maxChars = 200, const QString &codecOverride = QString());
    static QString supportedFilter();
    static QStringList supportedExtensions();

private:
    static EbookContent parseTxt(const QString &filePath, const QString &codecOverride);
    static EbookContent parseEpub(const QString &filePath);
    static EbookContent parseMobi(const QString &filePath);

    static QByteArray decompressPalmDoc(const QByteArray &data);
    static QString htmlToPlainText(const QString &html);
};

#endif // EBOOKPARSER_H
