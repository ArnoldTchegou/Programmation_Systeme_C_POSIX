//MARYAM AIT DAOUD et ARNOLD NGNOUBA NDIE TCHEGOU

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <signal.h>
void client_arrived(int sock);
void listen_port(int port_num);

int main(int argc, char *argv[]){
	if(argc!=2){
		printf("syntaxe error: %s port_num\n", argv[0]);
		exit(1);
	}
	int port = atoi(argv[1]);
	if(port == 0){
		printf("numero de port non valide\n");
		exit(1);
	}
	listen_port(port);
	//signal(SIGPIPE, SIG_IGN);
}

void client_arrived(int sock){
	char buff[100];
	char mess[128];
	int r;
	//lire le message Reçue
	while ( (r=read(sock,buff,99)) >0) {
		buff[r]='\0';
		//construire le message à envoyer
		strcpy(mess, "j’ai reçu: ");
		strncat(mess, buff, 127);
		mess[127] = '\n';
		//renvoyer le message au client 
		write(sock, mess, strlen(mess));
	}
}
void listen_port(int port_num){
	//crée une socket s
	int s=socket(PF_INET6,SOCK_STREAM, 0 );
	//crée une structure pour l’adresse
	struct sockaddr_in6 sin;
	//remplir de zéro avec memset
	memset(&sin, 0, sizeof(sin));
	//affecte les valeurs à ses deux champs
	sin.sin6_family = AF_INET6;
	sin.sin6_port = htons(port_num);
	//Réutilisation d'un numéro de port après un délai d'une séconde
	int optval = 1;
	int r = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	if(r<0){
        	perror("setsockopt"); 
    	}
	//lie la socket s et la structure sin avec bind()
	r = bind(s, (struct sockaddr *)&sin, sizeof(struct sockaddr_in6));
	if(r<0){
        	perror("bind"); 
        	close(s);
        	exit(1);
    	}
	//lance l’écoute
	r = listen(s,1024);
	if(r<0){
        	perror("listen");
        	close(s);
        	exit(1);
    	}
    	printf("j'attends mes clients\n");
	//lance une boucle infinie pour accepter les clients
	while(1){
        	int s2=accept(s,NULL,NULL);
		if (s2<0){
		    perror("accept"); 
		    continue;
		}
		printf("client connecté\n");
		//Lorsqu’un client arrive, on appelle client_arrived
		client_arrived(s2);
		//detruire le socket
		close(s2);
		printf("client déconnecté\n");
		printf("j'attends qu'un client se connecte\n");
    	}
}










