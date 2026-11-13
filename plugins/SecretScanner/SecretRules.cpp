// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SecretRules.h"

#include <QCryptographicHash>
#include <QHash>
#include <QRegularExpression>

#include <algorithm>
#include <cmath>

namespace edita::secrets {

namespace {

enum class Kind
{
    Exact,   // el formato ya dice que es un secreto
    Generic, // una asignación: además, el valor tiene que parecer aleatorio
    EnvFile, // solo en archivos .env
};

struct Rule
{
    const char* id;
    const char* description;
    const char* pattern; // el grupo 1 es el secreto
    Kind kind = Kind::Exact;
};

// Primero las más concretas: si dos reglas encuentran lo mismo, gana la primera
// (sk-ant-… es de Anthropic, aunque también encajaría en la de OpenAI)
const Rule kRules[] = {
    {"private-key", "Private key", R"((-----BEGIN (?:RSA |EC |DSA |OPENSSH |ENCRYPTED |PGP )?PRIVATE KEY(?: BLOCK)?-----))"},
    {"aws-access-key-id", "AWS access key ID", R"(\b((?:AKIA|ASIA|ABIA|ACCA)[A-Z0-9]{16})\b)"},
    {"aws-secret-access-key", "AWS secret access key",
     R"((?i)aws_?secret_?access_?key["']?\s*[:=]\s*["']?([A-Za-z0-9/+]{40})(?![A-Za-z0-9/+]))"},
    {"github-token", "GitHub token", R"(\b((?:ghp|gho|ghu|ghs|ghr)_[A-Za-z0-9]{36})\b)"},
    {"github-fine-grained-token", "GitHub fine-grained token", R"(\b(github_pat_[A-Za-z0-9_]{82})\b)"},
    {"gitlab-token", "GitLab token", R"(\b(glpat-[A-Za-z0-9_\-]{20,})(?![A-Za-z0-9_\-]))"},
    {"slack-webhook", "Slack webhook URL", R"((https://hooks\.slack\.com/services/T[A-Z0-9]+/B[A-Z0-9]+/[A-Za-z0-9]{20,}))"},
    {"slack-token", "Slack token", R"(\b(xox[abposr]-[A-Za-z0-9-]{10,})(?![A-Za-z0-9-]))"},
    {"discord-webhook", "Discord webhook URL",
     R"((https://(?:ptb\.|canary\.)?discord(?:app)?\.com/api/webhooks/\d+/[A-Za-z0-9_\-]{60,}))"},
    {"anthropic-key", "Anthropic API key", R"(\b(sk-ant-(?:api|admin)\d{2}-[A-Za-z0-9_\-]{80,})(?![A-Za-z0-9_\-]))"},
    {"openai-key", "OpenAI API key", R"(\b(sk-(?:proj-|svcacct-|admin-)?[A-Za-z0-9_\-]{40,})(?![A-Za-z0-9_\-]))"},
    {"stripe-secret-key", "Stripe secret key", R"(\b((?:sk|rk)_live_[A-Za-z0-9]{20,})\b)"},
    {"google-api-key", "Google API key", R"(\b(AIza[0-9A-Za-z_\-]{35})(?![0-9A-Za-z_\-]))"},
    {"huggingface-token", "Hugging Face token", R"(\b(hf_[A-Za-z]{34})\b)"},
    {"npm-token", "npm token", R"(\b(npm_[A-Za-z0-9]{36})\b)"},
    {"pypi-token", "PyPI token", R"(\b(pypi-AgEIcHlwaS5vcmc[A-Za-z0-9_\-]{50,})(?![A-Za-z0-9_\-]))"},
    {"sendgrid-key", "SendGrid API key", R"(\b(SG\.[A-Za-z0-9_\-]{22}\.[A-Za-z0-9_\-]{43})(?![A-Za-z0-9_\-]))"},
    {"digitalocean-token", "DigitalOcean token", R"(\b(do[opr]_v1_[a-f0-9]{64})\b)"},
    {"telegram-bot-token", "Telegram bot token", R"(\b(\d{8,10}:AA[A-Za-z0-9_\-]{33})(?![A-Za-z0-9_\-]))"},
    {"azure-storage-key", "Azure storage account key", R"((?i)AccountKey=([A-Za-z0-9+/]{86}==))"},
    {"jwt", "JSON Web Token", R"(\b(eyJ[A-Za-z0-9_\-]{10,}\.eyJ[A-Za-z0-9_\-]{10,}\.[A-Za-z0-9_\-]{10,})(?![A-Za-z0-9_\-]))"},
    {"url-password", "Password in a URL", R"(\b[a-zA-Z][a-zA-Z0-9+.\-]*://[^\s:/@'"]+:([^\s:/@'"]{3,})@[A-Za-z0-9.\-]+)",
     Kind::Generic},
    {"env-secret", "Secret in a .env file",
     R"(^\s*(?:export\s+)?[A-Za-z0-9_]*(?:PASSWORD|PASSWD|SECRET|TOKEN|API_?KEY|ACCESS_?KEY|PRIVATE_?KEY|AUTH)[A-Za-z0-9_]*\s*=\s*["']?([^"'\s#]{8,}))",
     Kind::EnvFile},
    {"generic-secret", "Hard-coded secret",
     R"((?i)[\w.\-]*(?:password|passwd|pwd|secret|token|api[_\-]?key|access[_\-]?key|auth[_\-]?key|private[_\-]?key)[\w.\-]*["']?\s*(?::=|=>|[:=])\s*["'`]([^"'`\s]{8,})["'`])",
     Kind::Generic},
};

struct CompiledRule
{
    const Rule* rule;
    QRegularExpression re;
};

const std::vector<CompiledRule>& compiled()
{
    static const std::vector<CompiledRule> list = [] {
        std::vector<CompiledRule> out;
        for (const Rule& rule : kRules) {
            QRegularExpression re(QString::fromUtf8(rule.pattern), QRegularExpression::UseUnicodePropertiesOption);
            re.optimize();
            out.push_back({&rule, re});
        }
        return out;
    }();
    return list;
}

bool isEnvFile(const QString& fileName)
{
    const QString name = fileName.toLower();
    return name == QLatin1String(".env") || name.startsWith(QLatin1String(".env.")) || name.endsWith(QLatin1String(".env"));
}

} // namespace

const std::vector<RuleInfo>& rules()
{
    static const std::vector<RuleInfo> list = [] {
        std::vector<RuleInfo> out;
        for (const Rule& rule : kRules)
            out.push_back({QString::fromLatin1(rule.id), QString::fromLatin1(rule.description)});
        return out;
    }();
    return list;
}

const QStringList& allowLineMarkers()
{
    static const QStringList markers{QStringLiteral("edita:allow-secret"), QStringLiteral("gitleaks:allow"),
                                     QStringLiteral("pragma: allowlist secret")};
    return markers;
}

QString mask(const QString& secret)
{
    if (secret.size() <= 8)
        return QString(secret.size(), QLatin1Char('*'));
    return secret.left(4) + QStringLiteral("…") + secret.right(2);
}

double entropy(const QString& text)
{
    if (text.isEmpty())
        return 0;
    QHash<QChar, int> counts;
    for (const QChar c : text)
        ++counts[c];
    double bits = 0;
    for (const int n : std::as_const(counts)) {
        const double p = static_cast<double>(n) / static_cast<double>(text.size());
        bits -= p * std::log2(p);
    }
    return bits;
}

bool looksLikePlaceholder(const QString& value)
{
    const QString v = value.toLower();
    static const char* const kWords[] = {"changeme", "change_me", "change-me", "example", "placeholder", "your_", "your-",
                                         "yourpass", "password", "passwd", "secret", "redacted", "dummy", "sample",
                                         "xxxx", "****", "....", "<", "${", "{{", "%(", "$(", "process.env",
                                         "os.environ", "getenv", "env("};
    for (const char* word : kWords) {
        if (v.contains(QLatin1String(word)))
            return true;
    }
    if (v.startsWith(QLatin1Char('$')))
        return true; // $VARIABLE
    // Un solo carácter repetido («aaaaaaaa», «00000000»)
    return std::all_of(value.begin(), value.end(), [&](QChar c) { return c == value.front(); });
}

QString fingerprint(const QString& ruleId, const QString& secret)
{
    return QString::fromLatin1(
        QCryptographicHash::hash((ruleId + QLatin1Char(':') + secret).toUtf8(), QCryptographicHash::Sha256).toHex());
}

std::vector<Finding> scan(const QByteArray& text, const QString& fileName)
{
    std::vector<Finding> found;
    const bool env = isEnvFile(fileName);
    qsizetype lineStart = 0;
    int lineNumber = 0;
    while (lineStart <= text.size()) {
        qsizetype lineEnd = text.indexOf('\n', lineStart);
        if (lineEnd < 0)
            lineEnd = text.size();
        QByteArray bytes = text.mid(lineStart, lineEnd - lineStart);
        if (bytes.endsWith('\r'))
            bytes.chop(1);
        // Una línea enorme (un archivo minificado) no se revisa: sería lento y casi nunca es
        // un secreto escrito a mano
        if (!bytes.isEmpty() && bytes.size() < 8192) {
            const QString line = QString::fromUtf8(bytes);
            const bool allowed = std::any_of(allowLineMarkers().begin(), allowLineMarkers().end(),
                                             [&line](const QString& m) { return line.contains(m, Qt::CaseInsensitive); });
            std::vector<std::pair<qsizetype, qsizetype>> taken; // [inicio, fin) ya encontrados en la línea
            for (const CompiledRule& c : compiled()) {
                if (allowed)
                    break;
                if (c.rule->kind == Kind::EnvFile && !env)
                    continue;
                for (auto it = c.re.globalMatch(line); it.hasNext();) {
                    const QRegularExpressionMatch m = it.next();
                    const QString secret = m.captured(1);
                    const qsizetype from = m.capturedStart(1);
                    const qsizetype to = m.capturedEnd(1);
                    if (secret.isEmpty())
                        continue;
                    const bool overlaps = std::any_of(taken.begin(), taken.end(), [&](const auto& r) {
                        return from < r.second && to > r.first;
                    });
                    if (overlaps)
                        continue;
                    if (c.rule->kind != Kind::Exact && (looksLikePlaceholder(secret) || entropy(secret) < 3.0))
                        continue;
                    taken.emplace_back(from, to);
                    Finding f;
                    f.ruleId = QString::fromLatin1(c.rule->id);
                    f.description = QString::fromLatin1(c.rule->description);
                    f.line = lineNumber;
                    f.column = static_cast<int>(from);
                    f.start = lineStart + line.left(from).toUtf8().size();
                    f.length = secret.toUtf8().size();
                    f.secret = secret;
                    f.fingerprint = fingerprint(f.ruleId, secret);
                    found.push_back(std::move(f));
                }
            }
        }
        if (lineEnd >= text.size())
            break;
        lineStart = lineEnd + 1;
        ++lineNumber;
    }
    std::sort(found.begin(), found.end(), [](const Finding& a, const Finding& b) { return a.start < b.start; });
    return found;
}

} // namespace edita::secrets
