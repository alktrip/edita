// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 51: la parte de Win32 sin ventanas de la capa de compatibilidad para plugins de
// Notepad++ (plugin-sdk/compat/npp_win32.cpp), compilada con UNICODE como los plugins
// modernos de N++: conversiones, printf de MSVC, archivos .ini, archivos, rutas,
// módulos, entorno, portapapeles, strsafe.h y el CRT de MSVC.
// Fase 52: las ventanas (npp_window.cpp): clases, controles estándar y sus notificaciones,
// subclassing, diálogos, temporizadores, PostMessage, menús, GDI sin dibujo, sistema, y
// los hilos, eventos y FormatMessage de npp_win32.cpp.

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScreen>
#include <QTimer>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QMimeData>
#include <QTemporaryDir>
#include <QTest>

#include <string>
#include <vector>

// Después de Qt: npp_compat.h define macros con nombres de Win32 y del CRT
#include "npp_compat.h"

namespace {

std::wstring w(const QString& s)
{
    return s.toStdWString();
}

QByteArray readAll(const QString& path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

void writeAll(const QString& path, const QByteArray& data)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

// Los mensajes que reciben las ventanas de prueba
struct Message
{
    HWND hwnd;
    UINT message;
    WPARAM wParam;
    LPARAM lParam;
};
std::vector<Message> g_messages;
LPVOID g_createParam = nullptr;

int countOf(UINT message, HWND hwnd = nullptr)
{
    int n = 0;
    for (const Message& m : g_messages)
        n += m.message == message && (!hwnd || m.hwnd == hwnd) ? 1 : 0;
    return n;
}

const Message* last(UINT message)
{
    for (auto it = g_messages.rbegin(); it != g_messages.rend(); ++it) {
        if (it->message == message)
            return &*it;
    }
    return nullptr;
}

LRESULT CALLBACK recordingProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    g_messages.push_back({hwnd, message, wParam, lParam});
    if (message == WM_CREATE)
        g_createParam = reinterpret_cast<LPCREATESTRUCT>(lParam)->lpCreateParams;
    return DefWindowProc(hwnd, message, wParam, lParam);
}

INT_PTR CALLBACK recordingDialog(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    g_messages.push_back({hwnd, message, wParam, lParam});
    if (message == WM_INITDIALOG) {
        SetWindowLongPtr(hwnd, DWLP_USER, lParam);
        return TRUE;
    }
    if (message == WM_USER + 5) {
        SetWindowLongPtr(hwnd, DWLP_MSGRESULT, 77);
        return TRUE;
    }
    return FALSE;
}

WNDPROC g_originalEdit = nullptr;

// Se come Intro y deja pasar lo demás (como un buscador de un plugin)
LRESULT CALLBACK subclassedEdit(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if ((message == WM_KEYDOWN || message == WM_KEYUP) && wParam == VK_RETURN) {
        g_messages.push_back({hwnd, message, wParam, lParam});
        return 0;
    }
    return CallWindowProc(g_originalEdit, hwnd, message, wParam, lParam);
}

int g_timerCalls = 0;
void CALLBACK timerProc(HWND, UINT, UINT_PTR, DWORD)
{
    ++g_timerCalls;
}

DWORD WINAPI threadMain(LPVOID param)
{
    WaitForSingleObject(static_cast<HANDLE>(param), INFINITE);
    return 42;
}

} // namespace

class TestNppCompat : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir _tmp;
    std::wstring tmp(const QString& name) const { return w(_tmp.filePath(name)); }

private slots:
    void unicodeMode()
    {
        // Como en un plugin moderno de N++: TCHAR y TEXT() son anchos
        static_assert(sizeof(TCHAR) == sizeof(wchar_t));
        const TCHAR* text = TEXT("ñ");
        QCOMPARE(QString::fromWCharArray(text), QStringLiteral("ñ"));
        QCOMPARE(lstrlen(TEXT("abc")), 3);
    }

    void textConversions()
    {
        const QString sample = QStringLiteral("Ñandú € 😀");
        const QByteArray utf8 = sample.toUtf8();
        // Con -1 se convierte también el nulo, y 0 en el destino pide el tamaño
        const int needed = MultiByteToWideChar(CP_UTF8, 0, utf8.constData(), -1, nullptr, 0);
        QCOMPARE(needed, static_cast<int>(sample.toStdWString().size()) + 1);
        std::wstring wide(static_cast<std::size_t>(needed), L'\0');
        QCOMPARE(MultiByteToWideChar(CP_UTF8, 0, utf8.constData(), -1, wide.data(), needed), needed);
        QCOMPARE(QString::fromWCharArray(wide.c_str()), sample);
        // Un búfer corto: 0 y ERROR_INSUFFICIENT_BUFFER
        wchar_t small[3];
        QCOMPARE(MultiByteToWideChar(CP_UTF8, 0, utf8.constData(), -1, small, 3), 0);
        QCOMPARE(GetLastError(), DWORD{ERROR_INSUFFICIENT_BUFFER});
        // UTF-8 inválido con MB_ERR_INVALID_CHARS
        QCOMPARE(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, "a\xFF", 2, small, 3), 0);
        QCOMPARE(GetLastError(), DWORD{ERROR_NO_UNICODE_TRANSLATION});
        // De vuelta: CP_ACP también es UTF-8
        const int bytes = WideCharToMultiByte(CP_ACP, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
        QCOMPARE(bytes, static_cast<int>(utf8.size()) + 1);
        std::string back(static_cast<std::size_t>(bytes), '\0');
        QCOMPARE(WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, back.data(), bytes, nullptr, nullptr), bytes);
        QCOMPARE(QByteArray(back.c_str()), utf8);
        // Latin-1 (1252): lo que no cabe es el carácter por defecto
        char latin[8] = {};
        BOOL usedDefault = FALSE;
        QCOMPARE(WideCharToMultiByte(1252, 0, L"ñ€", 2, latin, 8, "?", &usedDefault), 2);
        QCOMPARE(QByteArray(latin, 2), QByteArray("\xF1?"));
        QVERIFY(usedDefault);
    }

    void msvcPrintf()
    {
        // En las funciones anchas de MSVC, %s es wchar_t y %S es char; %I64d es de 64 bits
        wchar_t buffer[64];
        QCOMPARE(swprintf(buffer, 64, L"%s=%d %S %c %I64d", L"clave", 5, "narrow", L'x', 1234567890123LL), 30);
        QCOMPARE(QString::fromWCharArray(buffer), QStringLiteral("clave=5 narrow x 1234567890123"));
        QCOMPARE(wsprintf(buffer, L"%s-%05d", L"ñ", 42), 7);
        QCOMPARE(QString::fromWCharArray(buffer), QStringLiteral("ñ-00042"));
        // Las formas explícitas siguen igual: %ls ancho, %hs estrecho
        _snwprintf(buffer, 64, L"%ls|%hs|%%", L"a", "b");
        QCOMPARE(QString::fromWCharArray(buffer), QStringLiteral("a|b|%"));
        // La sobrecarga de MSVC para arreglos
        swprintf_s(buffer, L"%s", L"arreglo");
        QCOMPARE(QString::fromWCharArray(buffer), QStringLiteral("arreglo"));
        // Si no cabe: -1 y el texto cortado, terminado en nulo
        wchar_t tiny[4];
        QCOMPARE(_snwprintf(tiny, 4, L"%s", L"abcdef"), -1);
        QCOMPARE(QString::fromWCharArray(tiny), QStringLiteral("abc"));
        QCOMPARE(StringCchPrintf(tiny, 4, L"%d", 123456), STRSAFE_E_INSUFFICIENT_BUFFER);
        QCOMPARE(QString::fromWCharArray(tiny), QStringLiteral("123"));
        QCOMPARE(_scwprintf(L"%s%d", L"ab", 100), 5);
        // En las estrechas, %s es char y %S, wchar_t
        char narrow[32];
        sprintf_s(narrow, "%s %S %I64u", "uno", L"dos", 7ULL);
        QCOMPARE(QByteArray(narrow), QByteArray("uno dos 7"));
    }

    void iniFiles()
    {
        // Una ruta con '\', como las que arman los plugins
        const std::wstring ini = w(_tmp.path()) + L"\\opciones.ini";
        const QString native = _tmp.filePath(QStringLiteral("opciones.ini"));
        QVERIFY(WritePrivateProfileString(L"General", L"nombre", L"Edita ñ", ini.c_str()));
        QVERIFY(WritePrivateProfileString(L"General", L"ancho", L"42px", ini.c_str()));
        QVERIFY(WritePrivateProfileString(L"Otra", L"x", L"\"entre comillas\"", ini.c_str()));
        QVERIFY(QFileInfo::exists(native));
        QCOMPARE(readAll(native), QByteArray("[General]\nnombre=Edita \xC3\xB1\nancho=42px\n[Otra]\nx=\"entre comillas\"\n"));

        wchar_t buffer[64];
        // Sin distinguir mayúsculas
        QCOMPARE(GetPrivateProfileString(L"general", L"NOMBRE", L"", buffer, 64, ini.c_str()), DWORD{7});
        QCOMPARE(QString::fromWCharArray(buffer), QStringLiteral("Edita ñ"));
        // Las comillas se quitan; el valor por defecto pierde los espacios del final
        GetPrivateProfileString(L"Otra", L"x", L"", buffer, 64, ini.c_str());
        QCOMPARE(QString::fromWCharArray(buffer), QStringLiteral("entre comillas"));
        GetPrivateProfileString(L"Otra", L"falta", L"defecto  ", buffer, 64, ini.c_str());
        QCOMPARE(QString::fromWCharArray(buffer), QStringLiteral("defecto"));
        // Un búfer corto: size - 1 caracteres
        wchar_t shortBuffer[4];
        QCOMPARE(GetPrivateProfileString(L"General", L"nombre", L"", shortBuffer, 4, ini.c_str()), DWORD{3});
        QCOMPARE(QString::fromWCharArray(shortBuffer), QStringLiteral("Edi"));
        // Enteros: los dígitos del principio; 0 si no hay; el valor por defecto si falta
        QCOMPARE(GetPrivateProfileInt(L"General", L"ancho", 7, ini.c_str()), UINT{42});
        QCOMPARE(GetPrivateProfileInt(L"General", L"nombre", 7, ini.c_str()), UINT{0});
        QCOMPARE(GetPrivateProfileInt(L"General", L"falta", 7, ini.c_str()), UINT{7});
        // Secciones y claves: cadenas terminadas en nulo, con un nulo al final
        QCOMPARE(GetPrivateProfileString(nullptr, nullptr, L"", buffer, 64, ini.c_str()), DWORD{13});
        QCOMPARE(QString::fromWCharArray(buffer, 14), QString::fromWCharArray(L"General\0Otra\0\0", 14));
        QCOMPARE(GetPrivateProfileString(L"General", nullptr, L"", buffer, 64, ini.c_str()), DWORD{13});
        QCOMPARE(QString::fromWCharArray(buffer, 14), QString::fromWCharArray(L"nombre\0ancho\0\0", 14));
        QCOMPARE(GetPrivateProfileSection(L"General", buffer, 64, ini.c_str()), DWORD{26});
        QCOMPARE(QString::fromWCharArray(buffer, 27), QString::fromWCharArray(L"nombre=Edita ñ\0ancho=42px\0\0", 27));
        // Valor NULL borra la clave; clave NULL borra la sección
        QVERIFY(WritePrivateProfileString(L"General", L"ancho", nullptr, ini.c_str()));
        QVERIFY(WritePrivateProfileString(L"Otra", nullptr, nullptr, ini.c_str()));
        QCOMPARE(readAll(native), QByteArray("[General]\nnombre=Edita \xC3\xB1\n"));

        // Los comentarios se conservan y un .ini en UTF-16 (como los que deja N++) se lee
        // y se escribe en UTF-16
        const QString utf16 = _tmp.filePath(QStringLiteral("utf16.ini"));
        QByteArray data("\xFF\xFE");
        const QString text = QStringLiteral("; comentario\r\n[Sección]\r\nclave = valor ñ \r\n");
        data += QByteArray(reinterpret_cast<const char*>(text.utf16()), text.size() * 2);
        writeAll(utf16, data);
        GetPrivateProfileString(L"Sección", L"clave", L"", buffer, 64, w(utf16).c_str());
        QCOMPARE(QString::fromWCharArray(buffer), QStringLiteral("valor ñ"));
        QVERIFY(WritePrivateProfileString(L"Sección", L"otra", L"1", w(utf16).c_str()));
        const QByteArray written = readAll(utf16);
        QVERIFY(written.startsWith("\xFF\xFE"));
        const QString decoded = QString::fromUtf16(reinterpret_cast<const char16_t*>(written.constData() + 2), (written.size() - 2) / 2);
        QCOMPARE(decoded, QStringLiteral("; comentario\n[Sección]\nclave=valor ñ\notra=1\n"));
        // Las variantes A son UTF-8
        char narrow[32];
        GetPrivateProfileStringA("General", "nombre", "", narrow, 32, native.toUtf8().constData());
        QCOMPARE(QByteArray(narrow), QByteArray("Edita \xC3\xB1"));
    }

    void files()
    {
        const std::wstring path = tmp(QStringLiteral("datos.bin"));
        HANDLE file = CreateFile(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        QVERIFY(file != INVALID_HANDLE_VALUE);
        DWORD done = 0;
        QVERIFY(WriteFile(file, "hola mundo", 10, &done, nullptr));
        QCOMPARE(done, DWORD{10});
        QCOMPARE(GetFileSize(file, nullptr), DWORD{10});
        QCOMPARE(SetFilePointer(file, 5, nullptr, FILE_BEGIN), DWORD{5});
        char read[16] = {};
        QVERIFY(ReadFile(file, read, 16, &done, nullptr));
        QCOMPARE(done, DWORD{5});
        QCOMPARE(QByteArray(read), QByteArray("mundo"));
        SetFilePointer(file, 4, nullptr, FILE_BEGIN);
        QVERIFY(SetEndOfFile(file));
        QVERIFY(CloseHandle(file));
        QCOMPARE(readAll(_tmp.filePath(QStringLiteral("datos.bin"))), QByteArray("hola"));
        // OPEN_ALWAYS con un archivo que existe: ERROR_ALREADY_EXISTS
        file = CreateFile(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, 0, nullptr);
        QCOMPARE(GetLastError(), DWORD{ERROR_ALREADY_EXISTS});
        CloseHandle(file);
        QCOMPARE(CreateFile(tmp(QStringLiteral("no-existe")).c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr),
                 INVALID_HANDLE_VALUE);
        QCOMPARE(GetLastError(), DWORD{ERROR_FILE_NOT_FOUND});

        // Atributos
        QCOMPARE(GetFileAttributes(w(_tmp.path()).c_str()) & FILE_ATTRIBUTE_DIRECTORY, DWORD{FILE_ATTRIBUTE_DIRECTORY});
        QCOMPARE(GetFileAttributes(path.c_str()) & FILE_ATTRIBUTE_DIRECTORY, DWORD{0});
        QCOMPARE(GetFileAttributes(tmp(QStringLiteral("no-existe")).c_str()), INVALID_FILE_ATTRIBUTES);

        // Directorios, copiar, mover y borrar
        const std::wstring dir = tmp(QStringLiteral("carpeta"));
        QVERIFY(CreateDirectory(dir.c_str(), nullptr));
        QVERIFY(!CreateDirectory(dir.c_str(), nullptr));
        QCOMPARE(GetLastError(), DWORD{ERROR_ALREADY_EXISTS});
        const std::wstring copy = dir + L"\\copia.bin";
        QVERIFY(CopyFile(path.c_str(), copy.c_str(), TRUE));
        QVERIFY(!CopyFile(path.c_str(), copy.c_str(), TRUE));
        QVERIFY(!MoveFile(path.c_str(), copy.c_str()));
        QVERIFY(MoveFileEx(path.c_str(), copy.c_str(), MOVEFILE_REPLACE_EXISTING));
        QVERIFY(!PathFileExists(path.c_str()));
        QVERIFY(PathFileExists(copy.c_str()));
        QVERIFY(DeleteFile(copy.c_str()));
        QVERIFY(RemoveDirectory(dir.c_str()));

        // FindFirstFile: sin distinguir mayúsculas; *.* es todo
        for (const char* name : {"a.TXT", "b.txt", "c.log", "sinpunto"})
            writeAll(_tmp.filePath(QString::fromLatin1(name)), "x");
        auto find = [&](const std::wstring& pattern) {
            QStringList names;
            WIN32_FIND_DATA data;
            HANDLE handle = FindFirstFile(pattern.c_str(), &data);
            if (handle == INVALID_HANDLE_VALUE)
                return names;
            do {
                const QString name = QString::fromWCharArray(data.cFileName);
                if (name != QLatin1String(".") && name != QLatin1String(".."))
                    names << name + ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? QStringLiteral("/") : QString());
            } while (FindNextFile(handle, &data));
            FindClose(handle);
            names.sort();
            return names;
        };
        QCOMPARE(find(w(_tmp.path()) + L"\\*.txt"), (QStringList{"a.TXT", "b.txt"}));
        QVERIFY(find(w(_tmp.path()) + L"/*.*").contains(QStringLiteral("sinpunto")));
        QCOMPARE(find(w(_tmp.path()) + L"/c.log"), QStringList{"c.log"});
        QVERIFY(find(w(_tmp.path()) + L"/*.nada").isEmpty());
        QCOMPARE(GetLastError(), DWORD{ERROR_FILE_NOT_FOUND});

        // _wfopen con el modo de MSVC
        FILE* f = _wfopen((w(_tmp.path()) + L"\\texto.txt").c_str(), L"wt, ccs=UTF-8");
        QVERIFY(f);
        std::fputs("ok", f);
        std::fclose(f);
        QCOMPARE(readAll(_tmp.filePath(QStringLiteral("texto.txt"))), QByteArray("ok"));
    }

    void paths()
    {
        QCOMPARE(QString::fromWCharArray(PathFindFileName(L"C:\\carpeta\\archivo.txt")), QStringLiteral("archivo.txt"));
        QCOMPARE(QString::fromWCharArray(PathFindFileName(L"/a/b\\c.ini")), QStringLiteral("c.ini"));
        QCOMPARE(QString::fromWCharArray(PathFindExtension(L"/a.b/c")), QString());
        QCOMPARE(QString::fromWCharArray(PathFindExtension(L"/a/c.tar.gz")), QStringLiteral(".gz"));
        wchar_t path[MAX_PATH] = L"/home/usuario/plugin.so";
        QVERIFY(PathRemoveFileSpec(path));
        QCOMPARE(QString::fromWCharArray(path), QStringLiteral("/home/usuario"));
        QVERIFY(PathAppend(path, L"\\config\\opciones.ini"));
        QCOMPARE(QString::fromWCharArray(path), QStringLiteral("/home/usuario/config\\opciones.ini"));
        PathRemoveExtension(path);
        QCOMPARE(QString::fromWCharArray(path), QStringLiteral("/home/usuario/config\\opciones"));
        QVERIFY(PathRenameExtension(path, L".xml"));
        QCOMPARE(QString::fromWCharArray(path), QStringLiteral("/home/usuario/config\\opciones.xml"));
        PathStripPath(path);
        QCOMPARE(QString::fromWCharArray(path), QStringLiteral("opciones.xml"));
        wchar_t combined[MAX_PATH];
        QVERIFY(PathCombine(combined, L"/raiz", L"sub/archivo"));
        QCOMPARE(QString::fromWCharArray(combined), QStringLiteral("/raiz/sub/archivo"));
        QVERIFY(PathCombine(combined, L"/raiz", L"/absoluta"));
        QCOMPARE(QString::fromWCharArray(combined), QStringLiteral("/absoluta"));
        wchar_t dir[MAX_PATH] = L"/raiz";
        PathAddBackslash(dir);
        QCOMPARE(QString::fromWCharArray(dir), QStringLiteral("/raiz/"));
        PathRemoveBackslash(dir);
        QCOMPARE(QString::fromWCharArray(dir), QStringLiteral("/raiz"));
        QVERIFY(PathIsRelative(L"a\\b"));
        QVERIFY(!PathIsRelative(L"/a"));
        QVERIFY(!PathIsRelative(L"C:\\a"));
        QVERIFY(PathIsDirectory(w(_tmp.path()).c_str()));
    }

    void modulesAndEnvironment()
    {
        // GetModuleFileName(NULL) es el ejecutable; el módulo del plugin es el binario
        // en el que está compilada la capa (aquí, la prueba)
        wchar_t path[MAX_PATH];
        const DWORD length = GetModuleFileName(nullptr, path, MAX_PATH);
        QCOMPARE(QString::fromWCharArray(path, static_cast<qsizetype>(length)), QCoreApplication::applicationFilePath());
        GetModuleFileName(edita_compat_module(), path, MAX_PATH);
        QCOMPARE(QFileInfo(QString::fromWCharArray(path)).canonicalFilePath(),
                 QFileInfo(QCoreApplication::applicationFilePath()).canonicalFilePath());
        wchar_t tiny[4];
        QCOMPARE(GetModuleFileName(nullptr, tiny, 4), DWORD{4});
        QCOMPARE(GetLastError(), DWORD{ERROR_INSUFFICIENT_BUFFER});
        QCOMPARE(GetModuleHandle(nullptr) == GetModuleHandle(L"plugin.dll"), false);

        QVERIFY(SetEnvironmentVariable(L"EDITA_COMPAT_PRUEBA", L"valor ñ"));
        QCOMPARE(GetEnvironmentVariable(L"EDITA_COMPAT_PRUEBA", nullptr, 0), DWORD{8});
        wchar_t value[16];
        QCOMPARE(GetEnvironmentVariable(L"EDITA_COMPAT_PRUEBA", value, 16), DWORD{7});
        QCOMPARE(QString::fromWCharArray(value), QStringLiteral("valor ñ"));
        QCOMPARE(GetEnvironmentVariable(L"EDITA_COMPAT_NO_EXISTE", value, 16), DWORD{0});
        wchar_t expanded[64];
        QCOMPARE(ExpandEnvironmentStrings(L"%EDITA_COMPAT_PRUEBA%/x %NO_EXISTE%", expanded, 64), DWORD{22});
        QCOMPARE(QString::fromWCharArray(expanded), QStringLiteral("valor ñ/x %NO_EXISTE%"));
        QCOMPARE(QString::fromWCharArray(_wgetenv(L"EDITA_COMPAT_PRUEBA")), QStringLiteral("valor ñ"));

        wchar_t folder[MAX_PATH];
        QCOMPARE(SHGetFolderPath(nullptr, CSIDL_APPDATA, nullptr, 0, folder), S_OK);
        QVERIFY(QFileInfo(QString::fromWCharArray(folder)).isAbsolute());
        QCOMPARE(GetTempPath(MAX_PATH, folder), DWORD(QDir::tempPath().size() + 1));
        QVERIFY(QString::fromWCharArray(folder).endsWith(QLatin1Char('/')));

        SYSTEMTIME now;
        GetLocalTime(&now);
        QCOMPARE(int(now.wYear), QDate::currentDate().year());
        const ULONGLONG before = GetTickCount64();
        Sleep(20);
        QVERIFY(GetTickCount64() >= before + 15);
        QCOMPARE(GetKeyState(VK_SHIFT) & 0x8000, 0);
    }

    void clipboard()
    {
        // Escribir: EmptyClipboard + SetClipboardData + CloseClipboard
        const std::wstring text = L"portapapeles ñ 😀";
        HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, (text.size() + 1) * sizeof(wchar_t));
        QVERIFY(memory);
        QCOMPARE(GlobalSize(memory), (text.size() + 1) * sizeof(wchar_t));
        std::wcscpy(static_cast<wchar_t*>(GlobalLock(memory)), text.c_str());
        GlobalUnlock(memory);
        QVERIFY(OpenClipboard(nullptr));
        QVERIFY(!OpenClipboard(nullptr)); // ya está abierto
        QVERIFY(EmptyClipboard());
        QVERIFY(SetClipboardData(CF_UNICODETEXT, memory));
        // Un formato registrado (como el de la selección rectangular de N++)
        const UINT columns = RegisterClipboardFormat(L"MSDEVColumnSelect");
        QVERIFY(columns >= 0xC000);
        QCOMPARE(RegisterClipboardFormat(L"msdevcolumnselect"), columns);
        QVERIFY(SetClipboardData(columns, GlobalAlloc(GPTR, 1)));
        QVERIFY(CloseClipboard());
        QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("portapapeles ñ 😀"));

        // Leer
        QApplication::clipboard()->setText(QStringLiteral("desde Qt ñ"));
        QVERIFY(OpenClipboard(nullptr));
        QVERIFY(IsClipboardFormatAvailable(CF_UNICODETEXT));
        QVERIFY(!IsClipboardFormatAvailable(columns));
        HANDLE wide = GetClipboardData(CF_UNICODETEXT);
        QVERIFY(wide);
        QCOMPARE(QString::fromWCharArray(static_cast<const wchar_t*>(GlobalLock(wide))), QStringLiteral("desde Qt ñ"));
        HANDLE narrow = GetClipboardData(CF_TEXT);
        QCOMPARE(QByteArray(static_cast<const char*>(GlobalLock(narrow))), QByteArray("desde Qt \xC3\xB1"));
        QVERIFY(CloseClipboard());
    }

    void stringsAndCrt()
    {
        wchar_t buffer[8];
        QCOMPARE(StringCchCopy(buffer, 8, L"0123456789"), STRSAFE_E_INSUFFICIENT_BUFFER);
        QCOMPARE(QString::fromWCharArray(buffer), QStringLiteral("0123456"));
        QCOMPARE(StringCchCopy(buffer, 8, L"ab"), S_OK);
        QCOMPARE(StringCchCat(buffer, 8, L"cd"), S_OK);
        QCOMPARE(QString::fromWCharArray(buffer), QStringLiteral("abcd"));
        size_t length = 0;
        QCOMPARE(StringCchLength(buffer, 8, &length), S_OK);
        QCOMPARE(length, size_t{4});
        // wcscpy_s con arreglo: si no cabe, ERANGE y la cadena vacía
        QCOMPARE(wcscpy_s(buffer, L"0123456789"), ERANGE);
        QCOMPARE(buffer[0], L'\0');
        QCOMPARE(_tcscpy_s(buffer, L"hola"), 0);
        QCOMPARE(_tcsicmp(buffer, L"HOLA"), 0);
        lstrcpyn(buffer, L"abcdefghij", 5);
        QCOMPARE(QString::fromWCharArray(buffer), QStringLiteral("abcd"));
        QCOMPARE(QString::fromWCharArray(CharUpper(buffer)), QStringLiteral("ABCD"));
        QCOMPARE(QString::fromWCharArray(_itow(-42, buffer, 10)), QStringLiteral("-42"));
        QCOMPARE(QString::fromWCharArray(_itow(255, buffer, 16)), QStringLiteral("ff"));
        char narrow[16];
        QCOMPARE(QByteArray(_itoa(-1, narrow, 16)), QByteArray("ffffffff"));
        QCOMPARE(_wtoi(L"  17x"), 17);
        QCOMPARE(QString::fromWCharArray(StrStrI(L"Buscar AQUÍ", L"aquí")), QStringLiteral("AQUÍ"));
        QCOMPARE(lstrcmpi(L"Ñandú", L"ñANDÚ"), 0);
        // Las macros de memoria de winnt.h
        int numbers[4] = {1, 2, 3, 4};
        ZeroMemory(numbers, sizeof(numbers));
        QCOMPARE(numbers[3], 0);
        const int source[2] = {7, 8};
        CopyMemory(numbers, source, sizeof(source));
        QCOMPARE(numbers[1], 8);
    }

    void windowsAndControls()
    {
        g_messages.clear();
        WNDCLASSEX wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = recordingProc;
        wc.cbWndExtra = sizeof(LONG_PTR);
        wc.lpszClassName = L"PruebaClase";
        QVERIFY(RegisterClassEx(&wc));
        QCOMPARE(RegisterClassEx(&wc), ATOM{0}); // ya existe
        int param = 5;
        HWND parent = CreateWindowEx(0, L"PruebaClase", L"Título ñ", WS_OVERLAPPEDWINDOW, 10, 10, 300, 200, nullptr, nullptr,
                                     edita_compat_module(), &param);
        QVERIFY(parent);
        QVERIFY(IsWindow(parent));
        // WM_NCCREATE y WM_CREATE, con lpCreateParams
        QCOMPARE(countOf(WM_NCCREATE, parent), 1);
        QCOMPARE(countOf(WM_CREATE, parent), 1);
        QCOMPARE(g_createParam, static_cast<LPVOID>(&param));
        wchar_t text[64];
        QCOMPARE(GetWindowText(parent, text, 64), 8);
        QCOMPARE(QString::fromWCharArray(text), QStringLiteral("Título ñ"));
        RECT rc;
        QVERIFY(GetClientRect(parent, &rc));
        QCOMPARE(rc.right, LONG{300});
        QCOMPARE(rc.bottom, LONG{200});
        GetClassName(parent, text, 64);
        QCOMPARE(QString::fromWCharArray(text), QStringLiteral("PruebaClase"));
        // Datos de la ventana
        QCOMPARE(SetWindowLongPtr(parent, GWLP_USERDATA, 1234), LONG_PTR{0});
        QCOMPARE(GetWindowLongPtr(parent, GWLP_USERDATA), LONG_PTR{1234});
        SetWindowLongPtr(parent, 0, 99); // cbWndExtra
        QCOMPARE(GetWindowLongPtr(parent, 0), LONG_PTR{99});
        QVERIFY(SetProp(parent, L"Prop", reinterpret_cast<HANDLE>(7)));
        QCOMPARE(GetProp(parent, L"prop"), reinterpret_cast<HANDLE>(7));
        ShowWindow(parent, SW_SHOW);
        QVERIFY(IsWindowVisible(parent));
        // WM_SIZE al cambiar el tamaño
        MoveWindow(parent, 10, 10, 320, 210, TRUE);
        QTRY_VERIFY(last(WM_SIZE) && LOWORD(last(WM_SIZE)->lParam) == 320);

        // Un botón: GetDlgItem, GetDlgCtrlID, y BN_CLICKED al padre con su id
        HWND button = CreateWindowEx(0, L"BUTTON", L"&Aceptar", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 10, 10, 80, 24, parent,
                                     reinterpret_cast<HMENU>(100), nullptr, nullptr);
        QVERIFY(button);
        QCOMPARE(GetDlgItem(parent, 100), button);
        QCOMPARE(GetDlgCtrlID(button), 100);
        QCOMPARE(GetParent(button), parent);
        QCOMPARE(qobject_cast<QPushButton*>(static_cast<QWidget*>(edita_compat_widget(button)))->text(), QStringLiteral("&Aceptar"));
        g_messages.clear();
        SendMessage(button, BM_CLICK, 0, 0);
        const Message* command = last(WM_COMMAND);
        QVERIFY(command);
        QCOMPARE(LOWORD(command->wParam), WORD{100});
        QCOMPARE(HIWORD(command->wParam), WORD{BN_CLICKED});
        QCOMPARE(reinterpret_cast<HWND>(command->lParam), button);
        // Deshabilitar
        QCOMPARE(EnableWindow(button, FALSE), FALSE);
        QVERIFY(!IsWindowEnabled(button));
        QVERIFY(GetWindowLongPtr(button, GWL_STYLE) & WS_DISABLED);
        EnableWindow(button, TRUE);

        // Casillas: BS_AUTOCHECKBOX cambia sola; BS_CHECKBOX, no (la marca el plugin)
        HWND check = CreateWindowEx(0, L"BUTTON", L"Opción", WS_CHILD | BS_AUTOCHECKBOX, 0, 40, 100, 20, parent,
                                    reinterpret_cast<HMENU>(101), nullptr, nullptr);
        HWND manual = CreateWindowEx(0, L"BUTTON", L"Manual", WS_CHILD | BS_CHECKBOX, 0, 60, 100, 20, parent,
                                     reinterpret_cast<HMENU>(102), nullptr, nullptr);
        QCOMPARE(SendMessage(check, BM_GETCHECK, 0, 0), LRESULT{BST_UNCHECKED});
        CheckDlgButton(parent, 101, BST_CHECKED);
        QCOMPARE(IsDlgButtonChecked(parent, 101), UINT{BST_CHECKED});
        g_messages.clear();
        SendMessage(check, BM_CLICK, 0, 0);
        QCOMPARE(IsDlgButtonChecked(parent, 101), UINT{BST_UNCHECKED});
        QCOMPARE(countOf(WM_COMMAND), 1);
        SendMessage(manual, BM_CLICK, 0, 0);
        QCOMPARE(IsDlgButtonChecked(parent, 102), UINT{BST_UNCHECKED});
        QCOMPARE(countOf(WM_COMMAND), 2);
        // Opciones
        for (int id = 201; id <= 203; ++id)
            CreateWindowEx(0, L"BUTTON", L"R", WS_CHILD | BS_AUTORADIOBUTTON, 0, 0, 10, 10, parent, reinterpret_cast<HMENU>(id), nullptr, nullptr);
        CheckRadioButton(parent, 201, 203, 202);
        QCOMPARE(IsDlgButtonChecked(parent, 202), UINT{BST_CHECKED});
        QCOMPARE(IsDlgButtonChecked(parent, 201), UINT{BST_UNCHECKED});
        CheckRadioButton(parent, 201, 203, 203);
        QCOMPARE(IsDlgButtonChecked(parent, 202), UINT{BST_UNCHECKED});

        // Un campo de una línea: EN_CHANGE, selección, reemplazo, enteros
        HWND edit = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL, 0, 80, 120, 22, parent,
                                   reinterpret_cast<HMENU>(300), nullptr, nullptr);
        g_messages.clear();
        SetWindowText(edit, L"hola mundo");
        QCOMPARE(countOf(WM_COMMAND), 1);
        QCOMPARE(HIWORD(last(WM_COMMAND)->wParam), WORD{EN_CHANGE});
        QCOMPARE(GetWindowTextLength(edit), 10);
        SendMessage(edit, EM_SETSEL, 5, -1);
        DWORD start = 0, end = 0;
        SendMessage(edit, EM_GETSEL, reinterpret_cast<WPARAM>(&start), reinterpret_cast<LPARAM>(&end));
        QCOMPARE(start, DWORD{5});
        QCOMPARE(end, DWORD{10});
        SendMessage(edit, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(L"Edita"));
        GetDlgItemText(parent, 300, text, 64);
        QCOMPARE(QString::fromWCharArray(text), QStringLiteral("hola Edita"));
        SetDlgItemInt(parent, 300, static_cast<UINT>(-12), TRUE);
        BOOL translated = FALSE;
        QCOMPARE(static_cast<int>(GetDlgItemInt(parent, 300, &translated, TRUE)), -12);
        QVERIFY(translated);
        GetDlgItemInt(parent, 300, &translated, FALSE);
        QVERIFY(!translated);
        // Multilínea: "\r\n" de Windows, y WM_SETTEXT no notifica
        HWND multi = CreateWindowEx(0, L"EDIT", L"", WS_CHILD | ES_MULTILINE | WS_VSCROLL, 0, 110, 200, 60, parent,
                                    reinterpret_cast<HMENU>(301), nullptr, nullptr);
        g_messages.clear();
        SetWindowText(multi, L"uno\r\ndos");
        QCOMPARE(countOf(WM_COMMAND), 0);
        GetWindowText(multi, text, 64);
        QCOMPARE(QString::fromWCharArray(text), QStringLiteral("uno\r\ndos"));
        QCOMPARE(SendMessage(multi, EM_GETLINECOUNT, 0, 0), LRESULT{2});
        QCOMPARE(SendMessage(multi, EM_LINEINDEX, 1, 0), LRESULT{5});

        // Lista ordenada con LBS_NOTIFY
        HWND list = CreateWindowEx(0, L"LISTBOX", nullptr, WS_CHILD | LBS_SORT | LBS_NOTIFY, 130, 0, 100, 100, parent,
                                   reinterpret_cast<HMENU>(400), nullptr, nullptr);
        for (const wchar_t* item : {L"cereza", L"Arándano", L"banana"})
            SendMessage(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item));
        QCOMPARE(SendMessage(list, LB_GETCOUNT, 0, 0), LRESULT{3});
        SendMessage(list, LB_GETTEXT, 0, reinterpret_cast<LPARAM>(text));
        QCOMPARE(QString::fromWCharArray(text), QStringLiteral("Arándano"));
        QCOMPARE(SendMessage(list, LB_FINDSTRING, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(L"BAN")), LRESULT{1});
        QCOMPARE(SendMessage(list, LB_FINDSTRINGEXACT, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(L"ban")), LRESULT{LB_ERR});
        SendMessage(list, LB_SETITEMDATA, 2, 555);
        QCOMPARE(SendMessage(list, LB_GETITEMDATA, 2, 0), LRESULT{555});
        g_messages.clear();
        QCOMPARE(SendMessage(list, LB_SETCURSEL, 1, 0), LRESULT{1}); // sin notificación
        QCOMPARE(countOf(WM_COMMAND), 0);
        QCOMPARE(SendMessage(list, LB_GETCURSEL, 0, 0), LRESULT{1});
        // Elegir con el ratón: LBN_SELCHANGE
        qobject_cast<QListWidget*>(static_cast<QWidget*>(edita_compat_widget(list)))->setCurrentRow(2);
        QCOMPARE(HIWORD(last(WM_COMMAND)->wParam), WORD{LBN_SELCHANGE});
        QCOMPARE(SendMessage(list, LB_DELETESTRING, 0, 0), LRESULT{2});

        // Combo: CB_*, y CBN_SELCHANGE cuando elige el usuario
        HWND combo = CreateWindowEx(0, L"COMBOBOX", nullptr, WS_CHILD | CBS_DROPDOWNLIST, 130, 110, 100, 200, parent,
                                    reinterpret_cast<HMENU>(500), nullptr, nullptr);
        for (const wchar_t* item : {L"UTF-8", L"ANSI"})
            SendMessage(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item));
        QCOMPARE(SendMessage(combo, CB_GETCURSEL, 0, 0), LRESULT{CB_ERR});
        SendMessage(combo, CB_SETCURSEL, 1, 0);
        QCOMPARE(SendMessage(combo, CB_GETLBTEXTLEN, 1, 0), LRESULT{4});
        GetWindowText(combo, text, 64);
        QCOMPARE(QString::fromWCharArray(text), QStringLiteral("ANSI"));
        g_messages.clear();
        emit qobject_cast<QComboBox*>(static_cast<QWidget*>(edita_compat_widget(combo)))->activated(0);
        QCOMPARE(HIWORD(last(WM_COMMAND)->wParam), WORD{CBN_SELCHANGE});

        // Estático: muestra "Nombre:" pero su texto es "&Nombre:"
        HWND label = CreateWindowEx(0, L"STATIC", L"&Nombre:", WS_CHILD, 0, 180, 100, 20, parent, reinterpret_cast<HMENU>(600), nullptr, nullptr);
        QCOMPARE(qobject_cast<QLabel*>(static_cast<QWidget*>(edita_compat_widget(label)))->text(), QStringLiteral("Nombre:"));
        GetWindowText(label, text, 64);
        QCOMPARE(QString::fromWCharArray(text), QStringLiteral("&Nombre:"));
        // Una clase que no existe
        QVERIFY(!CreateWindowEx(0, L"SysListView32", nullptr, WS_CHILD, 0, 0, 1, 1, parent, nullptr, nullptr, nullptr));

        // Subclassing: el plugin ve el teclado del control y puede comérselo
        g_originalEdit = reinterpret_cast<WNDPROC>(SetWindowLongPtr(edit, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(subclassedEdit)));
        QVERIFY(g_originalEdit);
        auto* line = qobject_cast<QLineEdit*>(static_cast<QWidget*>(edita_compat_widget(edit)));
        line->clear();
        g_messages.clear();
        QTest::keyClicks(line, QStringLiteral("ab"));
        QTest::keyClick(line, Qt::Key_Return);
        QCOMPARE(line->text(), QStringLiteral("ab"));
        QCOMPARE(countOf(WM_KEYDOWN, edit), 1);
        QCOMPARE(last(WM_KEYDOWN)->wParam, WPARAM{VK_RETURN});

        // EnumChildWindows y GetWindow
        int children = 0;
        EnumChildWindows(parent, [](HWND, LPARAM count) { ++*reinterpret_cast<int*>(count); return TRUE; }, reinterpret_cast<LPARAM>(&children));
        QCOMPARE(children, 11);
        QCOMPARE(GetWindow(parent, GW_CHILD), button);
        QCOMPARE(GetWindow(button, GW_HWNDNEXT), check);

        // DestroyWindow: WM_DESTROY a la ventana y a sus hijos; luego ya no existen
        g_messages.clear();
        QVERIFY(DestroyWindow(parent));
        QCOMPARE(countOf(WM_DESTROY, parent), 1);
        QCOMPARE(countOf(WM_NCDESTROY, parent), 1);
        QVERIFY(!IsWindow(parent));
        QVERIFY(!IsWindow(button));
        QCOMPARE(SendMessage(parent, WM_USER, 0, 0), LRESULT{0});
        QVERIFY(UnregisterClass(L"PruebaClase", nullptr));
    }

    void dialogs()
    {
        g_messages.clear();
        HWND dialog = CreateDialogParam(edita_compat_module(), MAKEINTRESOURCE(100), nullptr, recordingDialog, 42);
        QVERIFY(dialog);
        // WM_INITDIALOG con el lParam; oculto hasta ShowWindow
        QCOMPARE(countOf(WM_INITDIALOG, dialog), 1);
        QCOMPARE(last(WM_INITDIALOG)->lParam, LPARAM{42});
        QCOMPARE(GetWindowLongPtr(dialog, DWLP_USER), LONG_PTR{42});
        QVERIFY(!IsWindowVisible(dialog));
        QCOMPARE(GetWindowLongPtr(dialog, DWLP_DLGPROC), reinterpret_cast<LONG_PTR>(recordingDialog));
        // Sin plantilla (los .rc todavía no se leen): un aviso, que se va con el primer control
        auto* widget = static_cast<QWidget*>(edita_compat_widget(dialog));
        QVERIFY(widget->findChild<QLabel*>());
        HWND edit = CreateWindowEx(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE, 10, 10, 100, 22, dialog, reinterpret_cast<HMENU>(1000), nullptr, nullptr);
        QVERIFY(!widget->findChild<QLabel*>());
        // DWLP_MSGRESULT es el resultado de un mensaje tratado
        QCOMPARE(SendMessage(dialog, WM_USER + 5, 0, 0), LRESULT{77});
        ShowWindow(dialog, SW_SHOW);
        QVERIFY(IsWindowVisible(dialog));
        // Intro en un campo: IDOK; Esc: IDCANCEL; cerrar: IDCANCEL (DefDlgProc)
        auto* line = qobject_cast<QLineEdit*>(static_cast<QWidget*>(edita_compat_widget(edit)));
        g_messages.clear();
        QTest::keyClick(line, Qt::Key_Return);
        QCOMPARE(LOWORD(last(WM_COMMAND)->wParam), WORD{IDOK});
        QTest::keyClick(line, Qt::Key_Escape);
        QCOMPARE(LOWORD(last(WM_COMMAND)->wParam), WORD{IDCANCEL});
        g_messages.clear();
        SendMessage(dialog, WM_CLOSE, 0, 0);
        QCOMPARE(LOWORD(last(WM_COMMAND)->wParam), WORD{IDCANCEL});
        QVERIFY(IsWindow(dialog)); // lo cierra el plugin, si quiere
        // El botón por defecto recibe Intro
        CreateWindowEx(0, L"BUTTON", L"Buscar", WS_CHILD | BS_DEFPUSHBUTTON, 120, 10, 60, 22, dialog, reinterpret_cast<HMENU>(1001), nullptr, nullptr);
        g_messages.clear();
        QTest::keyClick(line, Qt::Key_Return);
        QCOMPARE(LOWORD(last(WM_COMMAND)->wParam), WORD{1001});
        QVERIFY(EndDialog(dialog, IDOK));
        QVERIFY(!IsWindowVisible(dialog));
        DestroyWindow(dialog);
        QVERIFY(!IsWindow(dialog));
    }

    void timersAndPostMessage()
    {
        g_messages.clear();
        WNDCLASS wc{};
        wc.lpfnWndProc = recordingProc;
        wc.lpszClassName = L"Temporizada";
        RegisterClass(&wc);
        HWND hwnd = CreateWindowEx(0, L"Temporizada", nullptr, WS_POPUP, 0, 0, 10, 10, nullptr, nullptr, nullptr, nullptr);
        QCOMPARE(SetTimer(hwnd, 7, 20, nullptr), UINT_PTR{7});
        QTRY_VERIFY(countOf(WM_TIMER, hwnd) >= 2);
        QCOMPARE(last(WM_TIMER)->wParam, WPARAM{7});
        QVERIFY(KillTimer(hwnd, 7));
        QVERIFY(!KillTimer(hwnd, 7));
        const UINT_PTR id = SetTimer(nullptr, 0, 20, timerProc);
        QVERIFY(id);
        QTRY_VERIFY(g_timerCalls >= 1);
        KillTimer(nullptr, id);
        // PostMessage llega después, en el bucle de eventos
        g_messages.clear();
        QVERIFY(PostMessage(hwnd, WM_USER + 1, 3, 4));
        QCOMPARE(countOf(WM_USER + 1), 0);
        QTRY_COMPARE(countOf(WM_USER + 1), 1);
        DestroyWindow(hwnd);
        QVERIFY(!PostMessage(hwnd, WM_USER + 1, 0, 0));
    }

    void menus()
    {
        HMENU menu = CreatePopupMenu();
        QVERIFY(AppendMenu(menu, MF_STRING, 10, L"&Uno"));
        QVERIFY(AppendMenu(menu, MF_SEPARATOR, 0, nullptr));
        QVERIFY(AppendMenu(menu, MF_STRING | MF_CHECKED, 11, L"Dos"));
        HMENU sub = CreatePopupMenu();
        AppendMenu(sub, MF_STRING, 20, L"Dentro");
        QVERIFY(AppendMenu(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(sub), L"Más"));
        QCOMPARE(GetMenuItemCount(menu), 4);
        QCOMPARE(GetMenuItemID(menu, 0), UINT{10});
        QVERIFY(GetMenuState(menu, 11, MF_BYCOMMAND) & MF_CHECKED);
        QCOMPARE(CheckMenuItem(menu, 10, MF_BYCOMMAND | MF_CHECKED), DWORD{MF_UNCHECKED});
        QVERIFY(GetMenuState(menu, 0, MF_BYPOSITION) & MF_CHECKED);
        EnableMenuItem(menu, 20, MF_BYCOMMAND | MF_GRAYED);
        QVERIFY(GetMenuState(menu, 20, MF_BYCOMMAND) & MF_GRAYED);
        QVERIFY(GetSubMenu(menu, 3));
        // TrackPopupMenu con TPM_RETURNCMD: la orden elegida
        QTimer::singleShot(100, [] {
            // Como el usuario: la orden activa e Intro
            if (auto* popup = qobject_cast<QMenu*>(QApplication::activePopupWidget())) {
                for (QAction* action : popup->actions()) {
                    if (action->text() == QLatin1String("Dos"))
                        popup->setActiveAction(action);
                }
                QTest::keyClick(popup, Qt::Key_Return);
            }
        });
        QCOMPARE(TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, 100, 100, 0, nullptr, nullptr), 11);
        QVERIFY(DeleteMenu(menu, 0, MF_BYPOSITION));
        QCOMPARE(GetMenuItemCount(menu), 3);
        QVERIFY(DestroyMenu(menu));
        QVERIFY(!DestroyMenu(menu == nullptr ? sub : nullptr));
    }

    void gdiAndSystem()
    {
        HFONT font = CreateFont(-14, 0, 0, 0, FW_BOLD, TRUE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                DEFAULT_QUALITY, FIXED_PITCH, L"Monospace");
        QVERIFY(font);
        LOGFONT lf{};
        QCOMPARE(GetObject(font, sizeof(lf), &lf), int(sizeof(LOGFONT)));
        QCOMPARE(lf.lfHeight, LONG{-14});
        QCOMPARE(lf.lfWeight, LONG{FW_BOLD});
        QCOMPARE(lf.lfItalic, BYTE{1});
        HDC dc = GetDC(nullptr);
        HGDIOBJ previous = SelectObject(dc, font);
        QVERIFY(previous);
        SIZE size{};
        QVERIFY(GetTextExtentPoint32(dc, L"abcdef", 6, &size));
        QVERIFY(size.cx > 20);
        QVERIFY(size.cy >= 14);
        TEXTMETRIC tm{};
        QVERIFY(GetTextMetrics(dc, &tm));
        QVERIFY(tm.tmHeight >= 14);
        QCOMPARE(SetBkMode(dc, TRANSPARENT), OPAQUE);
        QCOMPARE(SetTextColor(dc, RGB(1, 2, 3)), COLORREF{0});
        QCOMPARE(GetTextColor(dc), RGB(1, 2, 3));
        QCOMPARE(GetDeviceCaps(dc, LOGPIXELSY), 96);
        SelectObject(dc, previous);
        QCOMPARE(ReleaseDC(nullptr, dc), 1);
        QVERIFY(DeleteObject(font));
        QVERIFY(DeleteObject(GetStockObject(DEFAULT_GUI_FONT))); // las de serie no se borran
        QVERIFY(GetStockObject(DEFAULT_GUI_FONT));

        const QColor base = QApplication::palette().color(QPalette::Base);
        QCOMPARE(GetSysColor(COLOR_WINDOW), RGB(base.red(), base.green(), base.blue()));
        QScreen* screen = QGuiApplication::primaryScreen();
        QCOMPARE(GetSystemMetrics(SM_CXSCREEN), screen->geometry().width());
        QVERIFY(GetSystemMetrics(SM_CXVSCROLL) > 0);
        RECT work{};
        QVERIFY(SystemParametersInfo(SPI_GETWORKAREA, 0, &work, 0));
        QCOMPARE(work.right - work.left, LONG(screen->availableGeometry().width()));
        MONITORINFOEX info{};
        info.cbSize = sizeof(info);
        QVERIFY(GetMonitorInfo(MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY), &info));
        QVERIFY(info.dwFlags & MONITORINFOF_PRIMARY);
        QCOMPARE(QString::fromWCharArray(info.szDevice), screen->name().left(CCHDEVICENAME - 1));
        QVERIFY(!MonitorFromPoint(POINT{-100000, -100000}, MONITOR_DEFAULTTONULL));
        QCOMPARE(GetDpiForWindow(nullptr), UINT{96});
        // MulDiv redondea como Windows
        QCOMPARE(MulDiv(10, 96, 72), 13);
        QCOMPARE(MulDiv(-5, 3, 2), -8);
        QCOMPARE(MulDiv(5, 3, -2), -8);
        QCOMPARE(MulDiv(7, 1, 2), 4);
        QCOMPARE(MulDiv(1, 1, 0), -1);
        // min y max de windows.h
        QCOMPARE(max(3, 5), 5);
        QCOMPARE(min(3L, 5L), 3L);
        // Lo que N++ quitó de Scintilla.h
        QCOMPARE(SCI_GETTEXTRANGE, 2162);
        Sci_TextRange range{{0, 5}, nullptr};
        QCOMPARE(range.chrg.cpMax, Sci_PositionCR{5});
        QVERIFY(LoadCursor(nullptr, IDC_ARROW));
        QVERIFY(!LoadIcon(edita_compat_module(), MAKEINTRESOURCE(100)));
        QVERIFY(!FindResource(edita_compat_module(), MAKEINTRESOURCE(100), RT_DIALOG));
    }

    void threadsAndFormatMessage()
    {
        // Un hilo que espera un evento
        HANDLE go = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        DWORD threadId = 0;
        HANDLE thread = CreateThread(nullptr, 0, threadMain, go, 0, &threadId);
        QVERIFY(thread);
        QCOMPARE(WaitForSingleObject(thread, 50), DWORD{WAIT_TIMEOUT});
        DWORD code = 0;
        QVERIFY(GetExitCodeThread(thread, &code));
        QCOMPARE(code, DWORD{STILL_ACTIVE});
        QVERIFY(SetEvent(go));
        QCOMPARE(WaitForSingleObject(thread, 5000), DWORD{WAIT_OBJECT_0});
        GetExitCodeThread(thread, &code);
        QCOMPARE(code, DWORD{42});
        CloseHandle(thread);
        CloseHandle(go);
        // Un evento automático se apaga al esperarlo
        HANDLE automatic = CreateEvent(nullptr, FALSE, TRUE, nullptr);
        QCOMPARE(WaitForSingleObject(automatic, 0), DWORD{WAIT_OBJECT_0});
        QCOMPARE(WaitForSingleObject(automatic, 0), DWORD{WAIT_TIMEOUT});
        HANDLE both[] = {automatic, CreateEvent(nullptr, TRUE, TRUE, nullptr)};
        QCOMPARE(WaitForMultipleObjects(2, both, FALSE, 0), DWORD{WAIT_OBJECT_0 + 1});
        CloseHandle(automatic);
        CloseHandle(both[1]);
        // Mutex y sección crítica
        HANDLE mutex = CreateMutex(nullptr, FALSE, nullptr);
        QCOMPARE(WaitForSingleObject(mutex, 0), DWORD{WAIT_OBJECT_0});
        QVERIFY(ReleaseMutex(mutex));
        QVERIFY(!ReleaseMutex(mutex));
        CloseHandle(mutex);
        CRITICAL_SECTION section;
        InitializeCriticalSection(&section);
        EnterCriticalSection(&section);
        QVERIFY(TryEnterCriticalSection(&section)); // recursiva, como en Windows
        LeaveCriticalSection(&section);
        LeaveCriticalSection(&section);
        DeleteCriticalSection(&section);
        long counter = 1;
        QCOMPARE(InterlockedIncrement(&counter), 2L);

        // FormatMessage: el mensaje de un código de error, en un búfer de LocalAlloc
        LPWSTR message = nullptr;
        const DWORD length = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                           nullptr, ERROR_FILE_NOT_FOUND, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                           reinterpret_cast<LPWSTR>(&message), 0, nullptr);
        QVERIFY(length > 0);
        QVERIFY(QString::fromWCharArray(message).contains(QLatin1String("cannot find the file")));
        LocalFree(message);
        // Con inserciones
        void* inserts[] = {const_cast<wchar_t*>(L"uno"), const_cast<wchar_t*>(L"dos")};
        wchar_t buffer[64];
        FormatMessage(FORMAT_MESSAGE_FROM_STRING | FORMAT_MESSAGE_ARGUMENT_ARRAY, L"%2 y %1%%", 0, 0, buffer, 64,
                      reinterpret_cast<va_list*>(inserts));
        QCOMPARE(QString::fromWCharArray(buffer), QStringLiteral("dos y uno%"));
        QCOMPARE(PRIMARYLANGID(MAKELANGID(LANG_SPANISH, SUBLANG_DEFAULT)), WORD{LANG_SPANISH});
    }
};

QTEST_MAIN(TestNppCompat)
#include "tst_npp_compat.moc"
