// SPDX-License-Identifier: GPL-3.0-or-later

#include "thirdpartylicensesdialog.h"

#include <QApplication>
#include <QDirIterator>
#include <QFile>
#include <QMouseEvent>
#include <QSet>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextCursor>
#include <QToolButton>

#include <cstdio>

namespace {
bool check(bool passed, const char *message)
{
    if (!passed)
        std::fprintf(stderr, "%s\n", message);
    return passed;
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setFont(QFont(QStringLiteral("Segoe UI"), 9));
    ThirdPartyLicensesDialog dialog;
    dialog.show();
    app.processEvents();
    auto *browser = dialog.findChild<QTextBrowser*>("licenseBrowser");
    auto *back = dialog.findChild<QToolButton*>("licenseBack");
    auto *home = dialog.findChild<QToolButton*>("licenseHome");
    if (!check(browser && back && home, "The license dialog must provide its browser and navigation"))
        return 1;
    const QUrl overview = browser->source();
    const QString summary = browser->toPlainText();
    for (const QString &name : {"Qt", "LibRaw", "libtiff", "yaml-cpp", "ExifTool"}) {
        if (!check(summary.contains(name), "The overview must credit every direct dependency"))
            return 2;
    }
    QTextCursor licenseLink = browser->document()->find("Qt license information and LGPL/GPL texts");
    if (!check(!licenseLink.isNull(), "The Qt license link must be visible in the overview"))
        return 9;
    licenseLink.setPosition(licenseLink.selectionStart() + 3);
    browser->setTextCursor(licenseLink);
    browser->ensureCursorVisible();
    const QPoint clickPosition = browser->cursorRect().center();
    for (const auto type : {QEvent::MouseButtonPress, QEvent::MouseButtonRelease}) {
        QMouseEvent click(type, QPointF(clickPosition),
                          QPointF(browser->viewport()->mapToGlobal(clickPosition)),
                          Qt::LeftButton,
                          type == QEvent::MouseButtonPress ? Qt::LeftButton : Qt::NoButton,
                          Qt::NoModifier);
        QApplication::sendEvent(browser->viewport(), &click);
    }
    if (!check(browser->source() == overview.resolved(QUrl("licenses/qt/LICENSE.txt")),
               "Clicking a relative license link must open the bundled document"))
        return 10;
    if (argc > 1)
        dialog.grab().save(QString::fromLocal8Bit(argv[1]) + ".detail.png");
    back->click();
    QSet<QUrl> localLinks;
    for (QTextBlock block = browser->document()->begin(); block.isValid(); block = block.next()) {
        for (auto fragment = block.begin(); !fragment.atEnd(); ++fragment) {
            const QString href = fragment.fragment().charFormat().anchorHref();
            if (!href.isEmpty()) {
                const QUrl url = overview.resolved(QUrl(href));
                if (url.scheme() == "qrc")
                    localLinks.insert(url);
            }
        }
    }
    QSet<QUrl> bundledFiles;
    QDirIterator files(":/third-party/licenses", QDir::Files, QDirIterator::Subdirectories);
    while (files.hasNext())
        bundledFiles.insert(QUrl("qrc" + files.next()));
    if (!check(!localLinks.isEmpty() && localLinks == bundledFiles,
               "Every bundled license file must be linked from the overview"))
        return 3;
    for (const QUrl &url : localLinks) {
        QFile file(":" + url.path());
        if (!check(file.open(QIODevice::ReadOnly), "Every local license link must resolve offline"))
            return 4;
        const QString original = QString::fromUtf8(file.readAll());
        browser->setSource(url);
        if (!check(browser->source() == url && !browser->toPlainText().trimmed().isEmpty()
                   && back->isEnabled(), "Each license must open with working Back navigation"))
            return 5;
        if (!url.path().endsWith(".md")) {
            const QStringList lines = original.trimmed().split('\n');
            if (!check(browser->toPlainText().contains(lines.first().trimmed())
                       && browser->toPlainText().contains(lines.last().trimmed()),
                       "Plain license files must display both their beginning and end")) {
                std::fprintf(stderr, "License: %s\n", qPrintable(url.toString()));
                return 6;
            }
        }
        back->click();
        if (!check(browser->source() == overview && browser->toPlainText() == summary,
                   "Back must restore the overview"))
            return 7;
    }
    browser->setSource(*localLinks.begin());
    home->click();
    if (!check(browser->source() == overview, "Home must return to the overview"))
        return 8;
    if (argc > 1)
        dialog.grab().save(QString::fromLocal8Bit(argv[1]));
    return 0;
}
