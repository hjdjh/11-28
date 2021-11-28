#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
/*#include "threadpool.h"*/


//#include <stdio.h>
#include <ctype.h>
#include <sys/socket.h>
#include <arpa/inet.h>
//#include <stdlib.h>
//#include <string.h>
//#include <unistd.h>
//#include <errno.h>
//#include <pthread.h>

//#include <stdio.h>
//#include <string.h>
//#include <string.h>
//#include <stdlib.h>
#include <netinet/in.h>
//#include <arpa/inet.h>
#include <sys/wait.h>
//#include <sys/types.h>
#include <sys/stat.h>
#include <sys/epoll.h>
//#include <unistd.h>
#include <fcntl.h>

struct Cfd_addr{
        int fd;
        struct  sockaddr_in addr;
};


struct Thdpool
{
        pthread_mutex_t pool_lock;
        pthread_mutex_t thd_num_lock;
        pthread_cond_t queue_not_empty;
        pthread_cond_t queue_not_full;

        pthread_t adjust_tid;
        int thd_alive_num;
        int thd_busy_num;
        int thd_wait_exit_num;
        int thd_max_num;
        int thd_min_num;

        struct task_t* queue;
        int queue_max_num;
        int queue_front;
        int queue_rear;
        int queue_size;
};




void sys_err(const char* s)
{
        printf("%s\n",s);
        exit(1);
}


void send_respond(int cfd, int no, char *disp, char *type, int len)
{
	char buf[4096] = {0};
	
	sprintf(buf, "HTTP/1.1 %d %s\r\n", no, disp);
	send(cfd, buf, strlen(buf), 0);
	
	sprintf(buf, "Content-Type: %s\r\n", type);
	sprintf(buf+strlen(buf), "Content-Length:%d\r\n", len);
	send(cfd, buf, strlen(buf), 0);
	
	send(cfd, "\r\n", 2, 0);
}


int send_respond1(int fd,int numb,char* judge,const char* type,int len)
{
        char buf[2018]={0};
        sprintf(buf,"HTTP/1.1 %d %s\r\n",numb,judge);
        sprintf(buf+strlen(buf),"Content-Type:%s\r\n",type);
        sprintf(buf+strlen(buf),"Content-Length:%d\r\n\r\n",len);

        printf("buf send:%s\n",buf);
        int n=write(fd,buf,strlen(buf));
        printf("write count:%d\n",n);

        return 0;
}


char* file_extract(int cfd)
{

        int begin=0,key_null=0;

        char buf[2048]={0},method[256]={0},pro[256]={0};
        char* path=(char* )malloc(sizeof(char)*256);

        int n=read(cfd,buf,sizeof(buf));
        printf("buf:%s\n",buf);
        if(n==-1)
        {
                close(cfd);
		printf("file read error\n");
		return NULL;
                //exit(1);
        }

        for(int i=0;i<1024;i++)
        {
          if(buf[i]==' ')
          {
                  key_null++;
                  begin=0;
                  continue;

          }
         // printf("key_null:%d\n",key_null);
          if(buf[i]=='\r' && buf[i+1]=='\n')
          {
                 // printf("file_extract exit\n");
                  break;
          }


//        printf("%c   %c\n",buf[i],buf[i+1]);

          if(key_null==0)
          {
             method[begin++]=buf[i];
             continue;
          }
          //printf("method:%s\n",method);



           if(key_null==1)
           {

              path[begin++]=buf[i];
              continue;
           }
          // printf("file path:%s\n",path);
           pro[begin++]=buf[i];
           //printf("pro:%s\n",pro);

         }

        printf("file path in path extract:%s\n",path+1);

         return path;
}


char* filetype_extrace(char* path)
{
        int flag=0,begin=0;
        char later[16];
        for(int i=0;path[i]!='\0';i++){
                 if(path[i]=='.')flag=1;
                 if(flag==1)later[begin++]=path[i];
         }
        later[begin]='\0';

        printf("%s   %s\n",path,later);

        if(strcmp(later,".jpg")==0)return "Content-Type:image/jpeg";
        if(strcmp(later,".text")==0)return "Content-Type: text/plain; charset=iso-8859-1";
        if(strcmp(later,".c")==0)return "Content-Type: text/plain; charset=iso-8859-1";
        if(strcmp(later,".html")==0)return "Content-Type: html/plain; charset=iso-8859-1";
        if(strcmp(later,".ico")==0)return "Content-Type:image/jpeg";

	return NULL;
}














int file_send(char* path,int cfd)
{
        char buf[1024];
        int n,ret;
        int fd=open(path+1,O_RDONLY);
         if(fd==-1)
         {
		 free(path);
		 //close(fd);
                 printf("open error  path:%s\n",path);
                 //exit(1);
		 return 0;
         }

         //printf("\nfile in file_send:%s\n",path);
	free(path);
         char* type=filetype_extrace(path);
         printf("file type:%s\n",type);


         send_respond(cfd,200,"OK",type,-1);

        while ((n = read(fd, buf, sizeof(buf))) > 0) {
                ret = write(cfd, buf, n);
                if (ret == -1) {
                       printf("send error\n");
                       //exit(1);
		       break;
                 }
        }
	close(fd);
	return 0;
}




void send_file(int cfd, const char *file)
{
	int n = 0, ret;
	char buf[4096] = {0};
	
	
	int fd = open(file, O_RDONLY);
	if (fd == -1) {
	
		perror("open error");
		exit(1);
	}
	
	while ((n = read(fd, buf, sizeof(buf))) > 0) {		
		ret = send(cfd, buf, n, 0);
		if (ret == -1) {
			perror("send error");	
			exit(1);
		}
		if (ret < 4096)		
			printf("-----send ret: %d\n", ret);
	}
	
	close(fd);		
}


void http_request(int cfd, char *file)
{
	struct stat sbuf;


	int ret = stat(file, &sbuf);
	if (ret != 0) {

		perror("stat");
		exit(1);
	}

	if(S_ISREG(sbuf.st_mode)) {
               char* type=filetype_extrace(file);
         printf("file type:%s\n",type);


//         send_respond(cfd,200,"OK",type,-1);


		send_respond(cfd, 200, "OK",type, -1);

		send_file(cfd, file);
	}
}











int queue_add(void* pool,void* (*fun)(void* argc),void* argc);
void*  data_delivery(void* argc);


int server_port_open(int port,struct Thdpool* tp)
{
	//struct Thdpool* tp=pool;
        struct Cfd_addr cfd_addr;
        struct sockaddr_in serv_addr,clit_addr;

        serv_addr.sin_family=AF_INET;
        serv_addr.sin_port=htons(port);
        serv_addr.sin_addr.s_addr=htonl(INADDR_ANY);

        int lfd=socket(AF_INET,SOCK_STREAM,0);
        if(lfd==-1)sys_err("socket error");
        if(bind(lfd,(struct sockaddr* )&serv_addr,sizeof(serv_addr))==-1)sys_err("bind error");


        listen(lfd,160);

        int clit_addr_len=sizeof(clit_addr);
        while(1){
                cfd_addr.fd=accept(lfd,(struct sockaddr* )&clit_addr,&clit_addr_len);
                if(cfd_addr.fd==-1)sys_err("accept error\n");
                cfd_addr.addr=clit_addr;
                queue_add((void* )tp,data_delivery,(void* )&cfd_addr);
		//if(data_delivery(cfd_addr)==0)break;
        }
        close(lfd);
        return 0;
}



void*  data_delivery(void* argc)
{
	struct Cfd_addr cfd_addr =*((struct Cfd_addr* )argc);
        char client_ip[1024],buf[1024];
        printf("client IP:%s  port:%d\n",
                        inet_ntop(AF_INET,&cfd_addr.addr.sin_addr.s_addr,client_ip,sizeof(client_ip)),
                        ntohs(cfd_addr.addr.sin_port));


	 char* path=file_extract(cfd_addr.fd);

  //      if (strncasecmp(method, "GET", 3) == 0)
//	{
		char *file = path+1;   // 鍙栧嚭 瀹㈡埛绔璁块棶鐨勬枃浠跺悕
		
		http_request(cfd_addr.fd, file);
		
		//disconnect(cfd_addr.fd, epfd);
		close(cfd_addr.fd);
//	}



 //       char* path=file_extract(cfd_addr.fd);
//	if(path==NULL){
//		printf("file path extract error\n");
//		close(cfd_addr.fd);
//		return NULL;
///	}

	//printf("file path:%s\n",path+1);

        //file_send(path,cfd_addr.fd);
	//close(cfd_addr.fd);
	return NULL;
	
	
	
}




struct task_t
{
        void* (*fun)(void*);
        void* argc;
};





void* thd_fun(void* pool_t);
void* adjust_fun(void* pool_t);
void* task_excutefun(void* argc);


void* task_excutefun(void* argc)
{
        int* i=(int *)argc;
        printf("%d\n",*i);
        return NULL;
}

struct Thdpool* thdpool_init(int thd_max_n,int thd_min_n,int queue_max_n)
{
        pthread_t tid;
        struct Thdpool* tp=(struct Thdpool* )malloc(sizeof(struct Thdpool));

//      struct Thdpool* tp;
       // size_t size=sizeof(struct Thdpool );
       // tp=(struct Thdpool* ) malloc(size);

        //if(tp==NULL)sys_err("molloc thdpool error");

        tp->thd_max_num=thd_max_n;
        tp->thd_min_num=thd_min_n;
        tp->thd_busy_num=0;
        tp->thd_alive_num=thd_min_n;
        tp->thd_wait_exit_num=0;

        tp->queue_max_num=queue_max_n;
        tp->queue_front=0;
        tp->queue_rear=0;
        tp->queue_size=0;
        if(pthread_mutex_init(&(tp->pool_lock),NULL)
                        ||pthread_mutex_init(&(tp->thd_num_lock),NULL)
                        ||pthread_cond_init(&(tp->queue_not_empty),NULL)
                        ||pthread_cond_init(&(tp->queue_not_full),NULL))
                sys_err("cond or mutex init error");

        tp->queue=(struct task_t* )malloc(sizeof(struct task_t)*queue_max_n);
        if(tp->queue==NULL)sys_err("queue molloc error");

        pthread_create(&(tp->adjust_tid),NULL,adjust_fun,(void* )tp);
        for(int i=0;i<thd_min_n;i++)
                pthread_create(&tid,NULL,thd_fun,(void* )tp);

        return tp;
}

void* adjust_fun(void* pool_t)
{
        pthread_t tid;
        int thd_alive_n,thd_busy_n;
        struct Thdpool* tp=(struct Thdpool* )pool_t;
        while(1){

        sleep(5);
        pthread_mutex_lock(&(tp->thd_num_lock));
        int thd_alive_n=tp->thd_alive_num;
        int thd_busy_n=tp->thd_busy_num;

        if(thd_alive_n*0.2>thd_busy_n && thd_alive_n-10>=tp->thd_min_num){
                tp->thd_wait_exit_num+=10;
                pthread_mutex_unlock(&(tp->thd_num_lock));
                for(int i;i<10;i++)
                        pthread_cond_signal(&(tp->queue_not_empty));
        }

else if(thd_alive_n*0.8<thd_busy_n && thd_alive_n+10<=tp->thd_max_num){
                tp->thd_alive_num+=10;
                pthread_mutex_unlock(&(tp->thd_num_lock));
                for(int i=0;i<10;i++)
                        pthread_create(&tid,NULL,thd_fun,(void* )tp);


        }
        else pthread_mutex_unlock(&(tp->thd_num_lock));
       // printf("thd_busy_num:%d   thd_alive_exit:%d\n",tp->thd_busy_num,tp->thd_alive_num);
       // printf("queue_size:%d    queue_front:%d\n",tp->queue_size,tp->queue_front);
        }
        return NULL;
}

void* thd_fun(void* pool_t)
{
        struct Thdpool* tp=(struct Thdpool* )pool_t;
        while(1){

                pthread_mutex_lock(&(tp->pool_lock));
                while(tp->queue_size==0){
                        pthread_cond_wait(&(tp->queue_not_empty),&(tp->pool_lock));
  //                      printf("................%d\n",tp->queue_size);
                        if(tp->queue_size!=0)break;

                        pthread_mutex_unlock(&(tp->pool_lock));
                        pthread_mutex_lock(&(tp->thd_num_lock));
                        tp->thd_wait_exit_num--;
                        tp->thd_alive_num--;
                        pthread_mutex_unlock(&(tp->thd_num_lock));

                        pthread_exit(NULL);
                }
//printf("thd_fun argc:%d\n\n",*((int* )tp->queue[tp->queue_front].argc));
                printf("queue_front:%d\n",tp->queue_front);
                struct task_t task;
                task.fun=tp->queue[tp->queue_front].fun;
                task.argc=tp->queue[tp->queue_front].argc;
                tp->queue_front=(tp->queue_front+1)%tp->queue_max_num;
                tp->queue_size--;
                pthread_mutex_unlock(&(tp->pool_lock));

                pthread_cond_broadcast(&(tp->queue_not_full));

                pthread_mutex_lock(&(tp->thd_num_lock));
                //printf("achieve lock\n");
                tp->thd_busy_num++;
                pthread_mutex_unlock(&(tp->thd_num_lock));


                printf("thread ox%x woking\n",(unsigned int)pthread_self());
                (*task.fun)(task.argc);
                printf("thread ox%x ending\n",(unsigned int)pthread_self());

                pthread_mutex_lock(&(tp->thd_num_lock));
                tp->thd_busy_num--;
                pthread_mutex_unlock(&(tp->thd_num_lock));

        }
        return NULL;
}


int queue_add(void* pool,void* (*fun)(void* argc),void* argc)
{
        struct Thdpool* tp=(struct Thdpool* )pool;
        pthread_mutex_lock(&(tp->pool_lock));
        while(tp->queue_size>=tp->queue_max_num){
                pthread_cond_wait(&(tp->queue_not_full),&(tp->pool_lock));
        }

        //printf("queue_add argc:%d\n\n",*((int*)argc));
        tp->queue[tp->queue_rear].fun=fun;
        tp->queue[tp->queue_rear].argc=argc;
        tp->queue_rear=(tp->queue_rear+1)%tp->queue_max_num;
        tp->queue_size++;


        pthread_mutex_unlock(&(tp->pool_lock));
       // printf("queue_add later argc:%d\n\n",*((int* )tp->queue[tp->queue_front].argc));
        //sleep(1);
        pthread_cond_signal(&(tp->queue_not_empty));

        return 0;
}










int main(int argc,char* argv[])
{
        if(argc!=2)sys_err("argc error");
	struct Thdpool* tp=thdpool_init(100,10,100);
        server_port_open(atoi(argv[1]),tp);
        return 0;
}










