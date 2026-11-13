// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AboutInfo.h"

#include "BuildInfo.h"
#include "ThirdPartyVersions.h"

#include <QCoreApplication>
#include <QFileInfo>

#include <openssl/crypto.h>

namespace edita::about {

namespace {

// «OpenSSL 3.0.2 15 Mar 2022» → «3.0.2» (OPENSSL_VERSION_STRING no existe en la 1.1.1)
QString openSslVersion()
{
    const QString text = QString::fromLatin1(OpenSSL_version(OPENSSL_VERSION));
    const QStringList words = text.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    return words.size() > 1 ? words[1] : text;
}

} // namespace

std::vector<Component> components()
{
    const QString npp = QString::fromLatin1(EDITA_NPP_COMMIT);
    return {
        {QStringLiteral("Qt"), QString::fromLatin1(qVersion()), QStringLiteral("LGPL-3.0-only"), QUrl(QStringLiteral("https://www.qt.io/")),
         QCoreApplication::translate("About", "The interface"), QString()},
        {QStringLiteral("Scintilla"), QStringLiteral(EDITA_SCINTILLA_VERSION), QStringLiteral("HPND"),
         QUrl(QStringLiteral("https://www.scintilla.org/")), QCoreApplication::translate("About", "The editing component"),
         QStringLiteral(":/edita/third_party/scintilla/License.txt")},
        {QStringLiteral("Lexilla"), QStringLiteral(EDITA_LEXILLA_VERSION), QStringLiteral("HPND"),
         QUrl(QStringLiteral("https://www.scintilla.org/Lexilla.html")), QCoreApplication::translate("About", "Syntax highlighting"),
         QStringLiteral(":/edita/third_party/lexilla/License.txt")},
        {QStringLiteral("Boost.Regex"), QStringLiteral(EDITA_BOOST_VERSION), QStringLiteral("BSL-1.0"),
         QUrl(QStringLiteral("https://www.boost.org/library/latest/regex/")), QCoreApplication::translate("About", "Regular expressions"),
         QStringLiteral(":/edita/resources/licenses/BSL-1.0.txt")},
        {QStringLiteral("uchardet"), QStringLiteral("Notepad++ ") + npp,
         QStringLiteral("MPL-1.1 OR GPL-2.0-or-later OR LGPL-2.1-or-later"),
         QUrl(QStringLiteral("https://www.freedesktop.org/wiki/Software/uchardet/")),
         QCoreApplication::translate("About", "Detecting the encoding of files"), QString()},
        {QStringLiteral("pugixml"), QStringLiteral(EDITA_PUGIXML_VERSION), QStringLiteral("MIT"),
         QUrl(QStringLiteral("https://pugixml.org/")), QCoreApplication::translate("About", "Reading and writing the XML settings"),
         QStringLiteral(":/edita/third_party/pugixml/LICENSE.md")},
        {QStringLiteral("libvterm"), QStringLiteral(EDITA_VTERM_VERSION), QStringLiteral("MIT"),
         QUrl(QStringLiteral("https://www.leonerd.org.uk/code/libvterm/")), QCoreApplication::translate("About", "The integrated terminal"),
         QStringLiteral(":/edita/third_party/libvterm/LICENSE")},
        {QStringLiteral("md4c"), QStringLiteral(EDITA_MD4C_VERSION), QStringLiteral("MIT"), QUrl(QStringLiteral("https://github.com/mity/md4c")),
         QCoreApplication::translate("About", "The Markdown preview"), QStringLiteral(":/edita/third_party/md4c/LICENSE.md")},
        {QStringLiteral("OpenSSL"), openSslVersion(), QStringLiteral("Apache-2.0"), QUrl(QStringLiteral("https://www.openssl.org/")),
         QCoreApplication::translate("About", "Checking the signature of the plugin catalog"), QString()},
        {QStringLiteral("Notepad++"), npp, QStringLiteral("GPL-3.0-or-later"), QUrl(QStringLiteral("https://notepad-plus-plus.org/")),
         QCoreApplication::translate("About", "Inspiration; its settings files, languages and plugin interface"),
         QStringLiteral(":/edita/COPYING")},
    };
}

Install detectInstall(const QString& executable, const QString& appImage, const std::function<bool(const QString&)>& exists)
{
    if (!appImage.isEmpty())
        return Install::AppImage;
    // El .deb y el .rpm llevan el AppDir a /opt/edita (packaging/deb y packaging/rpm)
    if (executable.startsWith(QLatin1String("/opt/edita/")))
        return exists(QStringLiteral("/var/lib/dpkg/info/edita.list")) ? Install::Deb : Install::Rpm;
    if (executable.startsWith(QLatin1String("/usr/")))
        return Install::System;
    return Install::Source;
}

Install currentInstall()
{
    return detectInstall(QCoreApplication::applicationFilePath(), qEnvironmentVariable("APPIMAGE"),
                         [](const QString& path) { return QFileInfo::exists(path); });
}

QString installName(Install install)
{
    switch (install) {
    case Install::AppImage:
        return QStringLiteral("AppImage");
    case Install::Deb:
        return QStringLiteral(".deb");
    case Install::Rpm:
        return QStringLiteral(".rpm");
    case Install::System:
        return QCoreApplication::translate("About", "system package");
    case Install::Source:
        return QCoreApplication::translate("About", "built from source");
    }
    return QString();
}

QString buildCommit()
{
    return QStringLiteral(EDITA_BUILD_COMMIT);
}

QString buildDate()
{
    return QStringLiteral(EDITA_BUILD_DATE);
}

QString versionLine(const QString& version, const QString& architecture, Install install)
{
    return QStringLiteral("Edita %1 (%2, %3)").arg(version, architecture, installName(install));
}

QString copyText(const QString& versionLine, const QString& build, const QString& system, const QStringList& plugins)
{
    QStringList lines{versionLine};
    if (!build.isEmpty())
        lines << QStringLiteral("Build: ") + build;
    lines << QStringLiteral("System: ") + system;
    lines << QStringLiteral("Plugins: ") + (plugins.isEmpty() ? QStringLiteral("none") : plugins.join(QStringLiteral(", ")));
    return lines.join(QLatin1Char('\n'));
}

QUrl releaseNotesUrl(const QString& homepage, const QString& version)
{
    return QUrl(homepage + QStringLiteral("/releases/tag/v") + version);
}

} // namespace edita::about
