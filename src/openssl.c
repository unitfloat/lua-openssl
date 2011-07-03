#include "openssl.h"


/* {{{ arginfo */

/* }}} */

/* true global; readonly after module startup */
char default_ssl_conf_filename[MAX_PATH];

/* {{{ openssl_functions[]
 */
const static luaL_Reg eay_functions[] = {
	/* pkey */

	{"pkey_read",			openssl_pkey_read	},
	{"pkey_new",			openssl_pkey_new	},

	/* x.509 cert funcs */
	{"x509_read",			openssl_x509_read	},

	/* cipher/digest functions */
	{"get_digest",			openssl_get_digest},
	{"get_cipher",			openssl_get_cipher},

	/* misc function */
	{"error_string",		openssl_error_string	},

/* PKCS12 funcs */
	{"pkcs12_export",		openssl_pkcs12_export	},
	{"pkcs12_read",			openssl_pkcs12_read	},

/* CSR funcs */
	{"csr_new",				openssl_csr_new	},
	{"csr_export",				openssl_csr_export	},
	{"csr_export_to_file",				openssl_csr_export_to_file	},
	{"csr_sign",				openssl_csr_sign	},
	{"csr_get_subject",				openssl_csr_get_subject	},
	{"csr_get_public_key",				openssl_csr_get_public_key	},

	{"sign",				openssl_sign	},
	{"verify",				openssl_verify	},
	{"seal",				openssl_seal	},
	{"open",				openssl_open	},


/* for S/MIME handling */
	{"pkcs7_verify",				openssl_pkcs7_verify	},
	{"pkcs7_decrypt",				openssl_pkcs7_decrypt	},
	{"pkcs7_sign",					openssl_pkcs7_sign	},
	{"pkcs7_encrypt",				openssl_pkcs7_encrypt	},

	{"private_encrypt",				openssl_private_encrypt	},
	{"private_decrypt",				openssl_private_decrypt	},
	{"public_encrypt",				openssl_public_encrypt	},
	{"public_decrypt",				openssl_public_decrypt	},

	{"dh_compute_key",				openssl_dh_compute_key	},
	{"random_pseudo_bytes",				openssl_random_pseudo_bytes	},

	{NULL, NULL}
};
/* }}} */


static int le_key;
static int le_x509;
static int le_csr;
static int ssl_stream_data_index;

int openssl_get_x509_list_id(void) /* {{{ */
{
	return le_x509;
}
/* }}} */

/* {{{ resource destructors */
static void pkey_free(lua_State *L)
{
	EVP_PKEY *pkey = CHECK_OBJECT(1,EVP_PKEY,"openssl.pkey");

	assert(pkey != NULL);

	EVP_PKEY_free(pkey);
}

static void csr_free(lua_State *L)
{
	X509_REQ * csr  = CHECK_OBJECT(1,X509_REQ,"openssl.x509_req");
	X509_REQ_free(csr);
}
/* }}} */
#if 0
/* {{{ openssl safe_mode & open_basedir checks */
inline static int openssl_safe_mode_chk(char *filename)
{
	if (PG(safe_mode) && (!checkuid(filename, NULL, CHECKUID_CHECK_FILE_AND_DIR))) {
		return -1;
	}
	if (check_open_basedir(filename)) {
		return -1;
	}
	
	return 0;
}
/* }}} */
/* openssl -> PHP "bridging" */
#endif



#if OPENSSL_VERSION_NUMBER >= 0x10000002L
int openssl_config_check_syntax(const char * section_label, const char * config_filename, const char * section, LHASH_OF(CONF_VALUE) * config) /* {{{ */
#else
int openssl_config_check_syntax(const char * section_label, const char * config_filename, const char * section, LHASH * config)
#endif
{
	X509V3_CTX ctx;

	X509V3_set_ctx_test(&ctx);
	X509V3_set_conf_lhash(&ctx, config);
	if (!X509V3_EXT_add_conf(config, &ctx, (char *)section, NULL)) {
		printf("Error loading %s section %s of %s",
			section_label,
			section,
			config_filename);
		return -1;
	}
	return 0;
}



static STACK_OF(X509) * array_to_X509_sk(lua_State *L, int n) /* {{{ */
{
	STACK_OF(X509) * sk = NULL;
    X509 * cert;
    int len,i;

	sk = sk_X509_new_null();

	/* get certs */
	if (lua_istable(L,n))
	{
		len = lua_objlen(L,n);
		for (i=1; i<=len; i++)
		{
			lua_rawgeti(L,n,i);
			cert = CHECK_OBJECT(-1,X509,"openssl.x509");
			cert = X509_dup(cert);
			sk_X509_push(sk, cert);
		}
	}else
	{
		cert = CHECK_OBJECT(n,X509,"openssl.x509");
		cert = X509_dup(cert);
		sk_X509_push(sk, cert);
	}
	return sk;
}
/* }}} */


/* {{{ proto crypted openssl_private_encrypt(string data, mixed key [, int padding])
   Encrypts data with private key */
LUA_FUNCTION(openssl_private_encrypt)
{
	EVP_PKEY *pkey;
	int cryptedlen;
	unsigned char *cryptedbuf = NULL;
	int ful = 0;
	long keyresource = -1;
	const char * data;
	int data_len;
	long padding = RSA_PKCS1_PADDING;
	int ret = 0;

	data = luaL_checklstring(L,1,&data_len);
	pkey = CHECK_OBJECT(2,EVP_PKEY,"openssl.evp_pkey");
	padding = luaL_optint(L,3,RSA_PKCS1_PADDING);

	cryptedlen = EVP_PKEY_size(pkey);
	cryptedbuf = malloc(cryptedlen + 1);

	switch (pkey->type) {
		case EVP_PKEY_RSA:
		case EVP_PKEY_RSA2:
			ful =  (RSA_private_encrypt(data_len, 
						(unsigned char *)data, 
						cryptedbuf, 
						pkey->pkey.rsa, 
						padding) == cryptedlen);
			break;
		default:
			luaL_error(L,"key type not supported in this PHP build!");
	}

	if (ful) {
		cryptedbuf[cryptedlen] = '\0';
		lua_pushlstring(L,(char *)cryptedbuf, cryptedlen);
		ret = 1;
	}
	if (cryptedbuf) {
		free(cryptedbuf);
	}
	if (keyresource == -1) { 
		EVP_PKEY_free(pkey);
	}
	return ret;
}
/* }}} */

/* {{{ proto decrypted openssl_private_decrypt(string data, mixed key [, int padding])
   Decrypts data with private key */
LUA_FUNCTION(openssl_private_decrypt)
{
	EVP_PKEY *pkey;
	int cryptedlen;
	unsigned char *cryptedbuf = NULL;
	unsigned char *crypttemp;
	int ful = 0;
	long padding = RSA_PKCS1_PADDING;
	long keyresource = -1;
	const char * data;
	int data_len;
	int ret = 0;

	data = luaL_checklstring(L,1,&data_len);
	pkey = CHECK_OBJECT(2,EVP_PKEY,"openssl.evp_pkey");
	padding = luaL_optint(L,3,RSA_PKCS1_PADDING);

	cryptedlen = EVP_PKEY_size(pkey);
	crypttemp = malloc(cryptedlen + 1);

	switch (pkey->type) {
		case EVP_PKEY_RSA:
		case EVP_PKEY_RSA2:
			cryptedlen = RSA_private_decrypt(data_len, 
					(unsigned char *)data, 
					crypttemp, 
					pkey->pkey.rsa, 
					padding);
			if (cryptedlen != -1) {
				cryptedbuf = malloc(cryptedlen + 1);
				memcpy(cryptedbuf, crypttemp, cryptedlen);
				ful = 1;
			}
			break;
		default:
			luaL_error(L,"key type not supported in this PHP build!");
	}

	free(crypttemp);

	if (ful) {
		lua_pushlstring(L,(char *)cryptedbuf, cryptedlen);
		ret = 1;
	}

	if (keyresource == -1) {
		EVP_PKEY_free(pkey);
	}
	if (cryptedbuf) { 
		free(cryptedbuf);
	}
	return 1;
}
/* }}} */

/* {{{ proto crypted openssl_public_encrypt(string data, mixed key [, int padding])
   Encrypts data with public key */
LUA_FUNCTION(openssl_public_encrypt)
{
	EVP_PKEY *pkey;
	int cryptedlen;
	unsigned char *cryptedbuf;
	int ful = 0;
	long keyresource = -1;
	long padding = RSA_PKCS1_PADDING;
	const char * data;
	int data_len;
	int ret = 0;

	data = luaL_checklstring(L,1,&data_len);
	pkey = CHECK_OBJECT(2,EVP_PKEY,"openssl.evp_pkey");
	padding = luaL_optint(L,3,RSA_PKCS1_PADDING);

	cryptedlen = EVP_PKEY_size(pkey);
	cryptedbuf = malloc(cryptedlen + 1);

	switch (pkey->type) {
		case EVP_PKEY_RSA:
		case EVP_PKEY_RSA2:
			ful = (RSA_public_encrypt(data_len, 
						(unsigned char *)data, 
						cryptedbuf, 
						pkey->pkey.rsa, 
						padding) == cryptedlen);
			break;
		default:
			luaL_error(L,"key type not supported in this PHP build!");

	}

	if (ful) {
		lua_pushlstring(L,(char *)cryptedbuf, cryptedlen);
		ret = 1;
	}
	if (keyresource == -1) {
		EVP_PKEY_free(pkey);
	}
	if (cryptedbuf) {
		free(cryptedbuf);
	}
	return 1;
}
/* }}} */

/* {{{ proto bool openssl_public_decrypt(string data, string &crypted, resource key [, int padding])
   Decrypts data with public key */
LUA_FUNCTION(openssl_public_decrypt)
{
	EVP_PKEY *pkey;
	int cryptedlen;
	unsigned char *cryptedbuf = NULL;
	unsigned char *crypttemp;
	int ful = 0;
	long keyresource = -1;
	long padding = RSA_PKCS1_PADDING;
	const char * data;
	int data_len;
	int ret = 0;

	data = luaL_checklstring(L,1,&data_len);
	pkey = CHECK_OBJECT(2,EVP_PKEY,"openssl.evp_pkey");
	padding = luaL_optint(L,3,RSA_PKCS1_PADDING);


	cryptedlen = EVP_PKEY_size(pkey);
	crypttemp = malloc(cryptedlen + 1);

	switch (pkey->type) {
		case EVP_PKEY_RSA:
		case EVP_PKEY_RSA2:
			cryptedlen = RSA_public_decrypt(data_len, 
					(unsigned char *)data, 
					crypttemp, 
					pkey->pkey.rsa, 
					padding);
			if (cryptedlen != -1) {
				cryptedbuf = malloc(cryptedlen + 1);
				memcpy(cryptedbuf, crypttemp, cryptedlen);
				ful = 1;
			}
			break;
			
		default:
			luaL_error(L,"key type not supported in this PHP build!");
		 
	}

	free(crypttemp);

	if (ful) {
		lua_pushlstring(L,(char *)cryptedbuf, cryptedlen);
		ret = 1;
	}

	if (cryptedbuf) {
		free(cryptedbuf);
	}
	if (keyresource == -1) {
		EVP_PKEY_free(pkey);
	}
	return 1;
}
/* }}} */

/* {{{ proto mixed openssl_error_string(void)
   Returns a description of the last error, and alters the index of the error messages. Returns false when the are no more messages */
LUA_FUNCTION(openssl_error_string)
{
	char buf[512];
	unsigned long val;
	int verbose = lua_toboolean(L,1);

	val = ERR_get_error();
	if (val) {
		lua_pushinteger(L,val);
		lua_pushstring(L, ERR_error_string(val, buf));

		return 2;
	}
	if(verbose)
	{
		ERR_print_errors_fp(stderr); 
		ERR_clear_error();
	}

	return 0;
}
/* }}} */

/* {{{ proto signature openssl_sign(string data,  mixed key[, mixed method])
   Signs data */
LUA_FUNCTION(openssl_sign)
{
	EVP_PKEY *pkey;
	int siglen;
	unsigned char *sigbuf;
	long keyresource = -1;
	const char * data;
	int data_len;
	EVP_MD_CTX md_ctx;
	long signature_algo = OPENSSL_ALGO_SHA1;
	const EVP_MD *mdtype;
	int ret = 0;

	data = luaL_checklstring(L,1,&data_len);
	pkey = CHECK_OBJECT(2,EVP_PKEY,"openssl.evp_pkey");
	mdtype = CHECK_OBJECT(3,EVP_MD,"openssl.evp_digest");

	siglen = EVP_PKEY_size(pkey);
	sigbuf = malloc(siglen + 1);

	EVP_SignInit(&md_ctx, mdtype);
	EVP_SignUpdate(&md_ctx, data, data_len);
	if (EVP_SignFinal (&md_ctx, sigbuf,(unsigned int *)&siglen, pkey)) {
		lua_pushlstring(L,(char *)sigbuf, siglen);
		ret = 1;
	} else {
		free(sigbuf);
	}
	EVP_MD_CTX_cleanup(&md_ctx);
	if (keyresource == -1) {
		EVP_PKEY_free(pkey);
	}
	return ret;
}
/* }}} */

/* {{{ proto int openssl_verify(string data, string signature, mixed key[, mixed method])
   Verifys data */
LUA_FUNCTION(openssl_verify)
{
	EVP_PKEY *pkey;
	int err;
	EVP_MD_CTX     md_ctx;
	const EVP_MD *mdtype;
	long keyresource = -1;
	const char * data;	int data_len;
	const char * signature;	int signature_len;
	long signature_algo = OPENSSL_ALGO_SHA1;
	
	data = luaL_checklstring(L,1,&data_len);
	signature = luaL_checklstring(L,2,&signature_len);
	pkey = CHECK_OBJECT(3,EVP_PKEY,"openssl.evp_pkey");
	mdtype = CHECK_OBJECT(4,EVP_MD,"openssl.evp_digest");

	EVP_VerifyInit   (&md_ctx, mdtype);
	EVP_VerifyUpdate (&md_ctx, data, data_len);
	err = EVP_VerifyFinal (&md_ctx, (unsigned char *)signature, signature_len, pkey);
	EVP_MD_CTX_cleanup(&md_ctx);

	if (keyresource == -1) {
		EVP_PKEY_free(pkey);
	}
	lua_pushinteger(L,err);
	return 1;
}
/* }}} */

/* {{{ proto sealdata,ekeys openssl_seal(string data, array pubkeys [, string method])
   Seals data */
LUA_FUNCTION(openssl_seal)
{
	EVP_PKEY **pkeys;
	long * key_resources;	/* so we know what to cleanup */
	int i, len1, len2, *eksl, nkeys;
	unsigned char *buf = NULL, **eks;
	const char * data; int data_len;
	const char *method =NULL;
	int method_len = 0;
	const EVP_CIPHER *cipher;
	EVP_CIPHER_CTX ctx;
	int ret = 0;

	data = luaL_checklstring(L,1,&data_len);
	luaL_checktype(L,2, LUA_TTABLE);
	nkeys = lua_objlen(L,2);
	method = luaL_optstring(L, 3, NULL);

	if (!nkeys) {
		luaL_error(L,"Fourth argument to openssl_seal() must be a non-empty array");
	}

	if (method) {
		cipher = EVP_get_cipherbyname(method);
		if (!cipher) {
			luaL_error(L, "Unknown signature algorithm.");
		}
	} else {
		cipher = EVP_rc4();
	}

	pkeys = malloc(nkeys*sizeof(*pkeys));
	eksl = malloc(nkeys*sizeof(*eksl));
	eks = malloc(nkeys*sizeof(*eks));
	memset(eks, 0, sizeof(*eks) * nkeys);
	key_resources = malloc(nkeys*sizeof(long));
	memset(key_resources, 0, sizeof(*key_resources) * nkeys);

	/* get the public keys we are using to seal this data */

	i = 0;
	for(i=1; i<=nkeys; i++) {
		lua_rawgeti(L,2,i);

		pkeys[i] =  CHECK_OBJECT(-1,EVP_PKEY, "openssl.evp_pkey");
		if (pkeys[i] == NULL) {
			luaL_error(L,"not a public key (%dth member of pubkeys)", i+1);
		}
		eks[i] = malloc(EVP_PKEY_size(pkeys[i]) + 1);
		lua_pop(L,1);
	}
	if (!EVP_EncryptInit(&ctx,cipher,NULL,NULL)) {
		luaL_error(L,"EVP_EncryptInit failed");
	}

#if 0
	/* Need this if allow ciphers that require initialization vector */
	ivlen = EVP_CIPHER_CTX_iv_length(&ctx);
	iv = ivlen ? malloc(ivlen + 1) : NULL;
#endif
	/* allocate one byte extra to make room for \0 */
	buf = malloc(data_len + EVP_CIPHER_CTX_block_size(&ctx));

	if (!EVP_SealInit(&ctx, cipher, eks, eksl, NULL, pkeys, nkeys) || !EVP_SealUpdate(&ctx, buf, &len1, (unsigned char *)data, data_len)) {
		free(buf);
		luaL_error(L,"EVP_SealInit failed");
	}

	EVP_SealFinal(&ctx, buf + len1, &len2);

	if (len1 + len2 > 0) {
		buf[len1 + len2] = '\0';
		lua_pushlstring(L,buf,len1 + len2);
		lua_newtable(L);
		for (i=0; i<nkeys; i++) {
			eks[i][eksl[i]] = '\0';
			lua_pushlstring(L, eks[i], eksl[i]);
			free(eks[i]);
			eks[i] = NULL;
			lua_rawseti(L,-2, i+1);
		}
		ret = 2;
#if 0
		/* If allow ciphers that need IV, we need this */
		zval_dtor(*ivec);
		if (ivlen) {
			iv[ivlen] = '\0';
			ZVAL_STRINGL(*ivec, erealloc(iv, ivlen + 1), ivlen, 0);
		} else {
			ZVAL_EMPTY_STRING(*ivec);
		}
#endif

	} else {
		free(buf);
	}


	for (i=0; i<nkeys; i++) {
		if (key_resources[i] == -1) {
			EVP_PKEY_free(pkeys[i]);
		}
		if (eks[i]) { 
			free(eks[i]);
		}
	}
	free(eks);
	free(eksl);
	free(pkeys);
	free(key_resources);
	return ret;
}
/* }}} */

/* {{{ proto opendata openssl_open(string data, string ekey, mixed privkey, string method)
   Opens data */
LUA_FUNCTION(openssl_open)
{
	EVP_PKEY *pkey;
	int len1, len2;
	unsigned char *buf;
	long keyresource = -1;
	EVP_CIPHER_CTX ctx;
	const char * data;	int data_len;
	const char * ekey;	int ekey_len;
	const char *method =NULL;
	int method_len = 0;
	const EVP_CIPHER *cipher;
	int ret = 0;

	data = luaL_checklstring(L, 1, &data_len);
	ekey = luaL_checklstring(L, 2, &ekey_len);
	pkey = CHECK_OBJECT(3,EVP_PKEY, "openssl.evp_pkey");
	method = luaL_optstring(L,4, NULL);

	if (method) {
		cipher = EVP_get_cipherbyname(method);
		if (!cipher) {
			luaL_error(L,"Unknown signature algorithm.");
		}
	} else {
		cipher = EVP_rc4();
	}
	
	buf = malloc(data_len + 1);

	if (EVP_OpenInit(&ctx, cipher, (unsigned char *)ekey, ekey_len, NULL, pkey) && EVP_OpenUpdate(&ctx, buf, &len1, (unsigned char *)data, data_len)) {
		if (!EVP_OpenFinal(&ctx, buf + len1, &len2) || (len1 + len2 == 0)) {
			free(buf);
		}
	} else {
		free(buf);
	}

	buf[len1 + len2] = '\0';
	lua_pushlstring(L, buf, len1 + len2);
	free(buf);
	return 1;
}
/* }}} */

/* SSL verification functions */

#define GET_VER_OPT(name)               (stream->context && 0 == stream_context_get_option(stream->context, "ssl", name, &val))
#define GET_VER_OPT_STRING(name, str)   if (GET_VER_OPT(name)) { convert_to_string_ex(val); str = Z_STRVAL_PP(val); }
#if 0
static int verify_callback(int preverify_ok, X509_STORE_CTX *ctx) /* {{{ */
{
	void *stream;
	SSL *ssl;
	X509 *err_cert;
	int err, depth, ret;

	ret = preverify_ok;

	/* determine the status for the current cert */
	err_cert = X509_STORE_CTX_get_current_cert(ctx);
	err = X509_STORE_CTX_get_error(ctx);
	depth = X509_STORE_CTX_get_error_depth(ctx);

	/* conjure the stream & context to use */
	ssl = X509_STORE_CTX_get_ex_data(ctx, SSL_get_ex_data_X509_STORE_CTX_idx());
	stream = SSL_get_ex_data(ssl, ssl_stream_data_index);

	/* if allow_self_signed is set, make sure that verification succeeds */
	if (err == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT && GET_VER_OPT("allow_self_signed") && zval_is_true(*val)) {
		ret = 1;
	}

	/* check the depth */
	if (GET_VER_OPT("verify_depth")) {
		convert_to_long_ex(val);

		if (depth > Z_LVAL_PP(val)) {
			ret = 0;
			X509_STORE_CTX_set_error(ctx, X509_V_ERR_CERT_CHAIN_TOO_LONG);
		}
	}

	return ret;

}
/* }}} */

int openssl_apply_verification_policy(SSL *ssl, X509 *peer, stream *stream) /* {{{ */
{
	zval **val = NULL;
	char *cnmatch = NULL;
	X509_NAME *name;
	char buf[1024];
	int err;

	/* verification is turned off */
	if (!(GET_VER_OPT("verify_peer") && zval_is_true(*val))) {
		return 0;
	}

	if (peer == NULL) {
		error_docref(NULL, E_WARNING, "Could not get peer certificate");
		return -1;
	}

	err = SSL_get_verify_result(ssl);
	switch (err) {
		case X509_V_OK:
			/* fine */
			break;
		case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
			if (GET_VER_OPT("allow_self_signed") && zval_is_true(*val)) {
				/* allowed */
				break;
			}
			/* not allowed, so fall through */
		default:
			error_docref(NULL, E_WARNING, "Could not verify peer: code:%d %s", err, X509_verify_cert_error_string(err));
			return -1;
	}

	/* if the cert passed the usual checks, apply our own local policies now */

	name = X509_get_subject_name(peer);

	/* Does the common name match ? (used primarily for https://) */
	GET_VER_OPT_STRING("CN_match", cnmatch);
	if (cnmatch) {
		int match = 0;
		int name_len = X509_NAME_get_text_by_NID(name, NID_commonName, buf, sizeof(buf));

		if (name_len == -1) {
			error_docref(NULL, E_WARNING, "Unable to locate peer certificate CN");
			return -1;
		} else if (name_len != strlen(buf)) {
			error_docref(NULL, E_WARNING, "Peer certificate CN=`%.*s' is malformed", name_len, buf);
			return -1;
		}

		match = strcmp(cnmatch, buf) == 0;
		if (!match && strlen(buf) > 3 && buf[0] == '*' && buf[1] == '.') {
			/* Try wildcard */

			if (strchr(buf+2, '.')) {
				char *tmp = strstr(cnmatch, buf+1);

				match = tmp && strcmp(tmp, buf+2) && tmp == strchr(cnmatch, '.');
			}
		}

		if (!match) {
			/* didn't match */
			error_docref(NULL, E_WARNING, "Peer certificate CN=`%.*s' did not match expected CN=`%s'", name_len, buf, cnmatch);
			return -1;
		}
	}

	return 0;
}
/* }}} */

static int passwd_callback(char *buf, int num, int verify, void *data) /* {{{ */
{
    stream *stream = (stream *)data;
    zval **val = NULL;
    char *passphrase = NULL;
    /* TODO: could expand this to make a callback into PHP user-space */

    GET_VER_OPT_STRING("passphrase", passphrase);

    if (passphrase) {
        if (Z_STRLEN_PP(val) < num - 1) {
            memcpy(buf, Z_STRVAL_PP(val), Z_STRLEN_PP(val)+1);
            return Z_STRLEN_PP(val);
        }
    }
    return 0;
}
/* }}} */

SSL *SSL_new_from_context(SSL_CTX *ctx, stream *stream) /* {{{ */
{
	zval **val = NULL;
	char *cafile = NULL;
	char *capath = NULL;
	char *certfile = NULL;
	char *cipherlist = NULL;
	int ok = 1;

	ERR_clear_error();

	/* look at context options in the stream and set appropriate verification flags */
	if (GET_VER_OPT("verify_peer") && zval_is_true(*val)) {

		/* turn on verification callback */
		SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, verify_callback);

		/* CA stuff */
		GET_VER_OPT_STRING("cafile", cafile);
		GET_VER_OPT_STRING("capath", capath);

		if (cafile || capath) {
			if (!SSL_CTX_load_verify_locations(ctx, cafile, capath)) {
				error_docref(NULL, E_WARNING, "Unable to set verify locations `%s' `%s'", cafile, capath);
				return NULL;
			}
		}

		if (GET_VER_OPT("verify_depth")) {
			convert_to_long_ex(val);
			SSL_CTX_set_verify_depth(ctx, Z_LVAL_PP(val));
		}
	} else {
		SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
	}

	/* callback for the passphrase (for localcert) */
	if (GET_VER_OPT("passphrase")) {
		SSL_CTX_set_default_passwd_cb_userdata(ctx, stream);
		SSL_CTX_set_default_passwd_cb(ctx, passwd_callback);
	}

	GET_VER_OPT_STRING("ciphers", cipherlist);
	if (!cipherlist) {
		cipherlist = "DEFAULT";
	}
	if (SSL_CTX_set_cipher_list(ctx, cipherlist) != 1) {
		return NULL;
	}

	GET_VER_OPT_STRING("local_cert", certfile);
	if (certfile) {
		X509 *cert = NULL;
		EVP_PKEY *key = NULL;
		SSL *tmpssl;
		char resolved_path_buff[MAXPATHLEN];
		const char * private_key = NULL;

		if (VCWD_REALPATH(certfile, resolved_path_buff)) {
			/* a certificate to use for authentication */
			if (SSL_CTX_use_certificate_chain_file(ctx, resolved_path_buff) != 1) {
				error_docref(NULL, E_WARNING, "Unable to set local cert chain file `%s'; Check that your cafile/capath settings include details of your certificate and its issuer", certfile);
				return NULL;
			}
			GET_VER_OPT_STRING("local_pk", private_key);

			if (private_key) {
				char resolved_path_buff_pk[MAXPATHLEN];
				if (VCWD_REALPATH(private_key, resolved_path_buff_pk)) {
					if (SSL_CTX_use_PrivateKey_file(ctx, resolved_path_buff_pk, SSL_FILETYPE_PEM) != 1) {
						error_docref(NULL, E_WARNING, "Unable to set private key file `%s'", resolved_path_buff_pk);
						return NULL;
					}
				}
			} else {
				if (SSL_CTX_use_PrivateKey_file(ctx, resolved_path_buff, SSL_FILETYPE_PEM) != 1) {
					error_docref(NULL, E_WARNING, "Unable to set private key file `%s'", resolved_path_buff);
					return NULL;
				}		
			}

			tmpssl = SSL_new(ctx);
			cert = SSL_get_certificate(tmpssl);

			if (cert) {
				key = X509_get_pubkey(cert);
				EVP_PKEY_copy_parameters(key, SSL_get_privatekey(tmpssl));
				EVP_PKEY_free(key);
			}
			SSL_free(tmpssl);

			if (!SSL_CTX_check_private_key(ctx)) {
				error_docref(NULL, E_WARNING, "Private key does not match certificate!");
			}
		}
	}
	if (ok) {
		SSL *ssl = SSL_new(ctx);

		if (ssl) {
			/* map SSL => stream */
			SSL_set_ex_data(ssl, ssl_stream_data_index, stream);
		}
		return ssl;
	}

	return NULL;
}
/* }}} */

#endif


/* {{{ proto string openssl_dh_compute_key(string pub_key, resource dh_key)
   Computes shared sicret for public value of remote DH key and local DH key */
LUA_FUNCTION(openssl_dh_compute_key)
{
	const char *pub_str;
	int pub_len;
	EVP_PKEY *pkey;
	BIGNUM *pub;
	char *data;
	int len;
	int ret = 0;

	pub_str = luaL_checklstring(L,1,&pub_len);
	pkey = CHECK_OBJECT(2,EVP_PKEY,"openssl.evp_pkey");

	if (!pkey || EVP_PKEY_type(pkey->type) != EVP_PKEY_DH || !pkey->pkey.dh) {
		luaL_error(L,"paramater 2 must dh key");
	}

	pub = BN_bin2bn((unsigned char*)pub_str, pub_len, NULL);

	data = malloc(DH_size(pkey->pkey.dh) + 1);
	len = DH_compute_key((unsigned char*)data, pub, pkey->pkey.dh);

	if (len >= 0) {
		data[len] = 0;
		lua_pushlstring(L,data,len);
		ret = 1;
	} else {
		free(data);
		ret = 0;
	}

	BN_free(pub);
	return ret;
}
/* }}} */

/* {{{ proto string openssl_random_pseudo_bytes(integer length [, &bool returned_strong_result])
   Returns a string of the length specified filled with random pseudo bytes */
LUA_FUNCTION(openssl_random_pseudo_bytes)
{
	long buffer_length;
	unsigned char *buffer = NULL;
	int strong_result = 0;
	int ret = 0;

	buffer_length = luaL_checkint(L,1);
	strong_result = lua_isnil(L,2)? 0 : lua_toboolean(L,2);

	if (buffer_length <= 0) {
		luaL_error(L,"paramater 1 must not be nego");
	}

	buffer = malloc(buffer_length + 1);

#ifdef WINDOWS
        RAND_screen();
#endif

	if ((strong_result = RAND_pseudo_bytes(buffer, buffer_length)) < 0) {
		free(buffer);
		luaL_error(L,"generate random data failed");
	}

	lua_pushlstring(L,(char *)buffer, buffer_length);

	if (strong_result) {
		lua_pushboolean(L,strong_result);
		return 2;
	}
	return 1;
}
/* }}} */


extern luaL_Reg pkey_funcs[];

LUA_API int luaopen_openssl(lua_State*L)
{
	char * config_filename;

	SSL_library_init();
	OpenSSL_add_all_ciphers();
	OpenSSL_add_all_digests();
	OpenSSL_add_all_algorithms();

	ERR_load_ERR_strings();
	ERR_load_crypto_strings();
	ERR_load_EVP_strings();


	/* Determine default SSL configuration file */
	config_filename = getenv("OPENSSL_CONF");
	if (config_filename == NULL) {
		config_filename = getenv("SSLEAY_CONF");
	}

	/* default to 'openssl.cnf' if no environment variable is set */
	if (config_filename == NULL) {
		snprintf(default_ssl_conf_filename, sizeof(default_ssl_conf_filename), "%s/%s",
			X509_get_default_cert_area(),
			"openssl.cnf");
	} else {
		strncpy(default_ssl_conf_filename, config_filename, sizeof(default_ssl_conf_filename));
	}

	openssl_register_pkey(L);
	openssl_register_x509(L);
	openssl_register_digest(L);
	openssl_register_cipher(L);
	luaL_register(L,"openssl",eay_functions);
	
	return 0;
}

/*
 * Local variables:
 * tab-width: 8
 * c-basic-offset: 8
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
