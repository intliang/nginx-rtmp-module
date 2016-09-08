
/*
 * Copyright (C) Roman Arutyunyan
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include "ngx_rtmp.h"
#include "ngx_rtmp_amf.h"
#include "ngx_rtmp_streams.h"
#include "ngx_rtmp_live_module.h"
#include "ngx_rtmp_custom_msg.h"

static ngx_chain_t * ngx_rtmp_create_client_count(ngx_rtmp_session_t *s,
        ngx_int_t count);

static ngx_int_t ngx_rtmp_send_shared_packet(ngx_rtmp_session_t *s,
        ngx_chain_t *cl);

static ngx_int_t
ngx_rtmp_send_shared_packet(ngx_rtmp_session_t *s, ngx_chain_t *cl)
{
    ngx_rtmp_core_srv_conf_t       *cscf;
    ngx_int_t                       rc;

    if (cl == NULL) {
        return NGX_ERROR;
    }

    cscf = ngx_rtmp_get_module_srv_conf(s, ngx_rtmp_core_module);

    rc = ngx_rtmp_send_message(s, cl, 0);

    ngx_rtmp_free_shared_chain(cscf, cl);

    return rc;
}


static ngx_chain_t *
ngx_rtmp_create_client_count(ngx_rtmp_session_t *s, ngx_int_t count)
{
    ngx_rtmp_header_t               h;
    static double                   dcount;

    dcount = (double)count;

    static ngx_rtmp_amf_elt_t       out_elts[] = {

        { NGX_RTMP_AMF_STRING,
          ngx_null_string,
          "ClientCount", 0 },

        { NGX_RTMP_AMF_NUMBER,
          ngx_null_string,
          &dcount, 0 },
    };

    memset(&h, 0, sizeof(h));

    h.type = NGX_RTMP_MSG_AMF_META;
    h.csid = NGX_RTMP_CSID_AMF;
    h.msid = NGX_RTMP_MSID;

    return ngx_rtmp_create_amf(s, &h, out_elts,
                               sizeof(out_elts) / sizeof(out_elts[0]));
}


ngx_int_t
ngx_rtmp_send_client_count(ngx_rtmp_session_t *s)
{
    ngx_rtmp_live_app_conf_t       *lacf;

    lacf = ngx_rtmp_get_module_app_conf(s, ngx_rtmp_live_module);

    if (lacf == NULL || !lacf->live) {
        return NGX_ERROR;
    }

    ngx_int_t                       nclients;
    ngx_rtmp_live_stream_t         *stream;
    nclients = 0;

    stream = lacf->streams[ngx_hash_key(s->name, ngx_strlen(s->name)) % lacf->nbuckets];

    for (; stream; stream = stream->next) {
        if (ngx_strcmp(stream->name, s->name)) {
            continue; 
        }

        ngx_rtmp_live_ctx_t            *ctx;
        for (ctx = stream->ctx; ctx; ctx = ctx->next) {
            if (!ctx->publishing) {
                ++nclients;
            }
        }
    }

    ngx_log_error(NGX_LOG_ERR, s->connection->log, 0,
       "live, name='%s' nclients=%i",
       stream->name, nclients);
    return ngx_rtmp_send_shared_packet(s,
           ngx_rtmp_create_client_count(s, nclients));
}

