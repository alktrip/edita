/* atlconv.h para compilar plugins de Notepad++ con el SDK de Edita: las macros de conversión
 * de ATL (USES_CONVERSION, W2A, A2W, T2A…) y las clases CW2A, CA2W… La página de código «del
 * sistema» de la capa es UTF-8. ATL devuelve memoria de la pila (_alloca), válida en la
 * función; aquí, de un anillo de 16 búferes por hilo: vale mientras no se hagan otras 16
 * conversiones en el mismo hilo. */
#pragma once
#include "npp_compat.h"

#ifdef __cplusplus
#include <string>

namespace edita_atl {

inline char* toNarrow(const wchar_t* text)
{
    if (!text)
        return nullptr;
    thread_local std::string ring[16];
    thread_local unsigned next = 0;
    std::string& out = ring[next++ % 16];
    const int size = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    out.assign(size > 0 ? static_cast<std::size_t>(size) : 1u, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, &out[0], size, nullptr, nullptr);
    return &out[0];
}

inline wchar_t* toWide(const char* text)
{
    if (!text)
        return nullptr;
    thread_local std::wstring ring[16];
    thread_local unsigned next = 0;
    std::wstring& out = ring[next++ % 16];
    const int size = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
    out.assign(size > 0 ? static_cast<std::size_t>(size) : 1u, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text, -1, &out[0], size);
    return &out[0];
}

// Las de TCHAR, según UNICODE
#ifdef UNICODE
inline char* tToNarrow(const wchar_t* text) { return toNarrow(text); }
inline wchar_t* narrowToT(const char* text) { return toWide(text); }
#else
inline char* tToNarrow(const char* text) { return const_cast<char*>(text); }
inline char* narrowToT(const char* text) { return const_cast<char*>(text); }
#endif

class CW2A
{
public:
    CW2A(const wchar_t* text, unsigned = 0) : _text(toNarrow(text) ? toNarrow(text) : "") {}
    operator char*() { return &_text[0]; }
    operator const char*() const { return _text.c_str(); }

private:
    std::string _text;
};

class CA2W
{
public:
    CA2W(const char* text, unsigned = 0) : _text(toWide(text) ? toWide(text) : L"") {}
    operator wchar_t*() { return &_text[0]; }
    operator const wchar_t*() const { return _text.c_str(); }

private:
    std::wstring _text;
};

} // namespace edita_atl

#define USES_CONVERSION ((void)0)
#define USES_CONVERSION_EX ((void)0)
#define W2A(s) edita_atl::toNarrow(s)
#define W2CA(s) ((const char*)edita_atl::toNarrow(s))
#define A2W(s) edita_atl::toWide(s)
#define A2CW(s) ((const wchar_t*)edita_atl::toWide(s))
#define OLE2A W2A
#define A2OLE A2W
#ifdef UNICODE
#define T2A(s) edita_atl::toNarrow(s)
#define A2T(s) edita_atl::toWide(s)
#define T2W(s) ((wchar_t*)(s))
#define W2T(s) ((wchar_t*)(s))
#else
#define T2A(s) ((char*)(s))
#define A2T(s) ((char*)(s))
#define T2W(s) edita_atl::toWide(s)
#define W2T(s) edita_atl::toNarrow(s)
#endif
#define T2CA(s) ((const char*)T2A(s))
#define T2CW(s) ((const wchar_t*)T2W(s))
#define W2CT(s) ((const TCHAR*)W2T(s))
#define A2CT(s) ((const TCHAR*)A2T(s))
using edita_atl::CW2A;
using edita_atl::CA2W;
#ifdef UNICODE
typedef edita_atl::CA2W CA2T;
typedef edita_atl::CW2A CT2A;
#else
typedef edita_atl::CW2A CW2T;
typedef edita_atl::CA2W CT2W;
#endif
#endif /* __cplusplus */
