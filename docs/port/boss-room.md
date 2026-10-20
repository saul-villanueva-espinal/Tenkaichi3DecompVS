# Jefe Final: asalto simultaneo

La sala sigue la referencia aprobada Arena de energia: fondo azul y oro, cinco casillas de jefes, seis plazas de aliados, miniatura del mapa y modelos 3D independientes. El roster clasico conserva su presentacion anterior.

## Revision de la entrega anterior

Base revisada: b2407c6. La ultima captura `smoke-boss-test43/window_00180.png` mostraba el modelo oculto. La miniatura llamaba Sprite_DrawPicture despues de los modelos: esa funcion dibuja siempre 512x448 y tapaba el personaje. La sala configuraba un unico jefe. Las etiquetas de tres fases, reanimaciones y jugadores listos no representaban esas reglas ni conexiones reales. Se conservo un respaldo de ejecutable, configuracion y codigo antes de editar.

## Controles

Arriba/abajo mueve el foco entre botones y opciones. L2/R2 cambia el jefe seleccionado. Elegir jefes abre el catalogo instalado y permite escoger personaje y traje. L1/R1 cambia traje. Personalizar edita el perfil del jefe seleccionado. Ordenar permite mover su casilla con izquierda/derecha. La cantidad de jefes se ajusta entre uno y cinco. Cuadrado elige el aliado de este PC; en practica permite configurar todas las plazas CPU. SELECT abre conexion/IP/puerto. START inicia; Circulo vuelve. Los personajes repetidos se permiten.

Cada perfil pertenece al personaje y traje: vida, dano, IA, furia, armadura y Ki. La furia es una fase de comportamiento; no se anuncia una tercera fase ni una transformacion automatica.

## Red y combate

De dos a seis PC contra uno a cinco jefes CPU simultaneos. La practica permite un humano con hasta cinco aliados CPU. El motor reserva once combatientes y nueve recursos extra, manteniendo intacto el banco nativo de cinco integrantes. El sexto aliado se carga antes que los jefes extra para preservar el mapa de pads de red. Todos los adversarios son equipo 1 y CPU; los humanos son equipo 0. La victoria exige derrotar al equipo completo.

PcBattle conserva los primeros 57 enteros del formato previo y anade cuatro perfiles de jefe: 105 enteros, 420 bytes. Las configuraciones antiguas de 45 y 57 enteros siguen aceptandose. El protocolo UDP pasa a version 6; todos los participantes requieren la misma compilacion y los mismos datos/mods. Se sincroniza la plantilla del anfitrion, los perfiles de los cinco jefes y las entradas de seis jugadores. El hash incluye los once combatientes. El indicador de vida del grupo suma la vida de todos los jefes.

La sala es IPv4 UDP directa (29740 por defecto); para Internet se necesita una ruta accesible. No hay un servidor relay ni codigos de invitacion. Las plazas remotas no se presentan como conectadas antes de completar la conexion. Los personajes de ejemplo provienen del roster instalado; Zeno requiere instalar su modelo, no se sustituye su vista 3D por una imagen.
