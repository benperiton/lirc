/*      $Id: hw_pixelview.c,v 5.3 1999/08/13 18:59:54 columbus Exp $      */

/****************************************************************************
 ** hw_pixelview.c **********************************************************
 ****************************************************************************
 *
 * routines for PixelView Play TV receiver
 * 
 * Copyright (C) 1999 Christoph Bartelmus <lirc@bartelmus.de>
 *
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include "hardware.h"
#include "serial.h"
#include "ir_remote.h"
#include "lircd.h"
#include "hw_pixelview.h"

extern struct ir_remote *repeat_remote,*last_remote;

unsigned char b[3];
struct timeval start,end,last;
unsigned long gap,signal_length;
ir_code pre,code;

struct hardware hw=
{
	-1,                       /* fd */
	LIRC_CAN_REC_LIRCCODE,    /* features */
	0,                        /* send_mode */
	LIRC_CAN_REC_LIRCCODE,    /* rec_mode */
	30,                       /* code_length */
	pixelview_init,           /* init_func */
	pixelview_deinit,         /* deinit_func */
	NULL,                     /* send_func */
	pixelview_rec,            /* rec_func */
	pixelview_decode          /* decode_func */
};

int pixelview_decode(struct ir_remote *remote,
		  ir_code *prep,ir_code *codep,ir_code *postp,
		  int *repeat_flagp,unsigned long *remaining_gapp)
{
	if(remote->pone!=0  ||  remote->sone!=833) return(0);
	if(remote->pzero!=833 || remote->szero!=0) return(0);

	*prep=pre;
	*codep=code;
	*postp=0;

	gap=0;
	if(start.tv_sec-last.tv_sec>=2) /* >1 sec */
	{
		*repeat_flagp=0;
	}
	else
	{
		gap=(start.tv_sec-last.tv_sec)*1000000+
		start.tv_usec-last.tv_usec;
		
		if(gap<remote->remaining_gap*(100+remote->eps)/100
		   || gap<=remote->remaining_gap+remote->aeps)
			*repeat_flagp=1;
		else
			*repeat_flagp=0;
	}
	
	*remaining_gapp=is_const(remote) ?
	(remote->gap>signal_length ? remote->gap-signal_length:0):
	remote->gap;

#       ifdef DEBUG
	logprintf(1,"pre: %llx\n",(unsigned long long) *prep);
	logprintf(1,"code: %llx\n",(unsigned long long) *codep);
	logprintf(1,"repeat_flag: %d\n",*repeat_flagp);
	logprintf(1,"gap: %ld\n",gap);
	logprintf(1,"rem: %ld\n",remote->remaining_gap);
	logprintf(1,"signal length: %ld\n",signal_length);
#       endif


	return(1);
}

int pixelview_init(void)
{
	signal_length=(hw.code_length+(hw.code_length/8)*2)*1000000/1200;
	
	if(!tty_create_lock(LIRC_DRIVER_DEVICE))
	{
		logprintf(0,"could not create lock files\n");
		return(0);
	}
	if((hw.fd=open(LIRC_DRIVER_DEVICE,O_RDWR))<0)
	{
		logprintf(0,"could not open lirc\n");
		logperror(0,"pixelview_init()");
		tty_delete_lock();
		return(0);
	}
	if(!tty_reset(hw.fd))
	{
		logprintf(0,"could not reset tty\n");
		pixelview_deinit();
		return(0);
	}
	if(!tty_setbaud(hw.fd,1200))
	{
		logprintf(0,"could not set baud rate\n");
		pixelview_deinit();
		return(0);
	}
	return(1);
}

int pixelview_deinit(void)
{
	close(hw.fd);
	tty_delete_lock();
	return(1);
}

char *pixelview_rec(struct ir_remote *remotes)
{
	char *m;
	int i;
	
	gettimeofday(&start,NULL);
	for(i=0;i<3;i++)
	{
		if(i>0)
		{
			if(!waitfordata(10000))
			{
				logprintf(0,"timeout reading byte %d\n",i);
				return(NULL);
			}
		}
		if(read(hw.fd,&b[i],1)!=1)
		{
			logprintf(0,"reading of byte %d failed\n",i);
			return(NULL);
		}
#               ifdef DEBUG
		logprintf(1,"byte %d: %02x\n",i,b[i]);
#               endif		
	}
	gettimeofday(&end,NULL);
	
	pre=(reverse((ir_code) b[0],8)<<1)|1;
	code=(reverse((ir_code) b[1],8)<<1)|1;
	code=code<<10;
	code|=(reverse((ir_code) b[2],8)<<1)|1;
	
	m=decode_all(remotes);
	last=end;
	return(m);
}