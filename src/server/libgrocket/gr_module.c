/**
 * @file libgrocket/gr_module.c
 * @author zouyueming(da_ming at hotmail.com)
 * @date 2013/10/05
 * @version $Revision$ 
 * @brief   user module
 * Revision History
 *
 * @if  ID       Author       Date          Major Change       @endif
 *  ---------+------------+------------+------------------------------+
 *       1     zouyueming   2013-10-05    Created.
 *       1     zouyueming   2013-10-24    add http protocol support.
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

#include "gr_module.h"
#include "gr_log.h"
#include "gr_global.h"
#include "gr_errno.h"
#include "gr_tools.h"
#include "gr_mem.h"
#include "gr_dll.h"
#include "gr_config.h"
#include "gr_http.h"

typedef struct
{
    gr_dll_t                dll;

    gr_version_t            version;
    gr_init_t               init;
    gr_term_t               term;
    gr_tcp_accept_t         tcp_accept;
    gr_tcp_close_t          tcp_close;
    gr_check_t              chk_binary;
    gr_proc_t               proc_binary;
    gr_proc_http_t          proc_http;

} gr_module_t;

static_inline
void module_unload(
    gr_module_t * module
)
{
    module->version     = NULL;
    module->init        = NULL;
    module->term        = NULL;
    module->tcp_accept  = NULL;
    module->tcp_close   = NULL;
    module->chk_binary  = NULL;
    module->proc_binary = NULL;
    module->proc_http   = NULL;

    if ( NULL != module->dll ) {
        gr_dll_close( module->dll );
        module->dll = NULL;
    }
}

static_inline
int module_load(
    gr_module_t * module,
    const char * path,
    bool is_absolute
)
{
    if ( NULL != module->dll ) {
        return GR_ERR_WRONG_CALL_ORDER;
    }

    if ( is_absolute ) {
        module->dll = gr_dll_open_absolute( path );
    } else {
        module->dll = gr_dll_open( path );
    }
    if ( NULL== module->dll ) {
        return GR_ERR_INIT_MODULE_FAILED;
    }

    module->version = (gr_version_t)gr_dll_symbol( module->dll, GR_VERSION_NAME );
    module->init = (gr_init_t)gr_dll_symbol( module->dll, GR_INIT_NAME );
    module->term = (gr_term_t)gr_dll_symbol( module->dll, GR_TERM_NAME );
    module->tcp_accept = (gr_tcp_accept_t)gr_dll_symbol( module->dll, GR_TCP_ACCEPT_NAME );
    module->tcp_close = (gr_tcp_close_t)gr_dll_symbol( module->dll, GR_TCP_CLOSE_NAME );
    module->chk_binary = (gr_check_t)gr_dll_symbol( module->dll, GR_CHECK_NAME );
    module->proc_binary = (gr_proc_t)gr_dll_symbol( module->dll, GR_PROC_NAME );
    module->proc_http = (gr_proc_http_t)gr_dll_symbol( module->dll, GR_PROC_HTTP_NAME );

    if ( NULL == module->version || ( NULL == module->proc_binary && NULL == module->proc_http ) ) {
        gr_fatal( "[init](%s) gr_proc_http & gr_proc both not found", path );
        module_unload( module );
        return GR_ERR_INVALID_PARAMS;
    }

    gr_info( "[init]module '%s' loadded", path );
    return 0;
}

static_inline
bool check_version(
    gr_module_t *   module
)
{
    int module_ver = 0;
    module->version( & module_ver );

    if ( module_ver <= 0 || module_ver > 0xFF ) {
        gr_fatal( "[init]version compatible check FAILED. module=%d, framework=%d, framework.low=%d",
            module_ver, GR_SERVER_VERSION, GR_SERVER_LOW_VERSION );
        return false;
    }

    // 版本号检查，框架的最低兼容版本号必须小于或等于模块开发者使用的开发框架的版本号。
    // 这个检查，使得可以直接升级甚至降级框架的二进制文件，而模块的兼容性检查只需要看模块是否能被成功装载。
    if ( GR_SERVER_LOW_VERSION > module_ver ) {
        // 服务框架接口兼容性检查失败。直接初始化失败，省得在运行时core增加追查成本
        gr_fatal( "[init]version compatible check FAILED. module=%d, framework=%d, framework.low=%d",
            module_ver, GR_SERVER_VERSION, GR_SERVER_LOW_VERSION );
        return false;
    }

    g_ghost_rocket_global.server_interface.module_version = (unsigned char)module_ver;

    gr_info( "[init]version check OK. module=%d, framework=%d, framework.low=%d",
        module_ver, GR_SERVER_VERSION, GR_SERVER_LOW_VERSION );
    return true;
}

int gr_module_init(
    gr_version_t    version,
    gr_init_t       init,
    gr_term_t       term,
    gr_tcp_accept_t tcp_accept,
    gr_tcp_close_t  tcp_close,
    gr_check_t      chk_binary,
    gr_proc_t       proc_binary,
    gr_proc_http_t  proc_http)
{
    gr_module_t *   module;
    int             r;

    if ( NULL != g_ghost_rocket_global.module ) {
        gr_fatal( "[init]gr_module_init already called" );
        return GR_ERR_WRONG_CALL_ORDER;
    }

    module = (gr_module_t *)gr_calloc( 1, sizeof( gr_module_t ) );
    if ( NULL == module ) {
        gr_fatal( "[init]malloc %d bytes failed, errno=%d,%s",
            (int)sizeof(gr_module_t), errno, strerror( errno ) );
        return GR_ERR_BAD_ALLOC;
    }

    module->version     = version;
    module->init        = init;
    module->term        = term;
    module->tcp_accept  = tcp_accept;
    module->tcp_close   = tcp_close;
    module->chk_binary  = chk_binary;
    module->proc_binary = proc_binary;
    module->proc_http   = proc_http;

    r = 0;

    do {

        if (   NULL == module->init
            && NULL == module->term
            && NULL == module->tcp_accept
            && NULL == module->tcp_close
            && NULL == module->chk_binary
            && NULL == module->proc_binary
            && NULL == module->proc_http
        )
        {
            // 没指定用户函数，要装载模块
            char path[ MAX_PATH ] = "";
            bool is_absolute;
            gr_config_get_module_path( path, sizeof( path ), & is_absolute );

            if ( '\0' != path[ 0 ] ) {
                r = module_load( module, path, is_absolute );
                if ( 0 != r ) {
                    gr_fatal( "[init]module_load( %s ) failed, return %d", path, r );
                    break;
                }
            }
        } else {
            if ( NULL == module->version ) {
                gr_fatal( "[init]module->version is NULL" );
                r = GR_ERR_INVALID_PARAMS;
                break;
            }
        }

        if ( ! check_version( module ) ) {
            gr_fatal( "[init]check_version failed" );
            r = GR_ERR_WRONG_VERSION;
            break;
        }

    } while ( false );

    if ( GR_OK != r ) {
        module_unload( module );
        gr_free( module );
        return r;
    }

    g_ghost_rocket_global.module = module;
    return GR_OK;
}

void gr_module_term()
{
    gr_module_t * module = (gr_module_t *)g_ghost_rocket_global.module;
    if ( NULL != module ) {
        module_unload( module );

        gr_free( module );
        g_ghost_rocket_global.module = NULL;
    }
}

int gr_module_master_process_init()
{
    gr_module_t * module = (gr_module_t *)g_ghost_rocket_global.module;
    if ( NULL != module && NULL != module->init ) {
        int r = module->init( GR_PROCESS_MASTER, & g_ghost_rocket_global.server_interface );
        if ( 0 != r ) {
            gr_fatal( "[init]init with GR_PROCESS_MASTER return %d", r );
            return r;
        }
    }
    return 0;
}

void gr_module_master_process_term()
{
    gr_module_t * module = (gr_module_t *)g_ghost_rocket_global.module;
    if ( NULL != module && NULL != module->term ) {
        module->term( GR_PROCESS_MASTER, NULL );
    }
}

int gr_module_child_process_init()
{
    gr_module_t * module = (gr_module_t *)g_ghost_rocket_global.module;
    if ( NULL != module && NULL != module->init ) {
        int r = module->init( GR_PROCESS_CHILD, & g_ghost_rocket_global.server_interface );
        if ( 0 != r ) {
            gr_fatal( "[init]init with GR_PROCESS_CHILD return %d", r );
            return r;
        }
    }
    return 0;
}

void gr_module_child_process_term()
{
    gr_module_t * module = (gr_module_t *)g_ghost_rocket_global.module;
    if ( NULL != module && NULL != module->term ) {
        module->term( GR_PROCESS_CHILD, NULL );
    }
}

int gr_module_worker_init( int worker_id )
{
    gr_module_t * module = (gr_module_t *)g_ghost_rocket_global.module;
    if ( NULL != module && NULL != module->init ) {
        int r = module->init(
            GR_PROCESS_THREAD_1 + worker_id,
            & g_ghost_rocket_global.server_interface );
        if ( 0 != r ) {
            gr_fatal( "[init]init with GR_PROCESS_THREAD_1 + %d return %d", worker_id, r );
            return r;
        }
    }
    return 0;
}

void gr_module_worker_term( int worker_id )
{
    gr_module_t * module = (gr_module_t *)g_ghost_rocket_global.module;
    if ( NULL != module && NULL != module->term ) {
        module->term( GR_PROCESS_THREAD_1 + worker_id, NULL );
    }
}

bool gr_module_on_tcp_accept( gr_tcp_conn_item_t * conn )
{
    gr_module_t * module = (gr_module_t *)g_ghost_rocket_global.module;
    if ( NULL != module && NULL != module->tcp_accept ) {
        bool need_disconnect = false;
        module->tcp_accept( conn->port_item->port, conn->fd, & conn->buddy, & need_disconnect );
        return ! need_disconnect;
    }

    return true;
}

void gr_module_on_tcp_close( gr_tcp_conn_item_t * conn )
{
    gr_module_t * module = (gr_module_t *)g_ghost_rocket_global.module;
    if ( NULL != module && NULL != module->tcp_close ) {
        module->tcp_close( conn->port_item->port, conn->fd, & conn->buddy );
    }
}

void gr_module_check_tcp(
    gr_tcp_req_t *      req,
    bool *              is_error,
    bool *              is_full
)
{
    gr_module_t *   module = (gr_module_t *)g_ghost_rocket_global.module;
    gr_check_ctxt_t ctxt;

    ctxt.base       = & req->check_ctxt;
    ctxt.port_info  = req->parent->port_item;
    ctxt.sock       = req->parent->fd;

    // 先判断HTTP
    http_check( req->buf, req->buf_len, & ctxt, is_error, is_full );
    if ( GR_PACKAGE_ERROR != req->check_ctxt.package_type ) {
        return;
    }

    if ( NULL != module && NULL != module->chk_binary ) {
        * is_error  = false;
        * is_full   = false;
        module->chk_binary(
            req->buf, req->buf_len,
            & ctxt,
            & req->parent->buddy,
            is_error,
            is_full
        );
    } else {
        * is_error = true;
    }
}

void gr_module_proc_tcp(
    gr_tcp_req_t *      req,
    gr_proc_ctxt_t *    ctxt,
    int *               processed_len
)
{
    gr_module_t * module = (gr_module_t *)g_ghost_rocket_global.module;

    if (   GR_PACKAGE_HTTP_REQ   == req->check_ctxt.package_type
        || GR_PACKAGE_HTTP_REPLY == req->check_ctxt.package_type )
    {
        http_proc( req->buf, req->buf_len, ctxt, & req->parent->buddy, processed_len );
        return;
    }

    if ( NULL != module && NULL != module->proc_binary ) {
        module->proc_binary(
            req->buf,
            req->buf_len,
            ctxt,
            & req->parent->buddy,
            processed_len
        );
    } else {
        // 没实现二进制数据包处理，我只能断连接了
        * processed_len = -1;
    }
}

void gr_module_proc_http(
    gr_http_ctxt_t *    http,
    gr_conn_buddy_t *   conn_buddy,
    int *               processed_len
)
{
    gr_module_t * module = (gr_module_t *)g_ghost_rocket_global.module;

    if ( NULL != module && NULL != module->proc_http ) {
        module->proc_http( http, conn_buddy, processed_len );
    } else {
        // 没实现HTTP数据包处理，我只能断连接了，或者发个404
        char buf[] = "404 Page Not Found";
        http->keep_alive = true;
        g_buildin->http_send( g_buildin, http, buf, sizeof( buf ) - 1, "text/plain" );
    }

}
