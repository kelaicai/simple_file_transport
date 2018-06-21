#include<stdio.h>
#include<stdlib.h>
#include<fcntl.h>
#include<pthread.h>
#include<string.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<sys/socket.h>
#include<assert.h>
#include<sys/stat.h>
#include<dirent.h>
#include<signal.h>

int create_md5file();
void updata_md5file(int sig);
int find_md5(char md5[]);
int normal_file_put(int sockfd,char* file);
int second_file_put(int sockfd,char *file,char md5[]);
int break_point_put(int sockfd,char *file,char md5[]);

int get_file(int sockfd,char *file);
int normal_file_get(int sockfd,char *file,char md5[]);
int break_point__get(int sockfd,char *file,char md5[]);

int make_hardlink(char *oldfile,char *newfile);
void *thread_fun(void *arg);
int analysis_command(int sockfd,char *command);
int get_md5(char* file,char md5[]);
//char* get_md5_log();
int find(char md5[]);
int is_file_exist(char* file);

int get_md5_log(char *file,char md5[]);
int get_file();
int normal_file_get(int sockfd,char *file,char md5[]);
int break_point_get(int socfdk,char *file,char md5[]);

int create_md5file()
{
   char * args[]={"/bin/sh","file_md5.sh",NULL};
   pid_t id=fork();
   if(id==0)
   {
	 execvp(args[0],args);
	 exit(0);
   }
   else
   {
		wait(NULL);
		return 0;
   }
}
void updata_md5file(int sig)
{
	create_md5file();
}
int create_socket();
int put_file(int sockfd,char file[])
{
	//判断文件已经存在
    //接收来自客户端的md5值
    char recv_md5[36]={"ok#"};
    char local_md5[36]={"ok#"};
	get_md5(file,local_md5);
    int ret=recv(sockfd,recv_md5,36,0);
	int find_md5 =find(recv_md5+3);
	int find_file=find(file);
	int compare=strncmp(local_md5+3,recv_md5+3,32);
    printf("recv_Md5=%s\n find_md5=%d  find_file=%d\n",recv_md5,find_md5,find_file);
	printf("local md5=%s\n",local_md5);
	printf("compare=%d\n",compare);
    if(ret<=0)
    {
    	perror("recv md5 error:");
    }
    
    if(find_md5<0 && find_file<0)   //文件不存在
    {
		printf("normal file transport\n");
    	send(sockfd,"no",2,0);
     	normal_file_put(sockfd,file);
		updata_md5file(1);
    }     //md5值获取成功并且二者md5值一样--->秒传
    else if(find_md5==0)
    {
	    printf("second file transport\n");
    	send(sockfd,"se",2,0);
    	second_file_put(sockfd,file,recv_md5);
		if(find_file<0)
		{
			char old_file[64]={0};
			ret=find_old_file(recv_md5+3,old_file);
			if(ret<0)
			{
				printf("原文件查找失败");
			}
			printf("************ret=%d**********\n",ret);
			printf("oldfile=%s file=%s",old_file,file);
	        if(!make_hard_link(old_file,file))
			{
				printf("hard link success");
			}
	        
		}
		updata_md5file(1);
    }
    else  if(find_md5<0 && find_file==0)//续传
    {
		printf("break_point file transport\n");
    	send(sockfd,"br",2,0);
    	break_point_put(sockfd,file,recv_md5);
		updata_md5file(1);
    }
	else
	{
			;
	}
	/*
*/

}
int normal_file_put(int sockfd,char* file)
{

         //创建文件
		int fd=open(file,O_WRONLY | O_CREAT,0600);
		if(fd<0)
		{
				perror("open error");
				send(sockfd,"open error",256,0);
				return -1;
		}
		//接受文件大小
		char recv_buf[256]={0};
		int num=recv(sockfd,recv_buf,256,0);
		if(num<=0)
		{
				perror("recv error");  //  对于客户端发送过来的文件大小错误情况的回复
				send(sockfd,"recv error",256,0);
				return -1;	
		}
	     
		int size=0;
		if((strncmp(recv_buf,"ok#",3))==0 && (size=atoi(recv_buf+3))>0)
		{
		//	printf("client put file %s and size is %d\n",file,size);
			send(sockfd,"ok",3,0);  //  对于收到文件大小的回复

			int cur_size=0;
			
			int num=0;
			fflush(stdout);
			while((num=recv(sockfd,recv_buf,255,0))>0)
			{
					cur_size+=num;
					write(fd,recv_buf,num);
					float f=cur_size*100.0/size;
					printf("put file %s :%2.f%%\r",file,f);
					fflush(stdout);
					if(cur_size==size)
					{
						break;
			        }
			}
			if(cur_size!=size)
			{
				printf("文件传输中断");
				return 0;
			}
			printf("\n");
			printf("传输结束\n");
		    printf("\n");
			char md5[36]={"ok#"};
			//服务端产生上传文件的md5值
			get_md5(file,md5);
			// 上传结束后服务器接受服务器端文件的md5值
			send(sockfd,md5,36,0);
	//		printf("%d %d\n",num,size);
			close(fd);
			return 0;
	}
}
int second_file_put(int sockfd,char *file,char md5[])
{
	//建立硬链接,秒传
//			char md5[36]={"ok#"};
		///	get_md5_log(file,md5);
		//	printf("second get_md5=%s\n",md5);
      //现在已获得md5值:
			char recv_md5[36]={0}; 
			int ret=recv(sockfd,recv_md5,36,0);
			printf("recv md5=%s\n",recv_md5);
			if(ret<=0)
			{
				perror("recv error");
				send(sockfd,"er",2,0);
				return -1;
			}
			else
			{
	                printf("find(md5+3=)%s\n",md5+3);
					if(find(md5+3)==0)
				    {
							send(sockfd,"have",4,0);
			//				break;
					}
					else
					{
                           send(sockfd,"err#",4,0);
		    				   return -1;
					}
			//	 }
			}
			return 0;
}

int break_point_put(int sockfd,char *file,char md5[])
{
//open成功并且md5值不一样，并且文件就属于断点续传
//上传
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
	 printf("已上传大小i %d\n",size);
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
		printf("获取客户端文件大小成功\n");
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
		printf("break point file transporting....\r.");
		write(fd,recv_buf,num);
		fflush(stdout);
		if(entry_size==size+cur_size)
		{
			printf("\n");
			break;
		}
	}
	close(fd);
	printf("\n");
	printf(" 用户传输结束，现在进行校验\n");

	
	memset(local_md5,0,sizeof(local_md5));
	strcpy(local_md5,"ok#");
	get_md5(file,local_md5);
	//发送被断点续传的文件的md5值
	send(sockfd,local_md5,36,0);
	printf("发送md5成功\n");
	//接收来自客户端文件的校验信息
	ret=recv(sockfd,local_md5,6,0);
	if(ret<=0)
	{
		perror("recv check message error");
	}
	if(strcmp(local_md5,"succe")==0)
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


//string一般为md5值或者文件名
int find(char string[])
{
	FILE* fp=fopen("md5.log","r");
	if(fp==NULL)
	{
		return  -1;
	}
	char *line=(char*)malloc(50);
	size_t len=49;
	ssize_t read;
	while((read=getline(&line,&len,fp))!=-1)
	{
	  // printf("line=%s\n",line);
		if((strstr(line,string)!=NULL))
		{
			fclose(fp);
			free(line);
			return 0;
		}
	}
	fclose(fp);
	free(line);
	return -1;
}

//find_oldfile用于在log文件中根据md5值查出文件名，用于在文件秒传时的硬连接处理
int find_old_file(char md5[],char old_file[])
{
		FILE* fp=fopen("md5.log","r");
        if(fp==NULL)
	     {
			 return  -1;
	     }
	     char *line=(char*)malloc(50);
		 size_t len=49;
	     ssize_t read;
		  while((read=getline(&line,&len,fp))!=-1)
		  {
		      printf("line=%s\n",line);
		      if((strstr(line,md5)!=NULL))
		      {
				  char* ptr;
				  char tmp[49]={0};
				  strcpy(tmp,line);
				  char *p=NULL;
				  p=strtok_r(tmp," ",&ptr);
				  p =strtok_r(NULL," ",&ptr);
				  p[strlen(p)-1]='\0';
				  strcpy(old_file,p);
				  
		           fclose(fp);
		          free(line);
		          return 0;
	          }
	     }
	    fclose(fp);
        free(line);
	    return -1;
}



//如果文件不存在返回-1，存在返回描述符
int is_file_exist(char* file)
{
		assert(file!=NULL);
		int fd=open(file,O_RDONLY);
		if(fd<0)
		{
			return -1;
		}
		close(fd);
		return 0;
}
int make_hard_link(char *oldfile,char *newfile)
{
	char* args[]={"/bin/ln","-L",oldfile,newfile,NULL};

	pid_t id=fork();
	if(id<0)
	{
		perror("fork error");
		return -1;
	}
	else if(id==0)
	{
		execvp(args[0],args);
		perror("execvp hard link error");
		exit(0);
	}
	else
	{
		wait(NULL);
		return 0;
	}
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
	if(id==0)
	{
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
		printf("md5=%s",md5);
		printf("md5=%d",strlen(md5+3));
		close(pipefd[0]);
		wait(NULL);
		return 0;
	}
}


int get_file(int sockfd,char file[])
{
 	assert(sockfd!=-1);
	assert(file!=NULL);
	char local_md5[36]={"#ok"};
	char recv_buf[10];
	int ret=recv(sockfd,recv_buf,2,0);
	if(ret<=0)
	{
		printf("获取文件下载方式失败\n");
		memset(recv_buf,0,10);
		strcpy(recv_buf,"er#");
		send(sockfd,recv_buf,3,0);
		return -1;
	}
	printf("recv_buf=%s\n",recv_buf);
	if((strncmp(recv_buf,"no",2))==0)
	{
		if(normal_file_get(sockfd,file,local_md5)<0)
		{
			return -1;
		}
		return 0;
	}
	/*
	if((strncmp(recv_buf,"br",2))==0)
	{
		if(get_md5(file,local_md5)<0)
		{
			printf("获取文件md5值失败\n");
			return -1;
		}
		if(break_point_get(sockfd,file,local_md5)<0)
		{
			printf("断点文件下载失败\n");
			return -1;
		}
		*/
		return 0;
	}

}

int normal_file_get(int sockfd,char *file,char md5[])
{
	assert(file!=NULL);
	char buff[128]={0};
    int fd=open(file,O_RDONLY);
	if(fd<0)
	{
		perror("open error");
		char send_buf[128]={"er#there is no file"};
		send(sockfd,send_buf,strlen(send_buf)+1,0);
		return -1;
		//pthread_exit(NULL);
	}

	int size=lseek(fd,0,SEEK_END);
	lseek(fd,0,SEEK_SET);
//	send(sockfd,);
//	 向客户端发送文件大小
	char send_buf[256]={0};
	sprintf(send_buf,"ok#%d",size);
	send(sockfd,send_buf,strlen(send_buf),0);
   // 客户端自动回复ok

	char recv_buf[256]={0};
   int ret=recv(sockfd,recv_buf,3,0);
  // printf("ret=%d\n",ret);
	if(ret<0 || ret!=3 && (strncmp(recv_buf,"ok",2)!=0))
	{
		printf("have no recv a file message");
		return -1;
	}


	int n=0;
	int con=0;

	while((n=read(fd,send_buf,256))>0)
	{
		con+=n;
		send(sockfd,send_buf,n,0);
		memset(send_buf,0,256);
	}
	
	close(fd);
//	printf("md5 file=%s\n",file);
	if(get_md5(file,md5)<0)
	{
		printf("get_md5 error\n");
		//send(sockfd,"er#",3,0);
		return -1;
	}
    
	printf("md5=%s\n",md5);
	char cli_md5[36]={"ok#"};
	ret=recv(sockfd,cli_md5,36,0);  //ok# +32char=36
	if(ret<=0)
	{
		perror("recv error:");
		return -1;
	}
	else
	{
		if((strncmp(cli_md5,"ok#",3)==0) && 
				(strncmp(cli_md5+3,md5+3,32)==0))
		{
				//如果接受到客户端的md5值和被下载文件值相同，就发送
				//ok给客户端，告知客户端文件下载正确
				send(sockfd,"ok#",3,0);
				 return 0;
		}
		else
	   {
				send(sockfd,"er#",3,0);
				return -1;
	   }
	}
	return 0;
}
int break_point_get(int sockfd,char *file,char md5[])
{

	assert(sockfd!=-1);
	assert(file!=NULL);
	//给服务端发送md5值
	char local_md5[36]={"ok#"};
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
		memset(recv_buf,0,256);
	}
    printf("文件系下载结束\n");
     //接受来自客户端的md5
	memset(recv_buf,0,36);
    ret=recv(sockfd,recv_buf,36,0);
	printf("fa\n");
    if(ret<=0)
    {
    	send(sockfd,"error",6,0);
    	return -1;
    }
	if(get_md5(file,local_md5)<0)
	{
		printf("文件md5值获取失败\n");
		send(sockfd,"errr",6,0);
		return -1;
	}
    if((strncmp(recv_buf,"ok#",3)==0) && strncmp(recv_buf+3,local_md5,32)==0)
    {
    	printf("文件校验成功\n");
		send(sockfd,"succe",6,0);
    }
	else
	{
		printf("文件校验失败\n");
		send(sockfd,"error",6,0);
		return -1;
	}

	close(fd);
	printf("文件续传成功\n");
	return 0;
}

int analysis_command(int sockfd,char *command)
{
	assert(command!=NULL);
	// 将命令暂存在buff中
	while(*command=='\0')
	{
		command++;
	}
	if((strcmp(command,"end"))==0)
	{
			printf("client over!");
			return 0;
	}
	char buff[128]={0};
	strcpy(buff,command);
	//不存在空串
	char* args[10]={NULL};
	char* q;
	args[0]=strtok_r(buff," ",&q);
	int i=1;
	while((args[i++]=strtok_r(NULL," ",&q))!=NULL)
	{

	}
   
	if((strcmp(args[0],"get")==0))
	{
			// 业务处理成功返回0 失败返回-1
		if(!get_file(sockfd,args[1]))
		{
	        return 0;
		}
		return -1;
	}
	else if((strcmp(args[0],"put"))==0)
	{
		if(!put_file(sockfd,args[1]))
		{
				return 0;
		}
		return -1;
	}
	else
	{
		char path[128]="/bin/";
		strcat(path,args[0]);
	//	printf("path=%s",path);
	//	path[strlen(path)-1]=0;
		int pipefd[2];
		pipe(pipefd);
		pid_t id=fork();
		if(id<0)
		{
			perror("fork error");
		}
		else if(id==0)
		{
			close(pipefd[0]);
			dup2(pipefd[1],1);
			dup2(pipefd[1],2);
				//	execl(path,p,0);
		     execvp(path,args);
     //			 perror("execvp error");
			 exit(0);
		}
		else
		{
			close(pipefd[1]);
			char buff[256]="ok#";
			int n=0;
		//	while((n=read(pipefd[0],buff+3,253)))
			    read(pipefd[0],buff+3,253);
				printf("read=%s",buff);
				send(sockfd,buff,256,0);//发送命令输出
			wait(NULL);
			close(pipefd[0]);
			return 0;
		}
	}
	return -1;
}

	

//
void *thread_fun(void *arg)
{
	int c=(int)arg;
	while(1)
	{
		char command[128]={0};
		int n=recv(c,command,128,0);  //接收命令
		if(n<0)
		{
				continue;
		}
		if((strncmp(command,"\n",1))==0)
		{
			continue;
		}
		if(n<=0)
		{
			break;
		}
		printf("buff=%s %d\n",command,strlen(command));
   	    analysis_command(c,command); //解析命令
		memset(command,0,128);
		updata_md5file(1);
	//	pthread_exit(NULL);
	}
	pthread_exit(NULL);
}


int create_socket()
{
	int sockfd=socket(AF_INET,SOCK_STREAM,0);
	if(sockfd<0)
	{
		return -1;
	}

	struct sockaddr_in saddr;
	memset(&saddr,0,sizeof(saddr));
	saddr.sin_family=AF_INET;
	saddr.sin_port=htons(6000);
	saddr.sin_addr.s_addr=inet_addr("127.0.0.1");

	int res=bind(sockfd,(struct sockaddr*)&saddr,sizeof(saddr));
	if(res<0)
	{
		perror("bind error");
	}
	listen(sockfd,5);
	return sockfd;
}

int  main()
{
	int sockfd=create_socket();
	assert(sockfd!=-1);
	create_md5file();
//	signal(S,updata_md5file);
	struct sockaddr_in caddr;
	while(1)
	{
		int len=sizeof(caddr);
		int c=accept(sockfd,(struct sockaddr*)&caddr,&len);
		if(c<0)
		{
			continue;
		}
		printf("accept:%d\n",c);

		pthread_t id;
		pthread_create(&id,NULL,thread_fun,(void*)c);
	}
	
}
