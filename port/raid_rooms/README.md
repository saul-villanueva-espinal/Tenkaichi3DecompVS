# Salas privadas de Jefe Final — estado: prototipo local

**Actualización 22/10/2026:** la integración de lobby y transporte ya existe.
Se añadió `RESUME=9` para conservar la pertenencia a la sala entre procesos,
probado con dos y seis participantes y 300 turnos. La ruta local del anfitrión
se configura por separado en `tbo/online_host_server.txt`; los invitados usan
`tbo/online_server.txt`. `local_host.py` comprueba/inicia el servidor local.
El acceso desde otras casas sigue pendiente de una ruta UDP pública; el
diagnóstico específico y las instrucciones están en `ESTADO_CREAR_SALA_22-10.md`
en la raíz del espacio de trabajo. Las secciones siguientes describen el
prototipo original y sus tareas iniciales, no el estado actualizado completo.

Este servicio es propio del port con mods. No utiliza las credenciales ni los
servidores del matchmaking oficial. Cambiar el número de versión o convertir una
IP en un texto BT3-XXXXXX no proporciona conectividad entre hogares.

## Qué está implementado y probado

- Directorio UDP de salas privadas de 2 a 6 participantes.
- Invitación aleatoria de seis caracteres, persistente al reintentar crear sala.
- Comprobación de versión del protocolo y huella de los mods al entrar.
- Relay de paquetes entre invitados y anfitrión usando conexiones UDP salientes.
- Token aleatorio por participante, validación de origen, aislamiento entre salas,
  límites de paquetes/capacidad, cierre por anfitrión y caducidad.
- Cliente C independiente en `port/src/gs/coop_rooms.c` para el mismo transporte.
- Prueba de seis procesos C contra el servidor Python y ocho pruebas del directorio.

**Todavía no conectado al ejecutable del juego ni al borrador de la interfaz.**
La prueba es local: no acredita una partida por Internet, sincronización real de
combate, ni conectividad con routers que bloqueen también el UDP saliente.

## Probar localmente

Python 3.10 o posterior, sin paquetes externos:

```sh
python port/raid_rooms/server.py
```

El servidor escucha únicamente `127.0.0.1:29741` por defecto. Para las pruebas
del cliente C se configura `BT3_ROOM_SERVER=127.0.0.1:29741`. Alternativamente,
el cliente lee una línea `servidor:puerto` en `tbo/online_server.txt`.

Pruebas, con GCC/cc en PATH:

```sh
python port/tests/run_room_relay_test.py
```

En Windows se necesita Winsock (`-lws2_32`); el ejecutor de pruebas ya lo añade.

## Requisito para jugar desde dos casas

Hace falta alojar este servicio en un equipo/servidor con dirección pública y
puerto UDP accesible. El servidor debe permitir el puerto UDP 29741 en su firewall
y el proveedor debe permitir tráfico UDP. Los jugadores solo necesitan enviar
UDP al servidor; no tienen que compartir su IP ni abrir un puerto entrante del
router. Ejecutar este programa en una PC detrás de CGNAT no la convierte en un
servidor público. No hay alojamiento público provisionado por este proyecto aún.

Comando para un servidor que ya tenga esa conectividad configurada:

```sh
python port/raid_rooms/server.py --bind 0.0.0.0 --port 29741
```

Para un servidor Linux, se recomienda un usuario de sistema sin privilegios y un
servicio que reinicie el proceso si falla. No ejecutar como administrador/root.
El puerto 29741 es superior a 1024 y no necesita privilegios elevados.

## Integración pendiente

1. Confirmar que Antigravity ya no modifica el borrador de `port_raid.c`; conservarlo.
2. Enlazar `RoomRelay_Open/Send/Receive/Close` al transporte de `coop_net.c`.
   Sus direcciones virtuales por jugador son solo internas y jamás se envían
   directamente a la red; los paquetes reales van al servidor configurado.
3. Reemplazar el código calculado desde IP por la invitación realmente devuelta
   por el servidor. No mostrar «Sala creada» antes de recibir confirmación.
4. Separar conexión y comprobación de mods de la confirmación «Listo» de cada
   jugador. Solo el anfitrión puede pedir comenzar, con al menos dos conectados
   y todos listos. Capacidad máxima y cantidad actual son valores distintos.
5. Integrar nombres, elección de personaje, indicadores de conexión, copiar/pegar
   código y cancelación dentro de la sala nativa de Jefe Final.
6. Elegir/provisionar el alojamiento público y configurar la dirección del
   servicio en ambos clientes. No poner secretos de administración en el paquete.
7. Probar entre dos PC de redes distintas: crear, unirse, mods incompatibles,
   listo, iniciar, combatir, desconexión y volver al menú sin cierre.

## Límites del prototipo

El relay no cifra el contenido de los datagramas. Los tokens y las invitaciones
se deben tratar como datos privados de la sesión; no equivalen a cuentas de
usuario ni a autenticación fuerte de identidad. Los nombres no se registran en
disco y el servidor no recibe archivos del juego, solo huellas y paquetes.
Es un servicio privado de pruebas, no un servicio público endurecido frente a
ataques distribuidos. No se recicla un puesto abandonado durante un combate:
la sesión del juego debe detectar su pérdida y regresar al menú.

## Protocolo de transporte, versión 1

Cabecera big endian de 40 bytes (`!4sBBHI16s8sBBH`): magia `BT3R`, versión,
operación, reservado cero, ID de solicitud, token de 16 bytes, código de sala
de 8 bytes con ceros finales, emisor, destino, longitud del contenido.
Contenido máximo: 1360 bytes; datagrama completo máximo: 1400 bytes.

Operaciones: CREATE=1, JOIN=2, ACCEPT=3, DATA=4, PING=5, PONG=6,
LEAVE=7, ERROR=8. CREATE/JOIN llevan `!QBB32s` (huella, versión del protocolo
interior de juego, capacidad, apodo). ACCEPT lleva dos bytes: puesto asignado y
capacidad. DATA contiene el paquete original del cooperativo sin modificarlo.
El servidor sustituye el token al reenviar y solo permite invitados hacia
anfitrión y anfitrión hacia invitados. Nunca envía las IP de los jugadores a
otros participantes.
