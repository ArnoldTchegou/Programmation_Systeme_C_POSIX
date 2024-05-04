//MARYAM AIT DAOUD et ARNOLD NGNOUBA NDIE TCHEGOU

#include <stdio.h>  /* printf */
#include <stdlib.h> /* EXIT_ */
#include <string.h> /* memset */
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#define BUFSIZ 8192

int connect_server(const char *host, const char *port);
void advance(char *buf, int *plen, int n);
int message_end(char *buf,int len);
int write_full(int fd, char *buf, int len);
int get_question(int sock, char *buf, int *plen, char* question);
void handle_client(int s);
void speak_to_server(int sock);

int main(int argc, char* argv[])
{
    if (argc!=3){
        printf("Syntaxe: %s adresse port\n", argv[0] );
        exit(1);
    }
    
    int s=connect_server(argv[1],argv[2]);
    if(s==-1){
        printf("echec de connexion\n");
        return 1;
    }
    printf("connexion ok\n");
    speak_to_server(s);
    return 0;
}
//se connecte au serveur et renvoie la socket permettant de connecter avec celui-ci
int connect_server(const char *host, const char *port){
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;
    hints.ai_flags = AI_V4MAPPED | AI_ALL ;
    
    struct addrinfo *res;
    int rc = getaddrinfo(host, port, &hints, &res);
    if(rc < 0) {
        printf("invalid address/port: %s\n", gai_strerror(rc));
        exit(1); }
    for (struct addrinfo *ad=res;ad != NULL;ad =ad->ai_next) {
        int srv_sock = socket(ad->ai_family, ad->ai_socktype, ad->ai_protocol);
        if(srv_sock < 0)
            continue;
        if(connect(srv_sock, ad->ai_addr, ad->ai_addrlen) < 0) {
            perror("connection failed");
            close(srv_sock);
            continue;
        }
        freeaddrinfo(res);
        return srv_sock;
    }
    return -1;
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

void handle_question(char *que){
    printf("%lu octets recieved %s\n",strlen(que),que);
}

int get_response(int sock, char *buf, int *plen, char* question){
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

int write_all(int fd, char *buf, int len){
    int k = 0; //nb d’octets envoyés déjà
    while(k < len) {
        int rc = write(fd,buf+k,len-k);
        if(rc <= 0)
            return -1;
        k += rc;
    }
    return len;
}
void speak_to_server(int sock) {
    char buffer[BUFSIZ];
    char tampon[BUFSIZ];
    char response[BUFSIZ];
    int len = 0;
    while (1) {
        printf(">>>");
        fgets(buffer, BUFSIZ, stdin); // lit l'entrée utilisateur
        write_all(sock, buffer, strlen(buffer)); // envoie le message au serveur
        // attend la réponse du serveur
        if(get_response(sock, tampon,&len, response)<0){
        	exit(1);
        } 
        printf("%s\n", response); // affiche la réponse du serveur
        if(strcmp(response, "[server disconnected]")==0){
        	exit(1);
        }
        strcpy(response, ""); 
    }
    close(sock); // ferme la connexion avec le serveur
}






