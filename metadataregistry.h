#ifndef METADATAREGISTRY_H
#define METADATAREGISTRY_H

#include <QList>
#include <QString>
#include <QStringList>

struct MetadataObservation {
    QString group;
    QString tag;
    QString value;
    QString sourceFile;

    QString key() const;
};

struct MetadataRule {
    QString id;
    QString group;
    QString tag;
    QString label;
    QString expected;

    QString key() const;
};

struct MetadataRuleAnalysis {
    QString group;
    QString tag;
    QStringList values;
    int presence = 0;
    int sourceCount = 0;
};

struct MetadataRuleDecision {
    MetadataRule rule;
    QString observedGroup;
    QString observedTag;
    QString observedValue;
    bool itemEquivalence = false;
    bool valueEquivalence = false;
};

class ExifToolService
{
public:
    static constexpr const char *Profile = "exiftool_g1_short_v1";

    static QString executablePath();
    static QStringList candidateExecutablePaths();
    static QString diagnostics();
    static bool isAvailable(QString *errorMessage = nullptr);
    static QString version(QString *errorMessage = nullptr);
    static bool extract(const QStringList &files,
                        QList<MetadataObservation> *observations,
                        QString *errorMessage = nullptr);
};

namespace MetadataRegistry {

QList<MetadataRuleAnalysis> analyze(
    const QStringList &sourceFiles,
    const QList<MetadataObservation> &observations);

bool writeDraft(const QString &filePath,
                const QString &projectRoot,
                const QStringList &sourceFiles,
                const QList<MetadataRule> &rules,
                const QString &exifToolVersion,
                QString *errorMessage = nullptr);

bool readRules(const QString &filePath,
               QList<MetadataRule> *rules,
               QString *errorMessage = nullptr);

QList<MetadataObservation> candidates(
    const MetadataRule &rule,
    const QList<MetadataObservation> &observations,
    int maximum = 5);

QString sha256(const QString &filePath, QString *errorMessage = nullptr);

bool writeReview(const QString &filePath,
                 const QString &projectRoot,
                 const QString &imagePath,
                 const QString &registryPath,
                 const QString &exifToolVersion,
                 const QList<MetadataRuleDecision> &decisions,
                 bool accepted,
                 QString *errorMessage = nullptr);

QString snakeCase(const QString &text);

}

#endif
