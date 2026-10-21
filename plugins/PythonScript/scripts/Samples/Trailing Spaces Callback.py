# -*- coding: utf-8 -*-
# Ejemplo de callback: antes de guardar, quita los espacios del final de las líneas
# de los archivos .py. Se queda activo hasta cerrar Edita (o clearCallbacks).
notepad.clearCallbacks([NOTIFICATION.FILEBEFORESAVE])


def strip_trailing(args):
    if notepad.getBufferFilename(args["bufferID"]).endswith(".py"):
        notepad.activateBufferID(args["bufferID"])
        editor.rereplace(r"[ \t]+$", "")


notepad.callback(strip_trailing, [NOTIFICATION.FILEBEFORESAVE])
console.write("FILEBEFORESAVE callback registered: .py files are trimmed on save.\n")
