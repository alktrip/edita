// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// FTP y SFTP (fase 90): un plugin propio de Edita, escrito desde cero en Qt. Hace lo que
// NppFTP en Notepad++: un panel con los perfiles de los servidores y el árbol de carpetas
// remotas; un archivo se abre con doble clic (se descarga a una copia local) y, al guardarlo
// en Edita, se sube solo. Se compila sin moc (el SDK de las recetas no lo trae) para poder
// distribuirlo con el catálogo de plugins.

#include "FtpCore.h"
#include "FtpSession.h"

#include <edita_plugin.h>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QSplitter>
#include <QStandardPaths>
#include <QTime>
#include <QToolButton>
#include <QTranslator>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <dlfcn.h>

#include <algorithm>
#include <cstring>
#include <map>
#include <string>

using namespace edita::ftp;

namespace {

constexpr const char* kPluginName = "FTP";
constexpr const char* kModuleName = "EditaFTP.so";
constexpr const char* kVersion = "1.0.0";

enum Command { kPanel, kProfiles, kUploadCurrent, kSeparator, kAbout, kCommandCount };

enum Role { PathRole = Qt::UserRole, KindRole, LoadedRole };
enum Kind { FileKind, DirKind, LinkKind };

struct State
{
    EditaData data{};
    std::string names[kCommandCount];
    // Las copias locales abiertas: su archivo remoto, para subirlo al guardar
    struct Remote
    {
        QString profile;
        QString path;
    };
    std::map<QString, Remote> opened;
    std::map<QString, QString> passwords; // por perfil, solo mientras Edita está abierto
} g;

EditaFuncItem g_funcs[kCommandCount];

intptr_t host(unsigned message, uintptr_t w = 0, intptr_t l = 0)
{
    return g.data.send(g.data.host, message, w, l);
}

QString hostString(unsigned message, uintptr_t w = 0)
{
    std::string buffer(4096, '\0');
    if (message == NPPM_GETFULLPATHFROMBUFFERID)
        host(message, w, reinterpret_cast<intptr_t>(buffer.data()));
    else
        host(message, buffer.size(), reinterpret_cast<intptr_t>(buffer.data()));
    return QString::fromUtf8(buffer.c_str());
}

QString profilesPath()
{
    return QDir(hostString(NPPM_GETPLUGINSCONFIGDIR)).filePath(QStringLiteral("EditaFTP.ini"));
}

QString cacheRoot()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)).filePath(QStringLiteral("edita/ftp"));
}

QString protocolName(Protocol p)
{
    switch (p) {
    case Protocol::Sftp:
        return QCoreApplication::translate("EditaFTP", "SFTP (SSH)");
    case Protocol::Ftp:
        return QCoreApplication::translate("EditaFTP", "FTP (not encrypted)");
    case Protocol::FtpExplicitTls:
        return QCoreApplication::translate("EditaFTP", "FTPES (FTP with explicit TLS)");
    case Protocol::FtpImplicitTls:
        return QCoreApplication::translate("EditaFTP", "FTPS (FTP with implicit TLS)");
    }
    return QString();
}

// --- Los perfiles ----------------------------------------------------------------------------

class ProfilesDialog : public QDialog
{
public:
    ProfilesDialog(std::vector<Profile> profiles, int current, QWidget* parent)
        : QDialog(parent)
        , _profiles(std::move(profiles))
    {
        setObjectName(QStringLiteral("ftpProfilesDialog"));
        setWindowTitle(QCoreApplication::translate("EditaFTP", "FTP Profiles"));
        auto* layout = new QHBoxLayout(this);
        auto* left = new QVBoxLayout;
        _list = new QListWidget(this);
        _list->setObjectName(QStringLiteral("ftpProfileList"));
        left->addWidget(_list, 1);
        auto* buttons = new QHBoxLayout;
        auto* add = new QPushButton(QCoreApplication::translate("EditaFTP", "Add"), this);
        add->setObjectName(QStringLiteral("ftpProfileAdd"));
        auto* copy = new QPushButton(QCoreApplication::translate("EditaFTP", "Copy"), this);
        auto* remove = new QPushButton(QCoreApplication::translate("EditaFTP", "Delete"), this);
        remove->setObjectName(QStringLiteral("ftpProfileRemove"));
        buttons->addWidget(add);
        buttons->addWidget(copy);
        buttons->addWidget(remove);
        left->addLayout(buttons);
        layout->addLayout(left, 1);

        auto* right = new QVBoxLayout;
        auto* form = new QFormLayout;
        _name = new QLineEdit(this);
        _name->setObjectName(QStringLiteral("ftpProfileName"));
        _protocol = new QComboBox(this);
        _protocol->setObjectName(QStringLiteral("ftpProfileProtocol"));
        for (const Protocol p : {Protocol::Sftp, Protocol::Ftp, Protocol::FtpExplicitTls, Protocol::FtpImplicitTls})
            _protocol->addItem(protocolName(p), static_cast<int>(p));
        _host = new QLineEdit(this);
        _host->setObjectName(QStringLiteral("ftpProfileHost"));
        _port = new QSpinBox(this);
        _port->setObjectName(QStringLiteral("ftpProfilePort"));
        _port->setRange(0, 65535);
        _port->setSpecialValueText(QCoreApplication::translate("EditaFTP", "Default"));
        _user = new QLineEdit(this);
        _user->setObjectName(QStringLiteral("ftpProfileUser"));
        _user->setPlaceholderText(QCoreApplication::translate("EditaFTP", "Empty: anonymous (FTP) or your user name (SFTP)"));
        _dir = new QLineEdit(this);
        _dir->setObjectName(QStringLiteral("ftpProfileDir"));
        _dir->setPlaceholderText(QCoreApplication::translate("EditaFTP", "Empty: the home folder"));
        auto* keyRow = new QHBoxLayout;
        _key = new QLineEdit(this);
        _key->setObjectName(QStringLiteral("ftpProfileKey"));
        _key->setPlaceholderText(QCoreApplication::translate("EditaFTP", "Empty: ~/.ssh and the SSH agent"));
        auto* browse = new QToolButton(this);
        browse->setText(QStringLiteral("…"));
        keyRow->addWidget(_key, 1);
        keyRow->addWidget(browse);
        _passive = new QCheckBox(QCoreApplication::translate("EditaFTP", "Passive mode"), this);
        _verify = new QCheckBox(QCoreApplication::translate("EditaFTP", "Verify the server certificate"), this);
        form->addRow(QCoreApplication::translate("EditaFTP", "Name:"), _name);
        form->addRow(QCoreApplication::translate("EditaFTP", "Protocol:"), _protocol);
        form->addRow(QCoreApplication::translate("EditaFTP", "Host:"), _host);
        form->addRow(QCoreApplication::translate("EditaFTP", "Port:"), _port);
        form->addRow(QCoreApplication::translate("EditaFTP", "User:"), _user);
        form->addRow(QCoreApplication::translate("EditaFTP", "Initial folder:"), _dir);
        form->addRow(QCoreApplication::translate("EditaFTP", "Private key:"), keyRow);
        form->addRow(QString(), _passive);
        form->addRow(QString(), _verify);
        right->addLayout(form);
        auto* note = new QLabel(QCoreApplication::translate(
            "EditaFTP", "Passwords are not saved: Edita asks for them when connecting and forgets them when it closes."), this);
        note->setWordWrap(true);
        right->addWidget(note);
        right->addStretch(1);
        auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        right->addWidget(box);
        layout->addLayout(right, 2);
        _form = {_name, _protocol, _host, _port, _user, _dir, _key, browse, _passive, _verify};

        for (const Profile& p : _profiles)
            _list->addItem(p.name);
        connect(_list, &QListWidget::currentRowChanged, this, [this](int row) { show(row); });
        auto edited = [this] { store(); };
        for (QLineEdit* e : {_name, _host, _user, _dir, _key})
            connect(e, &QLineEdit::textEdited, this, edited);
        connect(_port, &QSpinBox::valueChanged, this, edited);
        connect(_protocol, &QComboBox::currentIndexChanged, this, [this] {
            store();
            updateEnabled();
        });
        connect(_passive, &QCheckBox::toggled, this, edited);
        connect(_verify, &QCheckBox::toggled, this, edited);
        connect(browse, &QToolButton::clicked, this, [this] {
            const QString path = QFileDialog::getOpenFileName(this, QCoreApplication::translate("EditaFTP", "Private Key"),
                                                              QDir::home().filePath(QStringLiteral(".ssh")));
            if (!path.isEmpty()) {
                _key->setText(path);
                store();
            }
        });
        connect(add, &QPushButton::clicked, this, [this] {
            Profile p;
            p.name = uniqueName(QCoreApplication::translate("EditaFTP", "New server"));
            append(p);
            _name->setFocus();
            _name->selectAll();
        });
        connect(copy, &QPushButton::clicked, this, [this] {
            const int row = _list->currentRow();
            if (row < 0)
                return;
            Profile p = _profiles[static_cast<size_t>(row)];
            p.name = uniqueName(p.name);
            append(p);
        });
        connect(remove, &QPushButton::clicked, this, [this] {
            const int row = _list->currentRow();
            if (row < 0)
                return;
            _profiles.erase(_profiles.begin() + row);
            delete _list->takeItem(row);
            show(_list->currentRow());
        });
        connect(box, &QDialogButtonBox::accepted, this, [this] {
            for (const Profile& p : _profiles) {
                if (p.host.trimmed().isEmpty()) {
                    QMessageBox::warning(this, windowTitle(), QCoreApplication::translate("EditaFTP", "The profile %1 has no host.").arg(p.name));
                    return;
                }
            }
            accept();
        });
        connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
        _list->setCurrentRow(_profiles.empty() ? -1 : std::clamp(current, 0, static_cast<int>(_profiles.size()) - 1));
        show(_list->currentRow());
        resize(640, 360);
    }

    const std::vector<Profile>& profiles() const { return _profiles; }
    int currentRow() const { return _list->currentRow(); }

private:
    QString uniqueName(const QString& base) const
    {
        QString name = base;
        for (int n = 2;; ++n) {
            const bool taken = std::any_of(_profiles.begin(), _profiles.end(), [&](const Profile& p) { return p.name == name; });
            if (!taken)
                return name;
            name = QStringLiteral("%1 (%2)").arg(base).arg(n);
        }
    }

    void append(const Profile& p)
    {
        _profiles.push_back(p);
        _list->addItem(p.name);
        _list->setCurrentRow(_list->count() - 1);
    }

    void show(int row)
    {
        _loading = true;
        const bool valid = row >= 0 && row < static_cast<int>(_profiles.size());
        for (QWidget* w : _form)
            w->setEnabled(valid);
        const Profile p = valid ? _profiles[static_cast<size_t>(row)] : Profile{};
        _name->setText(p.name);
        _protocol->setCurrentIndex(_protocol->findData(static_cast<int>(p.protocol)));
        _host->setText(p.host);
        _port->setValue(p.port);
        _user->setText(p.user);
        _dir->setText(p.initialDir);
        _key->setText(p.keyFile);
        _passive->setChecked(p.passive);
        _verify->setChecked(p.verifyCertificate);
        _loading = false;
        if (valid)
            updateEnabled();
    }

    void updateEnabled()
    {
        const auto protocol = static_cast<Protocol>(_protocol->currentData().toInt());
        _key->setEnabled(protocol == Protocol::Sftp);
        _passive->setEnabled(isFtp(protocol));
        _verify->setEnabled(protocol == Protocol::FtpExplicitTls || protocol == Protocol::FtpImplicitTls);
    }

    void store()
    {
        const int row = _list->currentRow();
        if (_loading || row < 0)
            return;
        Profile& p = _profiles[static_cast<size_t>(row)];
        p.name = _name->text().trimmed();
        p.protocol = static_cast<Protocol>(_protocol->currentData().toInt());
        p.host = _host->text().trimmed();
        p.port = _port->value();
        p.user = _user->text().trimmed();
        p.initialDir = _dir->text().trimmed();
        p.keyFile = _key->text().trimmed();
        p.passive = _passive->isChecked();
        p.verifyCertificate = _verify->isChecked();
        _list->item(row)->setText(p.name);
    }

    std::vector<Profile> _profiles;
    QListWidget* _list = nullptr;
    QLineEdit* _name = nullptr;
    QComboBox* _protocol = nullptr;
    QLineEdit* _host = nullptr;
    QSpinBox* _port = nullptr;
    QLineEdit* _user = nullptr;
    QLineEdit* _dir = nullptr;
    QLineEdit* _key = nullptr;
    QCheckBox* _passive = nullptr;
    QCheckBox* _verify = nullptr;
    std::vector<QWidget*> _form;
    bool _loading = false;
};

// --- El panel --------------------------------------------------------------------------------

class Panel : public QWidget
{
public:
    Panel()
    {
        setObjectName(QStringLiteral("ftpPanel"));
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(4, 4, 4, 4);
        auto* top = new QHBoxLayout;
        _profile = new QComboBox(this);
        _profile->setObjectName(QStringLiteral("ftpProfile"));
        top->addWidget(_profile, 1);
        auto button = [this](QBoxLayout* bar, const QString& text, const char* name, const QString& tip) {
            auto* b = new QToolButton(this);
            b->setText(text);
            b->setToolTip(tip);
            b->setObjectName(QString::fromLatin1(name));
            b->setAutoRaise(true);
            bar->addWidget(b);
            return b;
        };
        _connect = button(top, QString(), "ftpConnect", QString());
        QToolButton* profiles = button(top, QCoreApplication::translate("EditaFTP", "Profiles…"), "ftpProfiles",
                                       QCoreApplication::translate("EditaFTP", "Add and edit servers"));
        layout->addLayout(top);

        auto* bar = new QHBoxLayout;
        _refresh = button(bar, QStringLiteral("⟳"), "ftpRefresh", QCoreApplication::translate("EditaFTP", "Refresh the folder"));
        _upload = button(bar, QStringLiteral("⇧"), "ftpUpload",
                         QCoreApplication::translate("EditaFTP", "Upload the current document to the selected folder"));
        _download = button(bar, QStringLiteral("⇩"), "ftpDownload", QCoreApplication::translate("EditaFTP", "Download the selected file to…"));
        _newFolder = button(bar, QStringLiteral("+"), "ftpNewFolder", QCoreApplication::translate("EditaFTP", "New folder"));
        _rename = button(bar, QStringLiteral("✎"), "ftpRename", QCoreApplication::translate("EditaFTP", "Rename"));
        _delete = button(bar, QStringLiteral("✕"), "ftpDelete", QCoreApplication::translate("EditaFTP", "Delete"));
        bar->addStretch(1);
        layout->addLayout(bar);

        auto* split = new QSplitter(Qt::Vertical, this);
        _tree = new QTreeWidget(split);
        _tree->setObjectName(QStringLiteral("ftpTree"));
        _tree->setHeaderLabels({QCoreApplication::translate("EditaFTP", "Name"), QCoreApplication::translate("EditaFTP", "Size"),
                                QCoreApplication::translate("EditaFTP", "Modified")});
        // El nombre ocupa lo que dejen el tamaño y la fecha (el panel suele ser angosto)
        _tree->header()->setStretchLastSection(false);
        _tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
        _tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        _tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        _tree->setColumnHidden(2, true); // la fecha y los permisos, en la descripción emergente
        _tree->setContextMenuPolicy(Qt::CustomContextMenu);
        _log = new QPlainTextEdit(split);
        _log->setObjectName(QStringLiteral("ftpLog"));
        _log->setReadOnly(true);
        _log->setMaximumBlockCount(1000);
        split->setStretchFactor(0, 4);
        split->setStretchFactor(1, 1);
        layout->addWidget(split, 1);

        _session.setLogger([log = QPointer<QPlainTextEdit>(_log)](const QString& line) {
            if (log)
                log->appendPlainText(QTime::currentTime().toString(QStringLiteral("HH:mm:ss ")) + line);
        });

        connect(_connect, &QToolButton::clicked, this, [this] { _session.isConnected() ? disconnectFromHost() : connectToHost(); });
        connect(profiles, &QToolButton::clicked, this, [this] { editProfiles(); });
        connect(_refresh, &QToolButton::clicked, this, [this] { refresh(selectedDirectory()); });
        connect(_upload, &QToolButton::clicked, this, [this] { uploadCurrent(); });
        connect(_download, &QToolButton::clicked, this, [this] { downloadSelected(); });
        connect(_newFolder, &QToolButton::clicked, this, [this] { newFolder(); });
        connect(_rename, &QToolButton::clicked, this, [this] { renameSelected(); });
        connect(_delete, &QToolButton::clicked, this, [this] { deleteSelected(); });
        connect(_tree, &QTreeWidget::itemExpanded, this, [this](QTreeWidgetItem* item) {
            if (!item->data(0, LoadedRole).toBool())
                list(item);
        });
        connect(_tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item, int) {
            if (item->data(0, KindRole).toInt() == FileKind)
                open(item->data(0, PathRole).toString());
        });
        connect(_tree, &QTreeWidget::currentItemChanged, this, [this] { updateButtons(); });
        connect(_tree, &QTreeWidget::customContextMenuRequested, this, [this](const QPoint& pos) { contextMenu(pos); });
        loadProfiles();
        updateButtons();
    }

    ~Panel() override { _session.disconnectFromHost(); }

    Session& session() { return _session; }

    void loadProfiles()
    {
        const QString current = _profile->currentText();
        _profiles = edita::ftp::loadProfiles(profilesPath());
        _profile->clear();
        for (const Profile& p : _profiles)
            _profile->addItem(p.name);
        const int index = _profile->findText(current);
        _profile->setCurrentIndex(index >= 0 ? index : 0);
        updateButtons();
    }

    void editProfiles()
    {
        ProfilesDialog dialog(_profiles, _profile->currentIndex(), this);
        if (dialog.exec() != QDialog::Accepted)
            return;
        if (!saveProfiles(profilesPath(), dialog.profiles())) {
            QMessageBox::warning(this, QLatin1String(kPluginName),
                                 QCoreApplication::translate("EditaFTP", "Cannot save the profiles to %1").arg(profilesPath()));
            return;
        }
        const QString chosen = dialog.currentRow() >= 0 ? dialog.profiles()[static_cast<size_t>(dialog.currentRow())].name : QString();
        loadProfiles();
        if (!chosen.isEmpty() && !_session.isConnected())
            _profile->setCurrentIndex(_profile->findText(chosen));
    }

    // Sube un archivo abierto desde el servidor al guardarlo
    void uploadSaved(const QString& local)
    {
        const auto it = g.opened.find(local);
        if (it == g.opened.end())
            return;
        const State::Remote remote = it->second;
        if (!_session.isConnected() || _session.profile().name != remote.profile) {
            appendLog(QCoreApplication::translate("EditaFTP", "! %1 was saved only on this computer: %2 is not connected.")
                          .arg(fileName(remote.path), remote.profile));
            QMessageBox::warning(this, QLatin1String(kPluginName),
                                 QCoreApplication::translate("EditaFTP", "%1 was saved only on this computer, because %2 is not connected.\n\n"
                                                                         "Connect to it and save again to upload it.")
                                     .arg(fileName(remote.path), remote.profile));
            return;
        }
        _session.run({Request::Upload, remote.path, local, {}}, [this, name = fileName(remote.path)](const Result& r) {
            if (r.ok)
                appendLog(QCoreApplication::translate("EditaFTP", "Uploaded %1.").arg(name));
            else
                failed(r);
        });
    }

private:
    void appendLog(const QString& line)
    {
        _log->appendPlainText(QTime::currentTime().toString(QStringLiteral("HH:mm:ss ")) + line);
    }

    const Profile* currentProfile() const
    {
        const int i = _profile->currentIndex();
        return i >= 0 && i < static_cast<int>(_profiles.size()) ? &_profiles[static_cast<size_t>(i)] : nullptr;
    }

    void updateButtons()
    {
        const bool connected = _session.isConnected();
        _connect->setText(connected ? QCoreApplication::translate("EditaFTP", "Disconnect") : QCoreApplication::translate("EditaFTP", "Connect"));
        _connect->setEnabled(connected || currentProfile());
        _profile->setEnabled(!connected);
        QTreeWidgetItem* item = _tree->currentItem();
        const bool isFile = item && item->data(0, KindRole).toInt() == FileKind;
        const bool isRoot = item && !item->parent();
        for (QToolButton* b : {_refresh, _upload, _newFolder})
            b->setEnabled(connected);
        _download->setEnabled(connected && isFile);
        _rename->setEnabled(connected && item && !isRoot);
        _delete->setEnabled(connected && item && !isRoot);
    }

    // --- Conectar ------------------------------------------------------------------------

    void connectToHost(bool acceptNewHostKey = false)
    {
        const Profile* profile = currentProfile();
        if (!profile)
            return;
        _session.setProfile(*profile);
        const auto saved = g.passwords.find(profile->name);
        if (saved != g.passwords.end())
            _session.setPassword(saved->second);
        else if (isFtp(profile->protocol) && !profile->user.isEmpty() && !askPassword())
            return; // FTP siempre la pide
        else if (!isFtp(profile->protocol))
            _session.setPassword(QString()); // SFTP primero prueba las claves y el agente
        _connect->setEnabled(false);
        _session.connectToHost(acceptNewHostKey, [this, guard = QPointer<Panel>(this)](const Result& r) {
            if (!guard)
                return;
            updateButtons();
            if (r.ok) {
                showRoot(r);
                return;
            }
            switch (r.error) {
            case ErrorKind::AuthenticationFailed:
                if (askPassword())
                    connectToHost();
                return;
            case ErrorKind::HostKeyUnknown:
                askHostKey();
                return;
            case ErrorKind::HostKeyChanged:
                QMessageBox::critical(this, QLatin1String(kPluginName),
                                      QCoreApplication::translate("EditaFTP",
                                                                  "The host key of %1 has changed since the last connection. Someone could be "
                                                                  "intercepting the connection, or the server was reinstalled.\n\n"
                                                                  "If you know why it changed, remove the old key with:\nssh-keygen -R %1")
                                          .arg(_session.profile().host));
                return;
            case ErrorKind::Certificate:
                QMessageBox::warning(this, QLatin1String(kPluginName),
                                     QCoreApplication::translate("EditaFTP", "The certificate of %1 is not valid:\n%2\n\n"
                                                                             "If the server uses its own certificate, turn off "
                                                                             "\"Verify the server certificate\" in its profile.")
                                         .arg(_session.profile().host, r.message));
                return;
            default:
                QMessageBox::warning(this, QLatin1String(kPluginName),
                                     QCoreApplication::translate("EditaFTP", "Cannot connect to %1:\n%2").arg(_session.profile().host, r.message));
            }
        });
    }

    bool askPassword()
    {
        const Profile& p = _session.profile();
        bool ok = false;
        const QString password = QInputDialog::getText(
            this, QLatin1String(kPluginName),
            QCoreApplication::translate("EditaFTP", "Password (or key passphrase) for %1:")
                .arg(p.user.isEmpty() ? p.host : p.user + QLatin1Char('@') + p.host),
            QLineEdit::Password, QString(), &ok);
        if (!ok) {
            g.passwords.erase(p.name);
            return false;
        }
        // Se recuerda mientras Edita esté abierto (para volver a conectar)
        g.passwords[p.name] = password;
        _session.setPassword(password);
        return true;
    }

    void askHostKey()
    {
        _session.hostFingerprint([this, guard = QPointer<Panel>(this)](const QString& fingerprint) {
            if (!guard)
                return;
            const auto answer = QMessageBox::question(
                this, QLatin1String(kPluginName),
                QCoreApplication::translate("EditaFTP", "This is the first connection to %1. Its key fingerprint is:\n\n%2\n\n"
                                                        "If it matches the one of the server, trust it and connect? It is saved in "
                                                        "~/.ssh/known_hosts, as ssh does.")
                    .arg(_session.profile().host, fingerprint.isEmpty() ? QCoreApplication::translate("EditaFTP", "(unknown)") : fingerprint));
            if (answer == QMessageBox::Yes)
                connectToHost(true);
        });
    }

    void disconnectFromHost()
    {
        _session.disconnectFromHost();
        _tree->clear();
        updateButtons();
    }

    // --- El árbol --------------------------------------------------------------------------

    static QString sizeText(const Entry& e)
    {
        if (e.directory || e.size < 0)
            return QString();
        return QLocale().formattedDataSize(e.size, 1, QLocale::DataSizeTraditionalFormat);
    }

    void fill(QTreeWidgetItem* parent, const QString& dir, const std::vector<Entry>& entries)
    {
        qDeleteAll(parent->takeChildren());
        for (const Entry& e : entries) {
            auto* item = new QTreeWidgetItem(parent, {e.name, sizeText(e), e.modified});
            const Kind kind = e.directory ? DirKind : (e.link ? LinkKind : FileKind);
            item->setData(0, PathRole, joinPath(dir, e.name));
            item->setData(0, KindRole, kind);
            item->setIcon(0, style()->standardIcon(e.directory ? QStyle::SP_DirIcon : (e.link ? QStyle::SP_FileLinkIcon : QStyle::SP_FileIcon)));
            item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
            if (kind != FileKind)
                item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
            QStringList tip{e.name};
            if (e.link)
                tip << QStringLiteral("→ ") + e.linkTarget;
            tip << QStringList{e.permissions, sizeText(e), e.modified}.filter(QRegularExpression(QStringLiteral("\\S"))).join(QStringLiteral("   "));
            item->setToolTip(0, tip.join(QLatin1Char('\n')));
        }
        parent->setData(0, LoadedRole, true);
    }

    void showRoot(const Result& r)
    {
        _tree->clear();
        auto* root = new QTreeWidgetItem(_tree, {r.directory.isEmpty() ? QStringLiteral("~") : r.directory});
        root->setData(0, PathRole, r.directory);
        root->setData(0, KindRole, DirKind);
        root->setIcon(0, style()->standardIcon(QStyle::SP_DriveNetIcon));
        fill(root, r.directory, r.entries);
        root->setExpanded(true);
        _tree->setCurrentItem(root);
        updateButtons();
    }

    void list(QTreeWidgetItem* item)
    {
        const QString path = item->data(0, PathRole).toString();
        _session.run({Request::List, path, {}, {}}, [this, path](const Result& r) {
            QTreeWidgetItem* target = find(path);
            if (!target)
                return;
            if (!r.ok) {
                // Un enlace a un archivo: no se puede listar
                if (target->data(0, KindRole).toInt() == LinkKind) {
                    target->setData(0, KindRole, FileKind);
                    target->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicator);
                    target->setExpanded(false);
                    return;
                }
                failed(r);
                return;
            }
            if (target->data(0, KindRole).toInt() == LinkKind)
                target->setData(0, KindRole, DirKind);
            fill(target, path, r.entries);
            if (target->childCount() == 0)
                target->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicator);
        });
    }

    // El elemento de una ruta (los elementos pueden haberse borrado mientras tanto)
    QTreeWidgetItem* find(const QString& path) const
    {
        QList<QTreeWidgetItem*> pending;
        for (int i = 0; i < _tree->topLevelItemCount(); ++i)
            pending << _tree->topLevelItem(i);
        while (!pending.isEmpty()) {
            QTreeWidgetItem* item = pending.takeFirst();
            if (item->data(0, PathRole).toString() == path)
                return item;
            for (int i = 0; i < item->childCount(); ++i)
                pending << item->child(i);
        }
        return nullptr;
    }

    QString selectedDirectory() const
    {
        QTreeWidgetItem* item = _tree->currentItem();
        if (!item)
            return _tree->topLevelItemCount() ? _tree->topLevelItem(0)->data(0, PathRole).toString() : QString();
        if (item->data(0, KindRole).toInt() == FileKind)
            item = item->parent() ? item->parent() : item;
        return item->data(0, PathRole).toString();
    }

    void refresh(const QString& dir)
    {
        if (QTreeWidgetItem* item = find(dir)) {
            item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
            list(item);
            item->setExpanded(true);
        }
    }

    void failed(const Result& r)
    {
        if (r.error == ErrorKind::ConnectionLost || r.error == ErrorKind::AuthenticationFailed) {
            QMessageBox::warning(this, QLatin1String(kPluginName),
                                 QCoreApplication::translate("EditaFTP", "The connection to %1 was lost:\n%2").arg(_session.profile().host, r.message));
            disconnectFromHost();
            return;
        }
        QMessageBox::warning(this, QLatin1String(kPluginName), r.message);
    }

    // --- Archivos --------------------------------------------------------------------------

    void open(const QString& path)
    {
        const QString local = cachePath(cacheRoot(), _session.profile().name, path);
        // Si ya está abierto, se va a él (puede tener cambios sin guardar)
        if (g.opened.count(local) && host(NPPM_SWITCHTOFILE, 0, reinterpret_cast<intptr_t>(local.toUtf8().constData())))
            return;
        QDir().mkpath(QFileInfo(local).absolutePath());
        _session.run({Request::Download, path, local, {}}, [this, path, local, profile = _session.profile().name](const Result& r) {
            if (!r.ok) {
                failed(r);
                return;
            }
            g.opened[local] = {profile, path};
            const QByteArray utf8 = local.toUtf8();
            host(NPPM_DOOPEN, 0, reinterpret_cast<intptr_t>(utf8.constData()));
        });
    }

    void uploadCurrent()
    {
        const QString local = hostString(NPPM_GETFULLCURRENTPATH);
        if (!QFileInfo(local).isFile()) {
            QMessageBox::information(this, QLatin1String(kPluginName),
                                     QCoreApplication::translate("EditaFTP", "Save the document to a file before uploading it."));
            return;
        }
        // Una copia de este servidor vuelve a su sitio; cualquier otro archivo, a la carpeta
        // seleccionada
        const auto it = g.opened.find(local);
        const bool fromHere = it != g.opened.end() && it->second.profile == _session.profile().name;
        const QString remote = fromHere ? it->second.path : joinPath(selectedDirectory(), QFileInfo(local).fileName());
        _session.run({Request::Upload, remote, local, {}}, [this, remote](const Result& r) {
            if (!r.ok) {
                failed(r);
                return;
            }
            appendLog(QCoreApplication::translate("EditaFTP", "Uploaded %1.").arg(fileName(remote)));
            refresh(parentPath(remote));
        });
    }

    void downloadSelected()
    {
        QTreeWidgetItem* item = _tree->currentItem();
        if (!item || item->data(0, KindRole).toInt() != FileKind)
            return;
        const QString path = item->data(0, PathRole).toString();
        const QString local = QFileDialog::getSaveFileName(this, QCoreApplication::translate("EditaFTP", "Download"),
                                                           QDir::home().filePath(fileName(path)));
        if (local.isEmpty())
            return;
        _session.run({Request::Download, path, local, {}}, [this, local](const Result& r) {
            if (!r.ok)
                failed(r);
            else
                appendLog(QCoreApplication::translate("EditaFTP", "Downloaded to %1.").arg(local));
        });
    }

    void newFolder()
    {
        const QString dir = selectedDirectory();
        bool ok = false;
        const QString name = QInputDialog::getText(this, QLatin1String(kPluginName), QCoreApplication::translate("EditaFTP", "New folder name:"),
                                                   QLineEdit::Normal, QString(), &ok).trimmed();
        if (!ok || name.isEmpty() || name.contains(QLatin1Char('/')))
            return;
        _session.run({Request::MakeDir, joinPath(dir, name), {}, {}}, [this, dir](const Result& r) {
            if (!r.ok)
                failed(r);
            refresh(dir);
        });
    }

    void renameSelected()
    {
        QTreeWidgetItem* item = _tree->currentItem();
        if (!item || !item->parent())
            return;
        const QString path = item->data(0, PathRole).toString();
        bool ok = false;
        const QString name = QInputDialog::getText(this, QLatin1String(kPluginName), QCoreApplication::translate("EditaFTP", "New name:"),
                                                   QLineEdit::Normal, item->text(0), &ok).trimmed();
        if (!ok || name.isEmpty() || name == item->text(0) || name.contains(QLatin1Char('/')))
            return;
        const QString dir = item->parent()->data(0, PathRole).toString();
        _session.run({Request::Rename, path, {}, joinPath(dir, name)}, [this, dir](const Result& r) {
            if (!r.ok)
                failed(r);
            refresh(dir);
        });
    }

    void deleteSelected()
    {
        QTreeWidgetItem* item = _tree->currentItem();
        if (!item || !item->parent())
            return;
        const bool isDir = item->data(0, KindRole).toInt() == DirKind;
        const auto answer = QMessageBox::question(
            this, QLatin1String(kPluginName),
            isDir ? QCoreApplication::translate("EditaFTP", "Delete the folder %1 from the server? It has to be empty.").arg(item->text(0))
                  : QCoreApplication::translate("EditaFTP", "Delete %1 from the server?").arg(item->text(0)));
        if (answer != QMessageBox::Yes)
            return;
        const QString dir = item->parent()->data(0, PathRole).toString();
        _session.run({isDir ? Request::RemoveDir : Request::RemoveFile, item->data(0, PathRole).toString(), {}, {}},
                     [this, dir](const Result& r) {
                         if (!r.ok)
                             failed(r);
                         refresh(dir);
                     });
    }

    void contextMenu(const QPoint& pos)
    {
        if (!_session.isConnected())
            return;
        QTreeWidgetItem* item = _tree->itemAt(pos);
        if (item)
            _tree->setCurrentItem(item);
        QMenu menu(this);
        if (item && item->data(0, KindRole).toInt() == FileKind)
            menu.addAction(QCoreApplication::translate("EditaFTP", "Open"), this, [this, item] { open(item->data(0, PathRole).toString()); });
        for (QToolButton* b : {_download, _upload, _newFolder, _rename, _delete, _refresh}) {
            if (b->isEnabled())
                menu.addAction(b->toolTip(), b, &QToolButton::click);
        }
        menu.exec(_tree->viewport()->mapToGlobal(pos));
    }

    Session _session;
    std::vector<Profile> _profiles;
    QComboBox* _profile = nullptr;
    QToolButton* _connect = nullptr;
    QToolButton* _refresh = nullptr;
    QToolButton* _upload = nullptr;
    QToolButton* _download = nullptr;
    QToolButton* _newFolder = nullptr;
    QToolButton* _rename = nullptr;
    QToolButton* _delete = nullptr;
    QTreeWidget* _tree = nullptr;
    QPlainTextEdit* _log = nullptr;
};

QPointer<Panel> g_panel;

void showPanel(bool show)
{
    if (!g_panel) {
        g_panel = new Panel;
        EditaDockData dock{};
        dock.hClient = g_panel.data();
        dock.pszName = kPluginName;
        dock.dlgID = kPanel;
        dock.uMask = DWS_DF_CONT_LEFT;
        dock.pszModuleName = kModuleName;
        host(NPPM_DMMREGASDCKDLG, 0, reinterpret_cast<intptr_t>(&dock));
    }
    host(show ? NPPM_DMMSHOW : NPPM_DMMHIDE, 0, reinterpret_cast<intptr_t>(g_panel.data()));
    host(NPPM_SETMENUITEMCHECK, static_cast<uintptr_t>(g_funcs[kPanel].cmdID), show);
}

void panelCommand()
{
    showPanel(!(g_panel && g_panel->isVisible()));
}

void profilesCommand()
{
    showPanel(true);
    g_panel->editProfiles();
}

void uploadCommand()
{
    showPanel(true);
    if (QToolButton* b = g_panel->findChild<QToolButton*>(QStringLiteral("ftpUpload")); b && b->isEnabled())
        b->click();
    else
        QMessageBox::information(QApplication::activeWindow(), QLatin1String(kPluginName),
                                 QCoreApplication::translate("EditaFTP", "Connect to a server first."));
}

void aboutCommand()
{
    QMessageBox::about(QApplication::activeWindow(), QLatin1String(kPluginName),
                       QCoreApplication::translate("EditaFTP",
                                                   "<b>FTP %1</b><br><br>Browse servers over SFTP, FTP, FTPES and FTPS, open their files in "
                                                   "Edita and upload them when you save.<br><br>It uses the programs of the system: "
                                                   "ssh and sftp (OpenSSH), with your ~/.ssh configuration and agent, and curl for FTP. "
                                                   "Passwords are only kept in memory.<br><br>Part of Edita. GPL-3.0-or-later.")
                           .arg(QLatin1String(kVersion)));
}

void setName(Command index, const QString& text)
{
    g.names[index] = text.toUtf8().toStdString();
    std::strncpy(g_funcs[index].itemName, g.names[index].c_str(), EDITA_MENU_ITEM_SIZE - 1);
    g_funcs[index].itemName[EDITA_MENU_ITEM_SIZE - 1] = '\0';
}

// La carpeta del .so: las traducciones van al lado (translations/editaftp_<idioma>.qm)
QString pluginDirectory()
{
    static const int marker = 0;
    Dl_info info{};
    if (dladdr(&marker, &info) && info.dli_fname)
        return QFileInfo(QString::fromLocal8Bit(info.dli_fname)).absolutePath();
    return QString();
}

} // namespace

EDITA_PLUGIN_EXPORT int edita_plugin_api_version(void)
{
    return EDITA_PLUGIN_API_VERSION;
}

EDITA_PLUGIN_EXPORT const char* edita_plugin_version(void)
{
    return kVersion;
}

EDITA_PLUGIN_EXPORT void setInfo(EditaData data)
{
    static QTranslator translator;
    if (translator.isEmpty()
        && translator.load(QLocale(), QStringLiteral("editaftp"), QStringLiteral("_"), QDir(pluginDirectory()).filePath(QStringLiteral("translations"))))
        QCoreApplication::installTranslator(&translator);
    g.data = data;
    g.opened.clear();
    g_panel = nullptr;
    std::memset(g_funcs, 0, sizeof g_funcs);
    setName(kPanel, QCoreApplication::translate("EditaFTP", "FTP Panel"));
    setName(kProfiles, QCoreApplication::translate("EditaFTP", "Profiles…"));
    setName(kUploadCurrent, QCoreApplication::translate("EditaFTP", "Upload Current File"));
    setName(kAbout, QCoreApplication::translate("EditaFTP", "About FTP…"));
    g_funcs[kPanel].pFunc = panelCommand;
    g_funcs[kProfiles].pFunc = profilesCommand;
    g_funcs[kUploadCurrent].pFunc = uploadCommand;
    g_funcs[kAbout].pFunc = aboutCommand;
}

EDITA_PLUGIN_EXPORT const char* getName(void)
{
    return kPluginName;
}

EDITA_PLUGIN_EXPORT EditaFuncItem* getFuncsArray(int* count)
{
    *count = kCommandCount;
    return g_funcs;
}

EDITA_PLUGIN_EXPORT void beNotified(SCNotification* n)
{
    if (n->nmhdr.hwndFrom != g.data.host)
        return;
    switch (n->nmhdr.code) {
    case NPPN_FILESAVED:
        if (g_panel)
            g_panel->uploadSaved(QFileInfo(hostString(NPPM_GETFULLPATHFROMBUFFERID, n->nmhdr.idFrom)).absoluteFilePath());
        break;
    case NPPN_BEFORESHUTDOWN:
        if (g_panel)
            g_panel->session().disconnectFromHost();
        break;
    default:
        break;
    }
}

EDITA_PLUGIN_EXPORT intptr_t messageProc(unsigned int, uintptr_t, intptr_t)
{
    return 0;
}
