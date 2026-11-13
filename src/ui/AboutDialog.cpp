// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AboutDialog.h"

#include "AppInfo.h"
#include "core/AboutInfo.h"

#include <QApplication>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QFile>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTabWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace edita {

namespace {

QString readResource(const QString& path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
}

QLabel* wrappingLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
    return label;
}

} // namespace

AboutDialog::AboutDialog(Context context, QWidget* parent)
    : QDialog(parent)
    , _context(std::move(context))
{
    setObjectName(QStringLiteral("aboutBox"));
    setWindowTitle(tr("About %1").arg(QString::fromUtf8(kAppName)));
    auto* layout = new QVBoxLayout(this);
    auto* tabs = new QTabWidget(this);
    tabs->setObjectName(QStringLiteral("aboutTabs"));
    tabs->addTab(aboutPage(), tr("&About"));
    tabs->addTab(componentsPage(), tr("&Components"));
    tabs->addTab(licensePage(), tr("&License"));
    layout->addWidget(tabs, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    QPushButton* copy = buttons->addButton(tr("Co&py Version Info"), QDialogButtonBox::ActionRole);
    copy->setObjectName(QStringLiteral("aboutCopy"));
    copy->setToolTip(tr("The version, the system and the plugins, to paste in a bug report"));
    connect(copy, &QPushButton::clicked, this, [this, copy] {
        QApplication::clipboard()->setText(copyText());
        copy->setText(tr("Copied"));
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
    resize(640, 520);
}

QWidget* AboutDialog::aboutPage()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setSpacing(12);

    // El ícono (con transparencia, se ve bien con los dos temas) y el nombre con el color del
    // tema: el logo con texto tiene el fondo negro
    auto* header = new QHBoxLayout;
    auto* icon = new QLabel(page);
    const qreal ratio = devicePixelRatioF();
    QPixmap pixmap(QStringLiteral(":/edita/icons/hicolor/256x256/apps/edita.png"));
    pixmap = pixmap.scaled(QSize(96, 96) * ratio, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    pixmap.setDevicePixelRatio(ratio);
    icon->setPixmap(pixmap);
    icon->setAlignment(Qt::AlignTop);
    header->addWidget(icon);
    auto* titles = new QVBoxLayout;
    auto* name = new QLabel(QString::fromUtf8(kAppName), page);
    QFont big = name->font();
    big.setPointSizeF(big.pointSizeF() * 2);
    big.setBold(true);
    name->setFont(big);
    titles->addWidget(name);
    auto* version = wrappingLabel(_context.versionLine, page);
    version->setObjectName(QStringLiteral("aboutVersion"));
    QFont bold = version->font();
    bold.setBold(true);
    version->setFont(bold);
    titles->addWidget(version);
    QStringList build;
    if (!_context.build.isEmpty())
        build << tr("Build %1").arg(_context.build);
    build << QStringLiteral("Qt %1").arg(QString::fromLatin1(qVersion()));
    auto* buildLabel = wrappingLabel(build.join(QStringLiteral(" · ")), page);
    buildLabel->setObjectName(QStringLiteral("aboutBuild"));
    titles->addWidget(buildLabel);
    titles->addStretch(1);
    header->addLayout(titles, 1);
    layout->addLayout(header);

    layout->addWidget(wrappingLabel(tr("A native text and code editor for Linux, inspired by Notepad++: its engine (Scintilla), "
                                       "its settings files and its plugin interface, with LSP, Git, a terminal and project tasks."),
                                    page));

    _update = wrappingLabel(QString(), page);
    _update->setObjectName(QStringLiteral("aboutUpdate"));
    _update->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
    connect(_update, &QLabel::linkActivated, this, [this](const QString&) {
        if (_context.openUrl && _downloadPage.isValid())
            _context.openUrl(_downloadPage);
    });
    layout->addWidget(_update);
    setUpdateState(UpdateState::Disabled);

    // Enlaces: href internos, para que cada uno haga lo suyo (abrir el manual, el administrador…)
    const QList<std::pair<QString, QString>> links = {
        {QStringLiteral("home"), tr("Home Page")},       {QStringLiteral("manual"), tr("User Manual")},
        {QStringLiteral("report"), tr("Report a Problem")}, {QStringLiteral("plugins"), tr("Plugin Catalog")},
        {QStringLiteral("notes"), tr("Release Notes")},
    };
    QStringList html;
    for (const auto& [href, text] : links)
        html << QStringLiteral("<a href=\"%1\">%2</a>").arg(href, text.toHtmlEscaped());
    auto* linkRow = wrappingLabel(html.join(QStringLiteral(" &nbsp;·&nbsp; ")), page);
    linkRow->setObjectName(QStringLiteral("aboutLinks"));
    linkRow->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
    connect(linkRow, &QLabel::linkActivated, this, [this](const QString& href) {
        auto call = [](const std::function<void()>& f) {
            if (f)
                f();
        };
        if (href == QLatin1String("home") && _context.openUrl)
            _context.openUrl(_context.homepage);
        else if (href == QLatin1String("notes") && _context.openUrl)
            _context.openUrl(_context.releaseNotes);
        else if (href == QLatin1String("manual"))
            call(_context.openManual);
        else if (href == QLatin1String("report"))
            call(_context.reportProblem);
        else if (href == QLatin1String("plugins"))
            call(_context.openPluginsAdmin);
    });
    layout->addWidget(linkRow);

    QString plugins = _context.plugins.isEmpty() ? tr("No plugins loaded.")
                                                 : tr("Plugins loaded (%1): %2").arg(_context.plugins.size()).arg(_context.plugins.join(QStringLiteral(", ")));
    auto* pluginLabel = wrappingLabel(plugins, page);
    pluginLabel->setObjectName(QStringLiteral("aboutPlugins"));
    layout->addWidget(pluginLabel);

    layout->addStretch(1);
    layout->addWidget(wrappingLabel(tr("Copyright © 2026 Edita contributors. Free software under the GNU General Public "
                                       "License, version 3 or later: see the License tab. It comes with no warranty."),
                                    page));
    return page;
}

QWidget* AboutDialog::componentsPage()
{
    auto* split = new QSplitter(Qt::Vertical, this);
    auto* list = new QTreeWidget(split);
    list->setObjectName(QStringLiteral("aboutComponents"));
    list->setRootIsDecorated(false);
    // La licencia al final: es la que puede cortarse (el texto completo está abajo)
    list->setHeaderLabels({tr("Component"), tr("Version"), tr("Used for"), tr("License")});
    list->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    list->header()->setStretchLastSection(true);
    auto* text = new QPlainTextEdit(split);
    text->setObjectName(QStringLiteral("aboutComponentLicense"));
    text->setReadOnly(true);
    text->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    const std::vector<about::Component> components = about::components();
    for (const about::Component& c : components) {
        auto* item = new QTreeWidgetItem(list, {c.name, c.version, c.usedFor, c.license});
        item->setToolTip(3, c.license);
        item->setToolTip(0, c.homepage.toString());
    }
    connect(list, &QTreeWidget::currentItemChanged, this, [list, text, components](QTreeWidgetItem* item) {
        const int row = item ? list->indexOfTopLevelItem(item) : -1;
        if (row < 0) {
            text->clear();
            return;
        }
        const about::Component& c = components[static_cast<size_t>(row)];
        QString body = QStringLiteral("%1 %2\n%3\n%4: %5\n\n").arg(c.name, c.version, c.homepage.toString(), tr("License"), c.license);
        body += c.licenseText.isEmpty() ? tr("The text of this license is on the component's web page.") : readResource(c.licenseText);
        text->setPlainText(body);
    });
    connect(list, &QTreeWidget::itemDoubleClicked, this, [this, list, components](QTreeWidgetItem* item) {
        const int row = list->indexOfTopLevelItem(item);
        if (row >= 0 && _context.openUrl)
            _context.openUrl(components[static_cast<size_t>(row)].homepage);
    });
    list->setCurrentItem(list->topLevelItem(0));
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 2);

    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->addWidget(wrappingLabel(tr("Edita is built on these projects. Double-click one to open its web page."), page));
    layout->addWidget(split, 1);
    return page;
}

QWidget* AboutDialog::licensePage()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->addWidget(wrappingLabel(tr("Edita is free software: you can redistribute it and modify it under the terms of the GNU "
                                       "General Public License, version 3 or (at your option) any later version."),
                                    page));
    auto* text = new QPlainTextEdit(readResource(QStringLiteral(":/edita/COPYING")), page);
    text->setObjectName(QStringLiteral("aboutLicense"));
    text->setReadOnly(true);
    text->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    layout->addWidget(text, 1);
    return page;
}

void AboutDialog::setUpdateState(UpdateState state, const QString& version, const QUrl& page, const QString& error)
{
    _downloadPage = page;
    switch (state) {
    case UpdateState::Disabled:
        _update->setText(tr("Checking for new versions is disabled."));
        break;
    case UpdateState::Checking:
        _update->setText(tr("Checking for a new version…"));
        break;
    case UpdateState::UpToDate:
        _update->setText(tr("✓ This is the latest version."));
        break;
    case UpdateState::Available:
        _update->setText(tr("<b>Edita %1 is available.</b> <a href=\"download\">Download page</a>").arg(version.toHtmlEscaped()));
        break;
    case UpdateState::Failed:
        _update->setText(tr("Could not check for a new version: %1").arg(error.toHtmlEscaped()));
        break;
    }
}

QString AboutDialog::copyText() const
{
    return about::copyText(_context.versionLine, _context.build, _context.system, _context.plugins);
}

} // namespace edita
