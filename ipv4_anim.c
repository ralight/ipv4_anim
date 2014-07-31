/*
Copyright (c) 2014 Roger Light <roger@atchoo.org>
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. Neither the name of this project nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
*/

#include <pthread.h>
#include <math.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define THREAD_COUNT 8

struct anim_job {
	png_color *palette;
	int year;
	int month;
	int day;

	struct anim_job *next;
};

struct anim_job *job_head = NULL;
pthread_mutex_t job_mutex;
char **prefixes = NULL;

void hilpoint(long number, long scale, double qs, double *cx, double *cy);

void parse(png_byte **image, const char *line, int date_limit)
{
	char *linedup = strdup(line);
	char *saddress, *ssize, *sdate, *sowner, *sstatus;
	long date;
	int size;
	int a0, a1, a2;
	char *token;
	double dx0, dy0, dx1, dy1, dx2, dy2;
	int i;
	int status;
	int x, y;
	char *saveptr;

	saddress = strtok_r(linedup, " ", &saveptr);
	ssize = strtok_r(NULL, " ", &saveptr);
	sdate = strtok_r(NULL, " ", &saveptr);
	sowner = strtok_r(NULL, " ", &saveptr);
	sstatus = strtok_r(NULL, " ", &saveptr);
	sstatus[strlen(sstatus)-1] = '\0';

	date = atoi(sdate);
	if(date > date_limit){
		free(linedup);
		return;
	}

	for(i=0; i<strlen(saddress); i++){
		if(saddress[i] == '/'){
			saddress[i] = '\0';
			break;
		}
	}
	token = strtok_r(saddress, ".", &saveptr);
	a0 = atoi(token);
	token = strtok_r(NULL, ".", &saveptr);
	a1 = atoi(token);
	token = strtok_r(NULL, ".", &saveptr);
	a2 = atoi(token);

	size = atoi(ssize)/256;

	if(!strcmp(sowner, "iana")){
		status = 1;
	}else if(!strcmp(sowner, "afrinic")){
		status = 5;
	}else if(!strcmp(sowner, "apnic")){
		status = 8;
	}else if(!strcmp(sowner, "arin")){
		status = 11;
	}else if(!strcmp(sowner, "lacnic")){
		status = 14;
	}else if(!strcmp(sowner, "ripencc")){
		status = 17;
	}else if(!strcmp(sowner, "var")){
		status = 20;
	}

#ifdef WITH_RIR
	if(!strcmp(sstatus, "adv")){
	}else if(!strcmp(sstatus, "rir")){
		status += 1;
	}else if(!strcmp(sstatus, "unadv")){
		status += 2;
	}else if(!strcmp(sstatus, "ietf")){
		status += 2;
	}else if(!strcmp(sstatus, "mcast")){
		status += 3;
	}
#else
	if(!strcmp(sstatus, "adv") || !strcmp(sstatus, "advertised")){
		status = 23;
	}else if(!strcmp(sstatus, "iana") || !strcmp(sstatus, "ianapool")){
		status = 0;
	}else if(!strcmp(sstatus, "rir") || !strcmp(sstatus, "rirpool") || !strcmp(sstatus, "rirreserved")){
		status = 0;
	}else if(!strcmp(sstatus, "unadv") || !strcmp(sstatus, "assigned")){
		status = 11;
	}else if(!strcmp(sstatus, "ietf")){
		status = 3;
	}else if(!strcmp(sstatus, "mcast") || !strcmp(sstatus, "multicast")){
		status = 4;
	}else{
		printf("sstatus: %s\n", sstatus);
	}
#endif
	for(i=0; i<size; i++){
		hilpoint(a0, 256, 16, &dx0, &dy0);
		hilpoint(a1, 256, 16, &dx1, &dy1);
		hilpoint(a2, 256, 16, &dx2, &dy2);
		x = 256*(int)dx0 + 16*(int)dx1 + (int)dx2;
		y = 256*(int)dy0 + 16*(int)dy1 + (int)dy2;
		image[y][x] = status;

		a2++;
		if(a2 == 256){
			a2 = 0;
			a1++;
		}
		if(a1 == 256){
			a1 = 0;
			a0++;
		}
	}
	free(linedup);
}

void *thread_main(void *obj)
{
	int i;
	png_color *palette = obj;
	struct anim_job *job;
	png_structp png_ptr;
	png_infop info_ptr;
	png_byte **image;
	png_bytep row_pointers[4096];
	FILE *outfile;
	int p, q;
	char outname[100];
	int date_limit;

	image = (png_byte **)calloc(4096, sizeof(png_byte *));
	for(i=0; i<4096; i++){
		image[i] = (png_byte *)calloc(4096, sizeof(png_byte));
	}

	while(1){
		pthread_mutex_lock(&job_mutex);
		if(job_head){
			job = job_head;
			job_head = job_head->next;
		}else{
			pthread_mutex_unlock(&job_mutex);
			break;
		}
		pthread_mutex_unlock(&job_mutex);

		for(i=0; i<4096; i++){
			memset(image[i], 0, 4096*sizeof(png_byte));
		}

		/* Process job */
		date_limit = job->year*10000 + job->month*100 + job->day*10;
		snprintf(outname, 100, "ipv4_map_%d.png", date_limit);
		printf("%d %s\n", pthread_self(), outname);
		outfile = fopen(outname, "wb");

		png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
		info_ptr = png_create_info_struct(png_ptr);
		setjmp(png_jmpbuf(png_ptr));
		png_init_io(png_ptr, outfile);
		png_set_IHDR(png_ptr, info_ptr,  4096, 4096, 8, PNG_COLOR_TYPE_PALETTE,
				PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

		png_set_PLTE(png_ptr, info_ptr, palette, 24);
		png_set_filter(png_ptr, 0, PNG_FILTER_NONE);
		png_write_info(png_ptr, info_ptr);

		i = 0;
		while(prefixes[i]){
			parse(image, prefixes[i], date_limit);
			i++;
		}

		for(p=0; p<4096; p++){
			for(q=0; q<4096; q++){
				if(p%256 == 0 || q%256 == 0){
					image[p][q] = 22;
				}
			}
		}
		for(i=0; i<4096; i++){
			row_pointers[i] = image[i];
		}
		png_write_image(png_ptr, row_pointers);
		png_write_end(png_ptr, info_ptr);
		png_destroy_write_struct(&png_ptr, &info_ptr);
		fclose(outfile);
	}
	for(i=0; i<4096; i++){
		free(image[i]);
	}
	free(image);
	return NULL;
}


int main(int argc, char *argv[])
{
	FILE *infile;
	png_color *palette;
	struct anim_job *job;
	pthread_t thread[THREAD_COUNT];
	int i;
	int date_year, date_month, date_day;
	char instr[1024];
	int linecount;

	pthread_mutex_init(&job_mutex, NULL);

	palette = (png_color *)calloc(24, sizeof(png_color));
	palette[0].red = 255; palette[0].green = 255; palette[0].blue = 255; // BG
	palette[1].red = 255; palette[1].green = 255; palette[1].blue = 0; // IANA
	palette[2].red = 191; palette[2].green = 191; palette[2].blue = 0; // IANA
	palette[3].red = 127; palette[3].green = 127; palette[3].blue = 0; // IANA
	palette[4].red = 63; palette[4].green = 63; palette[4].blue = 0; // IANA
	palette[5].red = 0; palette[5].green = 127; palette[5].blue = 127; // AFRINIC
	palette[6].red = 127; palette[6].green = 255; palette[6].blue = 255; // AFRINIC
	palette[7].red = 0; palette[7].green = 191; palette[7].blue = 191; // AFRINIC
	palette[8].red = 0; palette[8].green = 0; palette[8].blue = 127; // APNIC
	palette[9].red = 127; palette[9].green = 127; palette[9].blue = 255; // APNIC
	palette[10].red = 0; palette[10].green = 0; palette[10].blue = 191; // APNIC
	palette[11].red = 127; palette[11].green = 0; palette[11].blue = 0; // ARIN
	palette[12].red = 255; palette[12].green = 127; palette[12].blue = 127; // ARIN
	palette[13].red = 191; palette[13].green = 0; palette[13].blue = 0; // ARIN
	palette[14].red = 127; palette[14].green = 0; palette[14].blue = 127; // LACNIC
	palette[15].red = 255; palette[15].green = 127; palette[15].blue = 255; // LACNIC
	palette[16].red = 191; palette[16].green = 0; palette[16].blue = 191; // LACNIC
	palette[17].red = 0; palette[17].green = 127; palette[17].blue = 0; // RIPENCC
	palette[18].red = 127; palette[18].green = 255; palette[18].blue = 127; // RIPENCC
	palette[19].red = 0; palette[19].green = 191; palette[19].blue = 0; // RIPENCC
	palette[20].red = 63; palette[20].green = 63; palette[20].blue = 63; // VAR
	palette[21].red = 127; palette[21].green = 127; palette[21].blue = 127; // VAR
	palette[22].red = 0; palette[22].green = 0; palette[22].blue = 0; // VAR
	palette[23].red = 0; palette[23].green = 127; palette[23].blue = 0; // assigned

	for(date_year=1982; date_year<2015; date_year++){
	//date_year = atoi(argv[1]);
		for(date_month=1; date_month<13; date_month++){
			for(date_day=0; date_day<3; date_day++){
				job = malloc(sizeof(struct anim_job));
				if(!job){
					printf("Out of memory\n");
					exit(1);
				}

				job->year = date_year;
				job->month = date_month;
				job->day = date_day;

				job->next = job_head;
				job_head = job;
			}
		}
	}

	infile = fopen("prefixes.txt", "rt");
	linecount = 0;
	while(!feof(infile)){
		fgets(instr, 1024, infile);
		linecount++;
	}
	fclose(infile);

	prefixes = calloc(linecount+1, sizeof(char *));
	infile = fopen("prefixes.txt", "rt");
	i = 0;
	while(!feof(infile)){
		fgets(instr, 1024, infile);
		prefixes[i] = strdup(instr);
		i++;
	}
	fclose(infile);


	for(i=0; i<THREAD_COUNT; i++){
		pthread_create(&(thread[i]), NULL, thread_main, palette);
	}

	for(i=0; i<THREAD_COUNT; i++){
		pthread_join(thread[i], NULL);
	}

	for(i=0; i<linecount; i++){
		free(prefixes[i]);
	}
	free(prefixes);
	return 0;
}

