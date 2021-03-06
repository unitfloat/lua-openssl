OpenSSL binding for Lua

/*=========================================================================*\
* lua-openssl toolkit
*
* Full and Free OpenSSL binding for Lua.
* It is not finished.
\*=========================================================================*/

Index
-----
0. Introduction
1. Version
2. Certificate
3. Public/Private Key
4. Cipher
5. Message Digest
6. PKCS7 SMIME
7. CSR & CRL
8. Pkcs12
9. Misc

I.   Howto.
II.  Examples
III. Contacts

0. Introduction
---------------

        This product includes PHP software,
        freely available from <http://www.php.net/software/>

I need a full openssl binding for lua, after googled, I can't find a fit version.
I study PHP openssl binding, and php-openssl is a good implement, It inspire me.
So I decide to write a Full and Free openssl for lua.

Because not be familiar with English language, Sorry for dimness explain puzzle you.

The target of lua-openssl is below:

a. Fully support message digest. (Finished)
b. Fully support symmetrical encrypt/decrypt.(Finished)
c. Fully support RSA low level api.(NOT FINISHED)
d. Fully support X509 and PKCS.(NOT FINISHED)
e. Fully support unsymmetrical encrypt/decrypt/sign/verify.
f. Fully support SSL/TLS.(NOT BEGIN TO DEV)


Most of the functions require a key or a certificate as a parameter; to make
things easy for you to use openssl, this extension allows you to specify
certificates or key in the following way:

1. As an openssl.x509 object returned from openssl.x509_read
2. As an openssl.evp_pkey object return from openssl.pkey_read
   or openssl.pkey_new

Similarly, you can use the following methods of specifying a public key:

1. As a key object returned from object:get_public

There are many important lua object type:
	openssl.bio,		
	openssl.x509,
	openssl.stack_of_x509,
	openssl.x509_req,
	openssl.evp_pkey,
	openssl.evp_digest,
	openssl.evp_cipher,
	openssl.engine
	openssl.evp_cipher_ctx
	openssl.evp_digest_ctx
	...
They are short write as bio, x509, sk_x509, x509_req, evp_pkey,evp_digest, evp_cipher,
	engine(not used now!), cipher_ctx,  digest_ctx

Notes, In next section of this document,

    => means return a lua_openssl object
    -> means return a basic type of lua

If function return a nil, it will be follow by a error number and string

1. Introduction
---------------

lua-openssl head version 0.0.5.
lua-openssl tookit worked on lua(5.1 or 5.2), openssl(0.9.8 or above 1.0.0).

lua-openssl version howto get?

    openssl = require'openssl'
    lua_openssl_version, lua_version, openssl_version = openssl.version()

2. Certificate
--------------

openssl.x509_read(string val) => x509
    val is a string containing the data from the certificate file

x509:export([bool notext=true]) -> string
    export x509 as certificate content data

x509:parse([bool shortnames=true]) -> table
    return a table which contain all x509 information


x509:get_public() => evp_pkey

x509:check(evp_pkey pkey) -> boolean
x509:check(sk_x509 ca [,sk_x509 untrusted[,string purpose]])->boolean
    purpose canbe one of: ssl_client, ssl_server, ns_ssl_server, smime_sign,
    smime_encrypt, crl_sign, any, ocsp_helper, timestamp_sign

    ca is an openssl.stack_of_x509 object contain certchain.
    untrusted is an openssl.stack_of_x509 object containing a bunch of certs
    that are not trusted but may be useful in validating the certificate.


openssl.stack_of_x509 is an important object in lua-openssl, it can be used
as a certchaina, trusted CA files or unstrustcerts.

openssl.sk_x509_read(filename) => sk_x509
openssl.sk_x509_new([table array={}]} ->sk_x509

sk_x509:push(openssl.x509 cert) => sk_x509
sk_x509:pop()	=> x509

sk_x509:set(number i, x509 cert) =>sk_x509
sk_x509:get(number i) => x509

sk_x509:insert(x509 cert,number i, ) =>sk_x509
sk_x509:delete(number i) => x509

sk_x509:totable() -> table
    tableas array contain x509 from index 1

sk_x509:sort()

#sk_x509  -> number
    return number of certs in stack_of_x509

3. Public/Private key functions
-------------------------------

openssl.evp_new([string alg='rsa' [,int bits=1024|512, [...]]]) => evp_pkey

    default generate RSA key, bits=1024, 3rd paramater e default is 0x10001
    dsa,with bits default 1024 ,and seed data default have no data
    dh, with bits(prime_len) default 512, and generator default is
    ec, not use any paramater, not fully support,paramater maybe
    ('ec','prime256v1',[named=true]),if named give false,will have
    explicit ec parameters,others only include a curve name.

openssl.pkey_new([table args]) =>  evp_pkey
    args = {dsa={n=,e=,...}|dh={}|dsa={}}
    private key should has it factor named n,q,e and so on, value is hex
    encoded string

openssl.pkey_read(string data|x509 cert [,bool public_key=true
    [,string passphrase]]) => evp_pkey

   Read from a file or a data, coerce it into a EVP_PKEY object.
   It can be:
       1. X509 object -> public key will be extracted from it
       2. interpreted as the data from the cert/key file and interpreted in
       same way as openssl_get_privatekey()

    NOTE: If you are requesting a private key but have not specified a
    passphrase, you should use an empty string rather than NULL for the
    passphrase - NULL causes a passphrase prompt to be emitted Lua error !

evp_pkey:export([boolean only_public = false [,boolean raw_key=false [,boolean pem=true,[, string passphrase]]]])
   -> string

   If only_public is true, export will export public key
   If raw_key is true, export will export rsa,dsa or dh data
   if passphrase exist, export key will be encrypt with it

	
evp_peky:parse(evp_pkey key) -> table
    returns an table with the key details (bits, pkey, type)
    pkey may be rsa, dh, dsa showd as table with factor hex encoded bignum.

evp_pkey:is_private() -> boolean
    Check whether the supplied key is a private key by checking if the secret
    prime factors are set

About padding

    Now supporting 6 padding mode, they are:
	pkcs1:	RSA_PKCS1_PADDING	1
	sslv23:	RSA_SSLV23_PADDING	2
	no:	RSA_NO_PADDING		3
	oaep:	RSA_PKCS1_OAEP_PADDING	4
	x931:	RSA_X931_PADDING	5
	pss:	RSA_PKCS1_PSS_PADDING	6

   If input other padding string value other(ingore capatical) than above,
   will raise lua error.

evp_pkey:encrypt(string data [,string padding=pkcs1]) -> string
evp_pkey:decrypt(string data [,string padding=pkcs1]) -> string

4. Cipher
---------

openssl.get_cipher(string alg|number alg_id) => evp_pkey
    return a evp_cipher method
openssl.get_cipher([bool aliases = true]) ->table
    return all ciphers methods default with alias,

evp_cipher:info() ->table
    result with name, block_size,key_length,iv_length,flags,mode keys

evp_cipher:encrypt_init([ string key [,string iv [,boolean nopad [,engine engimp]]]])
    => cipher_ctx
evp_cipher:decrypt_init([ string key [,string iv [,boolean nopad [,engine engimp]]]])
    => cipher_ctx
evp_cipher:cipher_init(bool enc, [, string key [,string iv
    [,engine engimp]]]) => cipher_ctx

evp_cipher:encrypt(string data, [ string key [,string iv
    [,engine engimp]]]) -> string

evp_cipher:decrypt(string data, [ string key [,string iv
    [,engine engimp]]]) -> string


cipher_ctx:info() ->table
    result with block_size,key_length,iv_length,flags,mode,nid,type
    and evp_cipher object keys
cipher_ctx:encrypt_update(string data)->string
    return string may be 0 length
cipher_ctx:encrypt_final()->string
cipher_ctx:decrypt_update(string data)->string
    return string may be 0 length
cipher_ctx:decrypt_final()->string

cipher_ctx:update(string data)->string
    return string may be 0 length
cipher_ctx:final()->string

cipher_ctx:cleanup() -> boolean
    reset state make object resulable.

5. Message Digest
-----------------

openssl.get_digest(string alg|int alg_id) => digest_ctx
    return a evp_digest object
openssl.get_digest([bool alias=true]) -> table
    return all md methods default with alias

evp_digest:info() -> table
    return a table with key nid,name, size, block_size, pkey_type, flags
evp_digest:evp_digest(string in) -> string
    return binary evp_digest result

evp_digest:init() => digest_ctx

digest_ctx:info() -> table
    return a table with key block_size, size, type and diget object
digest_ctx:update(string data) -> boolean
digest_ctx:final() -> string
digest_ctx:cleanup() ->boolean

6. PKCS7 (S/MIME) Sign/Verify/Encrypt/Decrypt Functions:
-------------------------------------------------------

These functions allow you to manipulate S/MIME messages!

They are based on apps/smime.c from the openssl dist, so for information,
see the documentation for openssl.

Strings "detached", "nodetached", "text", "nointern", "noverify",  "nochain",
"nocerts", "noattr", "binary", "nosigs" are supported

Decrypts the S/MIME message in the BIO object and and output the results to
BIO object. recipcert is a CERT for one of the recipients. recipkey
specifies the private key matching recipcert.

headers is an array of headers to prepend to the message: they will
not be included in the encoded section.
flags is flag information as described above.
Hint: you will want to put "To", "From", and "Subject" headers in headers.
Headers can be either an assoc array keyed by header named, or can be
and indexed array containing a single header line per value.

openssl.pkcs7_sign(bio in, bio out, x509 signcert, evp_pkey signkey,
	table headers [, string flags [,stack_of_x509 extracerts]])->boolean

   Signs the MIME message in the BIO in with signcert/signkey and output the
   result to BIO out.
   headers lists plain text headers to exclude from the signed portion of the
   message, and should include to, from and subject as a minimum

openssl.pkcs7_verify(bio in, string flags [, stack_of_x509 signerscerts,
   [, stack_of_x509 cacerts, [, stack_of_x509 extracerts [,bio content])
	->boolean

   Verifys that the data block is intact, the signer is who they say they are,
   and returns the CERTs of the signers

openssl.pkcs7_encrypt(bio in, bio out, stack_of_x509 recipcerts, table header
    [, string flags [,evp_cipher md] -> boolean

    Encrypts the message in the file named infile with the certificates in
    recipcerts and output the result to the file named outfile

openssl.pkcs7_decrypt(bio in, bio out, x509 recipcert [,evp_pkey recipkey])
	->boolean	

7. Certificate sign request and Certificate revocked list
---------------------------------------------------------

openssl.csr_new(evp_pkey privkey, table dn={} [,table args = nil]) => x509_req
openssl.csr_read(string data) => x509_req

csr:sign(x509 cert, evp_pkey privkey, table arg) -> x509
	args must have serialNumber as hexecoded string, num_days as number
csr:export([boolean pem=true [, boolean noext=true]])->string
csr:get_public() -> evp_pkey


openssl.crl_read(string data) => x509_crl
openssl.crl_new(number version, x509 cacert,number lastUpdate,
    number nextUpdate, table revoked{hexserial=time,...}) => x509_crl

    Create a new X509 CRL object, lastUpdate and nextUpdate is time_t value,
    revoked is a table that hex cert serial is key, and time_t revocked time.

crl:verify(x509 cacert) -> boolean
crl:sign(evp_pkey privkey, [string alg=sha1WithRSAEncryption|digest md])
    -> boolean
crl:sort()

crl:set_version(number version=0) -> boolean
crl:set_update_time([number lastUpdate=date() [, number nextUpdate=last+7d]])
    -> boolean
crl:set_issuer(x509 cacert) -> boolean
crl:add_revocked(string hexserial [,number time=now()
    [, string reason|number reason = 0]) -> boolean

crl:parse() -> table
    table content is below.
    {
	sig_alg=sha1WithRSAEncryption
	lastUpdate=110627103001Z
	issuer={
		CN=CA
		C=CN
	}
	nextUpdate=110707110000Z
	nextUpdate_time_t=1310032800
	lastUpdate_time_t=1309167001
	crl_number=1008
	version=1
	hash=258a7571
	revoked={
		1={
			time=1225869543
			serial=4130323030303030303030314632
			reason=Superseded
		},
		...
	}
    }



8. PKCS12 Function
------------------

openssl.pkcs12_read(string pkcs12data, string pass) -> table
	Parses a PKCS12 data to an table
	return a table which have cert, pkey, and extracerts three keys

openssl.pkcs12_export(x509 cert, evp_pkey pkey, string pass
	[[, string friendname ], table extracerts])) -> string
	Creates and exports a PKCS12 data

	friendname is options, if exist, must at 4th paramater
	extracerts is options, it can be at 4th or 5th(if friendname exist) paramater


9. Misc Function
----------------

openssl.object_create(string oid, string name [, string alias] ) -> boolean
    Add one object.
openssl.object_create(tables args) -> boolean
    Add a lot of object, args must be array index start from 1, and every
    node is a table have oid,name and optional alias key.

openssl.random_bytes(number length [, boolean strong=false])
    -> string, boolean
    Returns a string of the length specified filled with random bytes

openssl.error_string()-> number, string
    If found error, it will return a error number code, followedd by string
    description or it will return nothing and clear error state,
    so you can call it twice.


openssl.sign(string data,  evp_pkey key [, evp_digest md|string md_alg=SHA1])
    ->string
    Uses key to create signature for data, returns signed result

openssl.verify(string data, string signature, evp_pkey key
    [, evp_digest md|string md_alg=SHA1]) ->boolean
    Uses key to verify that the signature is correct for the given data.

openssl.seal(string data, table pubkeys [, evp_cipher enc|string md_alg=RC4])
    -> string, table
    Encrypts data using pubkeys, so that only owners of the respective
    private keys and ekeys can decrypt and read the data. Returns the
    sealed data and table contain ekeys hold envelope keys on success,
    else nil.

openssl.open(string data, string ekey, int privkey,
    [, evp_cipher enc|string md_alg=RC4]) -> string

Opens (decrypts) sealed data using a private key and the corresponding
envelope key. Returns decrypted data on success and nil on failure.


openssl.bio_new_mem([string data]) => bio
    If data is none or nil, that will be a output object or input.

openssl.bio_new_file(string file [,string mode=r]) => bio
    If mode not gived, will return a input bio object.

openssl.bio_new_mem([string data]) => bio
    Create a memory bio, if data gived, that will be memory buffer data
openssl.bio_new_file(string file, [string mode='r'])->bio
    Create a file bio, if mode not gived, that default is 'r'

BIO object
bio:read(number len) -> string
bio:gets([number len=256]) -> string
bio:write(string data)->number
bio:puts(string data)->number

bio:get_mem()->string
    only support bio mem bio
bio:close()
bio:type()->string
bio:reset()

I.   HOWTO
----------

Howto 1: Build on Linux/Unix System.

   Before build, please change setting in config file
   work with Lua5.1�� sould support lua5.2 by update config file

	make
	make install
	make clean

Howto 2: Build on Windows with MSVC.

   Before build, please change setting in config.win file
   work with Lua5.1�� sould support lua5.2 by update config.win file

	nmake -f makefile.win
	nmake -f makefile.win install
	nmake -f makefile.win clean


Howto 3: Build on Windows with mingw.


II.  Example usage
------------------

Example 1: short encrypt/decrypt

	local evp_cipher = openssl.get_cipher('des')
	m = 'abcdefghick'
	key = m
	cdata = evp_cipher:encrypt(m,key)
	m1  = evp_cipher:decrypt(cdata,key)
	assert(cdata==m1)

Example 2: quich evp_digest

        md = openssl.get_digest('md5')
        m = 'abcd'
        aa = md:evp_digest(m)

        mdc=md:init()
        mdc:update(m)
        bb = mdc:final()
        assert(aa==bb)
	
Example 3:  Iterator a openssl.stack_of_x509(sk_x509) object

	n = #sk
	for i=1, n do
		x = sk:get(i)
	end

Example 4: read and parse certificate

	local openssl = require('openssl')

	function dump(t,i)
		for k,v in pairs(t) do
			if(type(v)=='table') then
				print( string.rep('\t',i),k..'={')
				dump(v,i+1)
				print( string.rep('\t',i),k..'=}')
			else
				print( string.rep('\t',i),k..'='..tostring(v))
			end
		end
	end

	function test_x509()
		local x = openssl.x509_read(certasstring)
		print(x)
		t = x:parse()
		dump(t,0)
		print(t)
	end

	test_x509()

More please see test lua script file.

  lua-openssl LICENSE
  ===================

  --------------------------------------------------------------------
  Copyright (c) 2011 - 2012 zhaozg, zhaozg(at)gmail.com

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to
  deal in the Software without restriction, including without limitation the
  rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
  sell copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
  --------------------------------------------------------------------
