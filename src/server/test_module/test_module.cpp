/**
 * @file test_module/test_module.cpp
 * @author zouyueming(da_ming at hotmail.com)
 * @date 2013/09/27
 * @version $Revision$ 
 * @brief   test module
 * Revision History
 *
 * @if  ID       Author       Date          Major Change       @endif
 *  ---------+------------+------------+------------------------------+
 *       1     zouyueming   2013-09-27    Created.
 **/
/* 
 *
 * Copyright (C) 2013-now da_ming at hotmail.com
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "gr_stdinc.h"
#include "grocket.h"

gr_server_t *   g_server    = NULL;
gr_i_server_t * g_funcs     = NULL;

extern "C"
{

struct conn
{
    int check_number;
};

void gr_version(
    int *               gr_server_version
)
{
    * gr_server_version = GR_SERVER_VERSION;
}

int gr_init(
    gr_process_type_t   proc_type,
    gr_server_t *       server
)
{
    g_server    = server;
    g_funcs     = g_server->library->buildin;

    // 主进程初始化、子进程初始化、所有线程初始化时都会调用本函数。
    // 那如何区分呢？答案是proc_type参数，它的取值会有如下几种：
    // 1、GR_PROCESS_MASTER 则表示是主进程, 适用于主进程初始化资源, 子进程使用资源的场景
    // 2、GR_PROCESS_CHILD 正常的工作进程
    // 3、GR_PROCESS_THREAD_1 表示工作进程中的第一个工作线程初始化
    // 4、GR_PROCESS_THREAD_1 + n 表示工作进程中的第 n 个工作线程初始化


    // 前面说：“写服务器模块唯一需要包含的文件，不需要链接任何库”，那服务器框架导出函数的实现
    // 在哪里？答案是：在服务器框架进程里，它在gr_init函数中通过gr_server_t *接口以函数指针的
    // 方式暴露给用户模块，所以用户模块当然要把这个指针保存起来。这行算行数。

    {
        int n;
        gr_i_server_t * o = g_funcs;

        // 调用它的config_get_int从服务器配置文件读取[server]段的tcp.accept.concurrent的值
        n = o->config_get_int( o, "server", "tcp.accept.concurrent", 0 );
        printf( "I guess, [server]tcp.accept.concurrent = %d, is it right? haha!!!!\n", n );

        // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        // 这就是这套服务器框架的一个特色，它提供类功能库给你用，还不要求你连接任何库。
        // 还支持你实现自己的功能函数库，以动态库的形式挂在服务器框架中供其它人使用，
        // 当然使用者依然不需要连接任何库！！
        // 当功能实现修改、更新、增加新函数都不会需要你的程序重新连接。
    }

    // 如果你很好奇，可以：
    // 1、通过server->argc和server_argv取得命令行参数
    // 2、通过server->worker_count取得worker数量
    // 3、通过server->ports_count取得监听端口数量
    // 4、通过server->ports取得每一个端口的配置
    // 如下代码是示例

    if ( GR_PROCESS_MASTER == proc_type || GR_PROCESS_CHILD == proc_type ) {
        int i;
        char buf[ 1024 ];
        char * log = (char *)malloc( 1024 * 1024 );
        if ( NULL != log ) {

            char pt[ 32 ] = "";

            * log = '\0';

            if ( GR_PROCESS_MASTER == proc_type ) {
                strcpy( pt, "MASTER" );
            } else {
                strcpy( pt, "CHILD" );
            }
            sprintf( buf, "[pid=%d] gr_init called, proc_type = %d(%s)\n", getpid(), (int)proc_type, pt );
            strcat( log, buf );

            // argc, argv
            strcat( log, "        args = " );
            for ( i = 0; i < server->argc; ++ i ) {
                if ( 0 != i ) {
                    strcat( log, " " );
                }
                strcat( log, server->argv[ i ] );
            }
            strcat( log, "\n" );
            // worker_count
            sprintf( buf, "        worker_count = %d\n", server->worker_count );
            strcat( log, buf );
            // port
            sprintf( buf, "        ports, count = %d:\n", server->ports_count );
            strcat( log, buf );

            // 不白看这儿，长经验呢！inet_ntoa在Windows下能转出0.0.0.0的地址，在linux下会core
            for ( i = 0; i < server->ports_count; ++ i ) {
                gr_port_item_t * item = & server->ports[ i ];
                sprintf( buf, "            %s port = %d, addr_len = %d, addr = %s:%d, fd = %d\n",
                    item->is_tcp ? "TCP" : "UDP",
                    item->port,
                    (int)item->addr_len,
                    (const char*)(( 0 != item->addr4.sin_addr.s_addr ) ? inet_ntoa(item->addr4.sin_addr) : "(empty)"),
                    (int)ntohs(item->addr4.sin_port),
                    item->fd );
                strcat( log, buf );
            }
            printf( "%s", log );
            free( log );
        } else {
            printf( "搞什么啊？分配内存还能出错？！\n" );
        }
    } else if ( proc_type >= GR_PROCESS_THREAD_1 ) {
        printf( "[pid=%d] gr_init called, proc_type = %d(WORKER + %d)\n",
            getpid(), (int)proc_type, proc_type - GR_PROCESS_THREAD_1 );
    } else {
        assert( 0 );
    }
    fflush(stdout);

    // 如果返回0，则服务器会继续初始化，否则服务器会退出。

    return 0;
}

void gr_term(
    gr_process_type_t   proc_type,
    void *              must_be_zero
)
{
    // 和gr_init一样，proc_type参数告诉我们是哪个线程或者哪个进程在反初始化

    printf( "[pid=%d] gr_term called, proc_type = %d\n", getpid(), (int)proc_type );fflush(stdout);
}

void gr_tcp_accept(
    int                 port,
    int                 sock,
    gr_conn_buddy_t *   conn_buddy,
    bool *              need_disconnect
)
{
    struct conn * cn;

    printf( "%d port accepted socket %d\n", port, sock );

    cn = (struct conn *)g_funcs->memory_alloc( g_funcs, sizeof( struct conn ) );
    if ( NULL == cn ) {
        * need_disconnect = true;
        return;
    }
    cn->check_number = 0;
    g_funcs->log( g_funcs, __FILE__, __LINE__, __FUNCTION__, GR_LOG_INFO,
        "[fd=%d[cn=%p][n=%d]]", sock, cn, cn->check_number );
    assert( 0 == cn->check_number );

    conn_buddy->ptr = cn;
}

void gr_tcp_close(
	int                 port,
    int                 sock,
    gr_conn_buddy_t *   conn_buddy
)
{
    if ( conn_buddy->ptr ) {
        struct conn * cn = (struct conn *)conn_buddy->ptr;
        g_funcs->log( g_funcs, __FILE__, __LINE__, __FUNCTION__, GR_LOG_INFO,
            "[fd=%d][cn=%p][n=%d]", sock, cn, cn->check_number );
        g_funcs->memory_free( g_funcs, cn );
        conn_buddy->ptr = NULL;
    }

    printf( "%d port will close socket %d\n", port, sock );
}

void gr_check(
    void *              data,
    int                 len,
    gr_check_ctxt_t *   ctxt,
    gr_conn_buddy_t *   conn_buddy,
    bool *              is_error,
    bool *              is_full
)
{
    // 看着参数挺多的，但听我解释一下：
    // 1、data与len就是你还没决定的数据包。框架保证data不会为NULL，len一定>=0
    // 2、port_info用于取得服务这个数据包端口信息，它有如下几个字段：
    //    1) 服务于这个数据包的监听地址，为什么用这个而不是sockaddr_in? 支持IPV6啊老大。
    //       struct sockaddr *           addr;
    //       socklen_t                   addr_len;
    //
    //    2) 服务于这个数据包的监听端口的SOCKET描述符。注意TCP的监听端口和通信SOCKET不是一个。
    //       int                         fd;
    // 
    //    3) 服务于这个数据包的监听端口号
    //       uint16_t                    port;
    //
    //    4) 服务于这个数据包的监听端口是TCP还是UDP。
    //       看到这儿亮了吧？你只需要管协议，支持TCP和UDP的事儿交给服务器框架。
    //       bool                        is_tcp;
    // 3、sock就是当前连接的描述符。对于UDP，该描述符和port_info->fd是相同的。
    // 4、ctxt存储一些额外的上下文信息，其中包括了对HTTP数据包的解析中间结果，但那都不是
    //    服务模块编写者需要关心的。真正需要填写的是如下两个字段。
    //
    //     1) 数据包类型
    //        uint16_t                   cc_package_type;
    //
    //            cc_package_type可以填写的值如下列出：
    //            typedef enum
    //            {
    //                // HTTP 请求包
    //                GR_PACKAGE_HTTP_REQ         = 1,
    //                // HTTP 回复包
    //                GR_PACKAGE_HTTP_REPLY       = 2,
    //                // 用户自定义数据包
    //                GR_PACKAGE_PRIVATE          = 3,
    //                // 使用3bit存储该数值，所以最大可用的值为7，
    //                // 也就是说服务器端模块最多可以在一个模块中支持5种完全不同的自定义协议。
    //            } gr_package_type_t;
    //
    //            由于HTTP协议是服务器框架内置支持，所以用户模块永远都不必填写GR_PACKAGE_HTTP_REQ和
    //            GR_PACKAGE_HTTP_REPLY。只有可能填写的就是GR_PACKAGE_PRIVATE，表示它是一个用户数据包，
    //            如果在一个程序里支持多个协议，则还可以用GR_PACKAGE_PRIVATE + 1 到 GR_PACKAGE_PRIVATE + 4。
    //
    //     2) 数据包完整长度
    //        uint32_t                   cc_package_length;
    //            如果在解析数据包的过程中发现它是一个有效数据包，那就必须在这个字段里填写完整数据包的长度。
    //            因为TCP是流协议，这个方法帮助服务器框架切分数据包。UDP数据包也会通过该函数的调用来确定是否有效。
    // 5、is_error。如果在解析数据包过程中发现这是个无效包，则直接*is_err = true即可，否则不用动这个字段。
    // 6、is_full。如果is_error值为false，也就是说这是个有效包，本字段才会被服务器框架判断。如果发现数据区
    //             中至少包括了一包完整数据包，则*is_full = true。这时服务器框架才会切出一个包把它扔给工作进程(或线程)。

    // 为了演示包没收全的处理，我这里假设包每过两字节收一次。

    if ( len < 2 ) {
        // 初步判断为有效的用户私有包
        ctxt->cc_package_type = GR_PACKAGE_PRIVATE;
        // 没出错
        *is_error = false;
        // 但不是完整包，让服务器框架继续收
        *is_full = false;
        return;
    }

    // 有效的用户私有包
    ctxt->cc_package_type = GR_PACKAGE_PRIVATE;
    // 没出错
    *is_error = false;
    // 是完整包
    *is_full = true;
    // 必须填写完整包长度

    ctxt->cc_package_length = len;
    // 看，实际上我们只用了data和len两个参数判断数据包，给了那么多参数，
    // 实际是为了方便模块编写者根据不同的场景、环境做不同的处理。

}

void gr_proc(
    const char *        data,
    int                 len,
    gr_proc_ctxt_t *    ctxt,
    gr_conn_buddy_t *   conn_buddy,
    int *               processed_len
)
{
    struct conn *   cn = (struct conn *)conn_buddy->ptr;
    gr_i_server_t * o = g_funcs;
    char *          rsp;
    int             n;

    assert( cn );
    //o->log( o, __FILE__, __LINE__, __FUNCTION__, GR_LOG_INFO, "[cn=%p][n=%d]", cn, cn->check_number );

    if ( len <= 1 ) {
        o->log( o, __FILE__, __LINE__, __FUNCTION__, GR_LOG_ERROR,
            "invalid len %d", len );
        * processed_len = -1;
        return;
    }

    if ( data[0] <= 0 || ! isdigit( data[0] ) ) {
        o->log( o, __FILE__, __LINE__, __FUNCTION__, GR_LOG_ERROR,
            "invalid data=\"%s\", len=%d", data, len );
        * processed_len = -1;
        return;
    }
    n = atoi( data );
    if ( n <= 0 ) {
        o->log( o, __FILE__, __LINE__, __FUNCTION__, GR_LOG_ERROR,
            "invalid data=\"%s\", len=%d", data, len );
        * processed_len = -1;
        return;
    }

    if ( n != cn->check_number + 1 ) {
        o->log( o, __FILE__, __LINE__, __FUNCTION__, GR_LOG_ERROR,
            "invalid data=\"%s\", len=%d, last=%d, now=%d", data, len, cn->check_number, n );
        * processed_len = -1;
        return;
    }

    //printf( "process %d byte user data\n", len );

    // 确认服务器框架填充的默认值
    assert( * processed_len == len );

    // 设置最大返回值字节数。该函数内部在必要时会分配内存，
    // 填充pc_result_buf和pc_result_buf_max字段，并将 * pc_result_buf_len 置为0
    rsp = (char *)o->set_max_response( o, ctxt, len );
    if ( NULL == rsp ) {
        // 服务器框架提供了个打日志功能
        o->log( o, __FILE__, __LINE__, __FUNCTION__, GR_LOG_ERROR,
            "set_max_response %d failed", len );
        * processed_len = -1;
        return;
    }

    // 拷贝数据
    memcpy( rsp, data, len );

    // 记录实际写入的数据字节数
    ctxt->pc_result_buf_len = len;

    cn->check_number = n;
}

void gr_proc_http(
    gr_http_ctxt_t *    http,
    gr_conn_buddy_t *   conn_buddy,
    int *               processed_len
)
{
    // 是的，你没猜错，当服务器收到了一个HTTP数据包时，就会调用该函数。
    // 为了不被累死，我简单贴一下gr_http_ctxt_t 的成员变量，你可以根据
    // 变量名来猜出它的作用和使用方法：
    /*
    // uint16_t                 hc_is_tcp( bit field )
    // uint16_t                 hc_package_type( bit field )
    // int                      hc_port
    // int                      hc_fd
    // int                      hc_thread_id
    // char *                   hc_result_buf
    // int                      hc_result_buf_max
    // int *                    hc_result_buf_len
    // bool                     hc_is_error
    // bool                     hc_keep_alive
    //char                      request_type;
    //char *                    version;
    //char *                    directory;
    //char *                    object;
    //char *                    content_type;
    //char *                    user_agent;
    //gr_http_param_t *         params;
    //size_t                    params_count;
    //gr_http_param_t *         header;
    //size_t                    header_count;
    //gr_http_param_t *         form;
    //size_t                    form_count;
    //char *                    body;
    //size_t                    body_len;
    //int                       http_reply_code;

    typedef struct
    {
        // 参数名
        char *                      name;
        // 参数值
        char *                      value;
    } gr_http_param_t;
    */

    // 通过服务器框架提供的函数，你可以方便的生成一个HTTP返回包并写入http->hc_result_buf中
    // 但为了简单，这个过程暂时忽略了。
    const char * dir;
    int i;

    // 以下代码简单打印出调用方访问的请求内容。
    // 从URL,QueryString,Headers,Form, HttpBody都有。
    dir = http->hc_directory;
    if ( 0 == strcmp( "/", dir ) )
        dir = "";
    printf( "HTTP port %d,ThreadID=%d,SOCKET=%d,%s/%s",
            http->hc_port, http->hc_thread_id, http->hc_fd,
            dir, http->hc_object );
    for ( i = 0; i != http->hc_params_count; ++ i ) {
        if ( 0 == i ) {
            printf( "%s", "?" );
        } else {
            printf( "%s", "&" );
        }

        printf( "%s=%s", http->hc_params[ i ].name, http->hc_params[ i ].value );
    }
    printf( "\n" );
    // HTTP Headers
    for ( i = 0; i != http->hc_header_count; ++ i ) {
        printf( "%s : %s\n", http->hc_header[ i ].name, http->hc_header[ i ].value );
    }
    printf( "\n" );
    // 数据区
    printf( "%s\n", http->hc_body );

    {
        char buf[] = "hello world";
        g_funcs->http_send( g_funcs, http, buf, sizeof( buf ) - 1, "text/plain" );
    }
}

} // extern "C"
