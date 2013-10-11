/**
 * @file libgrocket/gr_tcp_accept.c
 * @author zouyueming(da_ming at hotmail.com)
 * @date 2013/10/03
 * @version $Revision$ 
 * @brief   TCP Accept�߳�
 * Revision History ���¼���
 *
 * @if  ID       Author       Date          Major Change       @endif
 *  ---------+------------+------------+------------------------------+
 *       1     zouyueming   2013-10-03    Created.
 **/
#include "gr_tcp_accept.h"
#include "gr_thread.h"
#include "gr_log.h"
#include "gr_global.h"
#include "gr_errno.h"
#include "gr_tools.h"
#include "gr_mem.h"
#include "gr_config.h"
#include "gr_poll.h"
#include "gr_module.h"
#include "gr_socket.h"
#include "gr_conn.h"
#include "gr_tcp_in.h"

// gr_tcp_accept_t �Ѿ����ⲿ�ӿڻص�����������, ��ֻ�ܻ�һ������ gr_accept_t
typedef struct
{
    gr_threads_t    threads;

    gr_poll_t *     poll;

    int             concurrent;

} gr_accept_t;

///////////////////////////////////////////////////////////////////////
//
// TCP accept
//

/* static
int gr_tcp_accept_add_conn(
    gr_tcp_conn_item_t *    conn
); */

static inline
void on_tcp_accept(
    gr_accept_t *           self,
    gr_thread_t *           thread,
    gr_port_item_t *        port_item
)
{
    int                     fd;
    gr_tcp_conn_item_t *    conn;
    int                     r;
    union {
        struct sockaddr_in6 v6;
        struct sockaddr_in  v4;
        struct sockaddr     u;
    } addr;
    int                     addr_len;
    
    while ( true ) {

        // accept
        addr_len = (int)sizeof( addr.v6 );
        fd = gr_poll_accept(
            self->poll,
            thread,
            port_item->fd,
            & addr.u, & addr_len );       // ����׶��Ҹ�������Ҫ�Է���ַ����˭˭
        if ( fd < 0 ) {
            if ( EAGAIN == errno ) {
                return;
            }

            gr_error( "gr_poll_accept failed: %d,%s", errno, strerror( errno ) );
            return;
        }

        gr_debug( "tcp_in accept( %d ) return fd %d, addr=%s:%d",
            port_item->fd, fd, inet_ntoa(addr.v4.sin_addr), ntohs(addr.v4.sin_port) );

        // ����첽socket
        if ( ! gr_socket_set_block( fd, false ) ) {
            gr_error( "gr_socket_set_block failed: %d", get_errno() );
            gr_socket_close( fd );
            continue;
        }

        // ��ģ��ϲ��ϲ���������
        if ( ! gr_module_on_tcp_accept( port_item, fd ) ) {
            // ��ϲ�����ص�������
            gr_error( "gr_module_on_tcp_accept reject" );
            gr_socket_close( fd );
            continue;
        }
 
        // �������Ӷ���
        conn = gr_tcp_conn_alloc( port_item, fd );
        if ( NULL == conn ) {
            gr_error( "gr_conn_alloc_tcp failed" );
            gr_socket_close( fd );
            continue;
        }

        // ����socket�ӵ�tcp_in��
        r = gr_tcp_in_add_conn( conn );
        if ( 0 != r ) {
            gr_fatal( "gr_tcp_accept_add_conn return %d", r );
            gr_tcp_conn_free( conn );
            continue;
        }
    }
}

void tcp_accept_worker( gr_thread_t * thread )
{
#define     TCP_ACCEPT_WAIT_TIMEOUT    100
    int                     count;
    int                     i;
    gr_accept_t *           self;
    gr_poll_event_t *       events;
    gr_poll_event_t *       e;
    gr_server_t *           server;
    gr_port_item_t *        port_item;
    //gr_tcp_conn_item_t *    conn;

    server = & g_ghost_rocket_global.server_interface;
    self    = (gr_accept_t *)thread->param;

    events  = (gr_poll_event_t *)gr_malloc( sizeof( gr_poll_event_t ) * self->concurrent );
    if ( NULL == events ) {
        gr_fatal( "bad_alloc %d", (int)sizeof( gr_poll_event_t ) * self->concurrent );
        return;
    }

    while ( ! thread->is_need_exit ) {

        count = gr_poll_wait( self->poll, events, self->concurrent, TCP_ACCEPT_WAIT_TIMEOUT, thread );
        if ( count < 0 ) {
            gr_fatal( "gr_poll_wait return %d", count );
            continue;
        }

        for ( i = 0; i < count; ++ i ) {
            e = & events[ i ];
            
            if (   (gr_port_item_t*)e->data.ptr >= & server->ports[ 0 ]
                && (gr_port_item_t*)e->data.ptr <= & server->ports[ server->ports_count - 1 ] )
            {
                // TCP������
                port_item = (gr_port_item_t *)e->data.ptr;
                on_tcp_accept( self, thread, port_item );
            } else {
                // ��������
            }
        }
    };

    gr_free( events );
}

int gr_tcp_accept_init()
{
    gr_accept_t *  p;
    int thread_count = gr_config_tcp_accept_thread_count();
    int r;

    if ( thread_count < 1 ) {
        gr_fatal( "[init]tcp_accept thread_count invalid" );
        return GR_ERR_INVALID_PARAMS;
    }

    if ( NULL != g_ghost_rocket_global.tcp_accept ) {
        gr_fatal( "[init]gr_tcp_accept_init already called" );
        return GR_ERR_WRONG_CALL_ORDER;
    }

    p = (gr_accept_t *)gr_calloc( 1, sizeof( gr_accept_t ) );
    if ( NULL == p ) {
        gr_fatal( "[init]malloc %d bytes failed, errno=%d,%s",
            (int)sizeof(gr_accept_t), errno, strerror( errno ) );
        return GR_ERR_BAD_ALLOC;
    }

    p->concurrent = gr_config_tcp_accept_concurrent();

    r = GR_OK;

    do {

        const char * name = "tcp.listen";

        p->poll = gr_poll_create( p->concurrent, thread_count, GR_POLLIN, name );
        if ( NULL == p->poll ) {
            r = GR_ERR_INIT_POLL_FALED;
            break;
        }

        r = gr_threads_start(
            & p->threads,
            thread_count,
            NULL,
            tcp_accept_worker,
            p,
            gr_poll_raw_buff_for_accept_len(),
            true,
            name );
        if ( GR_OK != r ) {
            break;
        }

        gr_debug( "tcp_accept_init OK" );

        r = GR_OK;
    } while ( false );

    if ( GR_OK != r ) {

        gr_threads_close( & p->threads );

        if ( NULL != p->poll ) {
            gr_poll_destroy( p->poll );
            p->poll = NULL;
        }

        gr_free( p );
        return r;
    }

    g_ghost_rocket_global.tcp_accept = p;
    return GR_OK;
}

void gr_tcp_accept_term()
{
    gr_accept_t *  p = (gr_accept_t *)g_ghost_rocket_global.tcp_accept;
    if ( NULL != p ) {

        gr_threads_close( & p->threads );

        // �߳�ͣ�˾Ϳ��԰�ȫ�ֱ��������
        g_ghost_rocket_global.tcp_accept = NULL;

        if ( NULL != p->poll ) {
            gr_poll_destroy( p->poll );
            p->poll = NULL;
        }

        gr_free( p );
    }
}

int gr_tcp_accept_add_listen_ports()
{
    int             i;
    int             r               = 0;
    gr_server_t *   server_interface= & g_ghost_rocket_global.server_interface;
    gr_accept_t *   self            = (gr_accept_t *)g_ghost_rocket_global.tcp_accept;

    if ( NULL == self ) {
        gr_fatal( "[init]tcp_accept is NULL" );
        return -1;
    }

    for ( i = 0; i < server_interface->ports_count; ++ i ) {
        gr_port_item_t * item = & server_interface->ports[ i ];

        if ( item->is_tcp ) {
            r = gr_poll_add_listen_fd( self->poll, item->is_tcp, item->fd, item, & self->threads );
            if ( 0 != r ) {
                gr_fatal( "[init]gr_poll_add_listen_fd return %d", r );
                return -2;
            }

            gr_info( "start listen TCP port %d", item->port );
        }
    }

    return 0;
}

/* static inline
int gr_tcp_accept_add_conn(
    gr_tcp_conn_item_t *    conn
)
{
    int             r;
    gr_accept_t *   self;
    
    self = (gr_accept_t *)g_ghost_rocket_global.tcp_accept;
    if ( NULL == self ) {
        gr_fatal( "gr_tcp_accept_init never call" );
        return -1;
    }

    // ����socket�ӵ�poll��
    r = gr_poll_add_tcp_recv_fd(
        self->poll,
        conn,
        & self->threads
    );
    if ( 0 != r ) {
        gr_fatal( "gr_poll_add_tcp_recv_fd return %d", r );
        return -2;
    }

    gr_debug( "ok" );
    return 0;
} */