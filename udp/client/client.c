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
};



struct file_des FOut[FILENUM];
char RegiserServerIP[16];

//load filename from config.txt
int load_config()
{
	int i,j,ret;
	FILE *fp=fopen("config.txt","r");
	if(fp==NULL)
	{
		printf("can't open config.txt to load the file names.\n");
		return -1;
	}
	for(i=0;i<FILENUM;i++)
	{
		if(fgets(FOut[i].filename,NAMELEN,fp)==NULL)
		{
			printf("error in reading the %d file name.\n");
			fclose(fp);
			return -2;
		}
		j=strlen(FOut[i].filename)-1;
		while((FOut[i].filename[j]=='\n')||(FOut[i].filename[j]=='\r'))
		{
			FOut[i].filename[j]=0;
			j--;
		}
		FOut[i].fd=fopen(FOut[i].filename,"r");
		if(FOut[i].fd==NULL)
		{
			printf("error in opening file %s\n",FOut[i].filename);
			return -3;
		}
	}
	//get RegiserServerIP from config.txt
	// if not existed, you have to provide ServerIP from command Line
	ret=fscanf(fp,"%s",RegiserServerIP);
	if(ret==0||ret==EOF)
		printf("no RegiserServerIP found in config.txt");
	fclose(fp);
	return 0;
}


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

int send_filenames(int sock_fd,struct sockaddr_in *peer)
{
	unsigned char data[LINELEN];
	struct packh *ph=data;
	int i,len,ret;
	ph->cmd=FILENAME;
	for(i=0;i<FILENUM;i++)
	{
		ph->fid=i;
		len=strlen(FOut[i].filename);
		memcpy(data+sizeof(struct packh),FOut[i].filename,len);
		ph->len=htons(len+sizeof(struct packh));
		ret=Send_stable(sock_fd,data,len+sizeof(struct packh),peer);
		if(ret)
		{
			printf("error sending filename: %s\n",FOut[i].filename);
			return -1;
		}
		else
		{
			printf("successfully sending filename: %s\n",FOut[i].filename);
		}
	}
	return 0;
}



int get_randfile()
{
	int j,i=rand()%FILENUM;
	for(j=0;j<FILENUM;j++)
	{
		if(!FOut[i].finished)		
			return i;
		i++;
		if(i==FILENUM)
			i=0;
	}
	return -1;
}


int Send_GroupStable(int skfd,unsigned char data[][MAXPACKLEN],int num,struct sockaddr_in *peer)
{
	int ret,i,j,done,k;
	int len[RANDFN];
	struct sockaddr_in client;
	unsigned int line[RANDFN],x0;
	struct packh *A;
	unsigned char da[MAXPACKLEN];
	unsigned char cmd;
	for(i=0;i<num;i++)
	{
		A=data[i];
		len[i]=ntohs(A->len);
		line[i]=ntohl(*(unsigned int *)(data[i]+sizeof(struct packh)));
	}

	for(i=0;i<TRYTIME;i++)
	{
		done=1;
		k=0;
		for(j=0;j<num;j++)
		 //for(j=num-1;j>=0;j--) //test for disorder
		{
			if(len[j])
			{
				ret=Mysend(skfd,data[j],len[j],peer);
				if(ret)
				{
					printf("error in sending line:%d for file %d : %s\n",line[j],data[j][3],data[j]+sizeof(struct packh)+4);
					return -2;
				}
				else
				{
					printf("ok in sending line:%d for file %d : %s\n",line[j],data[j][3],data[j]+sizeof(struct packh)+4); //comment this when done
					
				}
				done=0;
				k++;
			}
		}
		if(done)
			return 0;
		ret=0;
		while(ret==0)
		{
			ret=MyReceive(skfd,da,&client);
			if(ret)
				break;
			A=da;
			if(A->cmd^cmd==MASK)  //cmd and its ACK		
			{
				x0=ntohl(*(unsigned int *)(da+sizeof(struct packh)));
				printf("line %d for file %d ack get.\n",x0,A->fid);  //comment this when done
				for(j=0;j<num;j++)
				{
					if(line[j]==x0)
					{
						len[j]=0;
						break;
					}
				}
			}
			k--;
			if(k==0)
				break;
		}
	}
	printf("Send_GroupStable:failed after trying %d times to transport....\n",TRYTIME);
	return -1;
}

int send_lines(int sock_fd, int fidx, int num,struct sockaddr_in *peer)
{
	int i,j,ret;
	unsigned char data[RANDFN][MAXPACKLEN];
	struct packh *ph;
	memset(data,0,sizeof(data));
	for(i=0;i<num;i++)
	{
		ph=data[i];
		ph->fid=fidx;
		ph->cmd=DATA;

		if(fgets(data[i]+sizeof(struct packh)+4,LINELEN,FOut[fidx].fd)==NULL)  //is over
		{
			FOut[fidx].finished=1;
			fclose(FOut[fidx].fd);
			*(unsigned int *)(data[i]+sizeof(struct packh))=htonl(FOut[fidx].nextlinenum);
			ph->len=htons(sizeof(struct packh)+4);
			num=i+1;
		}
		j=strlen(data[i]+sizeof(struct packh)+4);
		*(unsigned int *)(data[i]+sizeof(struct packh))=htonl(FOut[fidx].nextlinenum);
		FOut[fidx].nextlinenum++;
		ph->len=htons(sizeof(struct packh)+4+j);				
	}

	return Send_GroupStable(sock_fd,data,num,peer);
}

int send_file(int sock_fd,struct sockaddr_in *peer)
{
	int i,num,j,ret;
	srand(time(NULL));
	while(1)
	{
		j=get_randfile();	
		if(j==-1)
			return 0;
		num=(rand()%RANDFN)+1;
		ret=send_lines(sock_fd,j,num,peer);
		if(ret)
			return -1;
	}
}

int recv_concafile(int skfd,struct sockaddr_in *peer)
{
	unsigned int nextline=0;
	int ret,tryk=0;
	unsigned int x0,len;
	unsigned char data[MAXPACKLEN];
	unsigned char ack[16]; 
	struct sockaddr_in client;
	struct packh *ph=data;
	struct packh *phack=ack;
	FILE *fp=fopen("concate","wb");
	if(fp==NULL)
	{
		printf("error in opening file concate for writing.\n");
		return -1;
	}
	while(1)
	{
		ret=MyReceive(skfd, data,&client);
		if(ret)
		{
			tryk++;
			if(tryk==2*TRYTIME)
			{
				printf("recv_concafile---error in MyReceive after trying %d times.\n",2*TRYTIME);
				break;
			}
			continue;
		}
		tryk=0;
		//send ACK
		memcpy(ack,data,sizeof(struct packh));
		phack->cmd=ph->cmd^MASK;
		phack->len=htons(sizeof(struct packh));
		
		ret=Mysend(skfd, ack, sizeof(struct packh),peer);
		if(ret)
		{
			printf("error in sending back ack...\n");
			fclose(fp);
			return -2;			
		}
		x0=ntohl(*(unsigned int *)(data+sizeof(struct packh)));
		printf("recv line:%d\n",x0); //comment this when done
		if(x0==nextline)
		{
			len=ph->len-sizeof(struct packh)-4;
			if(len==0)  //over			
				break;
			fwrite(data+sizeof(struct packh)+4,1,len,fp);
			nextline++;
		}
	}
	fclose(fp);
	if(tryk==TRYTIME)
		return -1;
	return 0;
}


int GetServerIP(int sockfd,char *ServerIP)
{
	struct sockaddr_in address_sever;	 
	struct sockaddr_in client;	 
	int ret,i,len;
	unsigned char data[MAXPACKLEN];
	unsigned char da[MAXPACKLEN];
	struct packh *A=data;	
	  bzero(&address_sever,sizeof(address_sever));  
	  address_sever.sin_family=AF_INET;  
	  address_sever.sin_addr.s_addr=inet_addr(RegiserServerIP);
	  if(address_sever.sin_addr.s_addr== INADDR_NONE)
	  {
	  	printf("invalid serverip address!\n");
	  	return -2;
	  }
	 address_sever.sin_port=htons(8888);  	
	 A->cmd=QURY;
	 A->len=htons(sizeof(struct packh));
	for(i=0;i<TRYTIME;i++)
	{

		ret=Mysend(sockfd,data,sizeof(struct packh),&address_sever);
		if(ret)
			continue;

		ret=MyReceive(sockfd,da,&client);
		if(ret)
			continue;
		A=da;
		if(A->cmd==QACK)  //cmd and its ACK		
		{
			if(A->len==sizeof(struct packh))
			{
				printf("Register Server tell us no Server available now\n");
				return -1;
			}
			memcpy(ServerIP,da+sizeof(struct packh),A->len-sizeof(struct packh));
			return 0;
		}
	}
	printf("can't get SeverIP after trying %d times\n",TRYTIME);
	return -2;
}


int main(int argc, char **argv)
{

	 int i,sockfd,flags,ret;
	struct sockaddr_in address_sever;	 
	unsigned short server_port;
	if(load_config())	
		return -1;

	 sockfd=socket(AF_INET,SOCK_DGRAM,0);

	 flags = fcntl(sockfd, F_GETFL, 0);

	 if (-1 == fcntl(sockfd, F_SETFL, flags|O_NONBLOCK))
	    {
	        printf("fcntl socket error!\n");
	        return -3;
	    } 		 	
	if(argc==2)  // we have ip and port in command line
	{
		  bzero(&address_sever,sizeof(address_sever));  
		  address_sever.sin_family=AF_INET;  
		  address_sever.sin_addr.s_addr=inet_addr(argv[1]);
		  if(address_sever.sin_addr.s_addr== INADDR_NONE)
		  {
		  	printf("invalid serverip address!\n");
		  	return -2;
		  }
		  server_port=7777;
		 address_sever.sin_port=htons(server_port);  


		
	}
	else if(argc == 1)  // we have to ask  Registery Server about the Server Center IP
	{
		char ServerIP[16];
		memset(ServerIP,0,sizeof(ServerIP));
		ret=GetServerIP(sockfd,ServerIP);
		if(ret)
		{
			printf("can't get SeverIP from Register Server.\n");
			return -4;
		}
		printf("Register Server tells us serverip: %s\n ",ServerIP);
		  bzero(&address_sever,sizeof(address_sever));  
		  address_sever.sin_family=AF_INET;  
		  address_sever.sin_addr.s_addr=inet_addr(ServerIP);
		  if(address_sever.sin_addr.s_addr== INADDR_NONE)
		  {
		  	printf("invalid serverip address!\n");
		  	return -2;
		  }
		  server_port=7777;
		 address_sever.sin_port=htons(server_port);  
		
	}

	if(send_filenames(sockfd,&address_sever))
		return -4;

	if(send_file(sockfd,&address_sever))
		return -5;
	printf("sending all files over. start to recv concafile.\n");
	if(recv_concafile(sockfd,&address_sever))
		return -6;
	printf("recv concafile done.\n ");
	return 0;
	
}


