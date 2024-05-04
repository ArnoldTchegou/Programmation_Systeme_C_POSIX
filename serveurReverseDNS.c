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
#include <ctype.h>
#include <stdbool.h>
#include <netdb.h>
#define BUFSIZ 8192
#define NI_MAXHOST 1025
//variable globale pour compter le nombre de clients.
int nr_clients; //objet critique
//variable globale un verrou mutex Pour éviter les data races protégez la manipulation du compteur
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
typedef struct s_msg {
	char sender[9];
	char* text;
	struct s_msg * next;
} msg;

typedef struct s_mbox{
	msg *first, *last;
} mbox;

//pour stocker les paramètres de chaque client
typedef struct client_data_s {
	int sock;
	pthread_t thread;
	struct client_data_s *prev,*next;  //pointeurs qui pointent sur les données du client précédent et suivant
	char nick[9];//pseudo jusqu’a 8 lettres
	mbox* box; //boite aux lettres
}client_data;

//les variables globales pour les deux extrémités de la liste
client_data *first,*last;

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
int eval_quest(char *question, char *resp, client_data *cli);
int do_list(char* args, char* resp);
msg* create_msg(char* author, char* contents);//constructeur msg
void init_mbox(mbox* box);//initialiser une boite vide init_mbox(mbox* box)
void send_mess(mbox* box, char* author, char* contents);//procédure d’envoi pour créer et enfiler un message
void put(mbox* box, msg* mess);//put(mbox* box, msg* mess) pour ajouter un message à la fin de la file(enfiler)
void destroy_msg(msg* mess);//destructeur destroy_msg(msg* mess)
msg* get(mbox* box);//procédure msg* get(mbox* box) pour récupérer un message à la tête de la file (et l’enlever de la file)
void show_msg(msg* mess, char* resp);//procédure show_msg(msg* mess)
int recieve_mess(mbox* box , char* resp);//procédure de réception pour récupérer et afficher un message
int valid_nick(char* resp, char* nick); //qui vérifie que le pseudo nick satis-fait les contraintes
client_data* search_client(char* nick);//renvoie le client_data avec un pseudo donné (ou NULL s’il n’y en a pas).
int do_nick(char* args, char* resp, client_data *cli);
int do_send(char* args, char* resp, client_data *receiver, client_data *sender);
int do_rcv(char* args, char* resp, client_data *cli);

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
	//mettre à NULL les variable 
	first = NULL;
	last = NULL;
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
	//mettre la chaine vide dans le pseudo du nouveau client, et initialisera sa boite aux lettres.
	strncpy(data->nick,"",8);
	data->nick[8]='\0';
	data->box = malloc(sizeof(mbox));
	init_mbox(data->box);
	
	pthread_mutex_lock(&lock);
	//accrocher la structure nouvellement créée à la fin de la liste
    	if(nr_clients==0){ //premier element, on fabrique la liste
		data->prev = data->next = NULL;
		first=last=data;
    	}
    	else{ //on l'accroche à la fin de liste
		last->next=data;
		data->prev=last;
		data->next=NULL;
		last=data;
    	}
	//incrémenter le compteur nr_clients
	nr_clients++;
	pthread_mutex_unlock(&lock);
	//renvoyer le pointeur de client_data alloué
	return data;
}

//destructeur free_client
void free_client(client_data* cli){
	//sortir la structure de la liste, recoudre le trou, et libérer la mémoire
	if (cli!=NULL){
		pthread_mutex_lock(&lock);
		if (nr_clients==1){//on sort l'unique element
		    first=last=NULL;
		}
		else if (cli==first){// premier element
		    first = first->next;
		    first->prev = NULL;
		}
		else if (cli==last){// dernier element
		    last = last->prev;
		    last->next = NULL;
		}
		else{// element au milieu
		    cli->prev->next = cli->next;
		    cli->next->prev = cli->prev;
		}
		//décrémenter le compteur nr_clients
		nr_clients--;
		pthread_mutex_unlock(&lock);
		free(cli);
        }
        if(nr_clients==0){
		printf("j'attends mes clients\n");
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
		if(eval_quest(question, response, cli) < 0)
			break; /* erreur: on deconnecte le client */
		if(write_full(cli->sock, response, strlen(response)) < 0) {
			perror("could not send message");
			break; /* erreur: on deconnecte le client */
		}
	}
	write(cli->sock, response, strlen(response));
	//Lorsque le client se déconnecte, le worker clot la socket, appelle free_client et sort
	close(cli->sock);	
	printf("[client %d déconnecté]\n",cli->sock);
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
	printf("[client %d connecté]\n",cli->sock);
	//afficher la liste de clients actifs
	//do_show_list();
}

void listen_port(int port_num){
	struct sockaddr_in6 addr;
	socklen_t addr_len = sizeof(addr);
	char buffer[NI_MAXHOST];
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
        	int client_sock = accept(s, (struct sockaddr *)&addr, &addr_len);
        	//accept(s,NULL,NULL);
		if (client_sock<0){
		    perror("accept"); 
		    continue;
		}
		//Lorsqu’un client arrive, on appelle client_arrived
		if(getnameinfo((struct sockaddr *)&addr, addr_len, buffer, sizeof(buffer), NULL,0, 0) == 0){
			printf("connection from %s: ", buffer);
		}
		client_arrived(client_sock);
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
        question[n-1]=0;
        if(n>1&&question[n-2]=='\r'){
    		question[n-2]='\0';
    	} 
        advance(buf, plen, n);
        return n;
    }
    return -1;
}

int eval_quest(char *question, char *resp, client_data *cli){
	char *cmd, *args, *nick;
	//printf("%s %ld\n", question, strlen(question));
	cmd = strsep(&question, " ");
	args = question;
	//if(args == NULL){
		//cmd[strlen(cmd)-1] = ' ';
		//cmd = strsep(&cmd, " ");
	//}
	if(strcmp(cmd, "echo") == 0){
		return do_echo(args, resp);
	}
	else if(strcmp(cmd, "rand") == 0)
		return do_rand(args, resp);
	else if(strcmp(cmd, "time") == 0)
		return do_time (args, resp);
	else if(strcmp(cmd, "list") == 0)
		return do_list (args, resp);
	else if(strcmp(cmd, "nick") == 0){
		return do_nick(args, resp, cli);
	}
	else if((strcmp(cmd, "send") == 0)){
		if(args==NULL){
			snprintf(resp, BUFSIZ, "Error send: Invalid input\n");
			return 0;
		}
		if(strcmp(cli->nick, "")==0){
			snprintf(resp, BUFSIZ, "Error send: your user_nick is empty\n");
			return 0;
		}
		nick = strsep(&question, " ");
		char* args1 = question;
		client_data *reciver = search_client(nick);
		client_data *sender = cli;
		if(reciver==NULL){
			snprintf(resp, BUFSIZ, "Error send: could not find user '%s'\n", nick);
			return 0;
		}
		return do_send(args1, resp,sender,reciver);
	}
	else if(strcmp(cmd, "rcv") == 0)
		return do_rcv(args, resp, cli);
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
			snprintf(resp, BUFSIZ, "Invalid input '%s'\n", args);
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
		return 1;
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
		snprintf(resp, BUFSIZ, "Invalid input '%s'\n", args);
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
		snprintf(resp, BUFSIZ, "Invalid input '%s'\n", args);
		return 1;
	}
}
int do_list(char* args, char* resp){
	//la fonction do_list affiche la liste de pseudos
	if(args == NULL){
		client_data *current=first;
		snprintf(resp, BUFSIZ, "ok ");
		int n = strlen("ok ");
		while(current !=NULL){
			snprintf(resp+n, BUFSIZ, "%s ", current->nick);
			n = strlen(resp);
			current = current->next;
		}
		snprintf(resp+n, BUFSIZ, "\n");
		return 1;
	}
	else{
		snprintf(resp, BUFSIZ, "Invalide input '%s'\n", args);
		return 1;
	}
}

//constructeur msg* create_msg(char* author, char* contents)
msg* create_msg(char* author, char* contents){
	msg* envoi = malloc(sizeof(msg));
	strncpy(envoi->sender,author,8);
	envoi->sender[8]='\0';
	int l = strlen(contents);
	if(contents[l-1]=='\n'){
		contents[l-1]= '\0';
	}
	envoi->text = malloc((l+1)*sizeof(char));
	snprintf(envoi->text,l+2,"%s\n",contents);
	return envoi;	
}
//initialiser une boite vide init_mbox(mbox* box)
void init_mbox(mbox* box){
	box->first = NULL;
	box->last = NULL;
}

//put(mbox* box, msg* mess) pour ajouter un message à la fin de la file(enfiler)
void put(mbox* box, msg* mess){
	// si la boite est vide 
	if(box->first == NULL){
		box->first = mess;
		mess->next = box->last;
		box->last = mess; 
	}
	// si la boite n'est pas vide
	else{
		box->last->next = mess;
		box->last = mess;
	} 
}

//destructeur destroy_msg(msg* mess)
void destroy_msg(msg* mess){
	free(mess->text);
	free(mess);
}

//netoyer la boite
void destroy_boite(mbox* box){
	if(box->first != NULL){
		msg* mess = box->first;
		while(mess!=NULL){
			msg* t = mess->next;
			destroy_msg(mess);
			mess = t;
		}
	}
}

//procédure msg* get(mbox* box) pour récupérer un message à la tête de la file (et l’enlever de la file)
msg* get(mbox* box){
	msg* msg ;
	if(box->first == NULL){
		return NULL;
	}
	else if(box->first == box->last){
		msg = box->first;
		box->first = NULL;
		box->last = NULL;
		return msg;
	}
	else {
		msg = box->first;
		box->first = msg->next;
		return msg;
	}
}

//procédure d’envoi send_mess(mbox* box, char* author, char* contents) pour créer et enfiler un message
void send_mess(mbox* box, char* author, char* contents){
	msg* mess = create_msg(author, contents);
	put(box, mess);
}

//procédure show_msg(msg* mess)
void show_msg(msg* mess, char* resp){
	snprintf(resp, BUFSIZ, "author : %s\tmessage: %s", mess->sender,mess->text);
	//on libère la mémoire allouée pour le message recu
	destroy_msg(mess);
}

//procédure de réception recieve_mess(mbox* box) pour récupérer et afficher un message
int recieve_mess(mbox* box , char* resp){
	msg* mess = get(box);
	if(mess == NULL){
		snprintf(resp, BUFSIZ,"Vous n'avez aucun message non lu!!!\n");
		return 1;
	}
	show_msg(mess, resp);
	return 0;
}
//vérifie que le pseudo nick satis-fait les contraintes
int valid_nick(char* resp, char* nick){
	//tous alpha-numériques.
	for(int i=0; i<strlen(nick); i++){
		if (isalnum( nick[i] )==false){
			snprintf(resp, BUFSIZ, "Invalid nick name\n");
			return -1;
		}
	}
	//contient entre 1 et 8 caractères
	if(strlen(nick)<1||strlen(nick)>8){
		snprintf(resp, BUFSIZ, "Invalid nick name \n");
		return -1;
	}
	//le pseudonyme est déjà utilisé
	else if(search_client(nick)!=NULL){
		snprintf(resp, BUFSIZ, "nick name unavailable \n");
		return -1;
	}
	return 1;
}
//renvoie le client_data avec un pseudo donné (ou NULL s’il n’y en a pas).
client_data* search_client(char* nick){
	//cherche un element dans la liste
	client_data *current=first;
	while(current !=NULL){
		if (strcmp(current->nick, nick)==0) //trouve
			return current;
		current = current->next;
	}
	return NULL; //rien trouve
}

int do_nick(char* args, char* resp, client_data *cli){
	if(args == NULL){
		snprintf(resp, BUFSIZ, "Invalid input arguments \n");
		return 1;
	}
	int r = valid_nick(resp, args);
	if(r<0){
		return 1;
	}
	strcpy(cli->nick, args);
	snprintf(resp, BUFSIZ, "ok \n");
	return 2;
}

int do_send(char* args, char* resp, client_data *sender, client_data *receiver){
	if(args==NULL){
		snprintf(resp, BUFSIZ, "Error send: could not send empty message\n");
		return 1;
	}
	send_mess(receiver->box, sender->nick, args);
	snprintf(resp, BUFSIZ, "ok\n");
	return 1;	
}
int do_rcv(char* args, char* resp, client_data *cli){
	if(strcmp(cli->nick, "")==0){
		snprintf(resp, BUFSIZ, "Error rcv: your user_nick is empty\n");
		return 0;
	}
	if(args ==NULL){
		recieve_mess(cli->box , resp);
		return 1;
	}
	else{
		snprintf(resp, BUFSIZ, "Invalid input '%s'\n", args);
		return 1;
	}
}

