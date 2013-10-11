/**
 * @file libgrocket/gr_worker.c
 * @author zouyueming(da_ming at hotmail.com)
 * @date 2013/10/05
 * @version $Revision$ 
 * @brief   �����̻߳�������
 * Revision History ���¼���
 *
 * @if  ID       Author       Date          Major Change       @endif
 *  ---------+------------+------------+------------------------------+
 *       1     zouyueming   2013-10-05    Created.
 **/
#include "gr_worker.h"
#include "gr_thread.h"
#include "gr_log.h"
#include "gr_global.h"
#include "gr_errno.h"
#include "gr_tools.h"
#include "gr_mem.h"
#include "gr_config.h"
#include "gr_module.h"
#include "gr_queue.h"
#include "gr_conn.h"
#include "gr_tcp_out.h"
#include "gr_udp_out.h"

struct gr_worker_t;
typedef struct gr_worker_t      gr_worker_t;
struct gr_worker_item_t;
typedef struct gr_worker_item_t gr_worker_item_t;

typedef struct
{
    gr_proc_ctxt_t  bin_ctxt;
    gr_proc_http_t  http_ctxt;
} per_thread_t;

struct gr_worker_t
{
    gr_threads_t        threads;

    gr_worker_item_t *  items;
};

struct gr_worker_item_t
{
    gr_queue_t *        queue;
};

static
void worker_free_queue_item( void * param, gr_queue_item_t * p )
{
    gr_queue_item_compact_t *   queue_item = (gr_queue_item_compact_t *)p;

    if ( queue_item->is_tcp ) {
        gr_tcp_req_free( (gr_tcp_req_t *)queue_item );
    } else {
        gr_udp_req_free( (gr_udp_req_t *)queue_item );
    }
}

static inline
void process_tcp(
    gr_worker_t *   self,
    gr_thread_t *   thread,
    gr_tcp_req_t *  req
)
{
    per_thread_t *      per_thread      = (per_thread_t *)thread->cookie;
    gr_proc_ctxt_t *    ctxt            = & per_thread->bin_ctxt;
    // Ĭ���Ѵ����ֽ����������ֽ���
    int                 processed_len   = req->buf_len;
    // ��¼һ��ԭʼ�������
    char *              req_buf         = req->buf;
    int                 req_buf_max     = req->buf_max;
    int                 req_buf_len     = req->buf_len;
    int     r;

    ctxt->check_ctxt    = & req->check_ctxt;
    ctxt->port          = req->parent->port_item->port;
    ctxt->fd            = req->parent->fd;
    ctxt->thread_id     = thread->id;

    gr_module_proc_tcp( req, ctxt, & processed_len );

    if ( ctxt->pc_result_buf_len <= 0 ) {
        // û�лظ����ݰ�
        return;
    }

    // �лظ����ݰ�

    // �������ü���
    r = gr_tcp_req_add_refs( req );
    if ( 0 != r ) {
        gr_fatal( "gr_tcp_req_add_refs faiiled" );
        return;
    }

    // �����ذ�Ӧ�õ�req�У����req���������rsp
    gr_tcp_req_set_buf( req, ctxt->pc_result_buf, ctxt->pc_result_buf_max, ctxt->pc_result_buf_len );
    ctxt->pc_result_buf        = NULL;
    ctxt->pc_result_buf_max    = 0;
    ctxt->pc_result_buf_len    = 0;

    // ��req���rsp
    gr_tcp_req_to_rsp( req );

    // ��rsp�ӵ����ӵĶ�����
    gr_tcp_conn_add_rsp( req->parent, req );

    // ��rsp����tcp_out�з���
    r = gr_tcp_out_add( req );
    if ( 0 != r ) {
        gr_fatal( "gr_tcp_out_add faiiled" );
        return;
    }
}

static inline
void process_udp(
    gr_worker_t *   self,
    gr_thread_t *   thread,
    gr_udp_req_t *  req
)
{
    // echo
    gr_udp_out_add( req );
}

static inline
int hash_worker_tcp( gr_tcp_req_t * req, gr_worker_t * self )
{
    // TCP����SOCKET��������HASH
    return req->parent->fd % self->threads.thread_count;
}

static inline
int hash_worker_udp( gr_udp_req_t * req, gr_worker_t * self )
{
    // UDP, ���ͻ���IP��HASH
    if ( AF_INET == req->addr.sa_family ) {
        // IPV4
        return req->addr_v4.sin_addr.s_addr % self->threads.thread_count;
    } else if ( AF_INET6 == req->addr.sa_family ) {
        // IPV6
        //TODO: ������Ҫ�Ż�һ��
        const unsigned char *   p = (const unsigned char *)& req->addr_v6.sin6_addr;
        int                     n = 0;
        size_t                  i;
        for ( i = 0; i < sizeof( req->addr_v6.sin6_addr ); ++ i ) {
            n = n * 13 + p[ i ];
        }
        return abs( n ) % self->threads.thread_count;
    }

    return 0;
}

static inline
int gr_worker_add(
    gr_queue_item_compact_t *   queue_item,
    bool                        is_emerge
)
{
    int             hash_id;
    int             r;
    gr_worker_t *   self;
    
    self = (gr_worker_t *)g_ghost_rocket_global.worker;
    if ( NULL == self ) {
        return -1;
    }

    if ( queue_item->is_tcp ) {
        gr_tcp_req_t *  req     = (gr_tcp_req_t *)queue_item;
        hash_id = hash_worker_tcp( req, self );
    } else {
        gr_udp_req_t *  req = (gr_udp_req_t *)queue_item;
        hash_id = hash_worker_udp( req, self );
    }

    r = gr_queue_push( self->items[ hash_id ].queue, (gr_queue_item_t *)queue_item, is_emerge );
    if ( 0 != r ) {
        gr_fatal( "gr_queue_push return %d", r );
        return -2;
    }

    return 0;
}

static inline
void process_item(
    gr_worker_t *       self,
    gr_thread_t *       thread,
    gr_queue_item_t *   queue_item
)
{
    gr_queue_item_compact_t *   queue_item2 = (gr_queue_item_compact_t *)queue_item;

    if ( queue_item2->is_tcp ) {
        process_tcp( self, thread, (gr_tcp_req_t *)queue_item2 );
    } else {
        process_udp( self, thread, (gr_udp_req_t *)queue_item2 );
    }
}

static
void worker_init_routine( gr_thread_t * thread )
{
    gr_module_worker_init( thread->id );
}

static
void worker_routine( gr_thread_t * thread )
{
#define     WORK_WAIT_TIMEOUT    100
    gr_worker_t *       self        = (gr_worker_t *)thread->param;
    gr_worker_item_t *  item        = & self->items[ thread->id ];
    gr_queue_t *        queue       = item->queue;
    gr_queue_item_t *   queue_item;
    bool                b;

    while ( ! thread->is_need_exit ) {

        queue_item = gr_queue_top( queue, WORK_WAIT_TIMEOUT );
        if ( NULL == queue_item ) {
            continue;
        }

        process_item( self, thread, queue_item );

        b = gr_queue_pop_top( queue, queue_item );
        if ( ! b ) {
            gr_fatal( "gr_queue_pop_top failed!" );
            break;
        }
    };

    gr_module_worker_term( thread->id );
}

int gr_worker_init()
{
    gr_worker_t *  p;
    int thread_count = gr_config_worker_thread_count();
    int r;
    int i;

    if ( thread_count < 1 ) {
        gr_fatal( "[init]gr_worker_init thread_count invalid" );
        return GR_ERR_INVALID_PARAMS;
    }

    if ( NULL != g_ghost_rocket_global.worker ) {
        gr_fatal( "[init]gr_work_init already called" );
        return GR_ERR_WRONG_CALL_ORDER;
    }

    p = (gr_worker_t *)gr_calloc( 1, sizeof( gr_worker_t ) );
    if ( NULL == p ) {
        gr_fatal( "[init]malloc %d bytes failed, errno=%d,%s",
            (int)sizeof(gr_worker_t), errno, strerror( errno ) );
        return GR_ERR_BAD_ALLOC;
    }

    r = GR_ERR_UNKNOWN;

    do {

        const char * name = "svr.worker";

        p->items = (gr_worker_item_t *)gr_calloc( 1,
            sizeof( gr_worker_item_t ) * thread_count );
        if ( NULL == p->items ) {
            gr_fatal( "gr_calloc %d bytes failed: %d",
                (int)sizeof( gr_worker_item_t ) * thread_count,
                get_errno() );
            r = GR_ERR_BAD_ALLOC;
            break;
        }

        for ( i = 0; i < thread_count; ++ i ) {
            p->items[ i ].queue = gr_queue_create( worker_free_queue_item, & p->items[ i ] );
            if ( NULL == p->items[ i ].queue ) {
                gr_fatal( "gr_queue_create failed" );
                break;
            }
        }
        if ( i < thread_count ) {
            gr_fatal( "i %d < thread_count %d", i, thread_count );
            break;
        }

        r = gr_threads_start(
            & p->threads,
            thread_count,
            worker_init_routine,
            worker_routine,
            p,
            sizeof( per_thread_t ),
            true,
            name );
        if ( GR_OK != r ) {
            gr_fatal( "gr_threads_start return error %d", r );
            break;
        }

        gr_debug( "worker_init OK" );

        r = GR_OK;
    } while ( false );

    if ( GR_OK != r ) {

        gr_threads_close( & p->threads );

        for ( i = 0; i < thread_count; ++ i ) {
            if ( NULL != p->items[ i ].queue ) {
                gr_queue_destroy( p->items[ i ].queue );
                p->items[ i ].queue = NULL;
            }
        }

        if ( NULL != p->items ) {
            gr_free( p->items );
            p->items = NULL;
        }

        gr_free( p );
        return r;
    }

    g_ghost_rocket_global.worker = p;
    return GR_OK;
}

void gr_worker_term()
{
    gr_worker_t *  p = (gr_worker_t *)g_ghost_rocket_global.worker;
    if ( NULL != p ) {

        int i;

        gr_threads_close( & p->threads );

        for ( i = 0; i < p->threads.thread_count; ++ i ) {
            if ( NULL != p->items[ i ].queue ) {
                gr_queue_destroy( p->items[ i ].queue );
                p->items[ i ].queue = NULL;
            }
        }

        if ( NULL != p->items ) {
            gr_free( p->items );
            p->items = NULL;
        }

        g_ghost_rocket_global.worker = NULL;
        gr_free( p );
    }
}

int gr_worker_add_tcp(
    gr_tcp_req_t *  req,
    bool            is_emerge
)
{
    int                         r;
    int                         package_len;
    int                         left_len;
    gr_queue_item_compact_t *   queue_item;
    gr_tcp_req_t *              new_req         = NULL;
    
    queue_item = (gr_queue_item_compact_t *)req;
    if ( true != queue_item->is_tcp ) {
        return -1;
    }

    package_len = gr_tcp_req_package_length( req );
    left_len    = req->buf_len - package_len;
    if ( left_len > 0 ) {

        // pipe line ����֧��

        // ���������Ƕ�����ݣ�ʣ������ݲ��ܷ��ڵ�ǰ�����У�Ҫ�Ż����Ӷ���
        // ��������Կ��������ģ��֧�ֶ������ͬʱ������������жϰ�����ʱ�ж϶������
        // ��ʣ�����İ��������С�����������Ŀ�������С��
        // ���뽫 req �Ӹ� worker�����ܽ��·���� new_req �Ӹ� worker ��Ϊ�ϵ� req �кܶ�״̬
        // Ҫ���·����ٿ�����̫�����㡣

        // �������µ�������󣬽�ʣ�����ݿ������·������������С�������ʣ����ֽ�����С��Ƚϻ��㡣
        new_req = gr_tcp_req_alloc( req->parent, req->buf_max );
        if ( NULL == new_req ) {
            gr_fatal( "gr_tcp_req_alloc failed" );
            return -2;
        }

        new_req->buf_len = left_len;
        memcpy( new_req->buf, & req->buf[ package_len ], left_len );

        // ��ʣ������ݷŻ�conn��
        req->parent->req = new_req;

        // �޸���������ݳ��Ⱥ�ʵ�ʰ�����һ��
        req->buf_len = package_len;
    } else {
        // ����������conn��ժ����
        req->parent->req = NULL;
    }

    r = gr_worker_add( queue_item, is_emerge );
    if ( 0 == r ) {
        return 0;
    }

    // ��worker��ѹ��ʧ�ܣ���Ҫ�ٰ���������ӻ����Ӷ���
    if ( NULL != new_req ) {

        // �ж�����ݣ�Ҫɾ���շ�����Ĵ��ʣ�����ݵ�����
        // �ָ�ԭ�����ݳ���
        req->buf_len = package_len + left_len;

        // ɾ���ոշ���������
        gr_tcp_req_free( new_req );
    }
    req->parent->req = req;

    return r;
}

int gr_worker_add_udp(
    gr_tcp_req_t *  req,
    bool            is_emerge
)
{
    gr_queue_item_compact_t * queue_item = (gr_queue_item_compact_t *)req;
    if ( false != queue_item->is_tcp ) {
        return -1;
    }

    return gr_worker_add( queue_item, is_emerge );
}