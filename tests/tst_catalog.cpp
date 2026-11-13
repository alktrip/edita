// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 56: la lista firmada del catálogo de plugins (core/Minisign, core/RemoteCatalog).

#include "core/Minisign.h"
#include "core/PluginInstaller.h"
#include "core/RemoteCatalog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

using namespace edita;

namespace {

QByteArray readFile(const QString& path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

void writeFile(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

const QByteArray kKeyId = QByteArray::fromHex("0102030405060708");
const QByteArray kSeed = QByteArray::fromHex("9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60");

minisign::PublicKey testKey()
{
    minisign::PublicKey key;
    minisign::parsePublicKey(minisign::publicKeyFromSeed(kKeyId, kSeed), key);
    return key;
}

QJsonObject plugin(const QString& folder, const QString& repository, const QString& recipe = QString())
{
    QJsonObject o{{"folder-name", folder}, {"display-name", folder + " plugin"}, {"version", "1.0"},
                  {"npp-compatible-versions", "[0.0.1,]"}, {"id", repository.isEmpty() ? "" : QString(64, 'a')},
                  {"repository", repository}, {"description", "…"}, {"author", "Autor"}, {"homepage", "https://ejemplo.org"},
                  {"license", "MIT"}, {"edita-build", repository.isEmpty() ? "recipe" : "binary"}};
    if (!recipe.isEmpty())
        o["edita-recipe"] = recipe;
    return o;
}

QByteArray catalogJson(const QString& version, const QJsonArray& plugins, const QString& arch = QStringLiteral("linux-x86_64"),
                       int api = 1, const QString& qt = QStringLiteral("6.8.3"), const QString& recipesSha = QString())
{
    return QJsonDocument(QJsonObject{{"name", "Edita plugin catalog"}, {"version", version}, {"arch", arch},
                                     {"edita-plugin-api", api}, {"edita-qt", qt},
                                     {"edita-recipes", "https://ejemplo.org/recipes.zip"},
                                     {"edita-recipes-sha256", recipesSha}, {"npp-plugins", plugins}})
        .toJson();
}

} // namespace

class TestCatalog : public QObject
{
    Q_OBJECT

private slots:
    // La descripción en el idioma de la interfaz («description-es»), si la hay; si no, la
    // general, en inglés
    void descriptionInTheInterfaceLanguage()
    {
        QJsonObject withSpanish = plugin(QStringLiteral("Uno"), QStringLiteral("https://ejemplo.org/uno.zip"));
        withSpanish["description"] = QStringLiteral("Reverses the lines");
        withSpanish["description-es"] = QStringLiteral("Invierte las líneas");
        QJsonObject onlyEnglish = plugin(QStringLiteral("Dos"), QStringLiteral("https://ejemplo.org/dos.zip"));
        onlyEnglish["description"] = QStringLiteral("Sorts the lines");
        const QByteArray json = catalogJson(QStringLiteral("1"), {withSpanish, onlyEnglish});
        const QLocale previous;
        QLocale::setDefault(QLocale(QLocale::Spanish, QLocale::Mexico));
        PluginCatalog catalog;
        QVERIFY(PluginCatalog::parse(json, catalog));
        QCOMPARE(catalog.find(QStringLiteral("Uno"))->description, QStringLiteral("Invierte las líneas"));
        QCOMPARE(catalog.find(QStringLiteral("Dos"))->description, QStringLiteral("Sorts the lines"));
        QLocale::setDefault(QLocale(QLocale::English));
        QVERIFY(PluginCatalog::parse(json, catalog));
        QCOMPARE(catalog.find(QStringLiteral("Uno"))->description, QStringLiteral("Reverses the lines"));
        QLocale::setDefault(previous);
    }

    // La lista que publicó la CI del catálogo (catalog.py, en Python), con la clave que trae Edita
    void verifiesThePublishedCatalog()
    {
        minisign::PublicKey key;
        QVERIFY(minisign::parsePublicKey(QString::fromLatin1(remote_catalog::kPublicKey), key));
        QCOMPARE(minisign::keyIdText(key.keyId), QStringLiteral("F1C3EC604B709A70"));
        const QByteArray json = readFile(QStringLiteral(EDITA_FIXTURES_DIR "/catalog/plugins.json"));
        const QByteArray signature = readFile(QStringLiteral(EDITA_FIXTURES_DIR "/catalog/plugins.json.minisig"));
        QString comment;
        QString error;
        QVERIFY2(minisign::verify(json, signature, key, &comment, &error), qPrintable(error));
        QVERIFY(comment.contains(QStringLiteral("catalog:2026.11.03.6")));

        PluginCatalog catalog;
        QVERIFY2(remote_catalog::accept(json, signature, key, QString(), catalog, &error), qPrintable(error));
        QCOMPARE(catalog.version, QStringLiteral("2026.11.03.6"));
        QCOMPARE(catalog.arch, QStringLiteral("linux-x86_64"));
        QCOMPARE(catalog.pluginApi, 1);
        QCOMPARE(catalog.qtVersion, QStringLiteral("6.8.3"));
        QCOMPARE(catalog.recipesSha256.size(), 64);
        const PluginInfo* reverse = catalog.find(QStringLiteral("qkNppReverseLines"));
        QVERIFY(reverse && reverse->fromCatalog);
        QCOMPARE(reverse->license, QStringLiteral("GPL-3.0-or-later"));
        QVERIFY(reverse->repository.startsWith(QStringLiteral("https://github.com/alktrip/edita-plugins/releases/download/")));
        QVERIFY(!reverse->buildsFromSource());
        const PluginInfo* json5 = catalog.find(QStringLiteral("SimplifyJson"));
        QVERIFY(json5 && json5->repository.isEmpty());
        QCOMPARE(json5->catalogRecipe, QStringLiteral("recipes/SimplifyJson/recipe.json"));
        QVERIFY(json5->buildsFromSource());

        // Un byte cambiado en la lista, o la firma con un byte cambiado: no
        QByteArray tampered = json;
        tampered.replace("\"2.3\"", "\"2.4\"");
        QVERIFY(!minisign::verify(tampered, signature, key, nullptr, &error));
        QVERIFY(error.contains(QStringLiteral("does not match")));
        QVERIFY(!remote_catalog::accept(tampered, signature, key, QString(), catalog, &error));
        QByteArray forgedComment = signature;
        forgedComment.replace("catalog:2026.11.03.6", "catalog:2099.01.01.1");
        QVERIFY(!minisign::verify(json, forgedComment, key, nullptr, &error));
        QVERIFY(error.contains(QStringLiteral("trusted comment")));
        // Otra clave
        QVERIFY(!minisign::verify(json, signature, testKey(), nullptr, &error));
        QVERIFY(error.contains(QStringLiteral("another key")));
    }

    void signsAndVerifies()
    {
        // RFC 8032, 7.1, TEST 1: la clave pública de la semilla
        const minisign::PublicKey key = testKey();
        QCOMPARE(key.key.toHex(), QByteArray("d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a"));
        QCOMPARE(minisign::keyIdText(kKeyId), QStringLiteral("0807060504030201"));
        const QByteArray data = "{\"npp-plugins\": []}\n";
        const QByteArray signature = minisign::sign(data, kKeyId, kSeed, QStringLiteral("timestamp:1\tfile:plugins.json"));
        QString comment;
        QVERIFY(minisign::verify(data, signature, key, &comment));
        QCOMPARE(comment, QStringLiteral("timestamp:1\tfile:plugins.json"));
        QVERIFY(!minisign::verify(data + " ", signature, key));
        // Formatos que no son de minisign
        QString error;
        QVERIFY(!minisign::verify(data, "no es una firma", key, nullptr, &error));
        // Un algoritmo desconocido
        QList<QByteArray> lines = signature.split('\n');
        QByteArray raw = QByteArray::fromBase64(lines[1]);
        raw[1] = 'X';
        lines[1] = raw.toBase64();
        QVERIFY(!minisign::verify(data, lines.join('\n'), key, nullptr, &error));
        QVERIFY(error.contains(QStringLiteral("unknown")));
        minisign::PublicKey bad;
        QVERIFY(!minisign::parsePublicKey(QStringLiteral("RWQ="), bad));
        QVERIFY(!minisign::parsePublicKey(QStringLiteral("untrusted comment: x\nnobase64!!\n"), bad));
        QVERIFY(!minisign::verify(data, signature, bad));
    }

    void acceptsOnlyNewerLists()
    {
        const minisign::PublicKey key = testKey();
        const QByteArray json = catalogJson(QStringLiteral("2026.11.03.6"), {plugin("Uno", "https://ejemplo.org/Uno-1.zip")});
        const QByteArray signature = minisign::sign(json, kKeyId, kSeed, QStringLiteral("catalog"));
        PluginCatalog catalog;
        QString error;
        QVERIFY(remote_catalog::accept(json, signature, key, QString(), catalog, &error));
        QVERIFY(remote_catalog::accept(json, signature, key, QStringLiteral("2026.11.03.6"), catalog, &error)); // la misma
        QVERIFY(remote_catalog::accept(json, signature, key, QStringLiteral("2026.11.02.9"), catalog, &error));
        // Una lista anterior a la conocida (aunque esté bien firmada): no se vuelve atrás
        QVERIFY(!remote_catalog::accept(json, signature, key, QStringLiteral("2026.11.03.7"), catalog, &error));
        QVERIFY(error.contains(QStringLiteral("older")));
        QVERIFY(!remote_catalog::accept(json, signature, key, QStringLiteral("2027.1.1.1"), catalog, &error));
        // Bien firmada pero no es una lista
        const QByteArray notJson = "[1, 2]";
        QVERIFY(!remote_catalog::accept(notJson, minisign::sign(notJson, kKeyId, kSeed, QStringLiteral("x")), key, QString(), catalog,
                                        &error));
        QVERIFY(error.contains(QStringLiteral("not valid")));
    }

    void adaptsToThisComputer()
    {
        const QJsonArray plugins{plugin("ConReceta", "https://ejemplo.org/ConReceta-1.zip", "recipes/ConReceta/recipe.json"),
                                 plugin("SoloBinario", "https://ejemplo.org/SoloBinario-1.zip"),
                                 plugin("SoloReceta", QString(), "recipes/SoloReceta/recipe.json")};
        auto parsed = [&](const QString& arch, int api, const QString& qt) {
            PluginCatalog catalog;
            PluginCatalog::parse(catalogJson(QStringLiteral("1"), plugins, arch, api, qt), catalog);
            return catalog;
        };
        const remote_catalog::Runtime runtime{QStringLiteral("linux-x86_64"), 1, QStringLiteral("6.10.1")};

        // Todo sirve (un Qt más nuevo que el de la compilación también)
        PluginCatalog same = parsed(QStringLiteral("linux-x86_64"), 1, QStringLiteral("6.8.3"));
        QVERIFY(remote_catalog::adapt(same, runtime).isEmpty());
        QCOMPARE(same.plugins.size(), std::size_t{3});
        QVERIFY(!same.find(QStringLiteral("ConReceta"))->buildsFromSource());

        // Otra arquitectura, otra API, un Qt más antiguo u otro Qt mayor: los binarios no
        // sirven; el que tiene receta se compila, el que no, se quita
        const struct { QString arch; int api; QString qt; QString reason; } cases[] = {
            {QStringLiteral("linux-aarch64"), 1, QStringLiteral("6.8.3"), QStringLiteral("linux-aarch64")},
            {QStringLiteral("linux-x86_64"), 2, QStringLiteral("6.8.3"), QStringLiteral("version 2")},
            {QStringLiteral("linux-x86_64"), 1, QStringLiteral("6.11.0"), QStringLiteral("Qt 6.11.0")},
            {QStringLiteral("linux-x86_64"), 1, QStringLiteral("7.0.0"), QStringLiteral("Qt 7.0.0")},
        };
        for (const auto& c : cases) {
            PluginCatalog catalog = parsed(c.arch, c.api, c.qt);
            const QString reason = remote_catalog::adapt(catalog, runtime);
            QVERIFY2(reason.contains(c.reason), qPrintable(reason));
            QCOMPARE(catalog.plugins.size(), std::size_t{2});
            QVERIFY(!catalog.find(QStringLiteral("SoloBinario")));
            const PluginInfo* withRecipe = catalog.find(QStringLiteral("ConReceta"));
            QVERIFY(withRecipe->buildsFromSource());
            QVERIFY(withRecipe->id.isEmpty());
        }
        // El Edita en ejecución
        const remote_catalog::Runtime current = remote_catalog::currentRuntime(1);
        QCOMPARE(current.pluginApi, 1);
        QCOMPARE(current.qtVersion, QString::fromLatin1(qVersion()));
        QVERIFY(current.arch.startsWith(QStringLiteral("linux-")));
    }

    void mergesWithTheBundledList()
    {
        PluginCatalog bundled;
        PluginCatalog::parse(catalogJson(QStringLiteral("1.0.0"), {plugin("PythonScript", QString()), plugin("Comun", QString())}),
                             bundled);
        bundled.sourcePath = QStringLiteral("/usr/share/edita/plugins/editaPluginList.json");
        PluginCatalog remote;
        PluginCatalog::parse(catalogJson(QStringLiteral("2026.11.03.6"),
                                         {plugin("Comun", "https://ejemplo.org/Comun-1.zip"), plugin("Nuevo", "https://ejemplo.org/N.zip")}),
                             remote);
        const PluginCatalog merged = remote_catalog::merge(bundled, remote);
        QCOMPARE(merged.version, QStringLiteral("2026.11.03.6"));
        QCOMPARE(merged.sourcePath, bundled.sourcePath);
        QCOMPARE(merged.plugins.size(), std::size_t{3});
        QVERIFY(merged.find(QStringLiteral("Comun"))->repository.isEmpty()); // el de Edita gana
        QVERIFY(merged.find(QStringLiteral("Nuevo")));
        QCOMPARE(merged.recipesUrl, QStringLiteral("https://ejemplo.org/recipes.zip"));
    }

    void storesAndReloads()
    {
        QTemporaryDir tmp;
        qputenv("EDITA_PLUGIN_CATALOG_URL", tmp.filePath("servidor/plugins.json").toUtf8());
        qputenv("EDITA_PLUGIN_CATALOG_PUBKEY", minisign::publicKeyFromSeed(kKeyId, kSeed).toUtf8());
        const remote_catalog::Settings settings = remote_catalog::settings(tmp.filePath("plugins"));
        QVERIFY(settings.enabled());
        QVERIFY(settings.listUrl.isLocalFile());
        QCOMPARE(settings.signatureUrl.toLocalFile(), tmp.filePath("servidor/plugins.json.minisig"));
        QCOMPARE(settings.cacheDir, tmp.filePath("plugins/Config/catalog"));

        PluginCatalog catalog;
        QVERIFY(!remote_catalog::loadCached(settings, catalog)); // todavía no hay ninguna
        const QByteArray json = catalogJson(QStringLiteral("5"), {plugin("Uno", "https://ejemplo.org/Uno-1.zip")});
        const QByteArray signature = minisign::sign(json, kKeyId, kSeed, QStringLiteral("x"));
        QVERIFY(remote_catalog::store(settings, json, signature));
        QVERIFY(remote_catalog::loadCached(settings, catalog));
        QCOMPARE(catalog.version, QStringLiteral("5"));
        // Lo guardado se vuelve a comprobar al leerlo
        writeFile(settings.cacheDir + "/plugins.json", QByteArray(json).replace("Uno", "Dos"));
        QVERIFY(!remote_catalog::loadCached(settings, catalog));

        // Sin catálogo
        qputenv("EDITA_PLUGIN_CATALOG_URL", "none");
        QVERIFY(!remote_catalog::settings(tmp.filePath("plugins")).enabled());
        qunsetenv("EDITA_PLUGIN_CATALOG_URL");
        qunsetenv("EDITA_PLUGIN_CATALOG_PUBKEY");
        const remote_catalog::Settings defaults = remote_catalog::settings(tmp.filePath("plugins"));
        QCOMPARE(defaults.listUrl.toString(), QString::fromLatin1(remote_catalog::kDefaultListUrl));
        QCOMPARE(defaults.signatureUrl.toString(), QString::fromLatin1(remote_catalog::kDefaultListUrl) + ".minisig");
        QCOMPARE(minisign::keyIdText(defaults.key.keyId), QStringLiteral("F1C3EC604B709A70"));
    }

    void unpacksTheRecipes()
    {
        QTemporaryDir tmp;
        writeFile(tmp.filePath("src/recipes/Uno/recipe.json"), "{}");
        const QString zipPath = tmp.filePath("recipes.zip");
        QVERIFY(plugin_installer::createZip(zipPath, {{"recipes/Uno/recipe.json", tmp.filePath("src/recipes/Uno/recipe.json")}}));
        const QByteArray zip = readFile(zipPath);
        remote_catalog::Settings settings;
        settings.cacheDir = tmp.filePath("cache");
        PluginCatalog catalog;
        catalog.recipesSha256 = QString(64, '0');
        QString error;
        QVERIFY(!remote_catalog::unpackRecipes(settings, catalog, zip, &error)); // no es el de la lista
        QVERIFY(error.contains(QStringLiteral("do not match")));
        QVERIFY(!QFileInfo::exists(remote_catalog::recipesDir(settings, catalog)));
        catalog.recipesSha256 = plugin_installer::sha256Hex(zip);
        QVERIFY2(remote_catalog::unpackRecipes(settings, catalog, zip, &error), qPrintable(error));
        const QString dir = remote_catalog::recipesDir(settings, catalog);
        QVERIFY(QFileInfo::exists(dir + "/recipes/Uno/recipe.json"));
        QVERIFY(!QFileInfo::exists(dir + ".part"));
        QVERIFY(remote_catalog::unpackRecipes(settings, catalog, zip, &error)); // ya estaba
    }
};

QTEST_GUILESS_MAIN(TestCatalog)
#include "tst_catalog.moc"
