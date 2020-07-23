#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
 
#include <curl/curl.h>
#include <string.h>
#include <iostream>

#include <pthread.h>
#include <fstream>
#define NUMT 16

using namespace std;

static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream)
{
  size_t written = fwrite(ptr, size, nmemb, (FILE *)stream);
  return written;
}

struct link_argv
{
	char* url;
	static int thread;
	static int conn;
	char* out;
	char* range;
};

int link_argv::thread = 1;
int link_argv::conn = 1;

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

static void *download_file(void *handle)
{
	// Convert void* to struct
	struct link_argv *my_handle = (struct link_argv*) handle;

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
  		cout << "Size" << cl << endl;
  		(pair[i]).url = handle.url;
  		(pair[i]).thread = handle.thread;
  		(pair[i]).conn = handle.conn;
  		(pair[i]).out = handle.out;
  		long start = cl/ handle.thread * i;
  		long end = cl / handle.thread * (i+1) - 1;
  		if (i == handle.thread - 1)
  			end = cl -1;
  		string temp = std::to_string(start)+'-'+std::to_string(end); 
  		pair[i].range = new char[temp.length() + 1];
		strcpy(pair[i].range, temp.c_str()); 
		cout << pair[i].range<<endl;
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