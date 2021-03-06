#include <stdio.h>
#include <curl/curl.h>
#include <stdlib.h>
#include <pcre.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xinerama.h>
#include <X11/Xft/Xft.h>
#define MAXMEANING 20

char baseurl[]="http://dict.youdao.com/w/eng/%s/#keyfrom=dict2.index";
char url[BUFSIZ];
char yd_content[1024*1024*8];
char yd_pronounce[BUFSIZ];
char yd_meaning[MAXMEANING][BUFSIZ];
char yd_translation[BUFSIZ];
char yd_output_text[BUFSIZ];
char word_buf[BUFSIZ];

int offset;


#define WAITMS(x)                               \
  struct timeval wait = { 0, (x) * 1000 };      \
  (void)select(0, NULL, NULL, NULL, &wait);

void yd_construct_result(){
	yd_output_text[0]=0;
	if (yd_pronounce[0]){
		strcat(yd_output_text,yd_pronounce);
		strcat(yd_output_text,"\n");
	}
	for (int cnt=0;cnt<MAXMEANING && yd_meaning[cnt][0];cnt++){
		strcat(yd_output_text,yd_meaning[cnt]);
		strcat(yd_output_text,"\n");
	}
	if (!yd_meaning[0][0] && yd_translation[0]){
		strcat(yd_output_text,yd_translation);
		strcat(yd_output_text,"\n");
	}
}

void yd_find_translation(void){
	pcre *re;
	const char *errptr;
	int erroffset;
	int ovector[MAXMEANING*2];
	re=pcre_compile("<div id=\"fanyiToggle\">.*?<p>.*?</p>.*?<p>(.*?)</p>",PCRE_DOTALL,&errptr,&erroffset,NULL);
	int rc=pcre_exec(re,NULL,yd_content,strlen(yd_content),0,0,ovector,MAXMEANING*2);
	pcre_free(re);
	int ptr=0;
	if (rc==2){
		for (int i=ovector[2];i<ovector[3];i++)
			yd_translation[ptr++]=yd_content[i];
	}
	yd_translation[ptr]=0;
}

void yd_find_pronounce(void){
	pcre *re;
	const char *errptr;
	int erroffset;
	int ovector[MAXMEANING*2];
	re=pcre_compile("<span class=\"pronounce\">(\\S*)\\s*<span.*?>(\\S+)<",PCRE_DOTALL,&errptr,&erroffset,NULL);
	int content_len=strlen(yd_content);
	int start_offset=0;

	int ptr=0;
	while (1){
		int rc=pcre_exec(re,NULL,yd_content,content_len,start_offset,0,ovector,MAXMEANING*2);
		if (rc==3){
			for (int i=1;i<3;i++)
				for (int j=ovector[i*2];j<ovector[i*2+1];j++)
					yd_pronounce[ptr++]=yd_content[j];
			start_offset=ovector[1];
		}
		else
			break;
	}
	yd_pronounce[ptr]=0;
	pcre_free(re);
}

void yd_find_meaning(void){
	pcre *re;
	const char *errptr;
	int erroffset;
	int ovector[MAXMEANING*2];
	re=pcre_compile("<div class=\"trans-container\">.*?</div>",PCRE_DOTALL,&errptr,&erroffset,NULL);
	int rc=pcre_exec(re,NULL,yd_content,strlen(yd_content),0,0,ovector,MAXMEANING*2);
	pcre_free(re);

	int start_offset=ovector[0];
	int end_offset=ovector[1];

	int cnt=0;

	re=pcre_compile("<li>(.*?)</li>",PCRE_DOTALL,&errptr,&erroffset,NULL);
	while (cnt<MAXMEANING){
		rc=pcre_exec(re,NULL,yd_content,end_offset,start_offset,0,ovector,MAXMEANING*2);
		if (rc==2){
			int ptr=0;
			for (int i=ovector[2];i<ovector[3];i++)
				yd_meaning[cnt][ptr++]=yd_content[i];
			yd_meaning[cnt++][ptr]=0;
			start_offset=ovector[1];
		}
		else
			break;
	}
	pcre_free(re);
	for (int i=cnt;i<MAXMEANING;i++){
		yd_meaning[i][0]=0;
	}
}

char dec2hex(char d){
	if (d>=0 && d<=9)
		return '0'+d;
	else
		return 'A'+d-10;
}
void urlencode(char *s){
	char *r=(char *)malloc(BUFSIZ);
 	int i = 0;
	char c;
	while ((c=*s)){
		if ( ('0' <= c && c <= '9') ||
 	            ('a' <= c && c <= 'z') ||
 	            ('A' <= c && c <= 'Z') ||
 	            c == '/' || c == '.'){
			r[i++]=c;
		}
		else{
			int tmp=c&0xff;
			r[i++]='%';
			r[i++]=dec2hex(tmp/16);
			r[i++]=dec2hex(tmp%16);

		}
		s++;
	}
	r[i]=0;
	sprintf(url,baseurl,r);
	free(r);
}

void strip(char *txt){
	int ptr=0;
	int i;
	for (i=0;txt[i]==' ';i++)
		;
	int j;
	for (j=strlen(txt)-1;j>i && txt[i]==' ';j--)
		;
	for (int k=i;k<=j;k++)
		word_buf[ptr++]=txt[k];
	word_buf[ptr]=0;
}

size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata){
	size_t cnt=size*nmemb;
	for (int i=0;i<cnt;i++)
		yd_content[offset+i]=ptr[i];
	offset+=cnt;
	yd_content[offset]=0;
	return cnt;
}

extern Display *dpy;
extern Window win;
extern XIC xic;

int check_key_cancel(XKeyEvent *ev){
	char buf[32];
	KeySym ksym = NoSymbol;
	Status status;

	XmbLookupString(xic, ev, buf, sizeof buf, &ksym, &status);
	if (status == XBufferOverflow)
		return 0;
	switch(ksym){
		case XK_c:
			return (ev->state & ControlMask);
		default:
			return 0;
	}
	return 0;

}

int check_event(void){
	XEvent ev;
	int n = XEventsQueued(dpy, QueuedAfterReading);
	if (n==0)
		return 0;
	else{
		int stat=XNextEvent(dpy,&ev);
		if (stat)
			return stat;
		else{
			if (XFilterEvent(&ev, win))
				return stat;
			switch(ev.type) {
			case KeyPress:
				return check_key_cancel(&ev.xkey);
			default:
				break;
			}
			return stat;
		}
	}
	return 0;
}

char * lookup(char *txt){
    CURL *single;
    CURLM *multi;
    int still_running;
    int repeats=0;
    int cancel_flag=0;

    if (txt==NULL)
	    return NULL;
    strip(txt);
    if (!word_buf[0])
	    return NULL;

    curl_global_init(CURL_GLOBAL_DEFAULT);

    single = curl_easy_init();
    if (single==NULL)
	    return NULL;
    urlencode(word_buf);
    curl_easy_setopt(single, CURLOPT_URL, url);        
    struct curl_slist *chunk = NULL;
    chunk = curl_slist_append(chunk, "User-Agent: curl/7.56.1");
    curl_easy_setopt(single, CURLOPT_HTTPHEADER, chunk);
    curl_easy_setopt(single,CURLOPT_WRITEFUNCTION,write_callback);
    
    multi=curl_multi_init();
    if (multi==NULL){
	    curl_easy_cleanup(single);
	    curl_slist_free_all(chunk);
	    return NULL;
    }
    curl_multi_add_handle(multi,single);

    offset=0;
    curl_multi_perform(multi,&still_running);
    do{
	    CURLMcode mc;
	    int numfds;

	    mc=curl_multi_wait(multi,NULL,0,1000,&numfds);
	    if (mc!=CURLM_OK){
		    fprintf(stderr, "curl_multi_wait() failed, code %d.\n", mc);
		    break;
	    }

	    if(!numfds) {
      		repeats++; /* count number of repeated zero numfds */
      		if(repeats > 1) {
      			WAITMS(100); /* sleep 100 milliseconds */
      		}
   	    }
    	    else
      		repeats = 0;

	    curl_multi_perform(multi, &still_running);

	    cancel_flag=check_event();
    }while(still_running && !cancel_flag);


    curl_multi_remove_handle(multi, single);
    curl_slist_free_all(chunk);
    curl_easy_cleanup(single);
    curl_multi_cleanup(single);
    curl_global_cleanup();

    yd_output_text[0]=0;

    if (!cancel_flag){
    	yd_find_pronounce();
    	yd_find_meaning();
    	if (yd_meaning[0][0]==0)
    		yd_find_translation();

    	yd_construct_result();
    }

    return yd_output_text;
}
