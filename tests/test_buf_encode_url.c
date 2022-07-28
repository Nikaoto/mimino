#include <stdio.h>
#include <string.h>
#include "esma.h"
#include "http.h"
#include "buffer.h"

int
strncmp_buf(Buffer *buf, char *str)
{
    for (size_t i = 0; i < buf->n_items; i++) {
        if (str[i] == '\0' || buf->data[i] != str[i])
            return buf->data[i] - str[i];
    }
    return 0;
}

void
reset_buf(Buffer *buf)
{
    buf->n_items = 0;
}

void
test_buf_encode_url()
{
    #define cmpstr strncmp_buf

    esma_log_test("buf_encode_url()");

    Buffer *buf = new_buf(512);

    buf_encode_url(buf, "asdf");
    esma_assert(!cmpstr(buf, "asdf"));
    reset_buf(buf);

    buf_encode_url(buf, "/file space");
    esma_assert(!cmpstr(buf, "%2Ffile%20space"));
    reset_buf(buf);

    buf_encode_url(buf, "/file with spaces/");
    esma_assert(!cmpstr(buf, "%2Ffile%20with%20spaces%2F"));
    reset_buf(buf);

    buf_encode_url(buf, "ქართული");
    esma_assert(!cmpstr(buf, "%E1%83%A5%E1%83%90%E1%83%A0%E1%83%97%E1%83%A3%E1%83%9A%E1%83%98"));
    reset_buf(buf);

    buf_encode_url(buf, "http://asdf.com/../?#%");
    esma_assert(!cmpstr(buf, "http%3A%2F%2Fasdf.com%2F..%2F%3F%23%25"));
    reset_buf(buf);

    buf_encode_url(buf, "%/emAc$/kool");
    esma_assert(!cmpstr(buf, "%25%2FemAc%24%2Fkool"));
    reset_buf(buf);

    buf_encode_url(buf, "%/e%6Ac$/kool");
    esma_assert(!cmpstr(buf, "%25%2Fe%256Ac%24%2Fkool"));
    reset_buf(buf);

    buf_encode_url(buf, "Σκέφτομαι, άρα υπάρχω");
    esma_assert(!cmpstr(buf, "%CE%A3%CE%BA%CE%AD%CF%86%CF%84%CE%BF%CE%BC%CE%B1%CE%B9%2C%20%CE%AC%CF%81%CE%B1%20%CF%85%CF%80%CE%AC%CF%81%CF%87%CF%89"));
    reset_buf(buf);

    buf_encode_url(buf, "我思故我在");
    esma_assert(!cmpstr(buf, "%E6%88%91%E6%80%9D%E6%95%85%E6%88%91%E5%9C%A8"));
    reset_buf(buf);
}
