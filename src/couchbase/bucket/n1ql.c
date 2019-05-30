/**
 *     Copyright 2016-2019 Couchbase, Inc.
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */

#include "couchbase.h"

#define LOGARGS(instance, lvl) LCB_LOG_##lvl, instance, "pcbc/n1ql", __FILE__, __LINE__

#define PCBC_N1QL_CONSISTENCY_NOT_BOUNDED 1
#define PCBC_N1QL_CONSISTENCY_REQUEST_PLUS 2
#define PCBC_N1QL_CONSISTENCY_STATEMENT_PLUS 3

#define PCBC_N1QL_PROFILE_NONE 1
#define PCBC_N1QL_PROFILE_PHASES 2
#define PCBC_N1QL_PROFILE_TIMINGS 3


extern zend_class_entry *pcbc_query_result_impl_ce;
extern zend_class_entry *pcbc_meta_data_impl_ce;
zend_class_entry *pcbc_query_options_ce;

struct query_cookie {
    lcb_STATUS rc;
    zval *return_value;
};

static void n1qlrow_callback(lcb_INSTANCE *  instance, int ignoreme, const lcb_RESPN1QL *resp)
{
    TSRMLS_FETCH();

    struct query_cookie *cookie;
    lcb_respn1ql_cookie(resp, (void **)&cookie);
    cookie->rc = lcb_respn1ql_status(resp);
    zval *return_value = cookie->return_value;

    zend_update_property_long(pcbc_query_result_impl_ce, return_value, ZEND_STRL("status"), cookie->rc TSRMLS_CC);

    const char *row = NULL;
    size_t nrow = 0;
    lcb_respn1ql_row(resp, &row, &nrow);

    if (nrow > 0) {
        zval value;
        ZVAL_NULL(&value);

        int last_error;
        PCBC_JSON_COPY_DECODE(&value, row, nrow, PHP_JSON_OBJECT_AS_ARRAY, last_error);
        if(last_error != 0) {
            pcbc_log(LOGARGS(instance, WARN), "Failed to decode N1QL response as JSON: json_last_error=%d", last_error);
        }
        if (lcb_respn1ql_is_final(resp)) {
            zval meta, *mval;
            object_init_ex(&meta, pcbc_meta_data_impl_ce);
            HashTable *marr = Z_ARRVAL(value);

            mval = zend_symtable_str_find(marr, ZEND_STRL("status"));
            if (mval) zend_update_property(pcbc_meta_data_impl_ce, &meta, ZEND_STRL("status"), mval TSRMLS_CC);
            mval = zend_symtable_str_find(marr, ZEND_STRL("requestID"));
            if (mval) zend_update_property(pcbc_meta_data_impl_ce, &meta, ZEND_STRL("request_id"), mval TSRMLS_CC);
            mval = zend_symtable_str_find(marr, ZEND_STRL("clientContextID"));
            if (mval) zend_update_property(pcbc_meta_data_impl_ce, &meta, ZEND_STRL("client_context_id"), mval TSRMLS_CC);
            mval = zend_symtable_str_find(marr, ZEND_STRL("signature"));
            if (mval) zend_update_property(pcbc_meta_data_impl_ce, &meta, ZEND_STRL("signature"), mval TSRMLS_CC);
            mval = zend_symtable_str_find(marr, ZEND_STRL("errors"));
            if (mval) zend_update_property(pcbc_meta_data_impl_ce, &meta, ZEND_STRL("errors"), mval TSRMLS_CC);
            mval = zend_symtable_str_find(marr, ZEND_STRL("warnings"));
            if (mval) zend_update_property(pcbc_meta_data_impl_ce, &meta, ZEND_STRL("warnings"), mval TSRMLS_CC);
            mval = zend_symtable_str_find(marr, ZEND_STRL("metrics"));
            if (mval) zend_update_property(pcbc_meta_data_impl_ce, &meta, ZEND_STRL("metrics"), mval TSRMLS_CC);
            mval = zend_symtable_str_find(marr, ZEND_STRL("profile"));
            if (mval) zend_update_property(pcbc_meta_data_impl_ce, &meta, ZEND_STRL("profile"), mval TSRMLS_CC);

            zend_update_property(pcbc_query_result_impl_ce, return_value, ZEND_STRL("meta"), &meta TSRMLS_CC);
        } else {
            zval *rows, rv;
            rows = zend_read_property(pcbc_query_result_impl_ce, return_value, ZEND_STRL("rows"), 0, &rv);
            add_next_index_zval(rows, &value);
        }
    }
}

zend_class_entry *pcbc_query_consistency_ce;
static const zend_function_entry pcbc_query_consistency_methods[] = {
    PHP_FE_END
};

zend_class_entry *pcbc_query_profile_ce;
static const zend_function_entry pcbc_query_profile_methods[] = {
    PHP_FE_END
};

PHP_METHOD(QueryOptions, timeout)
{
    zend_long arg;
    int rv = zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &arg);
    if (rv == FAILURE) {
        RETURN_NULL();
    }
    zend_update_property_long(pcbc_query_options_ce, getThis(), ZEND_STRL("timeout"), arg TSRMLS_CC);
    RETURN_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(QueryOptions, consistency)
{
    zend_long arg;
    int rv = zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &arg);
    if (rv == FAILURE) {
        RETURN_NULL();
    }
    zend_update_property_long(pcbc_query_options_ce, getThis(), ZEND_STRL("consistency"), arg TSRMLS_CC);
    RETURN_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(QueryOptions, scanCap)
{
    zend_long arg;
    int rv = zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &arg);
    if (rv == FAILURE) {
        RETURN_NULL();
    }
    zend_update_property_long(pcbc_query_options_ce, getThis(), ZEND_STRL("scan_cap"), arg TSRMLS_CC);
    RETURN_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(QueryOptions, pipelineCap)
{
    zend_long arg;
    int rv = zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &arg);
    if (rv == FAILURE) {
        RETURN_NULL();
    }
    zend_update_property_long(pcbc_query_options_ce, getThis(), ZEND_STRL("pipeline_cap"), arg TSRMLS_CC);
    RETURN_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(QueryOptions, pipelineBatch)
{
    zend_long arg;
    int rv = zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &arg);
    if (rv == FAILURE) {
        RETURN_NULL();
    }
    zend_update_property_long(pcbc_query_options_ce, getThis(), ZEND_STRL("pipeline_batch"), arg TSRMLS_CC);
    RETURN_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(QueryOptions, maxParallelism)
{
    zval *arg;
    int rv = zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &arg);
    if (rv == FAILURE || Z_TYPE_P(arg) != IS_LONG) {
        RETURN_NULL();
    }
    zval *data, rv1;
    data = zend_read_property(pcbc_query_options_ce, getThis(), ZEND_STRL("raw_params"), 0, &rv1);
    if (Z_TYPE_P(data) == IS_NULL) {
        array_init(&rv1);
        data = &rv1;
        zend_update_property(pcbc_query_options_ce, getThis(), ZEND_STRL("raw_params"), data TSRMLS_CC);
    }
    smart_str buf = {0};
    int last_error;
    PCBC_JSON_ENCODE(&buf, arg, 0, last_error);
    if (last_error != 0) {
        pcbc_log(LOGARGS(NULL, WARN), "Failed to encode value of raw parameter as JSON: json_last_error=%d", last_error);
        smart_str_free(&buf);
        RETURN_NULL();
    }
    smart_str_0(&buf);
    add_assoc_str_ex(data, ZEND_STRL("max_parallelism"), buf.s TSRMLS_CC);

    RETURN_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(QueryOptions, profile)
{
    zend_long arg;
    int rv = zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &arg);
    if (rv == FAILURE) {
        RETURN_NULL();
    }
    zval *data, rv1;
    data = zend_read_property(pcbc_query_options_ce, getThis(), ZEND_STRL("raw_params"), 0, &rv1);
    if (Z_TYPE_P(data) == IS_NULL) {
        array_init(&rv1);
        data = &rv1;
        zend_update_property(pcbc_query_options_ce, getThis(), ZEND_STRL("raw_params"), data TSRMLS_CC);
    }
    switch (arg) {
        case PCBC_N1QL_PROFILE_NONE:
            add_assoc_string_ex(data, ZEND_STRL("profile"), "\"off\"" TSRMLS_CC);
            break;
        case PCBC_N1QL_PROFILE_PHASES:
            add_assoc_string_ex(data, ZEND_STRL("profile"), "\"phases\"" TSRMLS_CC);
            break;
        case PCBC_N1QL_PROFILE_TIMINGS:
            add_assoc_string_ex(data, ZEND_STRL("profile"), "\"timings\"" TSRMLS_CC);
            break;
    }

    RETURN_ZVAL(getThis(), 1, 0);
}


PHP_METHOD(QueryOptions, readonly)
{
    zend_bool arg;
    int rv = zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "b", &arg);
    if (rv == FAILURE) {
        RETURN_NULL();
    }
    zend_update_property_bool(pcbc_query_options_ce, getThis(), ZEND_STRL("readonly"), arg TSRMLS_CC);
    RETURN_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(QueryOptions, adhoc)
{
    zend_bool arg;
    int rv = zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "b", &arg);
    if (rv == FAILURE) {
        RETURN_NULL();
    }
    zend_update_property_bool(pcbc_query_options_ce, getThis(), ZEND_STRL("adhoc"), arg TSRMLS_CC);
    RETURN_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(QueryOptions, namedParameters)
{
    zval *arg;
    int rv = zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a", &arg);
    if (rv == FAILURE) {
        RETURN_NULL();
    }
    zval params;
    array_init(&params);
    HashTable *ht = HASH_OF(arg);
    zend_string *string_key = NULL;
    zval *entry;
    ZEND_HASH_FOREACH_STR_KEY_VAL(ht, string_key, entry) {
        if (string_key) {
            smart_str buf = {0};
            int last_error;
            PCBC_JSON_ENCODE(&buf, entry, 0, last_error);
            if (last_error != 0) {
                pcbc_log(LOGARGS(NULL, WARN), "Failed to encode value of parameter '%.*s' as JSON: json_last_error=%d",
                        (int) ZSTR_LEN(string_key), ZSTR_VAL(string_key), last_error);
                smart_str_free(&buf);
                continue;
            }
            smart_str_0(&buf);
            add_assoc_str_ex(&params, ZSTR_VAL(string_key), ZSTR_LEN(string_key), buf.s TSRMLS_CC);
        }
    }
    ZEND_HASH_FOREACH_END();
    zend_update_property(pcbc_query_options_ce, getThis(), ZEND_STRL("named_params"), &params TSRMLS_CC);
    RETURN_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(QueryOptions, positionalParameters)
{
    zval *arg;
    int rv = zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a", &arg);
    if (rv == FAILURE) {
        RETURN_NULL();
    }
    zval params;
    array_init(&params);
    HashTable *ht = HASH_OF(arg);
    zval *entry;
    ZEND_HASH_FOREACH_VAL(ht, entry) {
        smart_str buf = {0};
        int last_error;
        PCBC_JSON_ENCODE(&buf, entry, 0, last_error);
        if (last_error != 0) {
            pcbc_log(LOGARGS(NULL, WARN), "Failed to encode value of positional parameter as JSON: json_last_error=%d", last_error);
            smart_str_free(&buf);
            RETURN_NULL();
        } else {
            smart_str_0(&buf);
            add_next_index_str(&params, buf.s TSRMLS_CC);
        }
    }
    ZEND_HASH_FOREACH_END();
    zend_update_property(pcbc_query_options_ce, getThis(), ZEND_STRL("positional_params"), &params TSRMLS_CC);

    RETURN_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(QueryOptions, rawParameter)
{
    zend_string *key;
    zval *value = NULL;
    int rv = zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "Sz!", &key, &value);
    if (rv == FAILURE) {
        RETURN_NULL();
    }
    zval *data, rv1;
    data = zend_read_property(pcbc_query_options_ce, getThis(), ZEND_STRL("raw_params"), 0, &rv1);
    if (Z_TYPE_P(data) == IS_NULL) {
        array_init(&rv1);
        data = &rv1;
        zend_update_property(pcbc_query_options_ce, getThis(), ZEND_STRL("raw_params"), data TSRMLS_CC);
    }
    smart_str buf = {0};
    int last_error;
    PCBC_JSON_ENCODE(&buf, value, 0, last_error);
    if (last_error != 0) {
        pcbc_log(LOGARGS(NULL, WARN), "Failed to encode value of raw parameter as JSON: json_last_error=%d", last_error);
        smart_str_free(&buf);
        RETURN_NULL();
    }
    smart_str_0(&buf);
    add_assoc_str_ex(data, ZSTR_VAL(key), ZSTR_LEN(key), buf.s TSRMLS_CC);

    RETURN_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(ai_QueryOptions_timeout, 0, 1, \\Couchbase\\QueryOptions, 0)
ZEND_ARG_TYPE_INFO(0, arg, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(ai_QueryOptions_consistency, 0, 1, \\Couchbase\\QueryOptions, 0)
ZEND_ARG_TYPE_INFO(0, arg, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(ai_QueryOptions_scanCap, 0, 1, \\Couchbase\\QueryOptions, 0)
ZEND_ARG_TYPE_INFO(0, arg, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(ai_QueryOptions_pipelineCap, 0, 1, \\Couchbase\\QueryOptions, 0)
ZEND_ARG_TYPE_INFO(0, arg, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(ai_QueryOptions_pipelineBatch, 0, 1, \\Couchbase\\QueryOptions, 0)
ZEND_ARG_TYPE_INFO(0, arg, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(ai_QueryOptions_maxParallelism, 0, 1, \\Couchbase\\QueryOptions, 0)
ZEND_ARG_TYPE_INFO(0, arg, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(ai_QueryOptions_profile, 0, 1, \\Couchbase\\QueryOptions, 0)
ZEND_ARG_TYPE_INFO(0, arg, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(ai_QueryOptions_readonly, 0, 1, \\Couchbase\\QueryOptions, 0)
ZEND_ARG_TYPE_INFO(0, arg, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(ai_QueryOptions_adhoc, 0, 1, \\Couchbase\\QueryOptions, 0)
ZEND_ARG_TYPE_INFO(0, arg, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(ai_QueryOptions_namedParameters, 0, 1, \\Couchbase\\QueryOptions, 0)
ZEND_ARG_TYPE_INFO(0, arg, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(ai_QueryOptions_positionalParameters, 0, 1, \\Couchbase\\QueryOptions, 0)
ZEND_ARG_TYPE_INFO(0, arg, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(ai_QueryOptions_rawParameter, 0, 2, \\Couchbase\\QueryOptions, 0)
ZEND_ARG_TYPE_INFO(0, key, IS_STRING, 0)
ZEND_ARG_TYPE_INFO(0, value, 0, 1)
ZEND_END_ARG_INFO()

static const zend_function_entry pcbc_query_options_methods[] = {
    PHP_ME(QueryOptions, timeout, ai_QueryOptions_timeout, ZEND_ACC_PUBLIC)
    PHP_ME(QueryOptions, consistency, ai_QueryOptions_consistency, ZEND_ACC_PUBLIC)
    PHP_ME(QueryOptions, scanCap, ai_QueryOptions_scanCap, ZEND_ACC_PUBLIC)
    PHP_ME(QueryOptions, pipelineCap, ai_QueryOptions_pipelineCap, ZEND_ACC_PUBLIC)
    PHP_ME(QueryOptions, pipelineBatch, ai_QueryOptions_pipelineBatch, ZEND_ACC_PUBLIC)
    PHP_ME(QueryOptions, readonly, ai_QueryOptions_readonly, ZEND_ACC_PUBLIC)
    PHP_ME(QueryOptions, adhoc, ai_QueryOptions_adhoc, ZEND_ACC_PUBLIC)
    PHP_ME(QueryOptions, namedParameters, ai_QueryOptions_namedParameters, ZEND_ACC_PUBLIC)
    PHP_ME(QueryOptions, positionalParameters, ai_QueryOptions_positionalParameters, ZEND_ACC_PUBLIC)
    PHP_ME(QueryOptions, rawParameter, ai_QueryOptions_rawParameter, ZEND_ACC_PUBLIC)
    PHP_ME(QueryOptions, maxParallelism, ai_QueryOptions_maxParallelism, ZEND_ACC_PUBLIC)
    PHP_ME(QueryOptions, profile, ai_QueryOptions_profile, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

PHP_METHOD(Bucket, query)
{
    lcb_STATUS err;
    zend_string *statement;
    zval *options = NULL;

    int rv = zend_parse_parameters_throw(ZEND_NUM_ARGS() TSRMLS_CC, "S|O", &statement, &options, pcbc_query_options_ce);
    if (rv == FAILURE) {
        RETURN_NULL();
    }

    pcbc_bucket_t *bucket = Z_BUCKET_OBJ_P(getThis());

    lcb_CMDN1QL *cmd;
    lcb_cmdn1ql_create(&cmd);
    lcb_cmdn1ql_callback(cmd, n1qlrow_callback);
    lcb_cmdn1ql_statement(cmd, ZSTR_VAL(statement), ZSTR_LEN(statement));
    if (options) {
        zval *prop, ret;
        prop = zend_read_property(pcbc_query_options_ce, options, ZEND_STRL("timeout"), 0, &ret);
        if (Z_TYPE_P(prop) == IS_LONG) {
            lcb_cmdn1ql_timeout(cmd, Z_LVAL_P(prop));
        }
        prop = zend_read_property(pcbc_query_options_ce, options, ZEND_STRL("consistency"), 0, &ret);
        if (Z_TYPE_P(prop) == IS_LONG) {
            zend_long val = Z_LVAL_P(prop);
            switch (val) {
                case PCBC_N1QL_CONSISTENCY_NOT_BOUNDED:
                    lcb_cmdn1ql_consistency(cmd, LCB_N1QL_CONSISTENCY_NONE);
                    break;
                case PCBC_N1QL_CONSISTENCY_REQUEST_PLUS:
                    lcb_cmdn1ql_consistency(cmd, LCB_N1QL_CONSISTENCY_REQUEST);
                    break;
                case PCBC_N1QL_CONSISTENCY_STATEMENT_PLUS:
                    lcb_cmdn1ql_consistency(cmd, LCB_N1QL_CONSISTENCY_STATEMENT);
                    break;
            }
        }
        prop = zend_read_property(pcbc_query_options_ce, options, ZEND_STRL("readonly"), 0, &ret);
        switch (Z_TYPE_P(prop)) {
            case IS_TRUE:
                lcb_cmdn1ql_readonly(cmd, 1);
                break;
            case IS_FALSE:
                lcb_cmdn1ql_readonly(cmd, 0);
                break;
        }
        prop = zend_read_property(pcbc_query_options_ce, options, ZEND_STRL("adhoc"), 0, &ret);
        switch (Z_TYPE_P(prop)) {
            case IS_TRUE:
                lcb_cmdn1ql_adhoc(cmd, 1);
                break;
            case IS_FALSE:
                lcb_cmdn1ql_adhoc(cmd, 0);
                break;
        }
        prop = zend_read_property(pcbc_query_options_ce, options, ZEND_STRL("scan_cap"), 0, &ret);
        if (Z_TYPE_P(prop) == IS_LONG) {
            lcb_cmdn1ql_scan_cap(cmd, Z_LVAL_P(prop));
        }
        prop = zend_read_property(pcbc_query_options_ce, options, ZEND_STRL("pipeline_cap"), 0, &ret);
        if (Z_TYPE_P(prop) == IS_LONG) {
            lcb_cmdn1ql_pipeline_cap(cmd, Z_LVAL_P(prop));
        }
        prop = zend_read_property(pcbc_query_options_ce, options, ZEND_STRL("pipeline_batch"), 0, &ret);
        if (Z_TYPE_P(prop) == IS_LONG) {
            lcb_cmdn1ql_pipeline_batch(cmd, Z_LVAL_P(prop));
        }
        prop = zend_read_property(pcbc_query_options_ce, options, ZEND_STRL("named_params"), 0, &ret);
        if (Z_TYPE_P(prop) == IS_ARRAY) {
            HashTable *ht = HASH_OF(prop);
            zend_string *string_key = NULL;
            zval *entry;
            ZEND_HASH_FOREACH_STR_KEY_VAL(ht, string_key, entry) {
                if (string_key && Z_TYPE_P(entry) == IS_STRING) {
                    lcb_cmdn1ql_named_param(cmd, ZSTR_VAL(string_key), ZSTR_LEN(string_key),
                            Z_STRVAL_P(entry), Z_STRLEN_P(entry));
                }
            }
            ZEND_HASH_FOREACH_END();
        }
        prop = zend_read_property(pcbc_query_options_ce, options, ZEND_STRL("positional_params"), 0, &ret);
        if (Z_TYPE_P(prop) == IS_ARRAY) {
            HashTable *ht = HASH_OF(prop);
            zval *entry;
            ZEND_HASH_FOREACH_VAL(ht, entry) {
                if (Z_TYPE_P(entry) == IS_STRING) {
                    lcb_cmdn1ql_positional_param(cmd, Z_STRVAL_P(entry), Z_STRLEN_P(entry));
                }
            }
            ZEND_HASH_FOREACH_END();
        }
        prop = zend_read_property(pcbc_query_options_ce, options, ZEND_STRL("raw_params"), 0, &ret);
        if (Z_TYPE_P(prop) == IS_ARRAY) {
            HashTable *ht = HASH_OF(prop);
            zend_string *string_key = NULL;
            zval *entry;
            ZEND_HASH_FOREACH_STR_KEY_VAL(ht, string_key, entry) {
                if (string_key && Z_TYPE_P(entry) == IS_STRING) {
                    lcb_cmdn1ql_option(cmd, ZSTR_VAL(string_key), ZSTR_LEN(string_key),
                            Z_STRVAL_P(entry), Z_STRLEN_P(entry));
                }
            }
            ZEND_HASH_FOREACH_END();
        }
    }

    lcb_N1QL_HANDLE *handle = NULL;
    lcb_cmdn1ql_handle(cmd, &handle);

    lcbtrace_SPAN *span = NULL;
    lcbtrace_TRACER *tracer = lcb_get_tracer(bucket->conn->lcb);
    if (tracer) {
        span = lcbtrace_span_start(tracer,"php/n1ql", 0, NULL);
        lcbtrace_span_add_tag_str(span, LCBTRACE_TAG_COMPONENT, pcbc_client_string);
        lcbtrace_span_add_tag_str(span, LCBTRACE_TAG_SERVICE, LCBTRACE_TAG_SERVICE_N1QL);
        lcb_cmdn1ql_parent_span(cmd, span);
    }
    rv = object_init_ex(return_value, pcbc_query_result_impl_ce);
    if (rv != SUCCESS) {
        return;
    }
    zval rows;
    array_init(&rows);
    zend_update_property(pcbc_query_result_impl_ce, return_value, ZEND_STRL("rows"), &rows TSRMLS_CC);
    struct query_cookie cookie = {
        LCB_SUCCESS,
        return_value
    };
    err = lcb_n1ql(bucket->conn->lcb, &cookie, cmd);
    lcb_cmdn1ql_destroy(cmd);
    if (err == LCB_SUCCESS) {
        lcb_wait(bucket->conn->lcb);
        err = cookie.rc;
    }
    if (span) {
        lcbtrace_span_finish(span, LCBTRACE_NOW);
    }
    if (err != LCB_SUCCESS) {
        throw_lcb_exception(err, NULL);
    }
}

PHP_MINIT_FUNCTION(BucketQuery)
{
    zend_class_entry ce;

    INIT_NS_CLASS_ENTRY(ce, "Couchbase", "QueryOptions", pcbc_query_options_methods);
    pcbc_query_options_ce = zend_register_internal_class(&ce TSRMLS_CC);

    zend_declare_property_null(pcbc_query_options_ce, ZEND_STRL("timeout"), ZEND_ACC_PRIVATE TSRMLS_CC);
    zend_declare_property_null(pcbc_query_options_ce, ZEND_STRL("adhoc"), ZEND_ACC_PRIVATE TSRMLS_CC);
    zend_declare_property_null(pcbc_query_options_ce, ZEND_STRL("readonly"), ZEND_ACC_PRIVATE TSRMLS_CC);
    zend_declare_property_null(pcbc_query_options_ce, ZEND_STRL("scan_cap"), ZEND_ACC_PRIVATE TSRMLS_CC);
    zend_declare_property_null(pcbc_query_options_ce, ZEND_STRL("pipeline_batch"), ZEND_ACC_PRIVATE TSRMLS_CC);
    zend_declare_property_null(pcbc_query_options_ce, ZEND_STRL("pipeline_cap"), ZEND_ACC_PRIVATE TSRMLS_CC);
    zend_declare_property_null(pcbc_query_options_ce, ZEND_STRL("consistency"), ZEND_ACC_PRIVATE TSRMLS_CC);
    zend_declare_property_null(pcbc_query_options_ce, ZEND_STRL("positional_params"), ZEND_ACC_PRIVATE TSRMLS_CC);
    zend_declare_property_null(pcbc_query_options_ce, ZEND_STRL("named_params"), ZEND_ACC_PRIVATE TSRMLS_CC);
    zend_declare_property_null(pcbc_query_options_ce, ZEND_STRL("raw_params"), ZEND_ACC_PRIVATE TSRMLS_CC);
    zend_declare_property_null(pcbc_query_options_ce, ZEND_STRL("max_parallelism"), ZEND_ACC_PRIVATE TSRMLS_CC);
    zend_declare_property_null(pcbc_query_options_ce, ZEND_STRL("profile"), ZEND_ACC_PRIVATE TSRMLS_CC);
    zend_declare_property_null(pcbc_query_options_ce, ZEND_STRL("consistent_with"), ZEND_ACC_PRIVATE TSRMLS_CC);

    INIT_NS_CLASS_ENTRY(ce, "Couchbase", "QueryConsistency", pcbc_query_consistency_methods);
    pcbc_query_consistency_ce = zend_register_internal_interface(&ce TSRMLS_CC);
    zend_declare_class_constant_long(pcbc_query_consistency_ce, ZEND_STRL("NOT_BOUNDED"), PCBC_N1QL_CONSISTENCY_NOT_BOUNDED TSRMLS_CC);
    zend_declare_class_constant_long(pcbc_query_consistency_ce, ZEND_STRL("REQUEST_PLUS"), PCBC_N1QL_CONSISTENCY_REQUEST_PLUS TSRMLS_CC);
    zend_declare_class_constant_long(pcbc_query_consistency_ce, ZEND_STRL("STATEMENT_PLUS"), PCBC_N1QL_CONSISTENCY_STATEMENT_PLUS TSRMLS_CC);

    INIT_NS_CLASS_ENTRY(ce, "Couchbase", "QueryProfile", pcbc_query_profile_methods);
    pcbc_query_profile_ce = zend_register_internal_interface(&ce TSRMLS_CC);
    zend_declare_class_constant_long(pcbc_query_profile_ce, ZEND_STRL("NONE"), PCBC_N1QL_PROFILE_NONE TSRMLS_CC);
    zend_declare_class_constant_long(pcbc_query_profile_ce, ZEND_STRL("PHASES"), PCBC_N1QL_PROFILE_PHASES TSRMLS_CC);
    zend_declare_class_constant_long(pcbc_query_profile_ce, ZEND_STRL("TIMINGS"), PCBC_N1QL_PROFILE_TIMINGS TSRMLS_CC);

    return SUCCESS;
}

/*
 * vim: et ts=4 sw=4 sts=4
 */
