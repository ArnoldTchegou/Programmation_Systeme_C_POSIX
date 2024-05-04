//MARYAM AIT DAOUD et ARNOLD NGNOUBA NDIE TCHEGOU

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <signal.h>
#include <pthread.h>

//variable globale pour compter le nombre de clients.
int nr_clients; //objet critique
//variable globale un verrou mutex Pour éviter les data races protégez la manipulation du compteur
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
//pour stocker les paramètres de chaque client
typedef struct client_data_s {
	int sock;
	pthread_t thread;
}client_data;


void init_clients(); //procedure utilisée pour mettre le compteur nr_clients=0
client_data* alloc_client(int sock); //constructeur alloc_client
void free_client(client_data* cli) ;//destructeur free_client
void worker(void* arg); //fonction worker lancée dans chaque thread par client_arrived
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
	//La fonction main appel init_clients avant de se mettre à l’écoute
	init_clients();
	listen_port(port);
}

//procedure utilisée pour mettre le compteur nr_clients=0
void init_clients(){
	nr_clients = 0;
}

//constructeur alloc_client
client_data* alloc_client(int sock){
	//alloue la mémoire pour une structure client_data et renvoie NULL en cas d’échec
	client_data* data = (client_data*)malloc(sizeof(client_data));
	if(data==NULL){
		return NULL;
	}
	//initialise sont champ sock par le numéro passé en paramètre
	data->sock = sock;
	//incrémenter le compteur nr_clients-->section critique
	pthread_mutex_lock(&lock);
	nr_clients++;
	pthread_mutex_unlock(&lock);
	//renvoyer le pointeur de client_data alloué
	return data;
}

//destructeur free_client
void free_client(client_data* cli){
	free(cli);
	//décrémenter le compteur nr_clients-->section critique
	pthread_mutex_lock(&lock);
	nr_clients--;
	pthread_mutex_unlock(&lock);
}

//fonction worker lancée dans chaque thread par client_arrived
void worker(void* arg){
	client_data* cli = (client_data* )arg;
	char buff[100];
	char mess[128];
	int r;
	//lire le message Reçue
	while ( (r=read(cli->sock,buff,99)) >0) {
		buff[r]='\0';
		//construire le message à envoyer
		strcpy(mess, "Je vous entend: ");
		strncat(mess, buff, 127);
		mess[127] = '\n';
		//renvoyer le message au client 
		write(cli->sock, mess, strlen(mess));
	}
	//Lorsque le client se déconnecte, le worker clot la socket, appelle free_client et sort
	close(cli->sock);	
	free_client(cli);
	if(nr_clients==0){
		printf("j'attends mes clients\n");
	}
	else{
		printf("client déconnecté\n");
	    	printf("Il y'a %d clients actifs\n", nr_clients);
	    	printf("\n");
	}
}

void client_arrived(int sock){
	//créer une donnée client avec pour paramètre son socket
	client_data* cli=alloc_client(sock) ;
	//créer un thread en lui passant comme paramètre le pointeur du client
	int rc;
	if((rc=pthread_create(&cli->thread, NULL, (void* )worker, (void *)cli))!=0){
		fprintf(stderr, "pthread_create: code %d\n", rc);
		exit(1);
	}
	printf("Lancement de Thread file %d\n", nr_clients);
	//afficher le nombre de clients actifs ;
	printf("Il y'a %d client(s) actif(s)\n", nr_clients);
	printf("\n");
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
		//Lorsqu’un client arrive, on appelle client_arrived
		client_arrived(s2);
    	}
}











