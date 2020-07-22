# install libcur on Ubuntu 
sudo apt-get install libcurl4-openssl-dev
# Complie C++
g++ download.cpp -lcurl -o download 

# CURL Tutorial

# Compiling the Program
## set your compiler's include path to point to the directory
$ curl-config --cflags

# Linking the Program with libcurl
$ curl-config --libs

# SSL or Not
## If a supported SSL library was detected properly at build-time, libcurl will be built with SSL support
## To figure out if an installed libcurl has been built with SSL support enabled, use 'curl-config' like this:
$ curl-config --feature

# Portable Code in a Portable World -> run on a large amount of different operating systems and environments. (Maybe asked in interview)

# Global Preparation
## curl_global_init()

### CURL_GLOBAL_ALL:  will make it initialize all known internal sub modules, and might be a good default option

### CURL_GLOBAL_WIN32: which only does anything on Windows machines.
-> it'll make libcurl initialize the win32 socket stuff

### CURL_GLOBAL_SSL: which only does anything on libcurls compiled and built SSL-enabled. 


## libcurl has a default protection mechanism that detects if curl_global_init hasn't been called by the time curl_easy_perform is called

## When the program no longer uses libcurl, it should call curl_global_cleanup

# Features libcurl Provides
By calling curl_version_info and checking out the details of the returned struct

## Two Interfaces
libcurl first introduced the so called easy interface. All operations in the easy interface are prefixed with 'curl_easy'. 
libcurl also offers another interface that allows multiple simultaneous transfers in a single thread, the so called multi interface

## Handle the Easy libcurl
ou need one handle for each easy session you want to perform. Basically, you should use one handle for every thread you plan to use for transferring. You must never share the same handle in multiple threads.
Get an easy handle with

	easyhandle = curl_easy_init();

You set properties and options for this handle using curl_easy_setopt.

curl_easy_reset: blank all previously set options for a single easy handle

curl_easy_duphandle: make a clone of an easy handle (with all its set options)

curl_easy_setopt(handle, CURLOPT_URL, "http://domain.com/"); set your preferred URL to transfer with CURLOPT_URL in a manner similar to:

I assume that you would like to get the data passed to you directly instead of simply getting it passed to stdout. So, you write your own function that matches this prototype:

 size_t write_data(void *buffer, size_t size, size_t nmemb, void *userp);

You tell libcurl to pass all data to this function by issuing a function similar to this:

 curl_easy_setopt(easyhandle, CURLOPT_WRITEFUNCTION, write_data);

You can control what data your callback function gets in the fourth argument by setting another property:

 curl_easy_setopt(easyhandle, CURLOPT_WRITEDATA, &internal_struct);

libcurl itself won't touch the data you pass with CURLOPT_WRITEDATA.

write the data to a different file handle by passing a 'FILE * ' to a file opened for writing with the CURLOPT_WRITEDATA option.

On some platforms[2], libcurl won't be able to operate on files opened by the program. Thus, if you use the default callback and pass in an open file with CURLOPT_WRITEDATA, it will crash. You should therefore avoid this to make your program run fine virtually everywhere.
(CURLOPT_WRITEDATA was formerly known as CURLOPT_FILE)

If you're using libcurl as a win32 DLL, you MUST use the CURLOPT_WRITEFUNCTION if you set CURLOPT_WRITEDATA - or you will experience crashes.

 success = curl_easy_perform(easyhandle);
curl_easy_perform will connect to the remote site, do the necessary commands and receive the transfer.
Whenever it receives data, it calls the callback function we previously set
Your callback function should return the number of bytes it "took care of". If that is not the exact same amount of bytes that was passed to it, libcurl will abort the operation and return with an error code.

 CURLOPT_ERRORBUFFER to point libcurl to a buffer of yours where it'll store a human readable error message as well.

??? For some protocols, downloading a file can involve a complicated process of logging in, setting the transfer mode, changing the current directory and finally transferring the file data. libcurl takes care of all that complication for you. Given simply the URL to a file, libcurl will take care of all the details needed to get the file moved from one machine to another.

# libcurl thread safety

## Multi-threading with libcurl

### Handles. 
You must never share the same handle in multiple threads
You can pass the handles around among threads, but you must never use a single handle from more than one thread at any given time.

### Shared Objects
You can share certain data between multiple handles by using the share interface but you must provide your own locking and set curl_share_setopt CURLSHOPT_LOCKFUNC and CURLSHOPT_UNLOCKFUNC.

## TLS: Transport Layer Security
If you are accessing HTTPS or FTPS URLs in a multi-threaded manner, you are then of course using the underlying SSL library multi-threaded and those libs might have their own requirements on this issue. You may need to provide one or two functions to allow it to function properly:

OpenSSL

OpenSSL 1.1.0+ "can be safely used in multi-threaded applications provided that support for the underlying OS threading API is built-in." In that case the engine is used by libcurl in a way that is fully thread-safe.

https://www.openssl.org/docs/man1.1.0/man3/CRYPTO_THREAD_run_once.html#DESCRIPTION

OpenSSL <= 1.0.2 the user must set callbacks.

https://www.openssl.org/docs/man1.0.2/man3/CRYPTO_set_locking_callback.html#DESCRIPTION

https://curl.haxx.se/libcurl/c/opensslthreadlock.html

GnuTLS

https://gnutls.org/manual/html_node/Thread-safety.html

NSS

thread-safe already without anything required.

Secure-Transport

The engine is used by libcurl in a way that is fully thread-safe.

WinSSL

The engine is used by libcurl in a way that is fully thread-safe.

wolfSSL

The engine is used by libcurl in a way that is fully thread-safe.

BoringSSL

The engine is used by libcurl in a way that is fully thread-safe.

## Other areas of caution

Signals

Signals are used for timing out name resolves (during DNS lookup) - when built without using either the c-ares or threaded resolver backends. When using multiple threads you should set the CURLOPT_NOSIGNAL option to 1L for all handles. Everything will or might work fine except that timeouts are not honored during the DNS lookup - which you can work around by building libcurl with c-ares or threaded-resolver support. c-ares is a library that provides asynchronous name resolves. On some platforms, libcurl simply will not function properly multi-threaded unless the CURLOPT_NOSIGNAL option is set.

When CURLOPT_NOSIGNAL is set to 1L, your application needs to deal with the risk of a SIGPIPE (that at least the OpenSSL backend can trigger). Note that setting CURLOPT_NOSIGNAL to 0L will not work in a threaded situation as there will be race where libcurl risks restoring the former signal handler while another thread should still ignore it.

Name resolving

gethostby* functions and other system calls. These functions, provided by your operating system, must be thread safe. It is very important that libcurl can find and use thread safe versions of these and other system calls, as otherwise it can't function fully thread safe. Some operating systems are known to have faulty thread implementations. We have previously received problem reports on *BSD (at least in the past, they may be working fine these days). Some operating systems that are known to have solid and working thread support are Linux, Solaris and Windows.
#
curl_global_* functions

These functions are not thread safe. If you are using libcurl with multiple threads it is especially important that before use you call curl_global_init or curl_global_init_mem to explicitly initialize the library and its dependents, rather than rely on the "lazy" fail-safe initialization that takes place the first time curl_easy_init is called. For an in-depth explanation refer to libcurl section GLOBAL CONSTANTS.

Memory functions

These functions, provided either by your operating system or your own replacements, must be thread safe. You can use curl_global_init_mem to set your own replacement memory functions.

Non-safe functions

CURLOPT_DNS_USE_GLOBAL_CACHE is not thread-safe.

This HTML page was made with roffit.

## When It Doesn't Work
There will always be times when the transfer fails for some reason. You might have set the wrong libcurl option or misunderstood what the libcurl option actually does, or the remote server might return non-standard replies that confuse the library which then confuses your program.

There's one golden rule when these things occur: set the CURLOPT_VERBOSE option to 1. It'll cause the library to spew out the entire protocol details it sends, some internal info and some received protocol data as well (especially when using FTP). If you're using HTTP, adding the headers in the received output to study is also a clever way to get a better understanding why the server behaves the way it does. Include headers in the normal body output with CURLOPT_HEADER set 1.

Of course, there are bugs left. We need to know about them to be able to fix them, so we're quite dependent on your bug reports! When you do report suspected bugs in libcurl, please include as many details as you possibly can: a protocol dump that CURLOPT_VERBOSE produces, library version, as much as possible of your code that uses libcurl, operating system name and version, compiler name and version etc.

If CURLOPT_VERBOSE is not enough, you increase the level of debug data your application receive by using the CURLOPT_DEBUGFUNCTION.

Getting some in-depth knowledge about the protocols involved is never wrong, and if you're trying to do funny things, you might very well understand libcurl and how to use it better if you study the appropriate RFC documents at least briefly.