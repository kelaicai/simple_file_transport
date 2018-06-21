#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<assert.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<fcntl.h>
#include<unistd.h>
#include<signal.h>
void sigfun(int arg)
{
	sleep(1);
}
int put_file(int sockfd,char file[]);
int get_file(int sockfd,char file[]);
int base_command(int sockfd);
int get_md5(char *file,char md5[]);
int second_put(int sockfd,char file[],char md5[]);
int break_point_put(int sockfd,char* file,char md5[]);
int break_point_get(int sockfd,char* file,char md5[]);
int normal_file_get(int sockfd,char* file,char md5[]);

int main()
{
//	signal(SIGINT,sigfun);
	int sockfd=socket(AF_INET,SOCK_STREAM,0);
	 if(sockfd<0)
	 {
		return -1;
	}
	 struct sockaddr_in saddr;
	 saddr.sin_family=AF_INET;
	 saddr.sin_port=htons(6000);
	 saddr.sin_addr.s_addr=inet_addr("127.0.0.1");
 	 
	 int res=connect(sockfd,(struct sockaddr*)&saddr,sizeof(saddr));
	if(res<0)
	{
			perror("connect error");
			exit(0);
	}

	char command[128]={0};
	while(1)
	{
			printf("Connect success>>");
			fgets(command,128,stdin);

			command[strlen(command)-1]=0;
			char buff[128]={0};
			
			if(((strcmp(command,"\n")==0) || (strcmp(command,"\0")==0)))
			{
				continue;
			}

			if((strncmp(command,"end",3))==0)
			{
				send(sockfd,command,4,0);
				break;
			}

			send(sockfd,command,128,0);
			if((strncmp(command,"get",3))==0)
			{
				char file[128]={0};
				char* p=strtok(command," ");
				p=strtok(NULL," ");
				//没有文件名 
				if(p==NULL)
				{
						continue;
				}
				get_file(sockfd,p);
				continue;
			}

			if((strncmp(command,"put",3))==0)
			{
				char file[128]={0};
				char *p=strtok(command," ");
				p=strtok(NULL," ");
				put_file(sockfd,p);
				continue;
			}
			else
			{
				base_command(sockfd);
				continue;
			}

	}
}

int base_command(int sockfd)
{
		char buff[256]={0};
		int num=recv(sockfd,buff,256,0);
		if(num<=0)
		{
				return -1;
		}
		printf("%s ",buff+3);
		return 0;
}


int get_file(int sockfd,char* file)
{
	assert(sockfd!=-1);
	assert(file!=NULL);
	int fd=open(file,O_RDONLY);
	printf("fd=%d",fd);
	if(fd<0)
	{
		printf("普通文件下载路线\n");
		send(sockfd,"no",2,0);
		if(normal_file_get(sockfd,file,NULL)<0)
		{
			return -1;
		}
		return 0;
	}
	else
	{/*
		send(sockfd,"br",2,0);
		char local_md5[36]={"ok#"};
		get_md5(file,local_md5);
		printf("断点文件下载\n");
		if(break_point_get(sockfd,file,local_md5)<0)
		{
			return -1;
		}
		*/
			return 0;
	}
}


int normal_file_get(int sockfd,char file[],char md5[])
{
	// 获得文件名
	char recv_buf[256]={0};
	int num=recv(sockfd,recv_buf,128,0);
	if(num<=0)
	{
		return -1;
	}
	
	if((strncmp(recv_buf,"ok#",3)==0) && (strcmp(recv_buf+3,"there is no file")==0))
	{
		printf("%s\n",recv_buf);		
	}
	else 
	{
		//收到来自服务器端的文件大小
		int size=atoi(recv_buf+3);
		printf("%s size is %d",recv_buf+3,size);
		printf("\n");
		send(sockfd,"ok",3,0);

		int fd=open(file,O_WRONLY | O_CREAT,0600);
	    if(fd<0)
		{
				send(sockfd,"err",4,0);
		}

		int  cur_size=0;
		char recv_buf[256]={0};
		int num=0;
		while((num=recv(sockfd,recv_buf,256,0))>0)
		{
			cur_size+=num;
			write(fd,recv_buf,num);
			float f=cur_size*100.0/size;
			printf("get:%2.f%%\r",f);
			fflush(stdout);
			if(cur_size==size)
			{
					break;
			}

		}
		if(cur_size!=size)
		{
			printf("文件传输中断\n");
			send(sockfd,"er#",36,0);
			return -1;
		}
		printf("\n");
		printf("下载文件大小;%d 服务端文件大小： %d\n",cur_size,size);
		close(fd);
		char md5[36]={"ok#"};
		if(get_md5(file,md5)<0)
		{
			printf("下载文件的md5值获取失败\n");
			send(sockfd,"er#",36,0);
			return -1;
		}
		else
		{ 
				//获取到md5值，并发送到服务端
			send(sockfd,md5,36,0);
           
		    int ret=recv(sockfd,recv_buf,3,0);
			if(ret<=0)
			{
				perror("recv error:");
				return -1;
			}
			else
			{
				if((strncmp(recv_buf,"ok#",2))==0)
				{
					printf("校验一致\n");
					return 0;
				}
				else
				{
					printf("校验失败\n");		
					return -1;
				}
			}
		}
		
	}
}

int break_point_get(int sockfd,char *file,char md5[])
{
	assert(sockfd>0);
	assert(file!=NULL);

    int fd=open(file,O_WRONLY);
	char recv_md5[36]={"ok#"};
	char local_md5[36]={"ok#"};
    int ret=recv(sockfd,recv_md5,36,0);
	if(ret<=0)
	{
		perror("recv error:");
		return -1;
	}
	
	//文件存在并且md5值不一样,证明说两个文件的是同一文件，只不过大小不一样
	 //将客户端的文件大小发过去，
	 int size=lseek(fd,0,SEEK_END);
	 char buf[10];
	 sprintf(buf,"ok#%d",size);
	 printf("已上传下载 %d\n",size);
	 send(sockfd,buf,36,size);
	
	//获得客户端文件大小
	int entry_size=0;
	ret=recv(sockfd,buf,36,0);
	if(ret<=0)
	{
			perror("recv cli file size error");
	}
	if((strncmp(buf,"ok#",3)==0) && (entry_size=atoi(buf+3)))
	{
		printf("获取服务端文件大小成功\n");
	}

	//客户端回复ok表示说 可以继续传输剩余的'
	ret=recv(sockfd,buf,5,0);
	if(ret<=0)
	{
			perror("recv error:");
			send(sockfd,"er#",3,0);
			return -1;
	}
	printf("recv_ok message=%s\n",buf);
	if((strncmp(buf,"ok#ok",5))!=0)
	{
		printf("未收到ok的回复\n");
		return -1;
	}

	char recv_buf[256]={0};
	int cur_size=0;
	int num=0;
	
	while((num=recv(sockfd,recv_buf,256,0))>0)
	{
		cur_size+=num;
	//	write(fd,recv_buf,num);
		printf("break point file getting....\r");
		fflush(stdout);
		if(entry_size==size+cur_size)
		{
			printf("\n");
			break;
		}
	}
	if(entry_size!=cur_size+size)
	{
		printf("文件下载中断\n");
		send(sockfd,"#er",36,0);
		return -1;
	}
	close(fd);
	printf("\n");
	printf(" 下载结束，现在进行校验\n");

	
	memset(local_md5,0,sizeof(local_md5));
	strcpy(local_md5,"ok#");
	get_md5(file,local_md5);
	//发送被断点续传的文件的md5值
	send(sockfd,local_md5,36,0);
	printf("发送md5成功\n");
	//接收来自服务端文件的校验信息
	ret=recv(sockfd,local_md5,6,0);
	if(ret<=0)
	{
		perror("recv check message error");
	}
	if(strncmp(local_md5,"succe",5)==0)
	{
//		strcpy(recv_buf,"success");
//		send(sockfd,recv_buf,7,0);
		printf("用户文件续传成功\n");
		return 0;
	}
	else
	{
//		strcpy(recv_buf,"failed");
//		send(sockfd,recv_buf,7,0);
		printf("文件续传失败\n");
		return -1;
	}
}

int put_file(int sockfd,char file[])
{
	char _md5[36]={"ok#"};
	get_md5(file,_md5);
	//发送本地文件的md5值
	send(sockfd,_md5,36,0);
	assert(file!=NULL);
	char choice[10]={0};
	int ret=recv(sockfd,choice,2,0);
    printf("choice is =%s",choice); 
	if(ret<=0)
	{
		perror("have recv a choice sign");
	}
	if((strncmp(choice,"se",2))==0)
	{
		if(!second_put(sockfd,file,_md5))
		{
			return 0;	
		}
		return -1;
		
	}
	else if((strncmp(choice,"br",2))==0)
	{
		if(!break_point_put(sockfd,file,_md5))
		{
			return 0;
		}
		return -1;
	}
	else if(strncmp(choice,"no",2)==0)
	{    
		int fd=open(file,O_RDONLY);
		if(fd<0)
		{
			perror("open error");
			return -1;
		}
		int size=lseek(fd,0,SEEK_END);
		lseek(fd,0,SEEK_SET);
		char send_buf[256]={0};
		sprintf(send_buf,"ok#%d",size);
		//发送文件大小
		send(sockfd,send_buf,256,0);
    	printf(" 发送%s",send_buf);
		char recv_buf[256]={0};
     	ret=recv(sockfd,recv_buf,3,0);// 收到了ok
	 	printf("recv=%s\n",recv_buf);
		if(ret<=0 || ret!=3)
		{
			printf("have no recv a file message");
			return -1;
		}
    	printf("vector\n");
		int n=0;
		int con=0;
		while((n=read(fd,send_buf,255))>0)
		{
			con+=n;
			send(sockfd,send_buf,n,0);
			float f=con*100/size;
			printf("client %s %2.f%%\r",file,f);
			fflush(stdout);
			memset(send_buf,0,n);
			if(size==con)
			{
				break;
			}
		}
	printf("file %s put success!\n",file);
	close(fd);
	}
	char md5[36]={"ok#"};
	if(get_md5(file,md5)<0)
	{
			printf("本地文件md5值获取失败\n");
			send(sockfd,"er#",3,0);
			return -1;
	}
	else
	{
		char ser_md5[36]={0};
	  //  if( (ret=recv(sockfd,ser_md5,36,0))>0 && (strcmp(ser_md5,md5)==0))
	//	{
					
	//	}
		
	//	printf("上传文件的md5=%s\n",md5+3);
		ret=recv(sockfd,ser_md5,36,0);
	//	printf("服务端文件的md5=%s\n",ser_md5+3);
		if(ret<0)
		{
			printf("recv error:");
			return -1;
		}
		else
		{
			if((strncmp(ser_md5,"ok#",3)==0) &&
					(strncmp(ser_md5+3,md5+3,32)==0))
			{
					printf("文件校验成功\n");
					return 0;
			}
			else
			{
					printf("文件校验失败\n");
					return -1;
			}
		}
	}
}

int break_point_put(int sockfd,char* file,char md5[])
{
	printf("续传\n");
	assert(sockfd!=-1);
	assert(file!=NULL);
	//给服务端发送md5值
	send(sockfd,md5,36,0);
	char recv_buf[256]={0};
	//接收剩余大小
	int ret=recv(sockfd,recv_buf,36,0);
	if(ret<0)
	{
		perror("recv error:");
	}
	int else_size=0;
	if((strncmp(recv_buf,"ok#",3)==0) && (else_size=atoi(recv_buf+3)))
	{
		;
	}else
	{
		return -1;
	}
    int fd=open(file,O_RDONLY);
	int entry_size=lseek(fd,0,SEEK_END);
	sprintf(recv_buf,"ok#%d",entry_size);
	send(sockfd,recv_buf,36,0);
	
	lseek(fd,0,SEEK_SET);
	lseek(fd,else_size,SEEK_SET);
	send(sockfd,"ok#ok",5,0);

	int cur_size=0;
	int num=0;
	while((num=read(fd,recv_buf,256))>0)
	{
		cur_size+=num;
		send(sockfd,recv_buf,num,0);
		float f=(cur_size+else_size)*100.0/entry_size;
		printf("break point file transporting %2.f%%\r",f);
		memset(recv_buf,0,256);
	}
    printf("文件上传结束\n");
     //接受来自服务端的md5
	memset(recv_buf,0,36);
    ret=recv(sockfd,recv_buf,36,0);
    if(ret<=0)
    {
    	send(sockfd,"error",6,0);
    	return -1;
    }
    if((strncmp(recv_buf,"ok#",3)==0) && strncmp(recv_buf,md5,32)==0)
    {
    	printf("文件校验成功\n");
		send(sockfd,"succe",6,0);
    }
	close(fd);
	printf("文件续传成功\n");
	return 0;
}
int get_md5(char* file,char md5[])
{
			assert(file!=NULL);
		    assert(md5!=NULL);
		   int pipefd[2];
		   pipe(pipefd);
		   pid_t id=fork();
		   if(id<0)
		   {
			 perror("fork error:");
			 return -1;
	       }
		  if(id==0)																	  {
			close(pipefd[0]);	
			dup2(pipefd[1],1);
			dup2(pipefd[1],2);
		    char *args[]={"/usr/bin/md5sum",file,NULL};
			execvp(args[0],args);
			perror("execvp error:");
			exit(0);
		  }
	 	  else
		  {
			close(pipefd[1]);
			int n=0;
		  //md5值有32位的char值，最后使用\0结尾
	    	read(pipefd[0],md5+3,33);
		    md5[strlen(md5)-1]=0;
		//	printf("md5=%s",md5);
		//	printf("md5=%d",strlen(md5+3));
		    close(pipefd[0]);															wait(NULL);																	return 0;																  }
}


int second_put(int sockfd,char* file,char _md5[])
{
	assert(file!=NULL);
	char md5[36]={"ok#"};
	if(!get_md5(file,md5))
	{
		send(sockfd,md5,36,0);
//		printf("sended md5=%s\n",md5);
	}
	else
	{
		printf("文件md5值获取失败！\n");
		strcpy(md5,"er#");
		send(sockfd,md5,36,0);
		return -1;
	}
    //如果收到一个have 那就证明服务端有该文件，直接建立硬连接
	char recv_md5[36]={"ok#"};
    int ret=recv(sockfd,recv_md5,4,0);
//	printf("recv_md5=%s\n",recv_md5);
	if(ret<=0)
	{
	   perror("recv error");
	   //send
	   return -1;
	}
	
    printf("have recv a %s\n",recv_md5);
	if((strncmp(recv_md5,"have",4)==0))
	{
		printf("秒传成功\n");
		return 0;
	}
	else
	{
		send(sockfd,"er",3,0);
		printf("秒传失败\n");
		return -1;
	}
	//建立硬连接
	//printf("秒传\n");
}
