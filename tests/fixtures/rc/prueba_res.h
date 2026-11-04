// Recursos de prueba de edita_rc (fase 53)
#pragma once
#define IDD_OPCIONES 2000
#define IDD_PANEL 2001
#define IDC_BASE 2100
#define IDC_NOMBRE (IDC_BASE + 1)
#define IDC_ACTIVAR (IDC_BASE + 2)
#define IDC_MODO 2103
#define IDC_LISTA 2104
#define IDC_COLUMNAS 2105
#define IDC_ARBOL 2106
#define IDC_UNO 2106
#define IDR_CONTEXTO 2200
#define IDM_COPIAR 2201
#define IDM_MARCADO 2202
#define IDM_GRIS 2203
#define IDS_SALUDO 2300
#define IDS_OTRO 2301
#define IDS_UNICODE 2302
#define IDI_ICONO 2400
#define IDC_VERSION 2107
#define IDC_DOS 2108
#define IDS_LARGA 2303

// Macros con argumentos, como en los .rc de ComparePlus: # convierte en cadena, ## pega
#define VERSION_PLUGIN 1.2.3
#define CADENA_(x) #x
#define CADENA(x) CADENA_(x)
#define CONTROL_ID(nombre) IDC_##nombre

// En una cabecera, rc.exe solo lee las directivas: esto se ignora
int noEsUnRecurso = IDD_OPCIONES;
struct TampocoEsto { int x; };
