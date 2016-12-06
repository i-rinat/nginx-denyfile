// ngx_http_access_denyfile_module
//
// Copyright © 2016  Rinat Ibragimov
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct {
    ngx_str_t denyfile_name;
    ngx_flag_t recursive;
} ngx_http_access_denyfile_loc_conf_t;

extern ngx_module_t ngx_http_access_denyfile_module;


static void *
ngx_http_access_denyfile_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_access_denyfile_loc_conf_t *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(*conf));
    if (!conf)
        return NULL;

    // set by ngx_pcalloc:
    //
    // conf->denyfile_name = {0, NULL}

    conf->recursive = NGX_CONF_UNSET;

    return conf;
}

static char *
ngx_http_access_denyfile_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_access_denyfile_loc_conf_t *prev = parent;
    ngx_http_access_denyfile_loc_conf_t *conf = child;

    ngx_conf_merge_str_value(conf->denyfile_name, prev->denyfile_name, "");
    ngx_conf_merge_value(conf->recursive, prev->recursive, 1);

    return NGX_CONF_OK;
}

static ngx_command_t ngx_http_access_denyfile_commands[] = {
    {
        ngx_string("denyfile"),
        NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_access_denyfile_loc_conf_t, denyfile_name),
        NULL
    },
    {
        ngx_string("denyfile_recursive"),
        NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_flag_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_access_denyfile_loc_conf_t, recursive),
        NULL
    },
    ngx_null_command
};

static ngx_str_t
ngx_http_access_denyfile_get_document_root(ngx_http_request_t *r)
{
    ngx_http_core_loc_conf_t  *clcf;
    ngx_str_t path = ngx_null_string;
    ngx_str_t empty_string = ngx_null_string;

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    if (clcf->root_lengths == NULL)
        return clcf->root;

    if (ngx_http_script_run(r, &path, clcf->root_lengths->elts, 0, clcf->root_values->elts) == NULL)
        return empty_string;

    if (ngx_get_full_name(r->pool, (ngx_str_t *)&ngx_cycle->prefix, &path) != NGX_OK)
        return empty_string;

    return path;
}

static ngx_int_t
ngx_http_access_denyfile_handler(ngx_http_request_t *r)
{
    ngx_str_t uri = r->uri;
    ngx_str_t denyfile;
    ngx_str_t document_root;

    ngx_http_access_denyfile_loc_conf_t *cf =
        ngx_http_get_module_loc_conf(r, ngx_http_access_denyfile_module);

    if (cf->denyfile_name.len == 0)
        return NGX_DECLINED;

    document_root = ngx_http_access_denyfile_get_document_root(r);

    // Document root path, URI, denyfile name, and a '\0'.
    denyfile.data = ngx_palloc(r->pool, document_root.len + r->uri.len + cf->denyfile_name.len + 1);
    if (!denyfile.data)
        return NGX_HTTP_INTERNAL_SERVER_ERROR;

    do {
        struct stat sb;
        u_char *p;

        // Cut path component.
        while (uri.len > 0 && uri.data[uri.len - 1] != '/')
            uri.len -= 1;

        p = ngx_copy(denyfile.data, document_root.data, document_root.len);
        p = ngx_copy(p, uri.data, uri.len);
        p = ngx_copy(p, cf->denyfile_name.data, cf->denyfile_name.len);
        p = ngx_copy(p, "\0", 1);

        denyfile.len = p - denyfile.data;

        // Checking for a denyfile.
        if (lstat((const char *)denyfile.data, &sb) == 0) {
            // Found.
            return NGX_HTTP_FORBIDDEN;
        }

        // Cut slashes.
        while (uri.len > 0 && uri.data[uri.len - 1] == '/')
            uri.len -= 1;

        if (uri.len == 0)
            break;

    } while (cf->recursive);

    return NGX_DECLINED;
}

static ngx_int_t
ngx_http_access_denyfile_init(ngx_conf_t *cf)
{
    ngx_http_handler_pt *h;
    ngx_http_core_main_conf_t *cmcf;

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    h = ngx_array_push(&cmcf->phases[NGX_HTTP_ACCESS_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_http_access_denyfile_handler;
    return NGX_OK;
}

static ngx_http_module_t ngx_http_access_denyfile_module_ctx = {
    NULL,                                      // preconfiguration
    ngx_http_access_denyfile_init,             // postconfiguration
    NULL,                                      // create main configuration
    NULL,                                      // init main configuration
    NULL,                                      // create server configuration
    NULL,                                      // merge server configuration
    ngx_http_access_denyfile_create_loc_conf,  // create location configuration
    ngx_http_access_denyfile_merge_loc_conf    // merge location configuration
};

ngx_module_t ngx_http_access_denyfile_module = {
    NGX_MODULE_V1,
    &ngx_http_access_denyfile_module_ctx,  // module context
    ngx_http_access_denyfile_commands,     // module directives
    NGX_HTTP_MODULE,                       // module type
    NULL,                                  // init master
    NULL,                                  // init module
    NULL,                                  // init process
    NULL,                                  // init thread
    NULL,                                  // exit thread
    NULL,                                  // exit process
    NULL,                                  // exit master
    NGX_MODULE_V1_PADDING
};
