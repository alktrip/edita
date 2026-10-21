/* This file is part of Edita.
 * Copyright (C) 2026 Edita contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Mensajes para que otros plugins ejecuten Python a través de PythonScript, con
 * los mismos números que NppPythonScript.h del PythonScript de Notepad++. Se
 * envían con NPPM_MSGTOPLUGIN al módulo "PythonScript.so":
 *
 *     PythonScript_Exec exec = {1, "editor.addText('hola')", NULL, PYSCRF_SYNC, 0};
 *     EditaCommunicationInfo info = {PYSCR_EXECSTATEMENT, "MiPlugin.so", &exec};
 *     data.send(data.host, NPPM_MSGTOPLUGIN, (uintptr_t)"PythonScript.so", (intptr_t)&info);
 *
 * Diferencia con N++: las cadenas son UTF-8 y completedEvent no se usa (Edita
 * ejecuta en el hilo de la interfaz; sin PYSCRF_SYNC, al volver al bucle de eventos).
 */

#ifndef EDITA_NPP_PYTHON_SCRIPT_H
#define EDITA_NPP_PYTHON_SCRIPT_H

#define PYSCR_START (0x0400 + 3500)

/* Ejecutar un archivo (ruta absoluta) */
#define PYSCR_EXECSCRIPT    (PYSCR_START + 0)
/* Ejecutar una sentencia */
#define PYSCR_EXECSTATEMENT (PYSCR_START + 1)
/* Mostrar la consola (sin argumentos) */
#define PYSCR_SHOWCONSOLE   (PYSCR_START + 2)

/* Ejecutar antes de devolver el control */
#define PYSCRF_SYNC 0x0001

typedef struct PythonScript_Exec
{
    int structVersion;     /* siempre 1 */
    const char* script;    /* sentencia o ruta, en UTF-8 */
    void* completedEvent;  /* no se usa en Edita */
    int flags;             /* PYSCRF_* */
    int deliverySuccess;   /* PythonScript lo pone a 1 al recibirlo */
} PythonScript_Exec;

#endif /* EDITA_NPP_PYTHON_SCRIPT_H */
