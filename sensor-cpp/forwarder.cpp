/*Estrae in modo asincrono i pacchetti dalla coda gestita da packet_handler.cpp. 
Legge gli header di base (IP e porte), popola la struttura Protobuf, serializza i dati in formato binario
 compresso e li spara via socket di rete verso il microservizio in Go.*/

 