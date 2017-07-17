#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <stdexcept>
#include <string.h>
#include "imports.h"
#include "types.h"

using namespace std;

class Request {
    public:
        string remote_addr;
        string method;
        string uri;
        string body;
        Map<string, string> headers;
        Map<string, string> params;
        Map<string, char *> session_items;
        Context ctx;
        Session sess;
        string sess_id;

        Request() {
            ctx = NULL;
            sess = NULL;
        }

        ~Request() {
            if(sess) ice_core_destroy_session_handle(sess);
            if(ctx) ice_core_destroy_context_handle(ctx);

            for(Map<string, char *>::iterator itr = session_items.begin(); itr != session_items.end(); itr++) {
                if(itr -> second) ice_core_destroy_cstring(itr -> second);
            }
        }

        void set_context(Context new_ctx) {
            ctx = new_ctx;
        }

        void set_remote_addr(const char *addr) {
            remote_addr = addr;
        }

        void set_method(const char *_m) {
            method = _m;
        }

        void set_uri(const char *_uri) {
            uri = _uri;
        }

        void add_param(const char *_key, const char *value) {
            string key(_key);
            params[key] = value;
        }

        const string& get_param(const char *_key) {
            string key(_key);
            return params[key];
        }

        void add_header(const char *key, const char *value) {
            string lower_key = key;
            transform(lower_key.begin(), lower_key.end(), lower_key.begin(), ::tolower);

            headers[lower_key] = value;
        }

        const string& get_header(const char *key) {
            string lower_key = key;
            transform(lower_key.begin(), lower_key.end(), lower_key.begin(), ::tolower);

            return headers[lower_key];
        }

        Map<string, string>::iterator get_header_iterator_begin() {
            return headers.begin();
        }

        Map<string, string>::iterator get_header_iterator_end() {
            return headers.end();
        }

        void set_body(const u8 *_body, u32 len) {
            body = string((const char *) _body, len);
        }

        const u8 * get_body(u32 *len_out) {
            //cerr << "get_body() for Request begin" << endl;
            if(len_out) *len_out = body.size();

            if(body.size() == 0) return NULL;
            else return (const u8 *) &body[0];
        }

        bool load_session(const char *id) {
            if(!ctx || sess) return false;
            sess = ice_context_get_session_by_id(ctx, id);
            return true;
        }

        void create_session() {
            if(!ctx || sess) return;
            sess = ice_context_create_session(ctx);
        }

        const char * get_session_id() {
            if(!sess) return NULL;

            if(sess_id.empty()) {
                char *id = ice_core_session_get_id(sess);
                sess_id = id;
                ice_core_destroy_cstring(id);
            }

            return sess_id.c_str();
        }

        const char * get_session_item(const char *_k) {
            if(!sess) return NULL;

            string k(_k);

            if(session_items[k]) {
                return session_items[k];
            }

            char *v = ice_core_session_get_item(sess, _k);
            session_items[k] = v;
            return v;
        }

        void set_session_item(const char *_k, const char *v) {
            if(!sess) return;

            string k(_k);

            if(session_items[k]) {
                ice_core_destroy_cstring(session_items[k]);
                session_items[k] = NULL;
            }

            ice_core_session_set_item(sess, _k, v);
            session_items[k] = ice_core_session_get_item(sess, _k);
        }

        void remove_session_item(const char *_k) {
            if(!sess) return;

            string k(_k);

            if(session_items[k]) {
                ice_core_destroy_cstring(session_items[k]);
                ice_core_session_remove_item(sess, _k);
                session_items[k] = NULL;
            }
        }
};

extern "C" Request * ice_glue_create_request() {
    return new Request();
}

extern "C" void ice_glue_destroy_request(Request *req) {
    delete req;
}

extern "C" void ice_glue_request_set_context(Request *req, Context ctx) {
    req -> set_context(ctx);
}

extern "C" bool ice_glue_request_load_session(Request *req, const char *id) {
    return req -> load_session(id);
}

extern "C" void ice_glue_request_create_session(Request *req) {
    req -> create_session();
}

extern "C" const char * ice_glue_request_get_session_id(Request *req) {
    return req -> get_session_id();
}

extern "C" const char * ice_glue_request_get_session_item(Request *req, const char *k) {
    return req -> get_session_item(k);
}

extern "C" void ice_glue_request_set_session_item(Request *req, const char *k, const char *v) {
    req -> set_session_item(k, v);
}

extern "C" void ice_glue_request_remove_session_item(Request *req, const char *k) {
    req -> remove_session_item(k);
}

extern "C" void ice_glue_request_set_remote_addr(Request *req, const char *addr) {
    req -> set_remote_addr(addr);
}

extern "C" void ice_glue_request_set_method(Request *req, const char *m) {
    req -> set_method(m);
}

extern "C" void ice_glue_request_set_uri(Request *req, const char *uri) {
    req -> set_uri(uri);
}

extern "C" void ice_glue_request_add_param(Request *req, const char *k, const char *v) {
    req -> add_param(k, v);
}

extern "C" const char * ice_glue_request_get_param(Request *req, const char *k) {
    return req -> get_param(k).c_str();
}

extern "C" const char * ice_glue_request_get_remote_addr(Request *req) {
    return req -> remote_addr.c_str();
}

extern "C" const char * ice_glue_request_get_method(Request *req) {
    return req -> method.c_str();
}

extern "C" const char * ice_glue_request_get_uri(Request *req) {
    return req -> uri.c_str();
}

extern "C" void ice_glue_request_add_header(Request *t, const char *k, const char *v) {
    t -> add_header(k, v);
}

extern "C" Map<string, string>::iterator * ice_glue_request_create_header_iterator(Request *t) {
    Map<string, string>::iterator *itr_p = new Map<string, string>::iterator();
    Map<string, string>::iterator& itr = *itr_p;

    itr = t -> get_header_iterator_begin();
    return itr_p;
}

extern "C" const char * ice_glue_request_header_iterator_next(Request *t, Map<string, string>::iterator *itr_p) {
    Map<string, string>::iterator& itr = *itr_p;
    if(itr == t -> get_header_iterator_end()) return NULL;

    const char *ret = itr -> first.c_str();
    itr++;

    return ret;
}

extern "C" const char * ice_glue_request_get_header(Request *t, const char *k) {
    return t -> get_header(k).c_str();
}

extern "C" const u8 * ice_glue_request_get_body(Request *t, u32 *len_out) {
    //cerr << "ice_glue_get_body(" << t << ")" << endl;
    return t -> get_body(len_out);
}

extern "C" void ice_glue_request_set_body(Request *t, const u8 *body, u32 len) {
    t -> set_body(body, len);
}
