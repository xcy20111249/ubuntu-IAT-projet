#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/time.h>
#include <errno.h>

#define RCVSIZE 20
#define ACKPORT 12
#define MSGSIZE 4096
#define SEQSIZE 12
#define ACKSIZE 20

int main(int argc,char* argv[]) {
  if (argc<2) {
    printf("missing arguments\n");
    return -1;
  }

  int port_serverudp=atoi(argv[1]);
  int port_servertcp=8001;
  int domaine=AF_INET;
  int type=SOCK_DGRAM;
  int protocole=0;

  int sockets[2];
  sockets[0]=socket(domaine,type,protocole);
  printf("socket udp est: %d\n", sockets[0]);
  sockets[1]=socket(domaine,type,protocole);
  printf("socket udp-tcp est: %d\n", sockets[1]);

  int reuse=1;
  for (int i = 0; i < 1; i++) {
    setsockopt(sockets[i],SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
    if (sockets[i]<0){
      close(sockets[i]);
      printf("le descripteur est %d\n",sockets[i]);
      perror("Cannot create socket\n");
      return -1;
    }
  }

  struct sockaddr_in my_addr1;
  memset((char*)&my_addr1,0,sizeof(my_addr1));
  my_addr1.sin_family=domaine;
  my_addr1.sin_port=htons(port_serverudp);
  my_addr1.sin_addr.s_addr=htonl(INADDR_ANY);

  struct sockaddr_in my_addr2;
  memset((char*)&my_addr2,0,sizeof(my_addr2));
  my_addr2.sin_family=domaine;
  my_addr2.sin_port=htons(port_servertcp);
  my_addr2.sin_addr.s_addr=htonl(INADDR_ANY);

  if(bind(sockets[0],(struct sockaddr*)&my_addr1,sizeof(my_addr1))<0){
    perror("Bind failed\n");
    close(sockets[0]);
    return -1;
  };
  if(bind(sockets[1],(struct sockaddr*)&my_addr2,sizeof(my_addr2))<0){
    perror("Bind failed\n");
    close(sockets[1]);
    return -1;
  };


  struct sockaddr_in client_addr;
  memset((char*)&client_addr,0,sizeof(client_addr));
  socklen_t c_len = sizeof(client_addr);

  printf("waiting for connection\n");
  for (;;) {
    char serverbuffer[RCVSIZE];
    char ackport[ACKPORT];
    ackport[ACKPORT-1]='\0';
    sprintf(ackport,"%s%d","SYN-ACK",port_servertcp);
    printf("%s\n", ackport);
    int goon=1;//control of msg transmission
    int con=1;//control of handshake done
    while (con) {
      int cont=0;
      memset(serverbuffer,0,RCVSIZE);
      recvfrom(sockets[0],serverbuffer,RCVSIZE,0,(struct sockaddr*)&client_addr,&c_len);
      printf("****************\n");
      if (strcmp(serverbuffer,"SYN")==0) {
        printf("client udp say: hello\n");
        char* ip_client_udp=inet_ntoa(client_addr.sin_addr);
        int port_client_udp=ntohs(client_addr.sin_port);
        printf("IP addresse of client udp is %s\n", ip_client_udp);
        printf("Port of client udp is %d\n", port_client_udp);

        while (1) {
          sendto(sockets[0],ackport,ACKPORT,0,(struct sockaddr*)&client_addr,c_len);
          printf("port info sent\n");
          memset(serverbuffer,0,RCVSIZE);
          recvfrom(sockets[0],serverbuffer,RCVSIZE,0,(struct sockaddr*)&client_addr,&c_len);
          if (strcmp(serverbuffer,"ACK")==0) {
            printf("handshake done\n");
            con=0;
            break;
          }
        }
      }
      cont++;
      if (cont>=5) {//5 cercles no response
        goon=0;
        printf("failed\n");
        break;
      }
    }
    //handshake done, beginning the transmission
    if (goon) {//begin communication
      FILE *fp;
      char msgbuffer[SEQSIZE+MSGSIZE];//recieved msg
      char ackbuffer[ACKSIZE];//ack msg
      char tembuffer[MSGSIZE];//data of the pic
      int len;
      fd_set readfds;
      FD_ZERO(&readfds);
      struct timeval timeout;

      printf("go on\n");
      int cont=1;
      while (cont) {
        memset(serverbuffer,0,RCVSIZE);
        recvfrom(sockets[1],serverbuffer,RCVSIZE,0,(struct sockaddr*)&client_addr,&c_len);
        printf("client wants: %s\n", serverbuffer);
        if (strcmp(serverbuffer,"close")==0) {
          cont=0;
        }
        sendto(sockets[1],serverbuffer,RCVSIZE,0,(struct sockaddr*)&client_addr,c_len);

        //client ask for a non exist file
        if ((fp=fopen("index.jpeg","rb"))==NULL) {
          printf("file not found\n");
          exit(1);
        }

        printf("transmission begin\n");

        fflush(stdout);
        int seq=0;
        char ctrmsg[4];
        char ackmsg[12];
        for (int i = 0; i < sizeof(ctrmsg); i++) {
          ctrmsg[i]='1';
        }
        //while (!feof(fp)) {
          fd_set readfds;
          FD_ZERO(&readfds);
          char sequence[8];
          memset(sequence,0,8);
          int seqint=seq;
          int r;
          char exchange[2];

          //sequence number from int to char
          for (int i = 0; i < sizeof(sequence); i++) {
            r=seqint%10;
            sprintf(exchange,"%d",r);
            sequence[sizeof(sequence)-1-i]=exchange[0];
            seqint=(seqint-r)/10;
          }

          printf("sequence is %s\n",sequence );
          memset(msgbuffer,0,SEQSIZE+MSGSIZE);
          printf("ctr and seq is %s\n",msgbuffer);
          sprintf(ackmsg,"%s%.8s","ACK",sequence);
          ackmsg[sizeof(ackmsg)-1]='\0';
          printf("%s\n", ackmsg);

          len=fread(tembuffer,1,MSGSIZE,fp);
          printf("len of tembuffer %d\n", len);
          fflush(stdout);
          printf("ctrmsg %s\n", ctrmsg);
          printf("sequence num %s\n", sequence);
          sprintf(msgbuffer,"%.4s%.8s%s",ctrmsg,sequence,"tembuffer");
          fflush(stdout);
          printf("combined\n");

          while (1) {
            FD_SET(sockets[1],&readfds);
            timeout.tv_sec=3;
            timeout.tv_usec=0;

            printf("setted\n");
            sendto(sockets[1],msgbuffer,SEQSIZE+MSGSIZE,0,(struct sockaddr*)&client_addr,c_len);
            printf("%s\n", msgbuffer);
            int resul=select(sockets[1]+1,&readfds,NULL,NULL,&timeout);

            //send msg and wait for ack
            if (FD_ISSET(sockets[1],&readfds)) {
              sleep(1);
              memset(ackbuffer,0,ACKSIZE);
              recvfrom(sockets[1],ackbuffer,ACKSIZE,0,(struct sockaddr*)&client_addr,&c_len);
              printf("%s\n", ackbuffer);
              if(strcmp(ackbuffer,ackmsg)==0){
                printf("msg %s rcved\n", ackmsg);
                break;
              }
            }
            if(resul==0){
              printf("timeout no response\n");
              sleep(1);
              continue;
            }
          }
          seq+=MSGSIZE;


        //}
        printf("transmission done\n");
        fclose(fp);
      }
    }
    exit(0);
  }
  for (int i = 0; i < 2; i++) {
    close(sockets[i]);
  }
  return 0;
}
