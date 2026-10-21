# -*- coding: utf-8 -*-
# Ejemplo: inserta la fecha y la hora en el cursor y lo dice en la barra de estado.
import datetime

now = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
editor.replaceSel(now)
notepad.setStatusBar(STATUSBARSECTION.DOCTYPE, "Python Script: fecha insertada")
