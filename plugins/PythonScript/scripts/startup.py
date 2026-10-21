# -*- coding: utf-8 -*-
# startup.py de la máquina (el que trae el plugin PythonScript de Notepad++).
# Se ejecuta al iniciar Python: importa Npp en __main__ y lleva la salida a la
# consola. El startup.py del usuario (carpeta de configuración de los plugins,
# PythonScript/scripts/startup.py) se ejecuta después.
import sys
from Npp import *

# Los errores, cuanto antes a la consola
sys.stderr = console


# Los errores en rojo
class ConsoleError:
    def __init__(self):
        global console
        self._console = console

    def write(self, text):
        self._console.writeError(text)

    def flush(self):
        pass


sys.stderr = ConsoleError()

# Las funciones "normales", como help
import site  # noqa: E402,F401

sys.stdout = console

# Para que print escriba en el documento actual, descomentar esta línea:
# sys.stdout = editor
