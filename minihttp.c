#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>

#define SERVER_PORT 80

static int debug = 1;

void perror_exit(const char * des){
     fprintf(stderr, "%s error, reason: %s\n", des, strerror(errno));
     exit(1);
}

//获取client_sock的一行数据,返回值是读取的数据长度
int get_line(int client_sock, char *buf, int size);

//处理htpp请求，读取客户端发送的数据
void * do_http_request(void * pclient_sock);

//响应http
void do_http_response(int client_sock, const char * path);

//发送http头部
int headers(int client_sock, FILE * resource);

//发送http的body
void cat(int client_sock, FILE * resource);

//找不到文件的时候的回复响应404
void not_found(int client_sock);

//500
void unimplemented(int client_sock);

//400
void bad_request(int client_sock);

//服务器内部出错回复响应
void inner_error(int client_sock);

int main(void){
    
    int sock;//代表信箱
    int i;
    struct sockaddr_in server_addr;//标签
    
    //1创建信箱	socket函数第一个参数网络地址（网络通信家族，指定tcp/ip）第二个参数是指定tcp协议
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock == -1){
 	perror_exit("create socket");
    }
 
    //2清空标签，写上地址和端口号
    bzero(&server_addr, sizeof(server_addr));

    //server_addr是一个地址（结构体）
    server_addr.sin_family = AF_INET;//选择协议族IPV4
    //inet_pton(AF_INET, IP, &server_addr.sin_addr.s_addr);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);//监听本地所有IP地址,htonl转换为网络字节序（大端字节序），l表示long，因为ip地址是32位	
    server_addr.sin_port = htons(SERVER_PORT);//绑定端口号，htons转化位网络字节序（大端字节序），s表示short，因为端口号是16位
    
    //实现标签贴到收信的信箱上  
    int ret = bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if(ret == -1){
	perror_exit("bind");
    }

    //把信箱挂置传达室，这样，就可以接受信件了
    
    ret = listen(sock, 128);

    if(ret == -1){
 	perror_exit("listen");
    }
    //万事俱备，只等来信
    printf("等待客户端的连接\n");

    
    int done = 1;
    
    while(done){
        struct sockaddr_in client;
        int client_sock;
        char client_ip[64];
        char buf[256];
        int len;//接受客户端发送的长度
        pthread_t id;
        int * pclient_sock = NULL;       

        socklen_t client_addr_len;
        client_addr_len = sizeof(client);
        client_sock = accept(sock, (struct sockaddr *)&client, &client_addr_len);
        
	//打印客户端地址和端口号
        printf("client ip: %s\t port : %d\n", 
                   inet_ntop(AF_INET, &client.sin_addr.s_addr, client_ip, sizeof(client_ip)),
	 	   ntohs(client.sin_port));

	//处理htpp请求，读取客户端发送的数据
	//do_http_request(client_sock);
         	
	//启动线程处理http请求
	pclient_sock = (int *)malloc(sizeof(int));
        *pclient_sock = client_sock;
	
	pthread_create(&id, NULL, do_http_request, (void *)pclient_sock);
	
	//close(client_sock);
    }
    return 0;

}

void * do_http_request(void * pclient_sock){
     int len = 0;
     char buf[256];     
     char method[64];
     char url[256];     
     char path[256];
     struct stat st;
     int client_sock = *(int*)pclient_sock;

     //读取客户端发送的http请求

     //1.读取请求行
     len = get_line(client_sock, buf, sizeof(buf));
     
     if( len > 0 ){
          //读到了请求行
          int i = 0, j = 0;        
  	  while(!isspace(buf[j]) && i < sizeof(method) - 1){
	      method[i] = buf[j];
	      i++;
	      j++;
 	  }

	  method[i] = '\0';
	  if(debug){
               printf("request method: %s\n", method);
	  }

          //strncmp区分大小写的比较，strncasecmp是不区分大小写的比较
    	  if(strncasecmp(method, "GET", i)==0){
	       //只处理get请求
	       if(debug){
	    	   printf("method = GET\n");
	       }

	       //获取url
	       while(isspace(buf[j++])) ; //跳过空格	
	       i = 0;

               while(!isspace(buf[j]) && (i < sizeof(url) - 1)){
                   url[i] = buf[j];
                   i++;
                   j++;
               }

	       url[i] = '\0';
	       if(debug){
	            printf("url: %s\n", url);	
	       }

    	       //继续读取http头部
      	       do{
	            len = get_line(client_sock, buf, sizeof(buf));	
                    if(debug){
                         printf("read: %s\n", buf);
	            }
   	       }while(len > 0);     
	  
	       //****定位服务器本地的html文件****

	       //处理url中的？
	       {
	           char * pos = strchr(url, '?');
		       if(pos){
		           *pos = '\0';
			   printf("real url: %s\n", url);
		       }
	       }
               
	       sprintf(path, "./html_docs/%s", url);        
	       if(debug){
		   printf("path: %s\n", path);
	       }

	       //执行http响应	
	       //判断文件是否存在，如果存在就响应200 OK，同时发送响应的html 文件，如果不存在，就响应 404 NOT FOUND.
               if(stat(path, &st) == -1){
	           //文件不存在或者出错返回-1    
	           fprintf(stderr,"stat %s failed. reason: %s\n", path, strerror(errno));     
	           not_found(client_sock);               
               }else{
		   //文件存在
		   //判断文件是否是一个目录，如果是在path之后追加一个/index.html
		   if(S_ISDIR(st.st_mode)){
		       strcat(path, "/index.html");
		   }

		   do_http_response(client_sock, path);
	       }
 
          }else { 
               //非get请求,读取http头部，并响应客户端501  Method Not Implemented  
               fprintf(stderr, "warning! other request [%s]\n", method);
      	       do{
	            len = get_line(client_sock, buf, sizeof(buf));	
                    if(debug){
                         printf("read: %s\n", buf);
	            }
   	       }while( len > 0);
	
	       unimplemented(client_sock);    //在响应时再实现
	 
	  }    
	
     }else {
          //请求格式有问题，出错处理
	  bad_request(client_sock);  //在响应时在实现
     }
     
     close(client_sock);
     if(pclient_sock != NULL){
	  //释放动态分配的内存
          free(pclient_sock);
     }
    
     return NULL;
}

void do_http_response(int client_sock, const char * path){
    int ret = 0;
    FILE * resource = NULL;
    resource = fopen(path, "r");
    
    if(resource == NULL){
        not_found(client_sock);
	return;
    }
    
    //1.发送http 头部
    ret =  headers(client_sock, resource);
   
    //2.发送http body.
    if(!ret){
        cat(client_sock,resource);
    }

    fclose(resource);
}

/****************************
 *  *返回关于响应文件信息的http 头部
 *   *输入： 
 *    *     client_sock - 客服端socket 句柄
 *     *     resource    - 文件的句柄 
 *      *返回值： 成功返回0 ，失败返回-1
 *      ******************************/
int headers(int client_sock, FILE * resource){
    struct stat st;
    int fileid = 0;
    char tmp[64];
    char buf[1024] = {0};

    strcpy(buf, "HTTP/1.0 200 OK\r\n");
    strcat(buf, "Server: xhc Server\r\n");
    strcat(buf, "Content-Type: text/html\r\n");
    strcat(buf, "Connection: Close\r\n");
    
    fileid = fileno(resource);    
    
    if(fstat(fileid, &st) == -1){
        inner_error(client_sock);
        return -1; 
    }    
    
    snprintf(tmp, 64, "Content-Length: %d\r\n\r\n", st.st_size);
    strcat(buf, tmp);
   
    if(debug){
        fprintf(stdout, "buf: %s\n", buf);
    }    
    
    //send是用来送消息给socket，可以通过man send查看详情，送成功时返回送入字符的个数，失败返回-1，将错误代号送入errno
    if(send(client_sock, buf, strlen(buf), 0) < 0){
        fprintf(stderr, "send fail. data: %s,reason: %s\n", buf, strerror(errno));
	return -1;
    }
    return 0;
    
}

/****************************
 *  *说明：实现将html文件的内容按行
 *          读取并送给客户端
 *           ****************************/
void cat(int client_sock, FILE * resource){
    char buf[1024];

    fgets(buf, sizeof(buf), resource);   
    while(!feof(resource)){
        int len = write(client_sock, buf, strlen(buf));

        if(len < 0){
	    //发送body的过程中出现问题，怎么办？ 1.重试 2.
            fprintf(stderr, "send body error. reason: %s\n", strerror(errno));
	    break;
        }
        
        if(debug){
            fprintf(stdout, "%s", buf);
        }

        fgets(buf, sizeof(buf), resource);
    }
}

void do_http_response1(int client_sock){
    const char * main_header = "HTTP/1.0 200 OK\r\nServer: Martin Server\r\nContent-Type: text/html\r\nConnection: Close\r\n"; 
    //对于多行内容的字符串可以在每行最后加上"\"表示尚未完结，对于html中自带的双引号内容需要添加转义符
   
    const char * welcome_content = "\
<html lang=\"zh-CN\">\n\
<head>\n\
<meta content=\"text/html; charset=utf-8\" http-equiv=\"Content-Type\">\n\
<title>This is a test</title>\n\
</head>\n\
<body>\n\
<div align=center height=\"500px\" >\n\
<br/><br/><br/>\n\
<h2>大家好，欢迎来到xhc的个人主页 ！</h2><br/><br/>\n\
<form action=\"commit\" method=\"post\">\n\
尊姓大名: <input type=\"text\" name=\"name\" />\n\
<br/>芳龄几何: <input type=\"password\" name=\"age\" />\n\
<br/><br/><br/><input type=\"submit\" value=\"提交\" />\n\
<input type=\"reset\" value=\"重置\" />\n\
</form>\n\
</div>\n\
</body>\n\
</html>";

//1.送main_header
    int len = write(client_sock, main_header, strlen(main_header));
    
    if(debug){
        fprintf(stdout, "...do_http_response...\n");
    }
    if(debug){
        fprintf(stdout, "write[%d]: %s\n", len, main_header);
    }

//2.生成 Content-Length 行并发送
    char send_buf[64];
    int wc_len = strlen(welcome_content);
    len = snprintf(send_buf, 64, "Content-Length: %d\r\n\r\n", wc_len);
    len = write(client_sock, send_buf, len);   
    if(debug){
        fprintf(stdout, "write[%d]: %s", len, send_buf); 
    }
    len = write(client_sock, welcome_content, wc_len);   
    if(debug){
        fprintf(stdout, "write[%d]: %s", len, welcome_content); 
    }

//3.发送 html 文件内容
    
}

//返回值为-1表示读取出错，等于0表示读到空行，大于0表示成功读取一行
int get_line(int client_sock, char *buf, int size){
    int count = 0;
    int len = 0;
    char ch = '\0';
    
    while( (count < size - 1) && (ch != '\n') ){
	len = read(client_sock, &ch, 1);
	
	if( len == 1 ){
		if( ch == '\r' ){
		    continue;
		}else if( ch == '\n' ){
		     //buf[count] = '\0';
		     break;
		}else{
		     buf[count] = ch;
		     count++;
		}
			
 	}else if (len == -1){
		perror("read failed");
	  	count = -1;
		break;
	}else{
		//read返回0，客户端关闭sock连接
		fprintf(stderr, "client close.\n");
		count = -1;
		break;
	}	  
   }
   if(count >= 0){
      buf[count] = '\0';
   }
   return count;
}

void not_found(int client_sock){
    const char * reply = "HTTP/1.0 404 NOT FOUND\r\n\
Content-Type: text/html\r\n\
\r\n\
<HTML lang=\"zh-CN\">\r\n\
<meta content=\"text/html; charset=utf-8\" http-equiv=\"Content-Type\">\r\n\
<HEAD>\r\n\
<TITLE>NOT FOUND</TITLE>\r\n\
</HEAD>\r\n\
<BODY>\r\n\
	<P>文件不存在！\r\n\
    <P>The server could not fulfill your request because the resource specified is unavailable or nonexistent.\r\n\
</BODY>\r\n\
</HTML>";

    int len = write(client_sock, reply, strlen(reply));
    if(debug){
        fprintf(stdout, "reply");
    }
    
    if(len <= 0){
        fprintf(stderr, "send reply failed. reason: %s\n", strerror(errno));
    }

}

void unimplemented(int client_sock){
    const char * reply = "HTTP/1.0 501 Method Not Implemented\r\n\
Content-Type: text/html\r\n\
\r\n\
<HTML>\r\n\
<HEAD>\r\n\
<TITLE>Method Not Implemented</TITLE>\r\n\
</HEAD>\r\n\
<BODY>\r\n\
    <P>HTTP request method not supported.\r\n\
</BODY>\r\n\
</HTML>";

    int len = write(client_sock, reply, strlen(reply));
    if(debug){
        fprintf(stdout, "reply");
    }
    
    if(len <= 0){
        fprintf(stderr, "send reply failed. reason: %s\n", strerror(errno));
    }

}

void bad_request(int client_sock){
    const char * reply = "HTTP/1.0 400 BAD REQUEST\r\n\
Content-Type: text/html\r\n\
\r\n\
<HTML>\r\n\
<HEAD>\r\n\
<TITLE>BAD REQUEST</TITLE>\r\n\
</HEAD>\r\n\
<BODY>\r\n\
    <P>Your browser sent a bad request！\r\n\
</BODY>\r\n\
</HTML>";

    int len = write(client_sock, reply, strlen(reply));
    if(debug){
        fprintf(stdout, "reply");
    }
    
    if(len <= 0){
        fprintf(stderr, "send reply failed. reason: %s\n", strerror(errno));
    }

}

void inner_error(int client_sock){
    const char * reply = "HTTP/1.0 500 Internal Sever Error\r\n\
Content-Type: text/html\r\n\
\r\n\
<HTML lang=\"zh-CN\">\r\n\
<meta content=\"text/html; charset=utf-8\" http-equiv=\"Content-Type\">\r\n\
<HEAD>\r\n\
<TITLE>Inner Error</TITLE>\r\n\
</HEAD>\r\n\
<BODY>\r\n\
    <P>服务器内部出错.\r\n\
</BODY>\r\n\
</HTML>";

    int len = write(client_sock, reply, strlen(reply));
    if(debug) fprintf(stdout, reply);
	
    if(len <=0){
        fprintf(stderr, "send reply failed. reason: %s\n", strerror(errno));
    }
		
}
