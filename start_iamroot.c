#include <stdio.h>
#include <signal.h>
#include "functions.h"
#include "udp.h"
#include "tcp.h"

#include <time.h>

int main(int argc, char **argv)
{
  int maxfd, counter, newfd;
  char buffer[128] = {'\0'};
  fd_set rfds;
  int n,i,tries;
  char *ptr;
  struct timeval tv = {30, 0};

  //Timer para refrescar raiz no servidor de raizes e/ou refrescar lista de POPs
  time_t timer_start = time(NULL);

  //Ignore SIGPIPE signal
  struct sigaction act;
  memset(&act,0,sizeof act);
  act.sa_handler=SIG_IGN;
  if(sigaction(SIGPIPE,&act,NULL)==-1){printf("Unable to handle SIGPIPE\n");exit(1);}

  //Estrutura com a informação sobre o programa e dados passados pelo utilizador
  User *user=(User *)malloc(sizeof(User));

  //Inicializar estrutura com os valores DEFAULT
  USER_init(user);

  //Lê os dados passados pelo utilizador e escreve-os na respetiva estrutura
  if(read_args(argc, argv, user)==0)
  {
    //Apresenta lista de streams disponiveis caso nenhum tenha sido especificado
    strcpy(buffer,"DUMP\n");
    reach_udp(user->rsaddr,user->rsport,buffer);
    return 0;
  }

  //Programa começa fora da àrvore
  user->state = out;

  while(1){

    //Reinicia o buffer
    memset(buffer,'\0',sizeof(buffer));

    //Reinicia o timeout
    tv.tv_sec = 30;
    tv.tv_usec = 0;

    //Reinicia os vetor de file descriptors
    FD_ZERO(&rfds);
    FD_SET(STDIN_FILENO,&rfds); maxfd = 0; //Sets stdin file descriptor

    //Procedimento para aceder ao stream
    if(user->state == out)
    {
      if(join_tree(user) == 0)
      {
        printf("Unable to reach stream service tree\n");
        clean_exit(user);
        exit(1);
      }
      //Melhorar
      if(user->state == out)
      {
        tries++;
        printf("%d\n", tries);
        if(tries > 5){printf("Tried 5 times, not able to join stream\n"); clean_exit(user); exit(1);}
        continue; //Não conseguiu ligar-se ao IP fornecido, try again
      }
    }
    //Arma descritor para ligação a montante
    if(user->state != out)
    {
      FD_SET(user->fd_tcp_mont,&rfds);maxfd=max(maxfd,user->fd_tcp_mont);
    }
    //Arma descritor para receber mensagens por UDP, caso seja servidor de acesso
    if(user->state == access_server)
    {
      FD_SET(user->fd_udp_serv,&rfds);maxfd=max(maxfd,user->fd_udp_serv);  //Servidor de Acesso
    }
    //Arma descritores para ligações a jusante
    if((user->state == in)||(user->state == access_server))
    {
      FD_SET(user->fd_tcp_serv,&rfds);maxfd=max(maxfd,user->fd_tcp_serv);
      for(i=0;i<user->tcpsessions;i++)
      {
        FD_SET(user->fd_clients[i],&rfds);maxfd=max(maxfd,user->fd_clients[i]);
      }
    }

    //Select from set file descriptors
    counter=select(maxfd+1,&rfds,(fd_set*)NULL,(fd_set*)NULL, &tv);
    if(counter<0){printf("Counter < 0\n");exit(1);}

    if(FD_ISSET(user->fd_udp_serv,&rfds) && user->state == access_server) //Servidor de Acesso
    {
      recieveNsend_udp(user->fd_udp_serv, buffer, user);
    }
    if(FD_ISSET(user->fd_tcp_serv,&rfds) && user->state != waiting) //Clientes/Jusante
    {
      if(new_connection(user) == 0)
      {
        clean_exit(user);
        exit(1);
      }
    }
    if(FD_ISSET(user->fd_tcp_mont,&rfds) && user->state != out)     //Fonte/Acima
    {
      if((n=read(user->fd_tcp_mont,buffer,128))!=0)
      {
        //Montante falou
        printf("%s\n", buffer);
        if(n==-1){printf("error: read\n"); clean_exit(user); exit(1);}
        if(handle_PEERmessage(buffer,user) == 0){printf("Unable to process PEER message\n"); clean_exit(user); exit(1);}
      }else{
        //Montante saiu
        close(user->fd_tcp_mont);
        user->state = out;
        dissipate("BS\n",user);
      }
    }
    for(i=0;i<user->tcpsessions;i++)
    {
      if(FD_ISSET(user->fd_clients[i],&rfds) && user->fd_clients[i] != 0)
      {
        if((n=read(user->fd_clients[i],buffer,128))!=0)
        {
          printf("%s\n", buffer);
          if(n==-1){printf("error: read\n"); clean_exit(user); exit(1);}
          if(handle_PEERmessage(buffer,user) == 0){printf("Unable to process PEER message\n"); clean_exit(user); exit(1);}
        }else{
          printf("Client left?\n");
          close(user->fd_clients[i]);
          user->fd_clients[i] = 0;
          memset(user->myClients[i],'\0',128);
        }
      }
    }
    if(FD_ISSET(STDIN_FILENO,&rfds))   //STDIN
    {
      fgets(buffer, sizeof(buffer), stdin);

      if(handle_STDINmessage(buffer,user)==0)
      {
		    exit(1);
		  }
      n = strlen(buffer);
    }

    printf("%lu\n", time(NULL)-timer_start);
    if(time(NULL)-timer_start > 10)
    {
      printf("%lu\n", time(NULL)-timer_start);
      timer_start = time(NULL);
    }

    if(counter == 0) //select timed out
    {
      if(user->state == access_server)
      {
        //Refresca servidor de raizes
        msg_in_protocol(buffer,"WHOISROOT",user);
        reach_udp(user->rsaddr,user->rsport,buffer);

        //Faz POPQUERY para refrescar POPs
        strcpy(buffer,"POPQUERY\n");
        handle_PEERmessage(buffer,user);
      }
    }
  }//while(1)

}

//Pedro's PC
//./iamroot grupo44:193.136.138.142:58001 -i 192.168.1.67 -u 58001 -t 58001
//@tecnico
//./iamroot grupo44:193.136.138.142:58001 -i 194.210.159.193 -u 58001 -t 58001
//My Fonte
//./iamroot grupo44:192.168.1.67:57000 -i 192.168.1.67 -u 58001 -t 58001

//cd /Users/pedroflores/Documents/IST/5Ano2Sem/RCI/ProjectRepository
