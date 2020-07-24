# Ubuntu
## install libcur on Ubuntu 
sudo apt-get install libcurl4-openssl-dev
## Complie
g++ download.cpp -lcurl -lpthread -o download 

## Test
Link download test: http://courses.washington.edu/esrm590/data/esrm250_aut2004.iso.zip
./download --url=http://courses.washington.edu/esrm590/data/esrm250_aut2004.iso.zip --thread=5 --conn==5 --out=esrm250_aut2004.iso.zip
## Complete: 
download file using thread with single connection
## Incomplete:
Handle multi-connection in thread (Error: Failed writing received data to disk/application)
Support resume download
## References:
Tutorial CURL: https://curl.haxx.se/
Code CURL Examples: https://curl.haxx.se/libcurl/c/example.html
