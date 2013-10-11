/**
 * @file libgrocket/gr_server_impl.c
 * @author zouyueming(da_ming at hotmail.com)
 * @date 2013/10/05
 * @version $Revision$ 
 * @brief   ����������
 * Revision History ���¼���
 *
 * @if  ID       Author       Date          Major Change       @endif
 *  ---------+------------+------------+------------------------------+
 *       1     zouyueming   2013-10-05    Created.
 **/
#include "gr_server_impl.h"
#include "gr_log.h"
#include "gr_global.h"
#include "gr_errno.h"
#include "gr_tools.h"
#include "gr_config.h"
#include "gr_mem.h"
#include "gr_tcp_accept.h"
#include "gr_tcp_in.h"
#include "gr_udp_in.h"
#include "gr_tcp_out.h"
#include "gr_udp_out.h"
#include "gr_worker.h"
#include "gr_http.h"
#include "gr_conn.h"
#include "gr_backend.h"
#include "gr_module.h"
#include "gr_socket.h"
#include <time.h>       // for time()
#include <signal.h>

int
gr_server_init(
    int             argc,
    char **         argv
)
{
    gr_server_impl_t *  server;

    if ( NULL != g_ghost_rocket_global.server ) {
        gr_fatal( "[init]gr_server_init_config already called" );
        return GR_ERR_WRONG_CALL_ORDER;
    }

    server = (gr_server_impl_t *)gr_calloc( 1, sizeof( gr_server_impl_t ) );
    if ( NULL == server ) {
        gr_fatal( "[init]malloc %d bytes failed, errno=%d,%s",
            (int)sizeof(gr_server_impl_t), errno, strerror( errno ) );
        return GR_ERR_BAD_ALLOC;
    }

    // ��¼����������ʱ��
    server->start_time = time( NULL );
    // ��¼�����в���
    server->argc = argc;
    server->argv = argv;

    g_ghost_rocket_global.server = server;
    return GR_OK;
}

void gr_server_term()
{
    if ( NULL != g_ghost_rocket_global.server ) {
        gr_free( g_ghost_rocket_global.server );
        g_ghost_rocket_global.server = NULL;
    }
}

void gr_server_need_exit( gr_server_impl_t * server )
{
    server->is_server_stopping = true;
}

static inline
void
do_close()
{
    // ���˳�����, �����������deamon=1ʱ���ô˹���,�������������
    if ( NULL != g_ghost_rocket_global.server ) {
        gr_server_impl_t *  server = g_ghost_rocket_global.server;
        gr_server_need_exit( server );
    }
}

#if defined( WIN32 ) || defined( WIN64 )

static
BOOL WINAPI
process_signal(
    DWORD ctrl_type
)
{
    switch( ctrl_type )
    {
    case CTRL_BREAK_EVENT:      // A CTRL+C or CTRL+BREAK signal was
                                // received, either from keyboard input
                                // or from a signal generated by
                                // GenerateConsoleCtrlEvent.

    case CTRL_C_EVENT:          // SERVICE_CONTROL_STOP in debug mode or
                                // A CTRL+c signal was received, either
                                // from keyboard input or from a signal
                                // generated by the GenerateConsoleCtrlEvent
                                // function.

    case CTRL_CLOSE_EVENT:      // A signal that the system sends to all
                                // processes attached to a console when
                                // the user closes the console (either
                                // by choosing the Close command from
                                // the console window's System menu, or
                                // by choosing the End Task command from
                                // the Task List).

    case CTRL_SHUTDOWN_EVENT:   // A signal that the system sends to all
                                // console processes when the system is
                                // shutting down.
        printf( "!!!!!! receive server stopping signal %d !!!!!!\n", (int)ctrl_type );
        gr_info( "receive server stopping signal %d", (int)ctrl_type );
        do_close();

        return TRUE;

    default:
        printf( "!!!!!! receive unknown signal %d !!!!!!\n", (int)ctrl_type );
        gr_error( "receive unknown signal %d", (int)ctrl_type );
        return FALSE;
    }
}

#else

static
void
process_signal(
    int sig
)
{
    signal( sig, process_signal );
    printf( "!!!!!! receive server stopping signal %d !!!!!!\n", sig );
    gr_info( "receive server stopping signal %d", sig );
    do_close();
}

#endif  // signal

static inline
void init_signal()
{
#if defined( WIN32 ) || defined( WIN64 )
    SetConsoleCtrlHandler( process_signal, TRUE );
#else
    // SIGHUP ���ź����û��ն�����(�����������)����ʱ����, ͨ�������ն˵Ŀ� 
    // �ƽ��̽���ʱ, ֪ͨͬһsession�ڵĸ�����ҵ, ��ʱ����������ն� 
    // ���ٹ���. 
    signal( SIGHUP, SIG_IGN );

    // SIGINT ������ֹ(interrupt)�ź�, ���û�����INTR�ַ�(ͨ����Ctrl-C)ʱ����
    signal( SIGINT, process_signal );

    // SIGQUIT ��SIGINT����, ����QUIT�ַ�(ͨ����Ctrl-)������. ���������յ� 
    // SIGQUIT�˳�ʱ�����core�ļ�, �����������������һ����������� 
    // ��.
    signal( SIGQUIT, process_signal );

    // SIGILL ִ���˷Ƿ�ָ��. ͨ������Ϊ��ִ���ļ��������ִ���, ������ͼִ�� 
    // ���ݶ�. ��ջ���ʱҲ�п��ܲ�������ź�. 

    // SIGTRAP �ɶϵ�ָ�������trapָ�����. ��debuggerʹ��. 

    // SIGTERM �������(terminate)�ź�, ��SIGKILL��ͬ���Ǹ��źſ��Ա������� 
    // ����. ͨ������Ҫ������Լ������˳�. shell����killȱʡ������ 
    // ���ź�. 
    signal( SIGTERM, process_signal );

    // SIGIOT ��PDP-11����iotָ�����, �����������Ϻ�SIGABRTһ��. 
    signal( SIGIOT, process_signal );

    // SIGBUS �Ƿ���ַ, �����ڴ��ַ����(alignment)����. eg: ����һ���ĸ��ֳ� 
    // ������, �����ַ����4�ı���. 

    // SIGFPE �ڷ��������������������ʱ����. �������������������, �������� 
    // ��������Ϊ0���������е������Ĵ���. 

    // SIGKILL ���������������������. ���źŲ��ܱ�����, �����ͺ���. 

    // SIGPIPE Broken pipe 
    signal( SIGPIPE, SIG_IGN );

    // SIGTSTP ֹͣ���̵�����, �����źſ��Ա������ͺ���. �û�����SUSP�ַ�ʱ 
    // (ͨ����Ctrl-Z)��������ź� 
    signal( SIGTSTP, process_signal );

    // SIGCHLD �ӽ��̽���ʱ, �����̻��յ�����ź�. 
    signal( SIGCHLD, SIG_IGN );

    // SIGXCPU ����CPUʱ����Դ����. ������ƿ�����getrlimit/setrlimit����ȡ/ 
    // �ı�
    signal( SIGXCPU, SIG_IGN );

    // SIGXFSZ �����ļ���С��Դ����
    signal( SIGXFSZ, SIG_IGN );
#endif
}

static inline
bool has_tcp()
{
    gr_server_t * server_interface = & g_ghost_rocket_global.server_interface;
    int i;
    for ( i = 0; i < server_interface->ports_count; ++ i ) {
        gr_port_item_t * item = & server_interface->ports[ i ];
        if ( item->is_tcp ) {
            return true;
        }
    }
    return false;
}

static inline
bool has_udp()
{
    gr_server_t * server_interface = & g_ghost_rocket_global.server_interface;
    int i;
    for ( i = 0; i < server_interface->ports_count; ++ i ) {
        gr_port_item_t * item = & server_interface->ports[ i ];
        if ( ! item->is_tcp ) {
            return true;
        }
    }
    return false;
}

static inline
int
server_init(
    gr_server_impl_t * server
)
{
    int     r       = GR_OK;

#if defined( WIN32 ) || defined( WIN64 )
    gr_info( "Windows not allow to add a socket fd to difference IOCP, so I fuck this!" );
#endif

    do {

        if ( has_tcp() ) {

            // ��ʼ��TCP acceptģ��
            r = gr_tcp_accept_init();
            if ( 0 != r ) {
                gr_fatal( "[init]gr_tcp_accept_init() return error %d", r );
                r = GR_ERR_INIT_TCP_ACCEPT_FALED;
                break;
            }

            // ��ʼ��TCP��ģ��
            r = gr_tcp_in_init();
            if ( 0 != r ) {
                gr_fatal( "[init]gr_tcp_in_init() return error %d", r );
                r = GR_ERR_INIT_TCP_IN_FALED;
                break;
            }

            // ��ʼ��TCPдģ��
            r = gr_tcp_out_init();
            if ( 0 != r ) {
                gr_fatal( "[init]gr_tcp_out_init() return error %d", r );
                r = GR_ERR_INIT_TCP_OUT_FALED;
                break;
            }
        }

        if ( has_udp() ) {
            // ��ʼ��UDP��ģ��
            r = gr_udp_in_init();
            if ( 0 != r ) {
                gr_fatal( "[init]gr_udp_in_init() return error %d", r );
                r = GR_ERR_INIT_UDP_IN_FALED;
                break;
            }

            // ��ʼ��UDPдģ��
            r = gr_udp_out_init();
            if ( 0 != r ) {
                gr_fatal( "[init]gr_udp_out_init() return error %d", r );
                r = GR_ERR_INIT_UDP_OUT_FALED;
                break;
            }
        }

        // ��ʼ��httpģ��
        r = gr_http_init();
        if ( 0 != r ) {
            gr_fatal( "[init]gr_http_init() return error %d", r );
            r = GR_ERR_INIT_HTTP_FALED;
            break;
        }

        // ��ʼ��connģ��
        r = gr_conn_init();
        if ( 0 != r ) {
            gr_fatal( "[init]gr_conn_init() return error %d", r );
            r = GR_ERR_INIT_CONN_FALED;
            break;
        }

        // ��ʼ��backendģ��
        r = gr_backend_init();
        if ( 0 != r ) {
            gr_fatal( "[init]gr_backend_init() return error %d", r );
            r = GR_ERR_INIT_CONN_FALED;
            break;
        }

        // ��ʼ��workerģ��
        r = gr_worker_init();
        if ( 0 != r ) {
            gr_fatal( "[init]gr_worker_init() return error %d", r );
            r = GR_ERR_INIT_WORKER_FALED;
            break;
        }

    } while ( false );

    return r;
}

static inline
void
server_term(
    gr_server_impl_t * server
)
{
    // ж��workerģ��
    gr_worker_term();
    // ж��backendģ��
    gr_backend_term();
    // ж��connģ��
    gr_conn_term();
    // ж��httpģ��
    gr_http_term();

    if ( has_udp() ) {
        // ж��UDPдģ��
        gr_udp_out_term();
        // ж��UDP��ģ��
        gr_udp_in_term();
    }

    if ( has_tcp() ) {
        // ж��TCPPдģ��
        gr_tcp_out_term();
        // ж��TCP��ģ��
        gr_tcp_in_term();
        // ж��tcp acceptģ��
        gr_tcp_accept_term();
    }
}

static
int
start_listen(
    gr_server_impl_t * server
)
{
    gr_server_t * server_interface = & g_ghost_rocket_global.server_interface;
    int i;
    int r = 0;
    int listen_backlog = gr_config_get_listen_backlog();

    if ( has_tcp() ) {
        for ( i = 0; i < server_interface->ports_count; ++ i ) {
            gr_port_item_t * item = & server_interface->ports[ i ];

            if ( item->is_tcp ) {
                // listen
                if ( -1 == listen( item->fd, listen_backlog ) ) {
                    gr_error( "listen for port %d failed: %d", item->port, get_errno() );
                    r = -1;
                    break;
                }
            }
        }

        r = gr_tcp_accept_add_listen_ports();
        //r = gr_tcp_in_add_listen_ports();
        if ( 0 != r ) {
            gr_error( "gr_tcp_accept_add_listen_ports() return %d", r );
            r = -2;
        }
    }

    if ( has_udp() ) {
        r = gr_udp_in_add_listen_ports();
        if ( 0 != r ) {
            gr_error( "gr_udp_in_add_listen_ports() return %d", r );
            r = -3;
        }
    }

    return 0;
}

static inline
int
server_loop(
    gr_server_impl_t * server
)
{
    int r = start_listen( server );
    if ( 0 != r ) {
        gr_error( "start_listen() return %d", -1 );
        return r;
    }

    while ( ! server->is_server_stopping ) {
        sleep_ms( 100 );
    }

    return 0;
}

static inline
int
bind_tcp(
    gr_server_impl_t * server,
    gr_port_item_t * item
)
{
    int r = 0;
    item->fd = (int)socket( PF_INET, SOCK_STREAM, IPPROTO_TCP );
    if ( -1 == item->fd ) {
        gr_error( "socket invalid, errno = %d", get_errno() );
        return -1;
    }

    do {
        int reuse_addr = 1;

        // ���ͻ�����
        if ( ! gr_socket_set_send_buf( item->fd, gr_config_tcp_accept_send_buf() ) ) {
            gr_warning( "gr_socket_set_send_buf failed, ignore" );
        }

        // ���ջ�����
        if ( ! gr_socket_set_recv_buf( item->fd, gr_config_tcp_accept_recv_buf() ) ) {
            gr_warning( "gr_socket_set_recv_buf failed, ignore" );
        }

        // ������
        if ( ! gr_socket_set_block( item->fd, false ) ) {
            r = -5;
            break;
        }

        // ���õ�ַ
        setsockopt( item->fd, SOL_SOCKET, SO_REUSEADDR, (const char*)& reuse_addr, sizeof( reuse_addr ) );

        // bind
        if ( -1 == bind( item->fd, (struct sockaddr*)& item->addr, item->addr_len ) ) {
            gr_error( "bind tcp port %d failed: %d", item->port, get_errno() );
            r = -6;
            break;
        }

        r = 0;
    } while( 0 );

    if ( 0 != r ) {
        gr_error( "bind_tcp failed" );
        gr_socket_close( item->fd );
        item->fd = -1;
        return r;
    }

    return 0;
}

static inline
int
bind_udp(
    gr_server_impl_t * server,
    gr_port_item_t * item
)
{
    int r = 0;
    item->fd = (int)socket( PF_INET, SOCK_DGRAM, IPPROTO_UDP );
    if ( -1 == item->fd ) {
        gr_error( "socket invalid, errno = %d", get_errno() );
        return -1;
    }

    do {
        int reuse_addr = 1;

        // ���ͻ�����
        if ( ! gr_socket_set_send_buf( item->fd, gr_config_udp_send_buf() ) ) {
            r = -3;
            break;
        }

        // ���ջ�����
        if ( ! gr_socket_set_recv_buf( item->fd, gr_config_udp_recv_buf() ) ) {
            r = -4;
            break;
        }

        // ������
        if ( ! gr_socket_set_block( item->fd, false ) ) {
            r = -5;
            break;
        }

        // ���õ�ַ
        setsockopt( item->fd, SOL_SOCKET, SO_REUSEADDR, (const char*)& reuse_addr, sizeof( reuse_addr ) );

        // bind
        if ( -1 == bind( item->fd, (struct sockaddr*)& item->addr, item->addr_len ) ) {
            gr_error( "bind udp port %d failed: %d", item->port, get_errno() );
            r = -6;
            break;
        }

        r = 0;
    } while( 0 );

    if ( 0 != r ) {
        gr_error( "bind_udp failed" );
        gr_socket_close( item->fd );
        item->fd = -1;
        return r;
    }

    return 0;
}

static
int
server_bind_port(
    gr_server_impl_t * server
)
{
    gr_server_t * server_interface = & g_ghost_rocket_global.server_interface;
    int i;
    int r;

    for ( i = 0; i < server_interface->ports_count; ++ i ) {
        gr_port_item_t * item = & server_interface->ports[ i ];

        if ( item->is_tcp ) {
            r = bind_tcp( server, item );
            if ( 0 != r ) {
                gr_error( "listen_tcp return %d", r );
                return -1;
            }
        } else {
            r = bind_udp( server, item );
            if ( 0 != r ) {
                gr_error( "listen_udp return %d", r );
                return -2;
            }
        }
    }

    return 0;
}

static inline
int
server_run(
    gr_server_impl_t * server
)
{
    int r = GR_OK;
    bool child_process_init_ok = false;
    bool server_init_called = false;

    do {

        // �Ѷ˿ڰ���
        r = server_bind_port( server );
        if ( 0 != r ) {
            gr_fatal( "[init]server_bind_port return error %d", r );
            break;
        }

        // ����ģ���ӽ��̳�ʼ������
        r = gr_module_child_process_init();
        if ( 0 != r ) {
            gr_fatal( "[init]gr_module_child_process_init return error %d", r );
            break;
        }
        child_process_init_ok = true;

        // ��ʼ���������������а���ÿ��worker��ʼ���ĺ���
        server_init_called = true;
        r = server_init( server );
        if ( 0 != r ) {
            gr_fatal( "[init]server_init return error %d", r );
            break;
        }

        // ��������ѭ��
        r = server_loop( server );
        gr_info( "[init]server_loop return %d", r );

    } while ( false );

    if ( server_init_called ) {
        // ����ʼ���������������а���ÿ��worker����ʼ���ĺ���
        server_term( server );
    }

    // ����ģ���˽��̷���ʼ������
    if ( child_process_init_ok ) {
        gr_module_child_process_term();
    }

    return r;
}

int gr_server_main()
{
    // ע�⣺����������ӽ��̵��õĴ���
    int r;
    time_t start;
    time_t stop;
    gr_server_impl_t *  server;

    if ( NULL == g_ghost_rocket_global.server ) {
        gr_error( "[init]global.server is NULL" );
        return GR_ERR_INVALID_PARAMS;
    }
    server  = g_ghost_rocket_global.server;

    init_signal();

    gr_info( "GRocket Server Started" );

    time( & start );
    r = server_run( server );
    time( & stop );

    gr_info( "GRocket Server will exit(%d), running %d seconds(%d minutes | %d hours)",
        r,
        (int)(stop - start),
        (int)(stop - start) / 60,
        (int)(stop - start) / 60 / 60
    );

    return r;
}