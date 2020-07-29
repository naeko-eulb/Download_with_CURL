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
#include <errno.h>

#define NUMT 16				// Max multi-thread
#define MAX_MULTI_CONN 16	// Max connection in thread
#define PART_FILE "-backup-"
 
/* somewhat unix-specific */ 
#include <sys/time.h>

#ifndef CURLPIPE_MULTIPLEX
/* This little trick will just make sure that we don't enable pipelining for
   libcurls old enough to not have this symbol. It is _not_ defined to zero in
   a recent libcurl header. */ 
#define CURLPIPE_MULTIPLEX 0
#endif

using namespace std;
struct transfer {
    CURL *easy;
    unsigned int num;
    int position;
    char *range;
    FILE* fout; 
};

struct url_content
{
    char* url;
    char* out;
    curl_off_t content_len;
    static int thread;
    static int conn;
    bool can_Resume;
};

int url_content::thread = 1;
int url_content::conn = 1;

struct url_content url_info;  /*Gobal variable keeps url, file_out and content length*/

struct url_argv
{
  char* range;
  int part;
  int num;
};

static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream)
{
  size_t written = fwrite(ptr, size, nmemb, (FILE *)stream);
  return written;
}

void parse_argv(char* argv[], int argc)
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
			url_info.url = new char[content.length() + 1];
			strcpy(url_info.url, content.c_str());
		}	
		else if (header == "thread")
			url_info.thread = stoi(content);
		else if (header == "conn")
			url_info.conn = stoi(content);
		else if (header == "out")
			{
				url_info.out = new char[content.length() + 1];
				strcpy(url_info.out, content.c_str());
			}
	}
}

static void setup(struct transfer *t, int num, int position, bool &success)
{
    FILE* file;

    /* open the file*/  
    string path_out(url_info.out); 
    std::size_t found = path_out.rfind(".");
    std::string file_out = path_out.substr(0, found) + string(PART_FILE)+ to_string(url_info.thread) + to_string(url_info.conn) + to_string(position);

    string temp(t->range);
    size_t pos = temp.find("-");
    long start = stol(temp.substr(0, pos));
    if (url_info.can_Resume)
    {
        file = fopen(file_out.c_str(), "ab");
        long res = ftell(file); // size downloaded
        if (res != 0)
        {
            start = start + res;
            long end = stol(temp.substr(pos+1));
            
            if (start > end)  // Done before
            {
                t->range = NULL;
                printf("Done\n");
                success = false;
                return;
            }
            else
            {
                printf("Resume\n");
                string temp = std::to_string(start)+'-'+std::to_string(end); 
                t->range = new char[temp.length() + 1];
                strcpy(t->range, temp.c_str());
            }
        }       
    }
    else 
        file = fopen(file_out.c_str(), "wb");
    t->fout = file;
    CURL *hnd;
 
    hnd = t->easy = curl_easy_init();
 
    if(!file) {
        fprintf(stderr, "error: could not open file %s for writing: %s\n",
            file_out.c_str(), strerror(errno));
        exit(1);
    }
 
    /* set the same URL */ 
    curl_easy_setopt(hnd, CURLOPT_URL, url_info.url);
 
    /* please be verbose */ 
    curl_easy_setopt(hnd, CURLOPT_VERBOSE, 1L);

    /* Enable progress meter, set to 1L to disable it*/  
    curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 0L);
 
    /* get the range */
    curl_easy_setopt(hnd, CURLOPT_RANGE, t->range);
    delete[] t->range;
    
    /* write to this file */ 
    curl_easy_setopt(hnd, CURLOPT_WRITEDATA, file);
    success = true;
    #if (CURLPIPE_MULTIPLEX > 0)
    /* wait for pipe connection to confirm */ 
    curl_easy_setopt(hnd, CURLOPT_PIPEWAIT, 1L);
    #endif
}

static void *download_file(void *handle)
{
    // Convert void* to struct
    struct url_argv *my_handle = (struct url_argv*) handle;
    // Check thread have 1 connection
    if ((*my_handle).num == 1)
    {
        /* Find position*/
        int i = url_info.conn - url_info.thread + (*my_handle).part;
        CURL *curl_handle;
        FILE *file;
        CURLcode res;
        /* init the curl session */
        curl_handle = curl_easy_init();

        /* set URL to get here  */
        curl_easy_setopt(curl_handle, CURLOPT_URL, url_info.url);

        /* Switch on full protocol/debug output while testing*/
        curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 1L);

        /* Enable progress meter, set to 1L to disable it*/  
        curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 0L);

        /* open the file*/  
        string path_out(url_info.out); 
        std::size_t found = path_out.rfind(".");
        std::string file_out = path_out.substr(0, found) + string(PART_FILE) + to_string(url_info.thread) + to_string(url_info.conn) + to_string(i);

        string temp((*my_handle).range);
        size_t pos = temp.find("-");
        long start = stol(temp.substr(0, pos));
        if (url_info.can_Resume)
        {
            file = fopen(file_out.c_str(), "ab");
            long res = ftell(file); // size downloaded
            if (res != 0)
            { 
                start = start + res;
                long end = stol(temp.substr(pos+1));
                if (start > end) // Finished before
                {
                    printf("Done\n");
                    curl_easy_cleanup(curl_handle);
                    fclose(file);
                    return NULL;
                }
                printf("Resume\n");
                string temp = std::to_string(start)+'-'+std::to_string(end); 
                (*my_handle).range = new char[temp.length() + 1];
                strcpy((*my_handle).range, temp.c_str());
            }       
        }
        else 
            file = fopen(file_out.c_str(), "wb");

        /* get the range */
        curl_easy_setopt(curl_handle, CURLOPT_RANGE, (*my_handle).range);

        /* send all data to this function */ 
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data);
        if(file) {
            /* write the page body to this file handle*/ 
            curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, file); 

            /* get it!*/ 
            curl_easy_perform(curl_handle);

            /* close the header file*/ 
            fclose(file);
        }
        curl_easy_cleanup(curl_handle);
    }
  	else // multi - interface
  	{
  		  CURLM *cm;
  		  CURLMsg *msg;
  		  unsigned int transfers = 0;
  		  int msgs_left = -1;
  		  int still_alive = 1;
        int still_running = 0; /* keep number of running handles */ 
        
        struct transfer trans[MAX_MULTI_CONN];
  		  cm = curl_multi_init();
 		   
        if ((*my_handle).num > MAX_MULTI_CONN || (*my_handle).num < 1)
            (*my_handle).num = 4;/* suitable default */ 
        int count = (*my_handle).num;
        /* Limit the amount of simultaneous connections curl should allow: */ 
        curl_multi_setopt(cm, CURLMOPT_MAXCONNECTS , (long)(*my_handle).num);

        string temp((*my_handle).range);
  		  size_t pos = temp.find("-");
  		  long start = stol(temp.substr(0, pos));
  		  long end = stol(temp.substr(pos+1));

        int minus = 0; /* Keep part done before*/

  		  for(transfers = 0; transfers < count; transfers++)
        {
            int i;
            if ((*my_handle).num == url_info.conn/url_info.thread)
                i = url_info.conn - (*my_handle).num * (url_info.thread - (*my_handle).part) + transfers;
            else 
                i = (*my_handle).part * (*my_handle).num + transfers;
    		    long src = start + (end - start + 1) / (*my_handle).num * transfers;
    		    long dst = src + (end - start + 1) / (*my_handle).num - 1;
    		    if (transfers == (*my_handle).num - 1)
                dst = end;
    		    string range = std::to_string(src)+'-'+std::to_string(dst); 
            trans[transfers].range = new char[range.length() + 1];
            strcpy(trans[transfers].range, range.c_str());
            bool success;
            setup(&trans[transfers], transfers, i, success);

            /* add the individual transfer */ 
            if (success)
                curl_multi_add_handle(cm, trans[transfers].easy);
            else  
                minus++;
        }
        count -= minus;
        curl_multi_setopt(cm, CURLMOPT_PIPELINING, CURLPIPE_MULTIPLEX);
 
        /* we start some action by calling perform right away */ 
        curl_multi_perform(cm, &still_running);
        
        while(still_running) {
            struct timeval timeout;
            int rc; /* select() return code */ 
            CURLMcode mc; /* curl_multi_fdset() return code */ 
 
            fd_set fdread;
            fd_set fdwrite;
            fd_set fdexcep;
            int maxfd = -1;
 
            long curl_timeo = -1;
 
            FD_ZERO(&fdread);
            FD_ZERO(&fdwrite);
            FD_ZERO(&fdexcep);
 
            /* set a suitable timeout to play around with */ 
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;
 
            curl_multi_timeout(cm, &curl_timeo);
            if(curl_timeo >= 0) {
                timeout.tv_sec = curl_timeo / 1000;
            if(timeout.tv_sec > 1)
                timeout.tv_sec = 1;
            else
                timeout.tv_usec = (curl_timeo % 1000) * 1000;
        }
 
        /* get file descriptors from the transfers */ 
        mc = curl_multi_fdset(cm, &fdread, &fdwrite, &fdexcep, &maxfd);
 
        if(mc != CURLM_OK) {
            fprintf(stderr, "curl_multi_fdset() failed, code %d.\n", mc);
            break;
        }
 
        /* On success the value of maxfd is guaranteed to be >= -1. We call
        select(maxfd + 1, ...); specially in case of (maxfd == -1) there are
        no fds ready yet so we call select(0, ...) --or Sleep() on Windows--
        to sleep 100ms, which is the minimum suggested value in the
        curl_multi_fdset() doc. */ 
 
        if(maxfd == -1) {
            #ifdef _WIN32
            Sleep(100);
            rc = 0;
            #else
                /* Portable sleep for platforms other than Windows. */ 
                struct timeval wait = { 0, 100 * 1000 }; /* 100ms */ 
                rc = select(0, NULL, NULL, NULL, &wait);
            #endif
        }
        else {
            /* Note that on some platforms 'timeout' may be modified by select().
            If you need access to the original value save a copy beforehand. */ 
            rc = select(maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout);
        }
 
        switch(rc) {
            case -1:
                /* select error */ 
                break;
            case 0:
                default:
                /* timeout or readable/writable sockets */ 
                curl_multi_perform(cm, &still_running);
                break;
        }
    }
 
        for(int i = 0; i < count; i++) {
            curl_multi_remove_handle(cm, trans[i].easy);
            curl_easy_cleanup(trans[i].easy);
        }
        for (int i = 0 ; i < count;i++)
            fclose(trans[i].fout);
  		  curl_multi_cleanup(cm);
  		  curl_global_cleanup();
  	}
  	return NULL;
}

bool support_Resume()
{
    CURL *curl;
    CURLcode res;
    /* init the curl session */
    curl = curl_easy_init();
    
    /* set URL to get here  */
    curl_easy_setopt(curl, CURLOPT_URL, url_info.url);
 
    curl_easy_setopt(curl, CURLOPT_HEADER, 1);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
    curl_easy_setopt(curl, CURLOPT_RANGE, "0-1");
  
    curl_easy_perform(curl);  
    long http_code = 0;
    res = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);
    return http_code == 206?1:0;
}

void connect_file()
{
    FILE *fw, *fr;
    char c;
    fw = fopen(url_info.out, "wb");
    for (int i = 0; i < url_info.conn; i++)
    { 
        printf("Connect file %u", i);
        /* open the file*/  
        string path_out(url_info.out); 
        std::size_t found = path_out.rfind(".");
        std::string file_out = path_out.substr(0, found) + string(PART_FILE) + to_string(url_info.thread) + to_string(url_info.conn) + to_string(i);
        fr = fopen(file_out.c_str(), "rb");
        if (!fr)
            break;
        fseek(fr, 0L, SEEK_END);
        long int res = ftell(fr), j = 0;
        fseek(fr, 0, SEEK_SET);
        while ( j < res)
        {   
            c = getc(fr);
            fputc(c, fw);
            j++;
        }
        fclose(fr);
        // Remove part file
        int del = remove(file_out.c_str());
        if (!del)
            printf("Deleted successful!\n");
        else 
            printf("Not deleted\n");
    }
    fclose(fw);
}

int main(int argc, char *argv[])
{
  	if(argc < 2) {
    	printf("Usage: %s <URL>\n", argv[0]);
    	return 1;
  	}
  	parse_argv(argv, argc);
  	pthread_t tid[NUMT];

  	if (url_info.thread > NUMT || url_info.thread < 1)
  		url_info.thread = 4; /*if wrong set thread = 4*/

  	if (url_info.thread > url_info.conn)
  		url_info.thread = url_info.conn;

  	/* Must initialize libcurl before any threads are started */ 
  	curl_global_init(CURL_GLOBAL_ALL);

  	CURL *curl;
  	CURLcode res;
  	/* init the curl session */
  	curl = curl_easy_init();
  	
  	/* set URL to get here  */
  	curl_easy_setopt(curl, CURLOPT_URL, url_info.url);
 
  	curl_easy_setopt(curl, CURLOPT_HEADER, 1);
	curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
	
 	curl_easy_perform(curl);	
   	res = curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &url_info.content_len);
  	if(!res) {
  		printf("Download size1: %" CURL_FORMAT_CURL_OFF_T "\n", url_info.content_len);
   	}
  	curl_easy_cleanup(curl);
    if (url_info.content_len == -1) /* Don't support separate file*/
        url_info.thread = url_info.conn = 1;

    url_info.can_Resume = support_Resume();

  	struct url_argv *tuple = (struct url_argv*)malloc(url_info.thread*sizeof(struct url_argv));

  	for(int i = 0; i < url_info.thread; i++) {
      tuple[i].part = i;
  		int first = (url_info.conn % url_info.thread) > i ? 1: 0 ;
  		tuple[i].num = url_info.conn / url_info.thread + first; // Keep nuber conn thread control
  		int position = 0;
  		if (first == 1)
  			position = (url_info.conn / url_info.thread + 1) * i;
  		else 
  			position = url_info.conn - (url_info.thread - i)*(url_info.conn/url_info.thread);
  		long start = url_info.content_len / url_info.conn * position;
  		long end = start + (url_info.content_len / url_info.conn)*tuple[i].num - 1;
  		if (i == url_info.thread - 1)
  			end = url_info.content_len -1;
  		string temp = std::to_string(start)+'-'+std::to_string(end); 
  		tuple[i].range = new char[temp.length() + 1];
		strcpy(tuple[i].range, temp.c_str());
		int error = pthread_create(&tid[i],
                               NULL, /* default attributes please */ 
                               download_file,
                               (void *)&tuple[i]);
    if(0 != error)
      	fprintf(stderr, "Couldn't run thread number %d, errno %d\n", i, error);
    else
      	fprintf(stderr, "Thread %d, gets %s\n", i, url_info.url);
  	}

   	/* now wait for all threads to terminate */ 
  	for(int i = 0; i < url_info.thread; i++) {
    	pthread_join(tid[i], NULL);
    	fprintf(stderr, "Thread %d terminated\n", i);
  	}
  	curl_global_cleanup();
    url_info.conn  = 0;
  	for(int i = 0; i < url_info.thread; i++) {
  		  delete[] tuple[i].range;
        /*Calculate conn again*/
        url_info.conn += tuple[i].num;
    }
    connect_file();
  	free(tuple);
    delete[] url_info.url;
 	  delete[] url_info.out;
    return 0;
}