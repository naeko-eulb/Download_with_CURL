# Ubuntu
## install libcur on Ubuntu 
$sudo apt-get install libcurl4-openssl-dev
## Complie
$g++ download.cpp -lcurl -lpthread -o download 

## Test
Link download test: http://courses.washington.edu/esrm590/data/esrm250_aut2004.iso.zip

$./download --url=http://courses.washington.edu/esrm590/data/esrm250_aut2004.iso.zip --thread=5 --conn==5 --out=esrm250_aut2004.iso.zip
## Features:
Download file with multi-connection and multi-thread

Support resume download
## How program work:
To handle with multi-thread: Using pthread library in c

To handle with multi-connection: Using multi-interface to handle more easy-interface

First, program will check file support Accept Range. If it is valid (value isn't none), program may try to resume an interrupted download)

Next, program will get content length to seperate to easy interface to get range in file to download parallel. And with every interface, it will download to a part file. After they finish, program will connect all file together and remove all part files

## References:
Tutorial CURL: https://curl.haxx.se/

Code CURL Examples: https://curl.haxx.se/libcurl/c/example.html

Fix errors: https://stackoverflow.com/

Document HTTP: https://developer.mozilla.org/en-US/docs/Web