# Project ARCX

**Project ARCX** es un plugin `.so` para **AML (Android Mod Loader)** que añade un **selector de skins** a **GTA San Andreas 2.10 (64-bit, Android)**. Te deja cambiar la apariencia de CJ por cualquier skin de personaje (ped) desde un menú táctil dentro del juego, sin tocar los archivos del juego.

> Versión actual: **3.1.40-beta** · por **Hunter09**

## ¿Qué hace?

- **Cambia la skin de CJ en el juego**: abre el menú, toca una skin y CJ se convierte en ese personaje al instante.
- **La skin se mantiene durante las misiones** y solo se quita en las cinemáticas. También puedes cambiar de skin estando dentro de un vehículo.
- **Recuerda tu última skin** y la vuelve a poner al entrar al juego (opcional).
- **Menú propio con ImGui**: botón flotante en pantalla, pestañas **SKINS** y **AJUSTES**, y colores del menú personalizables (temas predefinidos o tu propio color con el panel selector: cuadro de color + barra de tono + valores RGB y hexadecimal).
- **Dos fuentes de skins**, cada una con su propio cargador (ver abajo).

## Requisitos

- GTA San Andreas **2.10 (64-bit)** para Android.
- **AML Core 1.4.1** o superior.
- **ZyZ ModLoader** (opcional): solo necesario si quieres usar skins de la carpeta `modloader/`. Si no está instalado, ARCX funciona solo con sus propias skins y la sección de ZyZ aparece bloqueada en Ajustes.

## Instalación

1. Copia `libARCX64.so` a la carpeta de mods de AML (por ejemplo `Android/data/com.rockstargames.gtasa/files/AML/mods/` o donde tu AML tenga los plugins).
2. Pon tus skins en una de estas dos carpetas (se crean dentro de la carpeta de datos del juego):
   - `modloader/` → skins cargadas al **100% por ZyZ ModLoader** (solo si ZyZ está instalado).
   - `ARCXLoader/` → skins cargadas por el **cargador propio de ARCX**.
3. Abre el juego y toca el botón flotante **ARCX** para abrir el menú.

## Cómo agregar skins (ARCXLoader)

Cada skin va en su propia carpeta dentro de `ARCXLoader/`:

```
ARCXLoader/
└── MiSkin/
    ├── MiSkin.dff      ← el modelo (obligatorio, el nombre del archivo es el nombre de la skin)
    ├── MiSkin.txd      ← las texturas (opcional, si el modelo las trae en .txd)
    └── *.png / *.jpg   ← texturas en imágenes (opcional, si no hay .txd)
```

- El `.dff` debe ser un **ped** (modelo de personaje con huesos).
- Si el modelo trae `.txd`, ARCX lo carga directo. Si no, puedes poner las texturas como **PNG o JPG** con el mismo nombre que pide el modelo y ARCX las aplica.
- En el menú, cada skin muestra de dónde se cargó: *"Cargado desde ARCXLoader"* o *"Cargado desde ZyZ ModLoader"*.
- En **AJUSTES** puedes activar o desactivar cada fuente. Solo una puede estar activa a la vez: al activar una, la otra se desactiva.

## Uso del menú

- **SKINS**: lista de skins detectadas. Toca una para ponértela; la que llevas puesta dice **PUESTA**. *"Volver a ser CJ"* te devuelve a CJ.
- **AJUSTES**: recordar última skin, bloquear el botón flotante, tamaño y opacidad, idioma (español / inglés / portugués) y **COLOR** del menú (temas o color personalizado con el cuadro arcoíris).
- Para esconder el panel de colores sin perder tu color, toca el cuadro arcoíris otra vez; tócalo de nuevo y el panel vuelve.

## Limitaciones conocidas

- **Modelos muy pesados pueden cerrar el juego al dibujarse.** Un ped normal de GTA SA tiene entre 4,000 y 10,000 triángulos; modelos portados de otros juegos con 60,000+ triángulos (por ejemplo Ghost) se aplican bien pero pueden cerrar el juego de forma aleatoria mientras se ven en pantalla. No es un error del cargador: es el modelo.
- Las texturas **DXT1** no las puede leer la GPU de algunos teléfonos (por ejemplo Mali); en ese caso usa las texturas en PNG.

## Compilación

El proyecto se compila automáticamente con **GitHub Actions** (`.github/workflows/build.yml`) usando el NDK de Android. Cada push a `main` genera el artefacto `ARCX` con el `libARCX64.so` listo para instalar.

## Créditos

- **Hunter09** — Project ARCX.
- Texturas y modelos de ejemplo por sus respectivos autores.
