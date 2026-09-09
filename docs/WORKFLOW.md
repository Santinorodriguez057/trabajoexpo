# Trabajo en equipo — Flujo de Git

Objetivo: cada persona desarrolla sus juegos por separado y todo queda
conectado en el menú sin pisarse.

## Reparto de carpetas

| Carpeta                | Quién la toca                          |
|------------------------|----------------------------------------|
| `menu/`                | Compartida (coordinar cambios)         |
| `games.registry`       | Compartida (añadir tu juego al final)  |
| `CMakeLists.txt` raíz  | Compartida (añadir tu `add_subdirectory`) |
| `games/sdl2/<tuyo>/`   | Solo tú                                |
| `games/unity/<tuyo>/`  | Solo tú                                |

## Flujo recomendado

1. Clonar el repositorio:

   ```bash
   git clone <url-del-repo> "trabajo expo"
   cd "trabajo expo"
   ```

2. Cada uno trabaja en su rama:

   ```bash
   git checkout -b juego/<tu-nombre>
   ```

3. Desarrolla tu juego en tu carpeta. Cuando esté listo para integrarse:

   ```bash
   git add games/<tu-carpeta> games.registry CMakeLists.txt
   git commit -m "Añade juego <nombre>"
   git push origin juego/<tu-nombre>
   ```

4. Crea un Pull Request hacia `main` y que el otro lo revise.

5. Antes de empezar a trabajar, actualiza siempre:

   ```bash
   git checkout main
   git pull
   ```

## Evitar conflictos

- **No edites los juegos del otro.** Cada uno solo toca su carpeta.
- `games.registry` y el `CMakeLists.txt` raíz se tocan poco: añade tu línea al
  final y haz commit pronto para que el otro no tenga que resolver conflictos.
- Si hay conflicto en `games.registry`, es trivial: quedarse con las dos líneas.
- El menú (`menu/`) es compartido: si lo vas a cambiar, avisa al otro o hazlo
  en una rama y merge rápido.

## Convención de commits

- `Añade juego <nombre>` — nuevo juego
- `Mejora <juego>: <qué>` — cambios en un juego
- `Menú: <qué>` — cambios en el menú
- `Docs: <qué>` — documentación

## Reglas de los juegos

- Deben terminar limpiamente (devolver 0) para que el menú recupere el control.
- Deben funcionar con mando PS4 (X = confirmar, Círculo = volver).
- No dependas de rutas absolutas: usa rutas relativas a la raíz del repo.