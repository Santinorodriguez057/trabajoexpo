# Juegos Unity

Cada juego Unity es un proyecto Unity independiente en su propia carpeta:

    games/unity/<nombre-del-juego>/

## Crear un juego nuevo

1. En Unity Hub, crea un proyecto nuevo en `games/unity/<nombre-del-juego>`.
2. Desarrolla el juego con normalidad.
3. IMPORTANTE: el juego debe poder cerrarse limpiamente (por ejemplo, con
   `Application.Quit()` al pulsar un boton) para que el menu recupere el control.

## Compilar el ejecutable

1. File > Build Settings.
2. Target Platform: Linux x86_64 (o Windows x86_64).
3. Marca "Development Build" si quieres logs.
4. Build y elige como destino `build/bin/`.
   - Linux: el ejecutable se llama como el proyecto (sin extension).
   - Windows: se llama `<nombre>.exe`.
5. Copia/renombra el ejecutable a `build/bin/<id>` (o `build/bin/<id>.exe`).

## Registrar el juego en el menu

Anade una linea en `games.registry` (campos separados por TAB):

    <id>	<Nombre visible>	<Descripcion>	build/bin/<id>	build/bin/<id>.exe

## Mandos PS4 en Unity

- Usa el paquete "Input System" (Window > Package Manager > Input System).
- Activa "Active Input Handling" = Input System Package (Project Settings > Player).
- Los mandos PS4 se detectan como "DualShock 4" / "DualSense".
- Mapea los botones en un Input Actions asset (X = A, Circulo = B, etc.).