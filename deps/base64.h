#ifndef __BASE64_H__
#define __BASE64_H__

#include <cstring>
#include <memory>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <string>
#include <vector>

namespace Base64 {
template<typename T, typename R = std::string>
static R encode(const T &in, int with_new_line = 0) {
    auto b64 = BIO_new(BIO_f_base64());

    if (!with_new_line) { BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL); }

    auto bmem = BIO_new(BIO_s_mem());
    BIO_push(b64, bmem);

    BIO_write(b64, in.data(), in.size());
    BIO_flush(b64);

    BUF_MEM *b_ptr;
    BIO_get_mem_ptr(b64, &b_ptr);

    R out;
    out.resize(b_ptr->length);
    memcpy(out.data(), b_ptr->data, b_ptr->length);

    BIO_free_all(b64);

    return out;
}

template<typename T = char, typename R = std::vector<unsigned char>>
static R decode(const T *in, size_t len, int with_new_line = 0) {
    auto out = R(len);

    BIO *b64 = BIO_new(BIO_f_base64());

    if (!with_new_line) { BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL); }

    BIO *bmem = BIO_new_mem_buf(in, len);
    bmem = BIO_push(b64, bmem);

    out.resize(BIO_read(bmem, out.data(), out.size()));

    BIO_free_all(b64);

    return out;
}
};// namespace Base64

#endif