#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <curl/curl.h>
#include <string.h>
#include <pthread.h>
#include <fstream>
#ifndef WIN32
#include <unistd.h>
#endif

#define NUMT 16				// Max thread
#define MAX_MULTI_CONN 10	// Max connection in thread
using namespace std;

struct link_argv
{
	char* url;
	static int thread;
	static int conn;
	char* out;
	char* range;
	int number;
};

int link_argv::thread = 1;
int link_argv::conn = 1;

static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream)
{
  size_t written = fwrite(ptr, size, nmemb, (FILE *)stream);
  return written;
}

void parse_argv(char* argv[], int argc, struct link_argv &handle)
{
	for (int i = 1; i < argc; i++)
	{
		string part(argv[i]);
		size_t pos = part.find("=");
		if (pos == std::string::npos)
			continue;
		string header = part.substr(2, pos -2);
		string content = part.substr(pos + 1);
		if (header == "url")
		{
			handle.url = new char[content.length() + 1];
			strcpy(handle.url, content.c_str());
		}	
		else if (header == "thread")
			handle.thread = stoi(content);
		else if (header == "conn")
			handle.conn = stoi(content);
		else if (header == "out")
			{
				handle.out = new char[content.length() + 1];
				strcpy(handle.out, content.c_str());
			}
	}
}

// Set option and add easy interface to multi interface
static void add_transfer(CURLM *cm, char* url, char* range,char* outfile, int start)
{

  	CURL *eh = curl_easy_init();
  	FILE* file = fopen(outfile, "wb");
  	fseek(file, start, SEEK_SET);

  	curl_easy_setopt(eh, CURLOPT_URL, url);
  	curl_easy_setopt(eh, CURLOPT_PRIVATE, url);

  	/* Switch on full protocol/debug output while testing*/ 
  	curl_easy_setopt(eh, CURLOPT_VERBOSE, 1L);
 
   	/* Enable progress meter, set to 1L to disable it*/  
  	curl_easy_setopt(eh, CURLOPT_NOPROGRESS, 0L);
 	
  	/* get the range */
  	curl_easy_setopt(eh, CURLOPT_RANGE, range);

  	//curl_easy_setopt(eh, CURLOPT_WRITEFUNCTION, write_data);
  	curl_easy_setopt(eh, CURLOPT_WRITEFUNCTION, write_data);
  	/* write the page body to this file handle*/ 
    curl_easy_setopt(eh, CURLOPT_WRITEDATA, file);
 
   	/* get it!*/ 
   	curl_easy_perform(eh);
 	
   	/* close the header file*/ 
   	fclose(file);
  	curl_multi_add_handle(cm, eh);
}

static void *download_file(void *handle)
{
	// Convert void* to struct
	struct link_argv *my_handle = (struct link_argv*) handle;
	// Check thread have 1 connection
	if ((*my_handle).number == 1)
	{
	CURL *curl_handle;
  	FILE *file;
  	CURLcode res;
  	/* init the curl session */
  	curl_handle = curl_easy_init();
  	
  	/* set URL to get here  */
  	curl_easy_setopt(curl_handle, CURLOPT_URL, (*my_handle).url);
 
  	/* Switch on full protocol/debug output while testing*/ 
  	curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 1L);
 
   	/* Enable progress meter, set to 1L to disable it*/  
  	curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 0L);
 	
  	/* get the range */
  	curl_easy_setopt(curl_handle, CURLOPT_RANGE, (*my_handle).range);

  	/* send all data to this function */ 
  	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data);
  	/* open the file*/  
  	file = fopen((*my_handle).out, "wb");

  	string temp((*my_handle).range);
  	size_t pos = temp.find("-");
  	long start = stol(temp.substr(0, pos));
  	fseek(file, start, SEEK_SET);
  	if(file) {
    	/* write the page body to this file handle*/ 
    	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, file);
 
    	/* get it!*/ 
    	curl_easy_perform(curl_handle);
 		
    	/* close the header file*/ 
    	fclose(file);
    }
 	curl_easy_cleanup(curl_handle);
 
  	curl_global_cleanup();
  	}
  	else // multi - interface
  	{
  		CURLM *cm;
  		CURLMsg *msg;
  		unsigned int transfers = 0;
  		int msgs_left = -1;
  		int still_alive = 1;
 
  		curl_global_init(CURL_GLOBAL_ALL);
  		cm = curl_multi_init();
 		
 		if ((*my_handle).number > MAX_MULTI_CONN)
 			(*my_handle).number = MAX_MULTI_CONN;
  		/* Limit the amount of simultaneous connections curl should allow: */ 
  		curl_multi_setopt(cm, CURLMOPT_MAXCONNECTS , (long)(*my_handle).number);

  		string temp((*my_handle).range);
  		size_t pos = temp.find("-");
  		long start = stol(temp.substr(0, pos));
  		long end = stol(temp.substr(pos+1));

  		for(transfers = 0; transfers < (*my_handle).number; transfers++)
    	{
    		long src = start + (end - start) / (*my_handle).number * transfers;
    		long dst = src + (end - start) / (*my_handle).number - 1;
    		if (transfers == (*my_handle).number - 1)
    			dst = end;
    		string range = std::to_string(src)+'-'+std::to_string(dst); 
  			char *range_transfers = new char[range.length() + 1];
			strcpy(range_transfers, range.c_str());
    		add_transfer(cm, (*my_handle).url, range_transfers, (*my_handle).out, src);
 		}
  		do {
    		curl_multi_perform(cm, &still_alive);
 			
    		while((msg = curl_multi_info_read(cm, &msgs_left))) {
      		if(msg->msg == CURLMSG_DONE) { // identifies a transfer that is done
        		char *url;
        		CURL *e = msg->easy_handle;
        		curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &url);
        		fprintf(stderr, "R: %d - %s <%s>\n",
                msg->data.result, curl_easy_strerror(msg->data.result), url);
        		curl_multi_remove_handle(cm, e);
        		curl_easy_cleanup(e);
      		}
      		else {
        		fprintf(stderr, "E: CURLMsg (%d)\n", msg->msg);
      		}
  		}
    	if(still_alive)
      		curl_multi_wait(cm, NULL, 0, 1000, NULL);
 
  		} while(still_alive); //  set to zero (0) on the return of this function, there is no longer any transfers in progress.
 
  		curl_multi_cleanup(cm);
  		curl_global_cleanup();
 
  	}
  	return NULL;
}

int main(int argc, char *argv[])
{
 
  	if(argc < 2) {
    	printf("Usage: %s <URL>\n", argv[0]);
    	return 1;
  	}
  	struct link_argv handle;
  	parse_argv(argv, argc, handle);
  	pthread_t tid[NUMT];

  	if (handle.thread > NUMT)
  		handle.thread = NUMT;

  	if (handle.thread > handle.conn)
  		handle.thread = handle.conn;

  	/* Must initialize libcurl before any threads are started */ 
  	curl_global_init(CURL_GLOBAL_ALL);

  	CURL *curl;
  	CURLcode res;
  	/* init the curl session */
  	curl = curl_easy_init();
  	
  	/* set URL to get here  */
  	curl_easy_setopt(curl, CURLOPT_URL, handle.url);
 
  	curl_easy_setopt(curl, CURLOPT_HEADER, 1);
	curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
	curl_off_t cl;
 	curl_easy_perform(curl);	
   	res = curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &cl);
  	if(!res) {
  		printf("Download size: %" CURL_FORMAT_CURL_OFF_T "\n", cl);
   	}
  	curl_easy_cleanup(curl);

  	/* create sparse file*/
  	std::ofstream ofs(handle.out, std::ios::binary | std::ios::out);
    ofs.seekp(cl - 1);
    ofs.write("", 1);
    ofs.close();

  	struct link_argv *pair = (struct link_argv*)malloc(handle.thread*sizeof(struct link_argv));
  	
  	//struct argv_download *url_range = (struct argv_download*)malloc(handle.thread*sizeof(struct argv_download));

  	for(int i = 0; i < handle.thread; i++) {
  		pair[i].url = handle.url;
  		pair[i].thread = handle.thread;
  		pair[i].conn = handle.conn;
  		pair[i].out = handle.out;
  		int first = (handle.conn % handle.thread) > i ? 1: 0 ;
  		pair[i].number = handle.conn / handle.thread + first; // Keep nuber conn thread control
  		int position = 0;
  		if (first == 1)
  			position = (handle.conn / handle.thread + 1) * i;
  		else 
  			position = handle.conn - (handle.thread - i)*(handle.conn/handle.thread);
  		long start = cl / handle.conn * position;
  		long end = start + (cl / handle.conn)*pair[i].number - 1;
  		if (i == handle.thread - 1)
  			end = cl -1;
  		string temp = std::to_string(start)+'-'+std::to_string(end); 
  		pair[i].range = new char[temp.length() + 1];
		strcpy(pair[i].range, temp.c_str());
		int error = pthread_create(&tid[i],
                               NULL, /* default attributes please */ 
                               download_file,
                               (void *)&pair[i]);
    if(0 != error)
      	fprintf(stderr, "Couldn't run thread number %d, errno %d\n", i, error);
    else
      	fprintf(stderr, "Thread %d, gets %s\n", i, pair[i].url);
  	}

   	/* now wait for all threads to terminate */ 
  	for(int i = 0; i < handle.thread; i++) {
    	pthread_join(tid[i], NULL);
    	fprintf(stderr, "Thread %d terminated\n", i);
  	}
  	curl_global_cleanup();
  	for(int i = 0; i < handle.thread; i++) {
  		delete[] pair[i].range;}
  	free(pair);
 	delete[] handle.url;
 	delete[] handle.out;
	return 0;
}