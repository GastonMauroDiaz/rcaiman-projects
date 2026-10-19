// SPDX-License-Identifier: GPL-3.0-or-later

#include "thirdpartylicensesdialog.h"

#include <QDialogButtonBox>
#include <QFile>
#include <QHBoxLayout>
#include <QStyle>
#include <QTextBrowser>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
class LicenseBrowser : public QTextBrowser
{
public:
    using QTextBrowser::QTextBrowser;

    QVariant loadResource(int type, const QUrl &url) override
    {
        if (url.scheme() == "qrc" && url.path().startsWith("/third-party/licenses/")) {
            QFile file(":" + url.path());
            if (file.open(QIODevice::ReadOnly)) {
                // Show upstream files literally, never interpret license text as markup.
                return QStringLiteral("<pre style=\"white-space: pre-wrap\">%1</pre>")
                    .arg(QString::fromUtf8(file.readAll()).toHtmlEscaped());
            }
        }
        return QTextBrowser::loadResource(type, url);
    }

protected:
    void doSetSource(const QUrl &url, QTextDocument::ResourceType type) override
    {
        const QUrl resolved = source().resolved(url);
        if (resolved.scheme() == "qrc" && resolved.path().startsWith("/third-party/licenses/"))
            type = QTextDocument::HtmlResource;
        QTextBrowser::doSetSource(url, type);
    }
};
}

ThirdPartyLicensesDialog::ThirdPartyLicensesDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Third-party licenses");
    resize(780, 600);
    setMinimumSize(500, 360);

    auto *layout = new QVBoxLayout(this);
    auto *navigation = new QHBoxLayout;
    auto *back = new QToolButton(this);
    back->setObjectName("licenseBack");
    back->setIcon(style()->standardIcon(QStyle::SP_ArrowBack));
    back->setToolTip("Back");
    back->setEnabled(false);
    navigation->addWidget(back);
    auto *home = new QToolButton(this);
    home->setObjectName("licenseHome");
    home->setIcon(style()->standardIcon(QStyle::SP_DirHomeIcon));
    home->setToolTip("Third-party software overview");
    navigation->addWidget(home);
    navigation->addStretch();
    layout->addLayout(navigation);

    auto *browser = new LicenseBrowser(this);
    browser->setObjectName("licenseBrowser");
    browser->setOpenExternalLinks(true);
    const QUrl overview(QStringLiteral("qrc:/third-party/notices.md"));
    browser->setSource(overview, QTextDocument::MarkdownResource);
    layout->addWidget(browser);
    connect(back, &QToolButton::clicked, browser, &QTextBrowser::backward);
    connect(browser, &QTextBrowser::backwardAvailable, back, &QToolButton::setEnabled);
    connect(home, &QToolButton::clicked, browser, [browser, overview] {
        browser->setSource(overview, QTextDocument::MarkdownResource);
    });

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}
