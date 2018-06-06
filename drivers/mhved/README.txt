MHVED means MHYPER Virtual Ethernet Driver

La idea seria disponer de un driver PROMISCUO en la VM0 que dispone de un set de buffers por cada VM
De esa forma el driver se comporta como un virtual HUB, SWITCH, ROUTER.
En principio se podria comportar como un HUB y asignaria una MAC ADDRESS en funcion de cada VM.

Lo mejor es disponer de un buffer general de frames y hacer una funcion
	alloc_frame() y free_frame()
Cada frame_buf debe tener un campo counter que indica cuantos punteros apuntan a el (se requiere por multicast)
	
Los TX/RX buffers apuntan a NULL si estan vacios o a uno de los buffers si estan ocupados.

EQUIVALENTE a multiples productores con multiples consumidores
	
1) Cada Cliente INET enviara un mensaje al MHVED para enviar una peticion simple o vectorizada de ESCRITURA/LECTURA datos 
	Hasta que no se termine la misma no se le responde al cliente INET.

2) Se almacena la peticion simple o vectorizada en la interface

3) se pollean todas las interfaces para ver si hay mensajes pendientes de envia	
	y tratan de enviarse

4) se pollean todas las interfaces para ver si hay solicitudes de lectura y se leen las mismas

5) se vuelven a pollear las interfaces de escritura para ver si las lecturas liberaron buffers.


El MHVED:
	a) asigna el siguiente slot en el buffer TX del emisor 
	b) alloc_frame()
	c) copia frame desde INET a un frame_buf 
	b) determina quien es el receptor (aqui es SWITCH o ROUTER) 
	c) asigna el siguiente slot en el buffer RX del receptor, copia el puntero al frame_buf
	d) UNICAST:counter=1 MULTICAST:counter=N (nro de interfaces activas) 
	d) libera el slot del buffer TX del emisor
	d) responde al emisor
	e) si el destinatario no esta esperando lo notifica

2) Cada Cliente INET enviara un mensaje al MHVED para recibir un paquete
El MHVED:
	a) copiara el frame desde el buffer de RX del receptor al INET
	c) counter--; if conter = 0 then free_frame()
	d) responde al receptor

QUIEN INSERTA LA MAC ADDRESS DEL DESTINO ??

CONFIRMADO, el INET le envia el FRAME ARMADO al driver Ethernet.

POSIBLE PROBLEMA:
	Que todos los buffers RX esten llenos de todas las interfaces y que todos los clientes
	quieran enviar mensajes con sus buffer TX llenos.


http://www.cis.syr.edu/~wedu/seed/documentation.html
http://www.os-forum.com/minix/net/index.html
http://www.nyx.net/~ctwong/minix/Minix_inet.htm


TODO:
	Cada slot dispone de un FRAME, esto solo sirve para UNICAST
	Para Multicast hay que hacer que el frame tenga puntero a otro frame
	implementar init_frame, alloc_frame, free_frame.











