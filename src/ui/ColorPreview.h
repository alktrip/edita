// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Vista previa de colores (no existe en Notepad++): `#3fb950`, `#fff8`, `rgb(…)`,
// `rgba(…)`, `hsl(…)` y `hsla(…)` se enmarcan con su propio color, en CSS, HTML, XML,
// JavaScript, TypeScript, JSON, Python, YAML, INI y propiedades. Al pasar el ratón se
// ve una muestra grande con los valores; Ctrl+clic abre el selector de color y
// reescribe el valor con el mismo formato.

#pragma once

#include "core/LangType.h"

#include <QColor>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>

#include <cstdint>
#include <functional>
#include <vector>

namespace edita {

class EditorView;

// Un color encontrado en el texto
struct ColorLiteral
{
    int start = 0;    // en bytes, dentro del texto analizado
    int length = 0;
    QColor color;
    enum class Format { Hex, Rgb, Hsl } format = Format::Hex;
};

// Los colores de un texto (UTF-8); los #RGB de 3 o 4 cifras solo si shortHex
std::vector<ColorLiteral> findColorLiterals(const QByteArray& text, bool shortHex = true);
// El color escrito en el mismo formato que `original` (#rrggbb, rgb(…), hsl(…)…)
QString formatColorLike(const QString& original, const QColor& color);

class ColorPreview : public QObject
{
    Q_OBJECT

public:
    static constexpr int kIndicator = 7; // N++ no usa los de 0 a 7 (los de los plugins empiezan en 9)
    // Un borde del color del texto para los colores que casi no se distinguen del fondo
    static constexpr int kOutlineIndicator = 6;

    explicit ColorPreview(EditorView* view);

    void setEnabled(bool enabled);
    bool isEnabled() const { return _enabled; }
    void setLanguage(LangType lang);
    static bool supports(LangType lang);

    // Vuelve a buscar en lo visible (en breve) o ya
    void schedule();
    void updateNow();

    // Ctrl+clic: el diálogo (se puede sustituir en las pruebas)
    void setColorPicker(std::function<QColor(const QColor&)> picker) { _picker = std::move(picker); }
    // El color que hay en esa posición, si hay
    bool colorAt(intptr_t pos, intptr_t* start, intptr_t* end, QColor* color) const;
    bool pickColorAt(intptr_t pos);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void clearAll();

    QPointer<EditorView> _view;
    bool _enabled = true;
    bool _supported = false;
    LangType _lang = LangType::Text;
    QTimer _timer;
    std::function<QColor(const QColor&)> _picker;
};

} // namespace edita
