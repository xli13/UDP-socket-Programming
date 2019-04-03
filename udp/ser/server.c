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

#define NAMELEN  256
#define LINELEN   1024
#define MAXPACKLEN (LINELEN+sizeof(struct packh)+4)
#define FILENUM	10
#define RANDFN  3
#define TRYTIME 10

#define MASK	  0xff
#define QURY  1
#define QACK   (QURY^MASK)
#define FILENAME 2
#define FACK   (FILENAME^MASK)
#define DATA  4
#define DACK   (DATA^MASK)

#define REG	5
#define RACK   (REG^MASK)


//the header
#pragma pack(1)

struct packh{
short len;
unsigned char cmd;
char fid;  // 0-9
};

#pragma pack()

struct file_des{
char filename[NAMELEN];
int finished;
int nextlinenum;
FILE * fd;
char data[RANDFN-1][MAXPACKLEN];
};

struct file_des FOut[FILENUM];

char RegisterServerIP[16];
char ServerIP[16];

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


//send only one packet and get ack
int Send_stable(int skfd, unsigned char *data, int len,struct sockaddr_in *peer)
{
	int ret,i;
	struct sockaddr_in client;
	struct packh *A=data;
	unsigned char da[MAXPACKLEN];
	unsigned char cmd=A->cmd;
	for(i=0;i<TRYTIME;i++)
	{

		ret=Mysend(skfd,data,len,peer);
		if(ret)
			continue;

		ret=MyReceive(skfd,da,&client);
		if(ret)
			continue;
		A=da;
		if(A->cmd^cmd==MASK)  //cmd and its ACK		
				return 0;		
	}
	return -1;
		
}


//recv one packet and send back ack
int Recv_stable(int skfd, unsigned char *data,struct sockaddr_in *peer)
{

	int ret, i;
	unsigned char ack[16]; 
	struct packh *phack=ack;
	struct packh *ph=data;

	for(i=0;i<TRYTIME;i++)
	{
		ret=MyReceive(skfd,data,peer);
		if(ret)
			continue;
		break;		
	}

	if(i==TRYTIME)
	{
		
		return -1;
	}
	if(ph->cmd==DATA)
	{
		printf("get line %d for file %d: %s\n",ntohl(*(unsigned int *)(data+sizeof(struct packh))), ph->fid,data+sizeof(struct packh)+4); //comment this when done
	}
	//sendback ack
	memcpy(ack,data,sizeof(struct packh)+4);
	phack->cmd=ph->cmd^MASK;
	phack->len=htons(sizeof(struct packh)+4);
	
	ret=Mysend(skfd, ack, sizeof(struct packh)+4,peer);
	if(ret)
	{
		printf("error in sending back ack...\n");
		return -2;			
	}
	return 0;
	
}


int FillFout(unsigned char *data)
{
	struct packh *ph=data;
	int i=ph->fid;
	if(ph->cmd!=FILENAME)
		return -1;
	memcpy(FOut[i].filename,data+sizeof(struct packh),ph->len-sizeof(struct packh));
	FOut[i].fd=fopen(FOut[i].filename,"wb");
	if(FOut[i].fd==NULL)
	{
		printf("error can't open file %s for writing\n",FOut[i].filename);
		return -1;
	}
	return 0;
}

int getfilenames(int skfd,struct sockaddr_in *client)
{
	int i,j,ret=0;
	unsigned char data[MAXPACKLEN];
	memset(data,0,sizeof(data));
	for(j=0;j<i;j++)
	{
	    if(FOut[j].fd)
		fclose(FOut[j].fd);
	}
	memset(FOut,0,sizeof(FOut));
	for(i=0;i<FILENUM;i++)
	{
		ret=Recv_stable(skfd, data, client);
		if(ret)
			break;
		ret=FillFout(data);
		if(ret)
		   break;
	}

	if(ret)
	{			
		for(j=0;j<i;j++)
			fclose(FOut[j].fd);
		return -1;
	}
	return 0;
}


void matchline(unsigned char *data)
{
	struct packh *ph=data;
	int j,i=ph->fid;
	int len;
	unsigned int x1,x0=*(unsigned int *)(data+sizeof(struct packh));
	x0=ntohl(x0);
	len=ph->len-sizeof(struct packh)-4;
	printf("matchline--file %d, line %d,len %d \n",i,x0,len); //comment this when done
	if(x0==FOut[i].nextlinenum)
	{
		if(len==0)
		{
			printf("file %d is over.\n",i);
			fclose(FOut[i].fd);
			FOut[i].fd=0;
			FOut[i].finished=1;
			return;
		}
		else
		{
			fwrite(data+sizeof(struct packh)+4,1,len,FOut[i].fd);
			FOut[i].nextlinenum++;
			for(j=0;j<RANDFN-1;)
			{
				ph=FOut[i].data[j];
				if(ph->len==0)
				{
					j++;
					continue;
				}
				x1=*(unsigned int *)(FOut[i].data[j]+sizeof(struct packh));
				x1=ntohl(x1);
				if(x1==FOut[i].nextlinenum)
				{
					len=ph->len-sizeof(struct packh)-4;
					if(len==0)
					{
						printf("file %d is over.\n",i);
						fclose(FOut[i].fd);
						FOut[i].fd=0;
						FOut[i].finished=1;		
						return;
					}
					else
					{
						fwrite(FOut[i].data[j]+sizeof(struct packh)+4,1,len,FOut[i].fd);
						FOut[i].nextlinenum++;		
						ph->len=0;
						j=0;
					}
				}
				else
					j++;
			}
		}
	}
	else if(x0>FOut[i].nextlinenum)
	{
		for(j=0;j<RANDFN-1;j++)
		{
			ph=FOut[i].data[j];
			if(ph->len==0)
				break;
		}
		if(j==RANDFN-1)  //impossible
		{
			printf("error can't find a free slot....\n");
			exit(-1);
		}
		memcpy(FOut[i].data[j],data,MAXPACKLEN);
	}
}

int Recv_Line(int sockfd,struct sockaddr_in *client)
{
	unsigned char data[MAXPACKLEN];
	int ret;
	memset(data,0,sizeof(data));
	ret=Recv_stable(sockfd, data,client);
	if(ret)
		return ret;
	matchline(data);
	return 0;
}

int Recv_Files(int sockfd,struct sockaddr_in *client)
{
	int ret,i;
	while(1)
	{
		ret=Recv_Line(sockfd,client);
		if(ret)
			return -1;
		for(i=0;i<FILENUM;i++)
		{
			if(FOut[i].finished==0)
				break;
		}
		if(i==FILENUM)
			break;
	}
	return 0;
}

void Send_Concate(int sockfd,struct sockaddr_in *client)
{
	int ret=0,i,j,len,linen=0;
	unsigned char data[MAXPACKLEN];
	FILE *fp;
	struct packh *ph=data;
	ph->cmd=DATA;
	for(i=0;i<FILENUM;i++)
	{
		fp=fopen(FOut[i].filename,"r");
		if(fp==NULL)
		{
			printf("Send_Concate---error in opening file %s\n",FOut[i].filename);
			return;
		}
		while(fgets(data+sizeof(struct packh)+4,LINELEN,fp))
		{
			j=strlen(data+sizeof(struct packh)+4);
			*(unsigned int *)(data+sizeof(struct packh))=htonl(linen);
			linen++;
			len=sizeof(struct packh)+4+j;
			ph->len=htons(len);		
			ret= Send_stable(sockfd, data, len,client);
			if(ret)
			{
			   printf("Send_Concate--- error in Send_stable\n");
			   break;
			 }
			printf("ok sending line %d: %s\n",linen-1,data+sizeof(struct packh)+4); //comment this when done
		}
		fclose(fp);
		if(ret)
			break;
	}

	if(ret)
		return;

	*(unsigned int *)(data+sizeof(struct packh))=htonl(linen);
	ph->len=htons(sizeof(struct packh)+4);    //END packet
	Send_stable(sockfd, data, sizeof(struct packh)+4,client);  //if no ack, that's ok, maybe client gone ,and ack lost
	printf("the concate file is sent over.\nwaiting for the next client\n");
}


int loadIPconfig()
{
	FILE *fp=fopen("config.txt","r");
	int ret;
	if(fp==NULL)
	{
		printf("can't open config.txt\n");
		return -1;
	}
	ret=fscanf(fp,"%s",RegisterServerIP); 
	if(ret==0||ret==EOF)
	{
		printf("can't read RegisterServerIP from config.txt\n");
		fclose(fp);
		return -2;
	}

	ret=fscanf(fp,"%s",ServerIP); 
	if(ret==0||ret==EOF)
	{
		printf("can't read ServerIP from config.txt\n");
		fclose(fp);
		return -2;
	}
	fclose(fp);
	return 0;
	
}

int RegisterServer(int sockfd)
{
	struct sockaddr_in address_sever;	 
	unsigned char data[MAXPACKLEN];	
	int len,ret;
	struct packh *A=data;
	 bzero(&address_sever,sizeof(address_sever));  
	 address_sever.sin_family=AF_INET;  
	 address_sever.sin_addr.s_addr=inet_addr(RegisterServerIP);
	  if(address_sever.sin_addr.s_addr== INADDR_NONE)
	  {
	  	printf("invalid RegisterServerIP address!\n");
	  	return -2;
	  }
	 address_sever.sin_port=htons(8888);  
	 A->cmd=REG;
	strcpy(data+sizeof(struct packh),ServerIP);
	len=strlen(data+sizeof(struct packh))+sizeof(struct packh);
	A->len=htons(len);
	ret=Send_stable(sockfd, data, len, &address_sever);
	return ret;
}

int main()
{
	int sockfd,flags;
 	struct sockaddr_in server;
	 struct sockaddr_in client;

	if(loadIPconfig())
	{
		printf("can't load IP config from config.txt.\n");
		return -5;
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
	 server.sin_port=htons(7777);
		
	 server.sin_addr.s_addr= inet_addr(ServerIP);
	 if(bind(sockfd, (struct sockaddr *)&server, sizeof(server)) == -1)  
	 {  
	  printf("Bind()error.");  
	  return -3;
	 }     		

	 if(RegisterServer(sockfd))
	 {
	 	printf("error in Register to RegitsterServer.\n");
		return -4;
	 }
	 printf("successfully Register ServerIP: %s to Register Sever\n",ServerIP);
	 while(1)
	 {
	 	if(getfilenames(sockfd,&client))
		 	continue;

		if(Recv_Files(sockfd,&client))
			continue;
		printf("start to sending Concatefile...\n");
		Send_Concate(sockfd,&client);
	 }
	 return 0;
}

