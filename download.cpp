#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
 
#include <curl/curl.h>
#include <string.h>
#include <iostream>

using namespace std;

static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream)
{
  size_t written = fwrite(ptr, size, nmemb, (FILE *)stream);
  return written;
}

struct link
{
	string url;
	static int thread;
	static int conn;
	string out;
};

int link::thread = 1;
int link::conn = 1;

void parse_argv(char* argv[], int argc, struct link &handle)
{
	for (int i = 1; i < argc; i++)
	{
		cout << argv[i] << endl;
		string part(argv[i]);
		size_t pos = part.find("=");
		if (pos == std::string::npos)
			continue;
		string header = part.substr(2, pos -2);
		string content = part.substr(pos + 1);

		if (header == "url")
			handle.url = content;
		else if (header == "thread")
			handle.thread = stoi(content);
		else if (header == "conn")
			handle.conn = stoi(content);
		else if (header == "out")
			handle.out = content;
	}
}

void download_file(string url)
{
	const char* url_char = url.c_str();
	string name;
	size_t pos = url.rfind("/");
	if (pos != std::string::npos)
		name = url.substr(pos + 1);
	else 
		name = "abc.out";
  CURL *curl_handle;
  const char *pagefilename = name.c_str();
  FILE *pagefile;

  curl_global_init(CURL_GLOBAL_ALL);
 
  /* init the curl session */ 
  curl_handle = curl_easy_init();
 
  /* set URL to get here */ 
  curl_easy_setopt(curl_handle, CURLOPT_URL, url_char);
 
  /* Switch on full protocol/debug output while testing */ 
  curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 1L);
 
  /* disable progress meter, set to 0L to enable it */ 
  curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 0L);
 
  /* send all data to this function  */ 
  curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data);
 
  /* open the file */ 
  pagefile = fopen(pagefilename, "wb");
  if(pagefile) {
 
    /* write the page body to this file handle */ 
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, pagefile);
 
    /* get it! */ 
    curl_easy_perform(curl_handle);
 
    /* close the header file */ 
    fclose(pagefile);
  }
 
  /* cleanup curl stuff */ 
  curl_easy_cleanup(curl_handle);
 
  curl_global_cleanup();
}



int main(int argc, char *argv[])
{
 
  if(argc < 2) {
    printf("Usage: %s <URL>\n", argv[0]);
    return 1;
  }
  struct link handle;
  parse_argv(argv, argc, handle);

  cout << handle.url << endl;
  download_file(handle.url);
 
  return 0;
}