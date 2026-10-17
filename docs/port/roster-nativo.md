# Selección nativa de personajes para PC

La selección offline de Duelo, Entrenamiento y los equipos de Batalla Definitiva utiliza una pantalla del propio juego. Dibuja los retratos, casillas y escenarios mediante los sprites y recursos del motor. No necesita abrir F1.

La cuadrícula muestra 36 casillas por página, hasta la capacidad de `PORT_TOTAL_CHARAS` (290 en esta compilación). Cada personaje y forma ocupa una casilla. Las casillas disponibles dependen de los paquetes instalados; las restantes aparecen reservadas y no se pueden confirmar. Aumentar la capacidad requiere recompilar; los límites y tablas derivan de `port/include/port_limits.h`.

Duelo individual y Entrenamiento mantienen un personaje por lado. Duelo por equipos admite hasta diez por lado y muestra las diez posiciones. En Batalla Definitiva se puede ampliar el equipo del jugador en los combates compatibles; se conservan los rivales, equipo, objetos y dificultad definidos por la misión. Las historias y pruebas con reglas especiales conservan sus restricciones.

Tag Team y Todos contra Todos mantienen su combate simultáneo y sus propios límites. Raid permite hasta cuatro aliados contra un jefe CPU. Se accede a estos modos desde las entradas adicionales del menú principal con SELECT.

## Controles

| Botón | Acción |
| --- | --- |
| Cruceta | Mover el cursor |
| L1 / R1 | Cambiar página |
| X | Añadir el personaje a la posición activa |
| Círculo | Deshacer o volver |
| Triángulo | Cambiar equipo |
| Cuadrado | Cambiar traje entre los que tienen archivos válidos |
| L2 / R2 | Cambiar posición del equipo |
| SELECT | Cambiar tamaño entre 1, 3, 5 y 10, según el modo |
| L3 | Añadir o quitar favorito |
| R3 | Mostrar favoritos o todos |
| START | Confirmar equipo y elegir escenario |

Se pueden repetir personajes y elegir trajes diferentes por integrante. El segundo mando también opera el selector cuando se prepara el equipo 2 de una partida para dos jugadores. El ratón permite elegir casillas, posiciones del equipo y escenarios.

Las plantillas y los favoritos se guardan en los ajustes del port. No se amplía ni se modifica el formato de las partidas originales de PS2.

## Sala Raid

Tras elegir aliados, jefe y escenario, la pantalla ofrece combate local, crear sala o unirse. SELECT cambia la conexión; Triángulo alterna crear/unirse. L1/R1 elige un campo de IPv4 o puerto y arriba/abajo ajusta su valor. START confirma.

El anfitrión establece el jefe y el número de jugadores. Al unirse se aporta el primer aliado de la plantilla local. Cada equipo necesita los mismos datos y paquetes. La conexión utiliza UDP directo (puerto inicial 29740); para Internet el puerto del anfitrión debe ser accesible. No incluye emparejamiento ni retransmisión de servidores.

## Verificación reproducible

`BT3_NATIVE_ROSTER=1` abre la selección de equipos, `2` Entrenamiento y `3` Duelo individual, después de cargar los archivos nativos. `BT3_ROSTER_SCRIPT` recibe eventos `fotograma:botones_hex` relativos a la entrada al selector. Solo estas opciones de prueba omiten las pantallas de arranque.

Las pruebas en `port/tests/run_modes_test.sh` comprueban límites, duplicados, reservas, máscaras de equipo y la persistencia de plantillas y favoritos entre procesos. El cooperativo tiene pruebas separadas de cuatro procesos UDP en Linux y Windows.
