#include "collectionvalidationdialog.h"

#include <QDialogButtonBox>
#include <QBrush>
#include <QColor>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {
QString displayStatus(CollectionFileValidationStatus status)
{
    QString value = CollectionValidation::statusName(status);
    value.replace('_', ' ');
    if (!value.isEmpty())
        value[0] = value[0].toUpper();
    return value;
}
}

CollectionValidationDialog::CollectionValidationDialog(
    const CollectionValidationRun &run,
    QWidget *parent)
    : QDialog(parent)
    , m_run(run)
{
    setWindowTitle("Collection Registry Validation");
    resize(980, 580);
    auto *layout = new QVBoxLayout(this);

    int exact = 0;
    int reviewed = 0;
    int rejected = 0;
    int problems = 0;
    for (const CollectionFileValidation &file : run.files) {
        if (file.status == CollectionFileValidationStatus::ExactMatch)
            ++exact;
        else if (file.status
                 == CollectionFileValidationStatus::AcceptedReview)
            ++reviewed;
        else if (file.status
                 == CollectionFileValidationStatus::RejectedReview)
            ++rejected;
        else
            ++problems;
    }
    auto *summary = new QLabel(
        QString("%1: %2. Exact: %3, accepted reviews: %4, "
                "rejected reviews: %5, unresolved problems: %6.")
            .arg(run.collection.dirName,
                 run.result == "passed_exact"
                     ? QStringLiteral("passed exact")
                     : (run.result == "passed_with_reviews"
                            ? QStringLiteral("passed with reviews")
                            : QStringLiteral("failed")))
            .arg(exact)
            .arg(reviewed)
            .arg(rejected)
            .arg(problems),
        this);
    summary->setWordWrap(true);
    layout->addWidget(summary);

    m_table = new QTableWidget(run.files.size(), 5, this);
    m_table->setHorizontalHeaderLabels({
        "Preprocessed image", "Basename", "RAW", "Status", "Rules"
    });
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    for (int row = 0; row < run.files.size(); ++row) {
        const CollectionFileValidation &file = run.files[row];
        m_table->setItem(
            row, 0,
            new QTableWidgetItem(
                QFileInfo(file.preprocessedPath).fileName()));
        m_table->setItem(row, 1, new QTableWidgetItem(file.baseName));
        QString rawDisplay = QFileInfo(file.rawPath).fileName();
        if (file.status == CollectionFileValidationStatus::MissingRaw)
            rawDisplay = "No matching RAW";
        else if (file.status
                 == CollectionFileValidationStatus::AmbiguousRaw)
            for (const QString &candidate : file.rawCandidates) {
                if (!rawDisplay.isEmpty())
                    rawDisplay += ", ";
                rawDisplay += QFileInfo(candidate).fileName();
            }
        m_table->setItem(row, 2, new QTableWidgetItem(rawDisplay));
        m_table->setItem(
            row, 3, new QTableWidgetItem(displayStatus(file.status)));
        m_table->setItem(
            row, 4,
            new QTableWidgetItem(
                QString("%1/%2")
                    .arg(file.matchedRuleCount)
                    .arg(file.totalRuleCount)));

        QString detail;
        if (file.status == CollectionFileValidationStatus::MissingRaw) {
            detail = QString(
                "No RAW with basename '%1' was found in raw_files.")
                         .arg(file.baseName);
        } else if (file.status
                   == CollectionFileValidationStatus::AmbiguousRaw) {
            detail = QString(
                "More than one RAW with basename '%1' was found:\n%2")
                         .arg(file.baseName,
                              file.rawCandidates.join("\n"));
        } else {
            QStringList problems;
            for (const MetadataRuleDecision &problem : file.problems) {
                problems.append(
                    QString("%1: expected %2 = %3; observed %4 = %5")
                        .arg(problem.rule.id,
                             problem.rule.key(),
                             problem.rule.expected,
                             problem.observedGroup.isEmpty()
                                 ? QStringLiteral("(missing)")
                                 : problem.observedGroup + ":"
                                     + problem.observedTag,
                             problem.observedValue));
            }
            detail = problems.join("\n");
        }
        for (int column = 0; column < m_table->columnCount(); ++column) {
            if (m_table->item(row, column))
                m_table->item(row, column)->setToolTip(detail);
        }
    }
    m_table->horizontalHeader()->setSectionResizeMode(
        QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(
        0, QHeaderView::Stretch);
    layout->addWidget(m_table);
    if (run.files.isEmpty()) {
        auto *emptyMessage = new QLabel(
            "The collection contains no relevant preprocessed images.",
            this);
        layout->addWidget(emptyMessage);
    }

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    m_reviewButton = buttons->addButton(
        "Review selected image...", QDialogButtonBox::ActionRole);
    connect(buttons, &QDialogButtonBox::rejected,
            this, &QDialog::reject);
    connect(m_reviewButton, &QPushButton::clicked,
            this, &CollectionValidationDialog::reviewSelectedRow);
    connect(m_table, &QTableWidget::cellDoubleClicked,
            this, [this](int row, int) {
                m_table->selectRow(row);
                reviewSelectedRow();
            });
    connect(m_table, &QTableWidget::itemSelectionChanged,
            this, &CollectionValidationDialog::updateReviewButton);
    layout->addWidget(buttons);
    if (!run.files.isEmpty())
        m_table->selectRow(0);
    updateReviewButton();
}

void CollectionValidationDialog::updateReviewButton()
{
    const int row = m_table ? m_table->currentRow() : -1;
    const bool enabled =
        row >= 0 && row < m_run.files.size()
        && !m_run.files[row].rawPath.isEmpty()
        && m_run.files[row].status
            == CollectionFileValidationStatus::Mismatch;
    m_reviewButton->setEnabled(enabled);
}

void CollectionValidationDialog::reviewSelectedRow()
{
    const int row = m_table ? m_table->currentRow() : -1;
    if (row < 0 || row >= m_run.files.size()
        || m_run.files[row].rawPath.isEmpty()
        || m_run.files[row].status
            != CollectionFileValidationStatus::Mismatch) {
        return;
    }

    emit reviewRequested(
        m_run.files[row].rawPath, m_run.registryPath);
    markRowReviewed(row);
}

void CollectionValidationDialog::markRowReviewed(int row)
{
    m_reviewedRows.insert(row);
    const QBrush reviewedBrush(QColor(255, 244, 194));
    for (int column = 0; column < m_table->columnCount(); ++column) {
        QTableWidgetItem *item = m_table->item(row, column);
        if (item)
            item->setBackground(reviewedBrush);
    }
}

void CollectionValidationDialog::done(int result)
{
    if (!m_closeNoticeShown && !m_reviewedRows.isEmpty()) {
        m_closeNoticeShown = true;
        QMessageBox::information(
            this,
            "Collection Validation",
            "Individual reviews do not modify this validation manifest.\n\n"
            "Run Registry > Validate collection... again to incorporate the "
            "new decisions before generating Project > Report.");
    }
    QDialog::done(result);
}
