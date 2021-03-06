#include "tcp.h"

int reach_tcp(char *ip, char *port)
{
  struct addrinfo hints, *res;
  struct sockaddr_in addr;
  int n, errcode, fd;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET; //IPv4
  hints.ai_socktype = SOCK_STREAM; //TCP socket
  hints.ai_flags = AI_NUMERICSERV;

  //addr info
  if((errcode = getaddrinfo(ip, port, &hints, &res))!=0)
  {
    fprintf(stderr, "error: getaddrinfo: %s\n", gai_strerror(errcode));
    exit(1);
  }
  //socket
  if((fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol))==-1)
  {
    printf("error: socket\n");
    exit(1);
  }
  //connect
  if((n=connect(fd,res->ai_addr,res->ai_addrlen))==-1)
  {
    printf("error: connect tcp\n");
    return 0;
  }

  return fd;
}

int serv_tcp(char *port)
{
  struct addrinfo hints,*res;
  int fd, addrlen, n, errcode;
  struct sockaddr_in addr;

  memset(&hints,0,sizeof hints);
  hints.ai_family=AF_INET;//IPv4
  hints.ai_socktype=SOCK_STREAM;//TCP socket
  hints.ai_flags=AI_PASSIVE|AI_NUMERICSERV;

  if((errcode = getaddrinfo(NULL, port, &hints, &res))!=0)
  {
    fprintf(stderr, "error: getaddrinfo: %s\n", gai_strerror(errcode));
    exit(1);
  }
  if((fd=socket(res->ai_family,res->ai_socktype,res->ai_protocol))==-1)
  {
    printf("error: socket\n");
    exit(1);
  }
  if(bind(fd,res->ai_addr,res->ai_addrlen)==-1)
  {
    printf("error: bind tcp\n");
    exit(1);
  }
  if(listen(fd,5)==-1)
  {
    printf("error: listen\n");
    exit(1);
  }
  return fd;
}

int send_tcp(char *msg, int fd)
{
  int nw = 0, n;
  char *ptr;

  n = strlen(msg);
  ptr = &msg[0];
  //printf("Size of message to send: %d",n);
  while(n>0)
  {
    if((nw=write(fd,ptr,n))<=0){printf("error: write\n"); return 0;}
    n-=nw; ptr+=nw;
  }
  return 1;
}

int new_connection(User *user)
{
  int n,newfd;
  char buffer[128] = {'\0'};
  struct sockaddr_in addr;
  unsigned int addrlen;

  if((newfd=accept(user->fd_tcp_serv,(struct sockaddr*)&addr,&addrlen))==-1)
  {
    printf("error: accept\n");
    return 0;
  }
  for(n=0; n < user->tcpsessions; n++) //Guarda descritor para comunicar com jusante caso haja espaço
  {
    if(user->fd_clients[n] == 0)
    {
      user->fd_clients[n] = newfd;
      //Send WELCOME
      msg_in_protocol(buffer,"WELCOME",user);
      if(send_tcp(buffer,newfd) == 0){printf("Client left?\n");close(newfd);}
      break;
    }
  }
  if(n == user->tcpsessions) //REDIRECT caso não haja espaço para mais ligações a jusante
  {
    //Send Redirect
    msg_in_protocol(buffer,"REDIRECT",user);
    if(send_tcp(buffer,newfd) == 0){printf("Client left? (Before redirect)\n");}
    close(newfd);
  }
  return 1;
}

void dissipate(char *msg, User *user)
{
  for(int i = 0; i < user->tcpsessions; i++)
  {
    if(user->fd_clients[i] != 0)
    {
      if(send_tcp(msg,user->fd_clients[i]) == 0)
      {
        printf("Client left?\n");
        close(user->fd_clients[i]);
        user->fd_clients[i] = 0;
        memset(user->myClients[i],'\0',128);
      }
    }
  }
  return;
}
