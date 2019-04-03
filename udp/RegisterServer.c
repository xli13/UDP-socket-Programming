#include <stdio.h>  
#include <stdlib.h>
#include <string.h>  
#include <unistd.h>  
#include <sys/types.h>  
#include <sys/socket.h>  
#include <stdlib.h>  
#include <netinet/in.h>  
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/time.h>

#define LINELEN   1024
#define MAXPACKLEN (LINELEN+sizeof(struct packh)+4)
#define MASK 0xff
#define REG	5
#define RACK   (REG^MASK)
#define QURY  1
#define QACK   (QURY^MASK)
#define TRYTIME 10

//the header
#pragma pack(1)

struct packh{
short len;
unsigned char cmd;
char fid;  // 0-9
};

#pragma pack()


char ServerIP[16];
struct timeval start;  

int Mysend(int skfd,unsigned char *data,int len,struct sockaddr_in *peer)
{
   int addrlen=sizeof(struct sockaddr_in);
   int sendlen=0;
   int all_len,ret;
  struct packh *A=data;
  all_len=len;
    struct timeval tv;
    fd_set writefds;
    tv.tv_sec = 0;
    tv.tv_usec = 500000;
  
    FD_ZERO(&writefds);
    FD_SET(skfd, &writefds);
    ret=select(skfd+1, NULL, &writefds, NULL, &tv);  
    if(ret<=0)
    	{
    		printf("timeout or error in select.\n");
		return -1;
    	}
     ret=sendto(skfd,data,all_len,0,peer,addrlen);    
     if(ret<0)
     {
	  	printf("error in sendto(),errno:%d\n",errno);
		return -2;
      }
     if(ret!=all_len)
     	{
     		printf("sending %d less than %d\n",ret,all_len);
		return -3;
     	}
					
	return 0;
}


int MyReceive(int skfd, unsigned char *data,struct sockaddr_in *client)
{
    struct timeval tv;

   fd_set readfds;
   int num,addrlen=sizeof(struct sockaddr_in);
   int all_len,ret;
   int k=0;
   struct packh *A=data;
   short len1=0;
   int revlen=0;
   
    tv.tv_sec = 0;
    tv.tv_usec = 500000;
	
    while(k<TRYTIME)
    {
	    FD_ZERO(&readfds);
	    FD_SET(skfd, &readfds);
	    ret=select(skfd+1, &readfds, NULL, NULL, &tv);  
	    if(ret<0)
	    	{
	    		printf("error in select.\n");
			return -1;
	    	}
	    else if(ret==0)
		{
			k++;
			continue;
		}

		  num =recvfrom(skfd,&len1,2,MSG_PEEK,(struct sockaddr*)client,&addrlen);   	   	
		  if(num!=2)
		  {
		  	printf("error in recvfrom to get msghead.\n");
			return -1;
		  }

		  len1=ntohs(len1);

		   num =recvfrom(skfd,data,len1,0,(struct sockaddr*)client,&addrlen);
		   if(num<0)
		   {
		   	 printf("error in recvfrom data.\n");
			 return -1;
		   }
		   if(num!=len1)
		   {
 	  		printf("recv %d less than %d\n",num,len1);
		       return -3;		   
		   }
		   A->len=len1;   //to host order, so we can use it directly
		   break;     	
    }
	if(k==TRYTIME)
		return -4;
	return 0;
}




int main(int argc , char **argv)
{
	int ret,sockfd,flags,len;
 	struct sockaddr_in server;
	 struct sockaddr_in client;
	 struct timeval now;  
	unsigned char data[MAXPACKLEN];
	struct packh *A=data;
	if(argc!=2)
	{
		printf("need IP address in command line...\n");
		return -1;
	}
	
	 if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)   
	 {  
	  printf("Creatingsocket failed.");  
	  return -1;
	 }  
	flags = fcntl(sockfd, F_GETFL, 0);

	 if (-1 == fcntl(sockfd, F_SETFL, flags|O_NONBLOCK))
	    {
	        printf("fcntl socket error!\n");
	        return -2;
	    } 
	 bzero(&server,sizeof(server));  
	 server.sin_family=AF_INET;  
	 server.sin_port=htons(8888);
		
	 server.sin_addr.s_addr= inet_addr(argv[1]);
	 if(bind(sockfd, (struct sockaddr *)&server, sizeof(server)) == -1)  
	 {  
	  printf("Bind()error.");  
	  return -3;
	 }     		


	 while(1)
	 {
		ret=MyReceive(sockfd, data,&client);
		if(ret)
			continue;
		if(A->cmd==REG)
		{
			memset(ServerIP,0,sizeof(ServerIP));
			memcpy(ServerIP,data+sizeof(struct packh),A->len-sizeof(struct packh));
			gettimeofday(&start,0);
			A->cmd=RACK;
			A->len=htons(sizeof(struct packh));
			ret=Mysend(sockfd,data,sizeof(struct packh),&client);
			if(ret)
			{
				printf("error in sending back REGACK\n");
				break;
			}
			printf("get new SeverIP:%s\n",ServerIP);
		}
		else if(A->cmd=QURY)
		{
			A->cmd=QACK;
			gettimeofday(&now,0);			
				
			if((ServerIP[0]==0)||(now.tv_sec-start.tv_sec>3600))  // 60 minutes, we neglect the ns part here			
				len=sizeof(struct packh);			
			else
			{
				strcpy(data+sizeof(struct packh),ServerIP);
				len=strlen(data+sizeof(struct packh))+sizeof(struct packh);
			}
			A->len=htons(len);
			ret=Mysend(sockfd,data,len,&client);
			if(ret)
			{
				printf("error in sending back QACK\n");
				break;
			}			
		}
		else
		{
			printf("recv unkown command : %u\n",A->cmd);
		}
	 }
	 return 0;
}

