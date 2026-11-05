// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/UpdateCheck.h"

#include "core/ConfigXml.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStringList>

#include <algorithm>
#include <optional>

namespace edita::update_check {

namespace {

constexpr const char* kConfig = "noUpdate";

struct Parsed
{
    int numbers[3] = {0, 0, 0};
    QStringList prerelease; // «beta.2» → {beta, 2}; vacío = versión final
};

std::optional<Parsed> parse(QString text)
{
    text = text.trimmed();
    if (text.startsWith(QLatin1Char('v')) || text.startsWith(QLatin1Char('V')))
        text.remove(0, 1);
    static const QRegularExpression re(QStringLiteral("^(\\d+)\\.(\\d+)\\.(\\d+)(?:-([0-9A-Za-z.]+))?$"));
    const QRegularExpressionMatch m = re.match(text);
    if (!m.hasMatch())
        return std::nullopt;
    Parsed p;
    for (int i = 0; i < 3; ++i)
        p.numbers[i] = m.captured(i + 1).toInt();
    if (!m.captured(4).isEmpty())
        p.prerelease = m.captured(4).split(QLatin1Char('.'));
    return p;
}

// SemVer 2.0 §11: los identificadores numéricos se comparan como números y van antes que
// los de texto; con todo igual, más identificadores es más nuevo
int comparePrerelease(const QStringList& a, const QStringList& b)
{
    if (a.isEmpty() || b.isEmpty())
        return a.isEmpty() == b.isEmpty() ? 0 : (a.isEmpty() ? 1 : -1); // la final es más nueva
    for (qsizetype i = 0; i < std::min(a.size(), b.size()); ++i) {
        bool aNum = false, bNum = false;
        const qulonglong an = a[i].toULongLong(&aNum);
        const qulonglong bn = b[i].toULongLong(&bNum);
        if (aNum && bNum) {
            if (an != bn)
                return an < bn ? -1 : 1;
        } else if (aNum != bNum) {
            return aNum ? -1 : 1;
        } else if (const int c = a[i].compare(b[i]); c != 0) {
            return c < 0 ? -1 : 1;
        }
    }
    return a.size() == b.size() ? 0 : (a.size() < b.size() ? -1 : 1);
}

} // namespace

bool isValidVersion(const QString& version)
{
    return parse(version).has_value();
}

int compareVersions(const QString& a, const QString& b)
{
    const std::optional<Parsed> pa = parse(a);
    const std::optional<Parsed> pb = parse(b);
    if (!pa || !pb)
        return pa.has_value() == pb.has_value() ? 0 : (pa ? 1 : -1);
    for (int i = 0; i < 3; ++i) {
        if (pa->numbers[i] != pb->numbers[i])
            return pa->numbers[i] < pb->numbers[i] ? -1 : 1;
    }
    return comparePrerelease(pa->prerelease, pb->prerelease);
}

bool parseLatestRelease(const QByteArray& json, Release& release, QString* error)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    const QJsonObject o = doc.object();
    QString tag = o.value(QStringLiteral("tag_name")).toString();
    if (doc.isNull() || !doc.isObject() || !isValidVersion(tag)) {
        if (error)
            *error = QCoreApplication::translate("UpdateCheck", "The answer is not a version of Edita");
        return false;
    }
    if (tag.startsWith(QLatin1Char('v')) || tag.startsWith(QLatin1Char('V')))
        tag.remove(0, 1);
    release.version = tag;
    release.page = QUrl(o.value(QStringLiteral("html_url")).toString());
    release.appImage = QUrl();
    for (const QJsonValue& v : o.value(QStringLiteral("assets")).toArray()) {
        const QJsonObject asset = v.toObject();
        if (asset.value(QStringLiteral("name")).toString().endsWith(QLatin1String("-x86_64.AppImage"))) {
            release.appImage = QUrl(asset.value(QStringLiteral("browser_download_url")).toString());
            break;
        }
    }
    return true;
}

State readState(const ConfigXml& config)
{
    const QString name = QString::fromLatin1(kConfig);
    State s;
    s.nextCheck = QDate::fromString(config.guiConfig(name, QStringLiteral("nextUpdateDate")), QStringLiteral("yyyyMMdd"));
    s.skippedVersion = config.guiConfig(name, QStringLiteral("skippedVersion"));
    return s;
}

void writeState(ConfigXml& config, const State& s)
{
    const QString name = QString::fromLatin1(kConfig);
    config.setGuiConfig(name, QStringLiteral("nextUpdateDate"),
                        s.nextCheck.isValid() ? s.nextCheck.toString(QStringLiteral("yyyyMMdd")) : QString());
    config.setGuiConfig(name, QStringLiteral("skippedVersion"), s.skippedVersion);
}

bool isDue(bool onStartup, const State& s, const QDate& today)
{
    return onStartup && (!s.nextCheck.isValid() || today >= s.nextCheck);
}

void scheduleNext(State& s, int intervalDays, const QDate& today)
{
    s.nextCheck = today.addDays(std::abs(intervalDays)); // como launchUpdater
}

QUrl releaseUrl(const QString& homepage)
{
    const QString env = qEnvironmentVariable("EDITA_UPDATE_URL");
    if (env == QLatin1String("none"))
        return {};
    if (!env.isEmpty())
        return QUrl(env).scheme().isEmpty() ? QUrl::fromLocalFile(env) : QUrl(env);
    // https://github.com/<dueño>/<repo> → https://api.github.com/repos/<dueño>/<repo>/releases/latest
    const QUrl home(homepage);
    if (home.host() != QLatin1String("github.com"))
        return {};
    return QUrl(QStringLiteral("https://api.github.com/repos") + home.path() + QStringLiteral("/releases/latest"));
}

} // namespace edita::update_check
