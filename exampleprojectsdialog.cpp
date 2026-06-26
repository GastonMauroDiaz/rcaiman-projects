#include "exampleprojectsdialog.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>

ExampleProjectsDialog::ExampleProjectsDialog(
    const QVector<ExampleProjectInfo> &examples,
    QWidget *parent)
    : QDialog(parent)
    , m_examples(examples)
{
    setWindowTitle("Example projects");
    resize(720, 420);

    auto *layout = new QVBoxLayout(this);
    auto *contentLayout = new QHBoxLayout();
    layout->addLayout(contentLayout);

    m_list = new QListWidget(this);
    m_list->setMinimumWidth(220);
    contentLayout->addWidget(m_list, 1);

    m_details = new QTextBrowser(this);
    m_details->setOpenExternalLinks(true);
    contentLayout->addWidget(m_details, 2);

    for (const ExampleProjectInfo &example : m_examples) {
        auto *item = new QListWidgetItem(example.name, m_list);
        item->setToolTip(example.path);
    }

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText("Create copy...");
    buttons->button(QDialogButtonBox::Ok)->setEnabled(!m_examples.isEmpty());
    layout->addWidget(buttons);

    connect(m_list, &QListWidget::currentRowChanged,
            this, &ExampleProjectsDialog::updateDetails);
    connect(m_list, &QListWidget::itemDoubleClicked,
            this, [this](QListWidgetItem *) { accept(); });
    connect(buttons, &QDialogButtonBox::accepted,
            this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected,
            this, &QDialog::reject);

    if (!m_examples.isEmpty())
        m_list->setCurrentRow(0);
    else
        m_details->setPlainText("No example projects were found.");
}

ExampleProjectInfo ExampleProjectsDialog::selectedExample() const
{
    const int row = m_list ? m_list->currentRow() : -1;
    if (row < 0 || row >= m_examples.size())
        return {};
    return m_examples.at(row);
}

void ExampleProjectsDialog::updateDetails(int row)
{
    if (row < 0 || row >= m_examples.size()) {
        m_details->clear();
        return;
    }

    const ExampleProjectInfo &example = m_examples.at(row);
    QString markdown =
        QStringLiteral("## %1\n\n").arg(example.name);
    if (!example.description.isEmpty())
        markdown += example.description + "\n\n";
    if (!example.readme.trimmed().isEmpty())
        markdown += "---\n\n" + example.readme;
    m_details->setMarkdown(markdown);
}
