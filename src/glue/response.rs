use std;
use std::any::Any;
use std::sync::Arc;
use std::collections::HashMap;
use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use hyper;
use futures;
use futures::future::Future;
use glue;
use ice_server;
use streaming;
use static_file;

#[derive(Debug)]
pub struct Response {
    pub body: Vec<u8>,
    pub file: Option<String>,
    pub status: u16,
    pub headers: hyper::header::Headers,
    pub cookies: HashMap<String, String>,
    pub stream_rx: Option<streaming::ChunkReceiver>,
    pub custom_properties: Option<Arc<glue::common::CustomProperties>>
}

impl Response {
    pub fn new() -> Response {
        Response {
            body: Vec::new(),
            file: None,
            status: 200,
            headers: hyper::header::Headers::new(),
            cookies: HashMap::new(),
            stream_rx: None,
            custom_properties: None
        }
    }

    pub fn into_boxed(self) -> Box<Response> {
        Box::new(self)
    }

    pub fn into_hyper_response(mut self, ctx: &ice_server::Context, local_ctx: &ice_server::LocalContext, req_headers: Option<hyper::header::Headers>) -> Box<Future<Error = String, Item = hyper::Response>> {
        self.headers.set_raw("X-Powered-By", "Ice Core");

        let mut cookies_vec = Vec::new();

        for (k, v) in self.cookies.iter() {
            cookies_vec.push(k.clone() + "=" + v.as_str());
        }

        self.headers.set(hyper::header::SetCookie(cookies_vec));

        let resp = hyper::server::Response::new()
            .with_headers(self.headers)
            .with_status(match hyper::StatusCode::try_from(self.status) {
                Ok(v) => v,
                Err(_) => hyper::StatusCode::InternalServerError
            });
        
        match self.file {
            Some(p) => {
                let etag = match req_headers {
                    Some(v) => match v.get::<hyper::header::IfNoneMatch>() {
                        Some(v) => {
                            match v {
                                &hyper::header::IfNoneMatch::Any => None,
                                &hyper::header::IfNoneMatch::Items(ref v) => {
                                    if v.len() == 0 {
                                        None
                                    } else {
                                        Some(v[0].tag().to_string())
                                    }
                                }
                            }
                        },
                        None => None
                    },
                    None => None
                };
                static_file::fetch_raw_unchecked(&ctx, &local_ctx, resp, p.as_str(), etag)
            },
            None => {
                Box::new(futures::future::ok(
                    match self.stream_rx {
                        Some(rx) => {
                            resp.with_body(rx)
                        },
                        None => resp.with_header(hyper::header::ContentLength(self.body.len() as u64)).with_body(self.body)
                    }
                ))
            }
        }
    }

    pub fn add_header(&mut self, k: &str, v: &str) {
        self.headers.set_raw(glue::header::transform_name(k), v.to_string());
    }

    pub fn set_cookie(&mut self, k: &str, v: &str) {
        self.cookies.insert(k.to_string(), v.to_string());
    }

    pub fn set_body(&mut self, data: &[u8]) {
        self.body = data.to_vec();
    }

    pub fn set_file(&mut self, path: &str) {
        self.file = Some(path.to_string());
    }

    pub fn set_status(&mut self, status: u16) {
        self.status = status;
    }

    pub fn stream(&mut self, ctx: &ice_server::Context) -> streaming::StreamProvider {
        if self.stream_rx.is_some() {
            panic!("Attempting to enable streaming for a response that has already enabled it");
        }

        let (provider, rx) = streaming::StreamProvider::new(&ctx.ev_loop_remote);
        self.stream_rx = Some(rx);

        provider
    }
}

impl Into<Box<Any>> for Box<Response> {
    fn into(self) -> Box<Any> {
        self as Box<Any>
    }
}

#[no_mangle]
pub fn ice_glue_create_response() -> *mut Response {
    Box::into_raw(Response::new().into_boxed())
}

#[no_mangle]
pub unsafe fn ice_glue_response_add_header(resp: *mut Response, k: *const c_char, v: *const c_char) {
    let resp = &mut *resp;

    resp.add_header(CStr::from_ptr(k).to_str().unwrap(), CStr::from_ptr(v).to_str().unwrap());
}

#[no_mangle]
pub unsafe fn ice_glue_response_set_cookie(resp: *mut Response, k: *const c_char, v: *const c_char) {
    let resp = &mut *resp;

    resp.set_cookie(CStr::from_ptr(k).to_str().unwrap(), CStr::from_ptr(v).to_str().unwrap());
}

#[no_mangle]
pub unsafe fn ice_glue_response_set_body(resp: *mut Response, data: *const u8, len: u32) {
    let resp = &mut *resp;

    if data.is_null() || len == 0 {
        resp.set_body(&[]);
    } else {
        resp.set_body(std::slice::from_raw_parts(data, len as usize));
    }
}

#[no_mangle]
pub unsafe fn ice_glue_response_set_file(resp: *mut Response, path: *const c_char) {
    let resp = &mut *resp;

    resp.set_file(CStr::from_ptr(path).to_str().unwrap());
}

#[no_mangle]
pub unsafe fn ice_glue_response_set_status(resp: *mut Response, status: u16) {
    let resp = &mut *resp;

    resp.set_status(status);
}

#[no_mangle]
pub unsafe fn ice_glue_response_consume_rendered_template(resp: *mut Response, content: *mut c_char) {
    let resp = &mut *resp;
    let content = CString::from_raw(content);

    resp.set_body(content.as_bytes());
}

#[no_mangle]
pub unsafe fn ice_glue_response_stream(resp: *mut Response, ctx: *const ice_server::Context) -> *mut streaming::StreamProvider {
    let resp = &mut *resp;
    let ctx = &*ctx;

    Box::into_raw(resp.stream(ctx).into_boxed())
}

#[no_mangle]
pub unsafe fn ice_glue_response_borrow_custom_properties(resp: *mut Response) -> *const glue::common::CustomProperties {
    let resp = &*resp;
    match resp.custom_properties {
        Some(ref v) => &**v,
        None => std::ptr::null()
    }
}
