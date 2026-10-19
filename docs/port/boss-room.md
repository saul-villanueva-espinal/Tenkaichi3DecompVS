# Jefe Final: sala cooperativa

La sala usa los recursos originales de BT3 y un modelo 3D rotatorio independiente. El selector clasico no cambia.

- Arriba/abajo: fila. Izquierda/derecha: personaje u opcion.
- L1/R1: traje instalado del personaje visible.
- Triangulo: personalizar jefe. X sobre Guardar conserva un perfil por personaje y traje.
- Mi guerrero: elegir el personaje de este PC. En practica tambien se eligen los aliados CPU.
- Red: cuatro campos IPv4 y puerto UDP. Cuadrado suma diez al campo.
- START: iniciar practica o abrir/unirse a la sala. X confirma subpantallas; Circulo vuelve.

La practica permite un humano con hasta cinco aliados CPU. La sala UDP admite de dos a seis PC; cada participante escoge su J1 antes de conectarse. Las otras plazas se muestran como esperando, no como jugadores conectados. La conexion inicia al reunir el total elegido por el anfitrion. Reglas y escenario se reciben del anfitrion; se comprueban las huellas de datos/mods. Para Internet se necesita una ruta UDP accesible (puerto por defecto 29740); no hay un servidor relay de codigos de sala.

Perfiles: vida de 10 000 a 250 000, dano de 50 a 400 %, nivel de IA de 1 a 30, furia opcional al 10..90 % de vida, armadura y Ki infinito. El modo conserva las fases normal/furia del motor. No se anuncian una tercera fase, parejas tag ni reglas de victoria adicionales que el combate aun no ejecuta.

El sexto aliado se carga fuera de BattleSide.members[5], cuyo ABI permanece intacto. Los jugadores de red tienen seis pads virtuales; los mandos locales y sus grabaciones conservan la ruta de cuatro. El protocolo cooperativo se incrementa a version 5: todos los participantes deben usar esta compilacion, ademas de datos identicos.
