﻿/*-
 * Copyright 2003-2005 Colin Percival
 * Copyright 2012-2018 Matthew Endsley
 * Copyright 2018-2020 Emanuel Komínek
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions 
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "bsdiff.h"

#include <limits.h>
#include <string.h>

#define MIN(x,y) (((x)<(y)) ? (x) : (y))
#define MEDIAN3(a,b,c) (((a)<(b)) ? \
	((b)<(c) ? (b) : ((a)<(c) ? (c) : (a))) : \
	((b)>(c) ? (b) : ((a)>(c) ? (c) : (a))))

static void split(int64_t *I,int64_t *V,int64_t start,int64_t len,int64_t h)
{
	int64_t i,j,k,x,y,z,tmp,jj,kk;

	if(len<16) {
		for(k=start;k<start+len;k+=j) {
			j=1;x=V[I[k]+h];
			for(i=1;k+i<start+len;i++) {
				if(V[I[k+i]+h]<x) {
					x=V[I[k+i]+h];
					j=0;
				};
				if(V[I[k+i]+h]==x) {
					tmp=I[k+j];I[k+j]=I[k+i];I[k+i]=tmp;
					j++;
				};
			};
			for(i=0;i<j;i++) V[I[k+i]]=k+j-1;
			if(j==1) I[k]=-1;
		};
		return;
	};

	/* Select pivot, algorithm by Bentley & McIlroy */
	j=start+len/2;
	k=start+len-1;
	x=V[I[j]+h];
	y=V[I[start]+h];
	z=V[I[k]+h];
	if(len>40) {  /* Big array: Pseudomedian of 9 */
		tmp=len/8;
		x=MEDIAN3(x,V[I[j-tmp]+h],V[I[j+tmp]+h]);
		y=MEDIAN3(y,V[I[start+tmp]+h],V[I[start+tmp+tmp]+h]);
		z=MEDIAN3(z,V[I[k-tmp]+h],V[I[k-tmp-tmp]+h]);
	};  /* Else medium array: Pseudomedian of 3 */
	x=MEDIAN3(x,y,z);

	jj=0;kk=0;
	for(i=start;i<start+len;i++) {
		if(V[I[i]+h]<x) jj++;
		if(V[I[i]+h]==x) kk++;
	};
	jj+=start;kk+=jj;

	i=start;j=0;k=0;
	while(i<jj) {
		if(V[I[i]+h]<x) {
			i++;
		} else if(V[I[i]+h]==x) {
			tmp=I[i];I[i]=I[jj+j];I[jj+j]=tmp;
			j++;
		} else {
			tmp=I[i];I[i]=I[kk+k];I[kk+k]=tmp;
			k++;
		};
	};

	while(jj+j<kk) {
		if(V[I[jj+j]+h]==x) {
			j++;
		} else {
			tmp=I[jj+j];I[jj+j]=I[kk+k];I[kk+k]=tmp;
			k++;
		};
	};

	if(jj>start) split(I,V,start,jj-start,h);

	for(i=0;i<kk-jj;i++) V[I[jj+i]]=kk-1;
	if(jj==kk-1) I[jj]=-1;

	if(start+len>kk) split(I,V,kk,start+len-kk,h);
}

static void qsufsort(int64_t *I,int64_t *V,const uint8_t *old,int64_t oldsize)
{
	int64_t buckets[256];
	int64_t i,h,len;

	for(i=0;i<256;i++) buckets[i]=0;
	for(i=0;i<oldsize;i++) buckets[old[i]]++;
	for(i=1;i<256;i++) buckets[i]+=buckets[i-1];
	for(i=255;i>0;i--) buckets[i]=buckets[i-1];
	buckets[0]=0;

	for(i=0;i<oldsize;i++) I[++buckets[old[i]]]=i;
	I[0]=oldsize;
	for(i=0;i<oldsize;i++) V[i]=buckets[old[i]];
	V[oldsize]=0;
	for(i=1;i<256;i++) if(buckets[i]==buckets[i-1]+1) I[buckets[i]]=-1;
	I[0]=-1;

	for(h=1;I[0]!=-(oldsize+1);h+=h) {
		len=0;
		for(i=0;i<oldsize+1;) {
			if(I[i]<0) {
				len-=I[i];
				i-=I[i];
			} else {
				if(len) I[i-len]=-len;
				len=V[I[i]]+1-i;
				split(I,V,i,len,h);
				i+=len;
				len=0;
			};
		};
		if(len) I[i-len]=-len;
	};

	for(i=0;i<oldsize+1;i++) I[V[i]]=i;
}

static int64_t matchlen(const uint8_t *old,int64_t oldsize,const uint8_t *new,int64_t newsize)
{
	int64_t i;

	for(i=0;(i<oldsize)&&(i<newsize);i++)
		if(old[i]!=new[i]) break;

	return i;
}

static int64_t search(const int64_t *I,const uint8_t *old,int64_t oldsize,
		const uint8_t *new,int64_t newsize,int64_t st,int64_t en,int64_t *pos)
{
	int64_t x,y;

	if(en-st<2) {
		x=matchlen(old+I[st],oldsize-I[st],new,newsize);
		y=matchlen(old+I[en],oldsize-I[en],new,newsize);

		if(x>y) {
			*pos=I[st];
			return x;
		} else {
			*pos=I[en];
			return y;
		}
	};

	x=st+(en-st)/2;
	if(memcmp(old+I[x],new,MIN(oldsize-I[x],newsize))<0) {
		return search(I,old,oldsize,new,newsize,x,en,pos);
	} else {
		return search(I,old,oldsize,new,newsize,st,x,pos);
	};
}

// Converts two's complement to signed magnitude.
static inline void offtout(int64_t * x)
{
	if (*x < 0)
		*x = (~*x + 1) | INT64_MIN;
}

static int64_t writedata(struct bsdiff_stream * stream, const void * buffer,
                         int64_t length, enum bsdiff_stream_type type)
{
	int64_t result = 0;

	while (length > 0)
	{
		const int smallsize = (int)MIN(length, INT_MAX);
		const int writeresult = stream->write(stream, buffer, smallsize, type);
		if (writeresult == -1)
		{
			return -1;
		}

		result += writeresult;
		length -= smallsize;
		buffer = (uint8_t*)buffer + smallsize;
	}

	return result;
}

struct bsdiff_request
{
	const uint8_t* old;
	int64_t oldsize;
	const uint8_t* new;
	int64_t newsize;
	struct bsdiff_stream* stream;
	int64_t *I;
	uint8_t *buffer;
};

static int bsdiff_internal(const struct bsdiff_request req)
{
	int64_t *I,*V;
	int64_t scan,pos,len;
	int64_t lastscan,lastpos,lastoffset,lastwrittenscan,lastwrittenpos;
	int64_t ctrlcur[3], ctrlnext[3];
	int64_t oldscore,scsc;
	int64_t s,Sf,lenf,Sb,lenb;
	int64_t overlap,Ss,lens;
	int64_t i;
	uint8_t *buffer;

	if((V=req.stream->malloc((req.oldsize+1)*sizeof(int64_t)))==NULL) return -1;
	I = req.I;

	qsufsort(I,V,req.old,req.oldsize);
	req.stream->free(V);

	buffer = req.buffer;

	/* Compute the differences, writing ctrl as we go */
	scan=0;len=0;pos=0;
	lastscan=0;lastpos=0;lastoffset=lastwrittenscan=lastwrittenpos=0;
	ctrlcur[0]=ctrlcur[1]=ctrlcur[2]=0;
	while(scan<req.newsize) {
		oldscore=0;

		for(scsc=scan+=len;scan<req.newsize;scan++) {
			len=search(I,req.old,req.oldsize,req.new+scan,req.newsize-scan,
					0,req.oldsize,&pos);

			for(;scsc<scan+len;scsc++)
			if((scsc+lastoffset<req.oldsize) &&
				(req.old[scsc+lastoffset] == req.new[scsc]))
				oldscore++;

			if(((len==oldscore) && (len!=0)) || 
				(len>oldscore+8)) break;

			if((scan+lastoffset<req.oldsize) &&
				(req.old[scan+lastoffset] == req.new[scan]))
				oldscore--;
		};

		if((len!=oldscore) || (scan==req.newsize)) {
			s=0;Sf=0;lenf=0;
			for(i=0;(lastscan+i<scan)&&(lastpos+i<req.oldsize);) {
				if(req.old[lastpos+i]==req.new[lastscan+i]) s++;
				i++;
				if(s*2-i>Sf*2-lenf) { Sf=s; lenf=i; };
			};

			lenb=0;
			if(scan<req.newsize) {
				s=0;Sb=0;
				for(i=1;(scan>=lastscan+i)&&(pos>=i);i++) {
					if(req.old[pos-i]==req.new[scan-i]) s++;
					if(s*2-i>Sb*2-lenb) { Sb=s; lenb=i; };
				};
			};

			if(lastscan+lenf>scan-lenb) {
				overlap=(lastscan+lenf)-(scan-lenb);
				s=0;Ss=0;lens=0;
				for(i=0;i<overlap;i++) {
					if(req.new[lastscan+lenf-overlap+i]==
					   req.old[lastpos+lenf-overlap+i]) s++;
					if(req.new[scan-lenb+i]==
					   req.old[pos-lenb+i]) s--;
					if(s>Ss) { Ss=s; lens=i+1; };
				};

				lenf+=lens-overlap;
				lenb-=lens;
			};

			ctrlnext[0]=lenf;
			ctrlnext[1]=(scan-lenb)-(lastscan+lenf);
			ctrlnext[2]=(pos-lenb)-(lastpos+lenf);

			if (ctrlnext[0]) {
				if (ctrlcur[0]||ctrlcur[1]||ctrlcur[2]) {
					offtout(&ctrlcur[0]);
					offtout(&ctrlcur[1]);
					offtout(&ctrlcur[2]);

					/* Write control data */
					if (writedata(req.stream, ctrlcur, sizeof(ctrlcur), BSDIFF_WRITECONTROL))
						return -1;

					/* Write diff data */
					for(i=0;i<ctrlcur[0];i++)
						buffer[i]=req.new[lastwrittenscan+i]-req.old[lastwrittenpos+i];
					if (writedata(req.stream, buffer, ctrlcur[0], BSDIFF_WRITEDIFF))
						return -1;

					/* Write extra data */
					for(i=0;i<ctrlcur[1];i++)
						buffer[i]=req.new[lastwrittenscan+ctrlcur[0]+i];
					if (writedata(req.stream, buffer, ctrlcur[1], BSDIFF_WRITEEXTRA))
						return -1;

					lastwrittenscan=lastscan;
					lastwrittenpos=lastpos;
				};
				ctrlcur[0]=ctrlnext[0];
				ctrlcur[1]=ctrlnext[1];
				ctrlcur[2]=ctrlnext[2];
			} else {
				ctrlcur[1]+=ctrlnext[1];
				ctrlcur[2]+=ctrlnext[2];
			};

			lastscan=scan-lenb;
			lastpos=pos-lenb;
			lastoffset=pos-scan;
		};
	};

	if (ctrlcur[0]||ctrlcur[1]) {
		offtout(ctrlcur);
		offtout(ctrlcur + 1);
		offtout(ctrlcur + 2);

		/* Write control data */
		if (writedata(req.stream, ctrlcur, sizeof(ctrlcur), BSDIFF_WRITECONTROL))
			return -1;

		/* Write diff data */
		for(i=0;i<ctrlcur[0];i++)
			buffer[i]=req.new[lastwrittenscan+i]-req.old[lastwrittenpos+i];
		if (writedata(req.stream, buffer, ctrlcur[0], BSDIFF_WRITEDIFF))
			return -1;

		/* Write extra data */
		for(i=0;i<ctrlcur[1];i++)
			buffer[i]=req.new[lastwrittenscan+ctrlcur[0]+i];
		if (writedata(req.stream, buffer, ctrlcur[1], BSDIFF_WRITEEXTRA))
			return -1;
	};

	return 0;
}

int bsdiff(const uint8_t* source, int64_t sourcesize, const uint8_t* target, int64_t targetsize, struct bsdiff_stream* stream)
{
	int result;
	struct bsdiff_request req;

	if((req.I=stream->malloc((sourcesize+1)*sizeof(int64_t)))==NULL)
		return -1;

	if((req.buffer=stream->malloc(targetsize+1))==NULL)
	{
		stream->free(req.I);
		return -1;
	}

	req.old = source;
	req.oldsize = sourcesize;
	req.new = target;
	req.newsize = targetsize;
	req.stream = stream;

	result = bsdiff_internal(req);

	stream->free(req.buffer);
	stream->free(req.I);

	return result;
}

#if defined(BSDIFF_EXECUTABLE)

#include <bzlib.h>
#include "bsdiff_common.h"

static int bz2_write(struct bsdiff_stream * stream, const void * buffer,
                     size_t size, ATTR_UNUSED enum bsdiff_stream_type type)
{
	int bz2err;
	BZFILE* bz2;

	bz2 = (BZFILE*)stream->opaque;
	BZ2_bzWrite(&bz2err, bz2, (void*)buffer, size);
	if (bz2err != BZ_STREAM_END && bz2err != BZ_OK)
		return -1;

	return 0;
}

int main(int argc,char *argv[])
{
	FILE * fp;
	uint8_t * source, * target;
	int64_t sourcesize, targetsize;
	BZFILE * bz2;
	int bz2err;
	struct bsdiff_stream stream;

	if (argc != 4)
		errx(1, "usage: %s oldfile newfile patchfile\n", argv[0]);

	// Reads source file.
	read_file_to_buffer(argv[1], &source, &sourcesize);

	// Reads target file.
	read_file_to_buffer(argv[2], &target, &targetsize);

	// Creates patch file.
	if ((fp = fopen(argv[3], "wb")) == NULL)
		errx(1, "fopen (%s)", argv[3]);

	// Writes patch header (signature + newsize)
	if (fwrite("ENDSLEY/BSDIFF43", 1, 16, fp) != 16 ||
		fwrite(&targetsize, 1, sizeof(targetsize), fp) != sizeof(targetsize))
		errx(1, "fwrite (%s)", argv[3]);

	// Opens bzip2 stream.
	if ((bz2 = BZ2_bzWriteOpen(&bz2err, fp, 9, 0, 0)) == NULL)
		errx(1, "BZ2_bzWriteOpen (bz2err=%d)", bz2err);

	// Creates patch.
	stream.opaque = bz2;
	stream.malloc = malloc;
	stream.free = free;
	stream.write = bz2_write;
	if (bsdiff(source, sourcesize, target, targetsize, &stream))
		errx(1, "bsdiff");

	// Closes patch file.
	BZ2_bzWriteClose(&bz2err, bz2, 0, NULL, NULL);
	if (bz2err != BZ_OK)
		errx(1, "BZ2_bzWriteClose (bz2err=%d)", bz2err);
	if (fclose(fp) != 0)
		errx(1, "fclose (%s)", argv[3]);

	/* Free the memory we used */
	free(source);
	free(target);

	return 0;
}

#endif
