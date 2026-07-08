// SPDX-License-Identifier: GPL-3.0-or-later
//
// Copyright (C) 2026 GASTON MAURO DIAZ
//
// This file is part of rcaiman-projects.
//
// rcaiman-projects is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// rcaiman-projects is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with rcaiman-projects. If not, see <https://www.gnu.org/licenses/>.
//
// Part of the rcaiman suite.

#include "hspimportdialog.h"
#include "project.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextStream>
#include <QVBoxLayout>

namespace {
QString hspProperty(const QString &filePath, const QString &key)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    QTextStream stream(&file);
    while (!stream.atEnd()) {
        const QString line = stream.readLine();
        const int separator = line.indexOf('=');
        if (separator <= 0)
            continue;
        if (line.left(separator).trimmed() == key)
            return line.mid(separator + 1).trimmed();
    }
    return {};
}
}

HspImportDialog::HspImportDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Import legacy HSP project");
    setMinimumWidth(560);

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout();

    auto *sourceRow = new QWidget(this);
    auto *sourceLayout = new QHBoxLayout(sourceRow);
    sourceLayout->setContentsMargins(0, 0, 0, 0);
    m_sourceDirectory = new QLineEdit(sourceRow);
    auto *sourceBrowse = new QPushButton("Browse...", sourceRow);
    sourceLayout->addWidget(m_sourceDirectory, 1);
    sourceLayout->addWidget(sourceBrowse);
    form->addRow("HSP project folder:", sourceRow);

    auto *parentRow = new QWidget(this);
    auto *parentLayout = new QHBoxLayout(parentRow);
    parentLayout->setContentsMargins(0, 0, 0, 0);
    m_parentDirectory = new QLineEdit(parentRow);
    auto *parentBrowse = new QPushButton("Browse...", parentRow);
    parentLayout->addWidget(m_parentDirectory, 1);
    parentLayout->addWidget(parentBrowse);
    form->addRow("Destination parent:", parentRow);

    m_folderName = new QLineEdit(this);
    form->addRow("Project folder name:", m_folderName);

    m_description = new QLineEdit(this);
    m_description->setText("Imported from legacy HSP project");
    form->addRow("Description:", m_description);

    layout->addLayout(form);

    m_warning = new QLabel(this);
    m_warning->setWordWrap(true);
    m_warning->setStyleSheet("color: #8a1f11;");
    m_warning->hide();
    layout->addWidget(m_warning);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    m_importButton = buttons->addButton("Import", QDialogButtonBox::AcceptRole);
    layout->addWidget(buttons);

    connect(sourceBrowse, &QPushButton::clicked,
            this, &HspImportDialog::browseSourceDirectory);
    connect(parentBrowse, &QPushButton::clicked,
            this, &HspImportDialog::browseParentDirectory);
    connect(m_sourceDirectory, &QLineEdit::textChanged,
            this, [this]() {
                updateDefaultsFromSource();
                updateImportButton();
            });
    connect(m_parentDirectory, &QLineEdit::textChanged,
            this, &HspImportDialog::updateImportButton);
    connect(m_folderName, &QLineEdit::textChanged,
            this, &HspImportDialog::updateImportButton);
    connect(m_description, &QLineEdit::textChanged,
            this, &HspImportDialog::updateImportButton);
    connect(m_importButton, &QPushButton::clicked,
            this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected,
            this, &QDialog::reject);

    updateImportButton();
}

QString HspImportDialog::sourceDirectory() const
{
    return m_sourceDirectory->text().trimmed();
}

QString HspImportDialog::parentDirectory() const
{
    return m_parentDirectory->text().trimmed();
}

QString HspImportDialog::folderName() const
{
    return m_folderName->text().trimmed();
}

QString HspImportDialog::description() const
{
    return m_description->text().trimmed();
}

void HspImportDialog::browseSourceDirectory()
{
    const QString path = QFileDialog::getExistingDirectory(
        this, "Select legacy HSP project folder", sourceDirectory());
    if (!path.isEmpty())
        m_sourceDirectory->setText(path);
}

void HspImportDialog::browseParentDirectory()
{
    const QString path = QFileDialog::getExistingDirectory(
        this, "Select destination parent folder", parentDirectory());
    if (!path.isEmpty())
        m_parentDirectory->setText(path);
}

void HspImportDialog::updateDefaultsFromSource()
{
    if (!m_folderName || !m_folderName->text().trimmed().isEmpty())
        return;

    const QString name = hspProperty(
        QDir(sourceDirectory()).filePath("project.conf"), "name");
    if (!name.isEmpty())
        m_folderName->setText(name);
}

void HspImportDialog::updateImportButton()
{
    QString warning;
    const QString source = sourceDirectory();
    const QString parent = parentDirectory();
    const QString name = folderName();
    QString descriptionError;
    const bool descriptionOk =
        Project::descriptionIsSupported(description(), &descriptionError);

    bool ok = true;
    if (source.isEmpty() || !QFileInfo(source).isDir()) {
        ok = false;
    } else {
        const QDir sourceDir(source);
        if (!QFileInfo::exists(sourceDir.filePath("project.conf"))
            || !QFileInfo(sourceDir.filePath("raw")).isDir()
            || !QFileInfo(sourceDir.filePath("manipulate")).isDir()) {
            ok = false;
            warning = "The selected folder does not look like a legacy HSP project.";
        }
    }

    if (parent.isEmpty() || !QFileInfo(parent).isDir())
        ok = false;
    if (name.isEmpty() || name == "." || name == ".."
        || name.contains('/') || name.contains('\\')) {
        ok = false;
    }
    if (!descriptionOk) {
        ok = false;
        warning = descriptionError;
    }

    m_warning->setVisible(!warning.isEmpty());
    m_warning->setText(warning);
    m_importButton->setEnabled(ok);
}
