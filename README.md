# minihttp
 实现一个http 服务器项目，服务器启动后监听80端口的tcp 连接，当用户通过任意一款浏览器（IE、火狐和腾讯浏览器等）访问我们的http服务器，http服务器会查找用户访问的html页面是否存在，如果存在则通过http 协议响应客户端的请求，把页面返回给浏览器，浏览器显示html页面；如果页面不存在，则按照http 协议的规定，通知浏览器此页面不存在（404 NOT FOUND），并且具有并发功能，支持多用户同时访问。

一、需求分析：

1、何为html页面

   html，全称Hypertext Markup Language，也就是“超文本链接标示语言”。HTML文本是由 HTML命令组成的描述性文本，HTML 命令可以说明文字、 图形、动画、声音、表格、链接等。 即平常上网所看到的的网页。
   
2、何为http协议

HTTP协议是Hyper Text Transfer Protocol(超文本传输协议)的缩写,是用于从万维网(WWW:World Wide Web )服务器传输超文本到本地浏览器的传送协议。

请求格式：

客户端请求：客户端发送一个HTTP请求到服务器的请求消息包括以下格式：请求行（request line）、请求头部（header）、空行和请求数据四个部分组成，下图给出了请求报文的一般格式。

![image](https://user-images.githubusercontent.com/99958269/174622225-095589cc-1912-4d42-9ef0-273a3d659d63.png)

服务端响应：服务器响应客户端的HTTP响应也由四个部分组成，分别是：状态行、消息报头、空行和响应正文。

![image](https://user-images.githubusercontent.com/99958269/174622681-df27fc5f-a051-44a0-b117-41fc6def46ca.png)

几种响应：

![image](https://user-images.githubusercontent.com/99958269/174622949-fa91168f-d9de-4fe3-9ac9-28871fc07a46.png)

二、实现mini型http服务器

1、接收http请求

按行接受请求，主要函数int get_line(int sock, char * buf, int size);

2、解析请求

从请求行开始一行行解析，主要函数void* do_http_request(void* pclient_sock)

3、响应http请求

主要函数void do_http_response(int client_sock, const char *path)；

先用headers函数发送头部，然后通过cat函数发送html文件内容

三、并发

通俗的并发通常是指同时能并行的处理多个任务。

主要是通过pthread_create函数来实现创建一个新线程，并行的执行任务。
