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
#include <time.h>
#define BUFSIZ 8192

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
void client_arrived(int sock);//ecoute sur la socket du client et lui renvoie tous ses messages
void listen_port(int port_num);
void advance(char *buf, int *plen, int n);
int message_end(char *buf,int len);
void handle_question(char *que);
int get_question(int sock, char *buf, int *plen, char* question);
void handle_client(int s);
int write_full(int fd, char *buf, int len);
int do_rand (char* args, char* resp);
int do_echo (char* args, char* resp);
int do_quit (char* args, char* resp);
int do_time (char* args, char* resp);
int eval_quest(char *question, char *resp);


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
	if(nr_clients==0){
		printf("j'attends mes clients\n");
	}
	else{
		printf("client déconnecté\n");
	    	printf("Il y'a %d clients actifs\n", nr_clients);
	    	printf("\n");	
	}
}

//fonction worker lancée dans chaque thread par client_arrived
void worker(void* arg){
	client_data* cli = (client_data* )arg;
	char question[BUFSIZ], response[BUFSIZ],buf[BUFSIZ];
	int len=0;
	//lire le message Reçue
	while(1) {
		if(get_question(cli->sock,buf, &len, question) < 0)
			break; /* erreur: on deconnecte le client */
		if(eval_quest(question, response) < 0)
			break; /* erreur: on deconnecte le client */
		if(write_full(cli->sock, response, strlen(response)) < 0) {
			perror("could not send message");
			break; /* erreur: on deconnecte le client */
		}
	}
	write(cli->sock, response, strlen(response));
	//Lorsque le client se déconnecte, le worker clot la socket, appelle free_client et sort
	close(cli->sock);	
	free_client(cli);
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

void advance(char *buf, int *plen, int n) {
    memmove(buf, buf + n, *plen - n);
    *plen -= n;
}

int message_end(char *buf,int len){
    char* pos= memchr(buf,'\n',len );
    if (pos==NULL)
        return -1;
    return pos-buf+1;
}

int get_question(int sock, char *buf, int *plen, char* question){
    int n=-1;
    while(*plen < BUFSIZ) {
        n = message_end(buf, *plen);
        if(n >= 0)
            break;
        int rc = read(sock, buf + *plen, BUFSIZ - *plen);
        if(rc <= 0)
            break;
        *plen += rc;
    }
    if(n >= 0){
        memcpy(question,buf,n);
        question[n-1]=0; //REMPLACE LE \n par \0
        advance(buf, plen, n);
        return n;
    }
    return -1;
}

int eval_quest(char *question, char *resp){
	char *cmd, *args;
	cmd = strsep(&question, " ");
	args = question;
	if(args == NULL){
		cmd[strlen(cmd)-1] = ' ';
		cmd = strsep(&cmd, " ");
	}
	if(strcmp(cmd, "echo") == 0){
		return do_echo(args, resp);
	}
	else if(strcmp(cmd, "rand") == 0)
		return do_rand(args, resp);
	else if(strcmp(cmd, "time") == 0)
		return do_time (args, resp);
	else if(strcmp(cmd, "quit") == 0)
		return do_quit(args, resp);
	else {
		snprintf(resp, BUFSIZ, "fail unknown command '%s'\n", cmd);
		return 0;
	}
}

int write_full(int fd, char *buf, int len){
    int k = 0; //nb d’octets envoyés déjà
    while(k < len) {
        int rc = write(fd,buf+k,len-k);
        if(rc <= 0)
            return -1;
        k += rc;
    }
    return len;
}

int do_rand (char* args, char* resp){
	srand (time (NULL));
	if(args == NULL){
		snprintf(resp, BUFSIZ, "ok %d\n", rand ());
		return 1;
	}
	else{
		int n = atoi(args);
		if(n==0){
			snprintf(resp, BUFSIZ, "Invalide input %s\n", args);
			return 1;
		}
		else{
			snprintf(resp, BUFSIZ, "ok %d\n", rand ()%n);
			return 1;
		}
	}
}
int do_echo (char* args, char* resp){
	if(args == NULL){
		snprintf(resp, BUFSIZ, "ok %s\n", "");
		return -1;
	}
	else{
		snprintf(resp, BUFSIZ, "ok %s\n", args);
		return 1;
	}	
}
int do_quit (char* args, char* resp){
	if(args == NULL){
		snprintf(resp, BUFSIZ, "[server disconnected]\n");
		return -1;
	}
	else{
		snprintf(resp, BUFSIZ, "Invalide input %s\n", args);
		return 1;
	}	
}

int do_time (char* args, char* resp){
	if(args == NULL){
		int h, min, s, day, mois, an;
		time_t now; 
		// Renvoie l'heure actuelle
		time(&now);
		// Convertir au format heure locale
		struct tm *local = localtime(&now);
		h = local->tm_hour;        
		min = local->tm_min;       
		s = local->tm_sec;       
		day = local->tm_mday;          
		mois = local->tm_mon + 1;     
		an = local->tm_year + 1900;
		snprintf(resp, BUFSIZ, "time of the day: %02d:%02d:%02d  %02d/%02d/%d\n",h, min, s, day, mois, an);  
		return 1;
	}
	else{
		snprintf(resp, BUFSIZ, "Invalide input %s\n", args);
		return 1;
	}
}



