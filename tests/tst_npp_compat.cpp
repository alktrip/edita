// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 51: la parte de Win32 sin ventanas de la capa de compatibilidad para plugins de
// Notepad++ (plugin-sdk/compat/npp_win32.cpp), compilada con UNICODE como los plugins
// modernos de N++: conversiones, printf de MSVC, archivos .ini, archivos, rutas,
// módulos, entorno, portapapeles, strsafe.h y el CRT de MSVC.

#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QMimeData>
#include <QTemporaryDir>
#include <QTest>

#include <string>

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
};

QTEST_MAIN(TestNppCompat)
#include "tst_npp_compat.moc"
