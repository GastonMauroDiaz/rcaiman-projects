#include "metadataregistry.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDateTime>
#include <QMap>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QTextStream>
#include <QTemporaryFile>
#include <QVector>

#include <yaml-cpp/yaml.h>

#include <algorithm>

QString MetadataObservation::key() const
{
    return group + ":" + tag;
}

QString MetadataRule::key() const
{
    return group + ":" + tag;
}

QString ExifToolService::executablePath()
{
    const QStringList candidates = candidateExecutablePaths();
    for (const QString &path : candidates) {
        if (QFileInfo(path).isExecutable())
            return path;
    }
    return {};
}

QStringList ExifToolService::candidateExecutablePaths()
{
    const QDir appDir(QCoreApplication::applicationDirPath());
#ifdef Q_OS_WIN
    const QString executable = "exiftool.exe";
#else
    const QString executable = "exiftool";
#endif
    const QStringList relativeCandidates = {
        "tools/exiftool/" + executable,
        "exiftool/" + executable,
#ifdef Q_OS_MACOS
        "../Resources/exiftool/" + executable,
#endif
    };
    QStringList candidates;
    candidates.reserve(relativeCandidates.size());
    for (const QString &relative : relativeCandidates) {
        candidates.append(
            QFileInfo(appDir.filePath(relative)).absoluteFilePath());
    }
    candidates.removeDuplicates();
    return candidates;
}

QString ExifToolService::diagnostics()
{
    QStringList lines;
    const QString path = executablePath();
    lines << QStringLiteral("ExifTool profile: %1")
                 .arg(QString::fromLatin1(Profile));
    lines << QStringLiteral("Resolved path: %1")
                 .arg(path.isEmpty() ? QStringLiteral("(not found)") : path);
    QString versionError;
    const QString reportedVersion = version(&versionError);
    lines << QStringLiteral("Version: %1")
                 .arg(reportedVersion.isEmpty()
                          ? QStringLiteral("(unavailable)")
                          : reportedVersion);
    if (!versionError.isEmpty())
        lines << QStringLiteral("Version error: %1").arg(versionError);
    lines << QStringLiteral("Candidate paths:");
    for (const QString &candidate : candidateExecutablePaths())
        lines << QStringLiteral("- %1").arg(candidate);
    return lines.join('\n');
}

bool ExifToolService::isAvailable(QString *errorMessage)
{
    if (!executablePath().isEmpty())
        return true;
    if (errorMessage) {
        *errorMessage =
            "The bundled ExifTool executable was not found in the application "
            "directory. Reinstall rcaiman-projects.\n\n"
            "Candidate paths:\n"
            + candidateExecutablePaths().join('\n');
    }
    return false;
}

QString ExifToolService::version(QString *errorMessage)
{
    const QString path = executablePath();
    if (path.isEmpty()) {
        isAvailable(errorMessage);
        return {};
    }
    QProcess process;
    process.start(path, {"-ver"});
    if (!process.waitForFinished(10000)
        || process.exitStatus() != QProcess::NormalExit
        || process.exitCode() != 0) {
        if (errorMessage)
            *errorMessage = "ExifTool could not report its version.";
        return {};
    }
    return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
}

bool ExifToolService::extract(const QStringList &files,
                              QList<MetadataObservation> *observations,
                              QString *errorMessage)
{
    if (!observations || files.isEmpty())
        return false;
    observations->clear();

    const QString path = executablePath();
    if (path.isEmpty())
        return isAvailable(errorMessage);

    // Family 4 is included only to keep duplicate JSON keys distinct.
    // The stored contract deliberately retains Family 1 as its group.
    QStringList arguments = {"-j", "-G4:1", "-s", "-a", "-lang", "en"};
    QTemporaryFile argumentFile;
    if (files.size() > 40) {
        if (!argumentFile.open()) {
            if (errorMessage)
                *errorMessage = "Could not create the ExifTool argument file.";
            return false;
        }
        for (const QString &file : files) {
            argumentFile.write(
                QDir::toNativeSeparators(file).toUtf8());
            argumentFile.write("\n");
        }
        argumentFile.flush();
        argumentFile.close();
        arguments.append({
            "-charset", "filename=UTF8", "-@", argumentFile.fileName()
        });
    } else {
        arguments.append(files);
    }
    QProcess process;
    process.start(path, arguments);
    if (!process.waitForStarted(10000) || !process.waitForFinished(120000)) {
        if (errorMessage)
            *errorMessage = "ExifTool did not finish metadata extraction.";
        return false;
    }

    const QByteArray output = process.readAllStandardOutput();
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(output, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
        if (errorMessage) {
            *errorMessage = QString("Invalid ExifTool JSON output: %1")
                                .arg(parseError.errorString());
        }
        return false;
    }

    const QJsonArray images = document.array();
    for (const QJsonValue &imageValue : images) {
        const QJsonObject image = imageValue.toObject();
        const QString source = image.value("SourceFile").toString();
        for (auto it = image.constBegin(); it != image.constEnd(); ++it) {
            if (it.key() == "SourceFile")
                continue;
            const QStringList keyParts = it.key().split(':');
            if (keyParts.size() < 2)
                continue;

            QString value;
            if (it.value().isString())
                value = it.value().toString();
            else if (it.value().isDouble())
                value = QString::number(it.value().toDouble(), 'g', 16);
            else if (it.value().isBool())
                value = it.value().toBool() ? "true" : "false";
            else
                value = QString::fromUtf8(
                    QJsonDocument(it.value().toObject()).toJson(
                        QJsonDocument::Compact));

            observations->append({
                keyParts[keyParts.size() - 2],
                keyParts.last(),
                value,
                QDir::fromNativeSeparators(source)
            });
        }
    }

    if (observations->isEmpty()) {
        if (errorMessage)
            *errorMessage = "ExifTool returned no metadata observations.";
        return false;
    }
    return true;
}

namespace {

QString portablePath(const QString &projectRoot, const QString &path)
{
    const QString relative = QDir(projectRoot).relativeFilePath(path);
    if (!relative.startsWith("../") && relative != "..")
        return QDir::fromNativeSeparators(relative);
    return QDir::fromNativeSeparators(QFileInfo(path).absoluteFilePath());
}

QString rString(const QString &value)
{
    QString escaped = value;
    escaped.replace('\\', "\\\\");
    escaped.replace('"', "\\\"");
    escaped.replace('\r', "\\r");
    escaped.replace('\n', "\\n");
    escaped.replace('\t', "\\t");
    return QStringLiteral("\"%1\"").arg(escaped);
}

QString rSnippet(const QString &draftFilePath,
                 const QList<MetadataRule> &rules,
                 const QString &exifToolVersion)
{
    QString text;
    QTextStream out(&text);
    out << "# Generated by rcaiman-projects from "
        << QFileInfo(draftFilePath).fileName() << ".\n";
    out << "# Metadata profile: " << ExifToolService::Profile << "\n";
    out << "# ExifTool version: " << exifToolVersion << "\n";
    out << "# Use this object as `rules = metadata_rules` in "
           "`add_embedded_metadata_sig()`.\n\n";
    out << "metadata_rules <- list(\n";
    for (int i = 0; i < rules.size(); ++i) {
        const MetadataRule &rule = rules[i];
        out << "  " << rule.id << " = list(\n";
        out << "    group = " << rString(rule.group) << ",\n";
        out << "    tag = " << rString(rule.tag) << ",\n";
        out << "    label = " << rString(rule.label) << ",\n";
        out << "    expected = " << rString(rule.expected) << "\n";
        out << "  )" << (i + 1 < rules.size() ? "," : "") << "\n";
    }
    out << ")\n";
    return text;
}

std::string utf8(const QString &value)
{
    const QByteArray bytes = value.toUtf8();
    return std::string(bytes.constData(), static_cast<size_t>(bytes.size()));
}

QString scalar(const YAML::Node &node, const char *field)
{
    const YAML::Node value = node[field];
    if (!value || !value.IsScalar())
        return {};
    return QString::fromStdString(value.as<std::string>());
}

bool hasAccidentalSpace(const QString &value)
{
    return value != value.trimmed();
}

QString normalized(const QString &text)
{
    QString result = text.toLower();
    result.remove(QRegularExpression("[^a-z0-9]"));
    return result;
}

int editDistance(const QString &a, const QString &b)
{
    QVector<int> previous(b.size() + 1);
    QVector<int> current(b.size() + 1);
    for (int j = 0; j <= b.size(); ++j)
        previous[j] = j;
    for (int i = 1; i <= a.size(); ++i) {
        current[0] = i;
        for (int j = 1; j <= b.size(); ++j) {
            current[j] = std::min({
                current[j - 1] + 1,
                previous[j] + 1,
                previous[j - 1] + (a[i - 1] == b[j - 1] ? 0 : 1)
            });
        }
        previous.swap(current);
    }
    return previous[b.size()];
}

double similarity(const QString &a, const QString &b)
{
    const QString left = normalized(a);
    const QString right = normalized(b);
    const int maximum = std::max(left.size(), right.size());
    if (maximum == 0)
        return 1.0;
    return 1.0 - static_cast<double>(editDistance(left, right)) / maximum;
}

}

QList<MetadataRuleAnalysis> MetadataRegistry::analyze(
    const QStringList &sourceFiles,
    const QList<MetadataObservation> &observations)
{
    QMap<QString, MetadataRuleAnalysis> byKey;
    QMap<QString, QSet<QString>> sources;
    QMap<QString, QSet<QString>> values;
    for (const MetadataObservation &observation : observations) {
        MetadataRuleAnalysis &entry = byKey[observation.key()];
        entry.group = observation.group;
        entry.tag = observation.tag;
        entry.sourceCount = sourceFiles.size();
        sources[observation.key()].insert(
            QFileInfo(observation.sourceFile).absoluteFilePath());
        values[observation.key()].insert(observation.value);
    }

    QList<MetadataRuleAnalysis> result;
    for (auto it = byKey.begin(); it != byKey.end(); ++it) {
        MetadataRuleAnalysis entry = it.value();
        entry.presence = sources[it.key()].size();
        entry.values = values[it.key()].values();
        entry.values.sort(Qt::CaseInsensitive);
        result.append(entry);
    }
    std::sort(result.begin(), result.end(),
              [](const MetadataRuleAnalysis &a,
                 const MetadataRuleAnalysis &b) {
        const bool aIdeal = a.presence == a.sourceCount && a.values.size() == 1;
        const bool bIdeal = b.presence == b.sourceCount && b.values.size() == 1;
        if (aIdeal != bIdeal)
            return aIdeal > bIdeal;
        if (a.presence != b.presence)
            return a.presence > b.presence;
        return a.group + ":" + a.tag < b.group + ":" + b.tag;
    });
    return result;
}

bool MetadataRegistry::writeDraft(
    const QString &filePath,
    const QString &projectRoot,
    const QStringList &sourceFiles,
    const QList<MetadataRule> &rules,
    const QString &exifToolVersion,
    QString *errorMessage)
{
    try {
        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "schema_version" << YAML::Value << 1;
        out << YAML::Key << "artifact" << YAML::Value
            << "embedded_metadata_rules_draft";
        out << YAML::Key << "created_with" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "application" << YAML::Value << "rcaiman-projects";
        out << YAML::Key << "exiftool_version" << YAML::Value
            << utf8(exifToolVersion);
        out << YAML::EndMap;
        out << YAML::Key << "metadata_profile" << YAML::Value << ExifToolService::Profile;
        out << YAML::Key << "source_images" << YAML::Value << YAML::BeginSeq;
        for (const QString &source : sourceFiles)
            out << utf8(portablePath(projectRoot, source));
        out << YAML::EndSeq;
        out << YAML::Key << "rules" << YAML::Value << YAML::BeginMap;
        for (const MetadataRule &rule : rules) {
            out << YAML::Key << utf8(rule.id) << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "group" << YAML::Value << utf8(rule.group);
            out << YAML::Key << "tag" << YAML::Value << utf8(rule.tag);
            out << YAML::Key << "label" << YAML::Value << utf8(rule.label);
            out << YAML::Key << "expected" << YAML::Value << utf8(rule.expected);
            out << YAML::EndMap;
        }
        out << YAML::EndMap << YAML::EndMap;

        QDir().mkpath(QFileInfo(filePath).absolutePath());
        QSaveFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
            throw std::runtime_error("Could not open the draft file.");
        file.write(out.c_str());
        file.write("\n");
        if (!file.commit())
            throw std::runtime_error("Could not commit the draft file.");

        const QString snippetPath =
            QFileInfo(filePath).absolutePath() + "/"
            + QFileInfo(filePath).completeBaseName() + "_snippet.R";
        QSaveFile snippet(snippetPath);
        if (!snippet.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QFile::remove(filePath);
            QFile::remove(snippetPath);
            throw std::runtime_error("Could not open the R snippet file.");
        }
        snippet.write(rSnippet(
            filePath, rules, exifToolVersion).toUtf8());
        if (!snippet.commit()) {
            QFile::remove(filePath);
            QFile::remove(snippetPath);
            throw std::runtime_error("Could not commit the R snippet file.");
        }
        return true;
    } catch (const std::exception &error) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8(error.what());
        return false;
    }
}

bool MetadataRegistry::readRules(const QString &filePath,
                                 QList<MetadataRule> *rules,
                                 QString *errorMessage)
{
    if (!rules)
        return false;
    rules->clear();
    try {
        const YAML::Node document = YAML::LoadFile(utf8(filePath));
        YAML::Node rulesNode;
        if (scalar(document, "artifact") == "embedded_metadata_rules_draft") {
            if (document["schema_version"].as<int>() != 1)
                throw std::runtime_error("Unsupported draft schema version.");
            rulesNode = document["rules"];
        } else {
            if (!document["schema_version"]
                || document["schema_version"].as<int>() != 2) {
                throw std::runtime_error(
                    "Only registry snapshot schema version 2 is supported.");
            }
            rulesNode =
                document["registry_entry"]["embedded_metadata_sig"]["rules"];
        }
        if (!rulesNode || !rulesNode.IsMap())
            throw std::runtime_error("The YAML file contains no metadata rules.");

        QSet<QString> ids;
        QSet<QString> keys;
        const QRegularExpression idPattern("^[a-z][a-z0-9_]*$");
        for (const auto &entry : rulesNode) {
            MetadataRule rule;
            rule.id = QString::fromStdString(entry.first.as<std::string>());
            rule.group = scalar(entry.second, "group");
            rule.tag = scalar(entry.second, "tag");
            rule.label = scalar(entry.second, "label");
            rule.expected = scalar(entry.second, "expected");
            if (!idPattern.match(rule.id).hasMatch())
                throw std::runtime_error("Invalid rule identifier.");
            if (ids.contains(rule.id) || keys.contains(rule.key()))
                throw std::runtime_error("Duplicated metadata rule.");
            if (rule.group.isEmpty() || rule.tag.isEmpty()
                || rule.label.isEmpty() || rule.expected.isEmpty()
                || hasAccidentalSpace(rule.group)
                || hasAccidentalSpace(rule.tag)
                || hasAccidentalSpace(rule.label)
                || hasAccidentalSpace(rule.expected)) {
                throw std::runtime_error(
                    "A metadata rule has missing fields or accidental spaces.");
            }
            ids.insert(rule.id);
            keys.insert(rule.key());
            rules->append(rule);
        }
        return !rules->isEmpty();
    } catch (const std::exception &error) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8(error.what());
        return false;
    }
}

QList<MetadataObservation> MetadataRegistry::candidates(
    const MetadataRule &rule,
    const QList<MetadataObservation> &observations,
    int maximum)
{
    struct Ranked {
        MetadataObservation observation;
        double score = 0.0;
    };
    QList<Ranked> ranked;
    QSet<QString> seen;
    for (const MetadataObservation &observation : observations) {
        const QString identity = observation.key() + QChar(0)
            + observation.value;
        if (seen.contains(identity))
            continue;
        seen.insert(identity);
        double score = similarity(rule.tag, observation.tag) * 4.0;
        score += similarity(rule.label, observation.tag) * 2.0;
        if (rule.group.compare(observation.group, Qt::CaseInsensitive) == 0)
            score += 2.0;
        if (rule.expected == observation.value)
            score += 3.0;
        ranked.append({observation, score});
    }
    std::sort(ranked.begin(), ranked.end(),
              [](const Ranked &a, const Ranked &b) {
        if (a.score != b.score)
            return a.score > b.score;
        return a.observation.key() < b.observation.key();
    });
    QList<MetadataObservation> result;
    const int count = std::min(maximum, static_cast<int>(ranked.size()));
    for (int i = 0; i < count; ++i)
        result.append(ranked[i].observation);
    return result;
}

QString MetadataRegistry::sha256(const QString &filePath,
                                 QString *errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage)
            *errorMessage = QString("Could not read %1").arg(filePath);
        return {};
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        if (errorMessage)
            *errorMessage = QString("Could not hash %1").arg(filePath);
        return {};
    }
    return QString::fromLatin1(hash.result().toHex());
}

bool MetadataRegistry::writeReview(
    const QString &filePath,
    const QString &projectRoot,
    const QString &imagePath,
    const QString &registryPath,
    const QString &exifToolVersion,
    const QList<MetadataRuleDecision> &decisions,
    bool accepted,
    QString *errorMessage)
{
    QString hashError;
    const QString imageHash = sha256(imagePath, &hashError);
    const QString registryHash = sha256(registryPath, &hashError);
    if (imageHash.isEmpty() || registryHash.isEmpty()) {
        if (errorMessage)
            *errorMessage = hashError;
        return false;
    }
    try {
        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "schema_version" << YAML::Value << 1;
        out << YAML::Key << "artifact" << YAML::Value << "registry_review";
        out << YAML::Key << "image" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "path" << YAML::Value
            << utf8(portablePath(projectRoot, imagePath));
        out << YAML::Key << "sha256" << YAML::Value << utf8(imageHash);
        out << YAML::EndMap;
        out << YAML::Key << "registry" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "path" << YAML::Value
            << utf8(portablePath(projectRoot, registryPath));
        out << YAML::Key << "sha256" << YAML::Value << utf8(registryHash);
        out << YAML::EndMap;
        out << YAML::Key << "metadata_reader" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "name" << YAML::Value << "exiftool";
        out << YAML::Key << "version" << YAML::Value << utf8(exifToolVersion);
        out << YAML::Key << "profile" << YAML::Value << ExifToolService::Profile;
        out << YAML::EndMap;
        out << YAML::Key << "rules" << YAML::Value << YAML::BeginMap;
        for (const MetadataRuleDecision &decision : decisions) {
            const QString observedKey =
                decision.observedGroup.isEmpty()
                    ? QString()
                    : decision.observedGroup + ":" + decision.observedTag;
            const bool itemRequired =
                observedKey.isEmpty() || observedKey != decision.rule.key();
            const bool valueRequired =
                decision.observedValue.isEmpty()
                || decision.observedValue != decision.rule.expected;
            out << YAML::Key << utf8(decision.rule.id)
                << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "expected_key" << YAML::Value
                << utf8(decision.rule.key());
            out << YAML::Key << "observed_key" << YAML::Value
                << utf8(observedKey);
            out << YAML::Key << "item_equivalence" << YAML::Value
                << (decision.itemEquivalence
                        ? "accepted"
                        : (itemRequired ? "not_accepted" : "not_required"));
            out << YAML::Key << "expected_value" << YAML::Value
                << utf8(decision.rule.expected);
            out << YAML::Key << "observed_value" << YAML::Value
                << utf8(decision.observedValue);
            out << YAML::Key << "value_equivalence" << YAML::Value
                << (decision.valueEquivalence
                        ? "accepted"
                        : (valueRequired ? "not_accepted" : "not_required"));
            out << YAML::EndMap;
        }
        out << YAML::EndMap;
        out << YAML::Key << "decision" << YAML::Value
            << (accepted ? "accepted" : "rejected");
        out << YAML::Key << "reviewed_at" << YAML::Value
            << utf8(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
        out << YAML::EndMap;

        QDir().mkpath(QFileInfo(filePath).absolutePath());
        QSaveFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
            throw std::runtime_error("Could not open the review file.");
        file.write(out.c_str());
        file.write("\n");
        if (!file.commit())
            throw std::runtime_error("Could not commit the review file.");
        return true;
    } catch (const std::exception &error) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8(error.what());
        return false;
    }
}

QString MetadataRegistry::snakeCase(const QString &text)
{
    QString value = text;
    value.replace(QRegularExpression("([a-z0-9])([A-Z])"), "\\1_\\2");
    value = value.toLower();
    value.replace(QRegularExpression("[^a-z0-9]+"), "_");
    value.remove(QRegularExpression("^_+|_+$"));
    if (value.isEmpty() || !value.front().isLetter())
        value.prepend("rule_");
    return value;
}
