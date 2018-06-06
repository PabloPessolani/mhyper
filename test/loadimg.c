/* load VM image */

#define BIOS		0	/* Can only be used under the BIOS. */
#define nil 		0
#define _POSIX_SOURCE	1
#define _MINIX		1

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <lib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <alloca.h>
#include <minix/ipc.h>
#include <minix/syslib.h>
#include <minix/com.h>
#include <minix/const.h>
#include <a.out.h>
#include "../boot/rawfs.h"
#include "../boot/image.h"
#include "../boot/boot.h"
#define PROCESS_MAX	16	/* Must match the space in kernel/mpx.x */

/* struc image_header defined en image.h and sub-struct exec defined in a.out.h */
struct image_header ihdr[PROCESS_MAX]; 

_PROTOTYPE(int main, (int argc, char *argv []));
 
#define align(a, n)	(((u32_t)(a) + ((u32_t)(n) - 1)) & ~((u32_t)(n) - 1))

u32_t proc_size(struct image_header *hdr)
/* Return the size of a process in sectors as found in an image. */
{
	u32_t len= hdr->process.a_text;

	if (hdr->process.a_flags & A_PAL) len+= hdr->process.a_hdrlen;
	if (hdr->process.a_flags & A_SEP) len= align(len, SECTOR_SIZE);
	len= align(len + hdr->process.a_data, SECTOR_SIZE);

	return len >> SECTOR_SHIFT;
} 
 
int main(argc, argv)
int argc;
char *argv[];
{
 FILE *fp;
 int rcode, i;
 struct stat istat;
 char *img_addr, *iptr;
 int imgbytes, rbytes, rembytes;
 struct image_header hdr;

  /*--------------- check arguments -----------------------------*/
 if (argc != 2) {
	printf("usage: loadvmimg <VM_image_pathname>\n");
	exit(1);
	}
 printf("VM image pathname= %s\n", argv[1]);
  
 /*--------------- try to open the image file for reading ------*/
 fp  = fopen(argv[1],"r");
 if (fp == NULL) {
	printf("fopen: errno= %d\n", errno);
	exit(1);
	}
 
 /*--------------- get the image file size  ------*/
 rcode = stat(argv[1], &istat);
 if (rcode != 0) {
	printf("stat: errno= %d\n", errno);
	exit(1);
	}
 printf("istat.st_size=%d\n",istat.st_size);  	
 imgbytes = istat.st_size;
 
 /*--------------- get memory for the image  ------*/
 img_addr = malloc(imgbytes+CLICK_SIZE);
 printf("malloc(%d) = %X\n",(imgbytes+SECTOR_SIZE), img_addr);  	
  
 /*--------------- load image file into mermory  ------*/
 rembytes = imgbytes;
 iptr = img_addr;
 printf("reading image: ");
 while( rembytes > 0 )  {
    rbytes = fread(iptr, sizeof(char), SECTOR_SIZE, fp);
	rembytes -= rbytes;
	iptr += rbytes;
	printf(".");
	}
 printf("\n");
 
 /*--------------- close the image file   ------*/
 fclose(fp);

 /* ---------- read boot process' a.aout headers --------------------*/
 for( iptr = img_addr, i = 0 ; 
	iptr < (img_addr+imgbytes+CLICK_SIZE); 
	iptr += ((proc_size(&ihdr[i])+1)*SECTOR_SIZE), i++ )
	{
	if (i == PROCESS_MAX) {
		printf("There are more then %d programs in %s\n", PROCESS_MAX, argv[1]);
		errno= 0;
		return;
		}

	memcpy(&ihdr[i], iptr, sizeof(struct image_header));					

	if (BADMAG(ihdr[i].process)) { 
		if( !strcmp(ihdr[i].name,"init") ) {
			printf("Bad header1\n");
			errno= ENOEXEC; 
			exit(-1);
			}
		break;
		}
		
	printf("name[%d]:%s proc_size=%d \n",
		i, 
		ihdr[i].name, 
		(proc_size(&ihdr[i])*SECTOR_SIZE));
	}
 printf("All headers (%d) read \n", i-1);
 
 /* ---------- patch the image file and inform the VMM ---------------------*/

 
 /* El VMM informa de todo a la SYSTASK la que marca al proceso actual como NO DESTRUIBLE y NO EJECUTABLE
      hasta tanto el VMM se lo indique para no liberar la memoria 
      El VMM le pasa los datos de la imagen a la SYSTASK para que ponga en marcha la nueva VM. 	  
 */
 
 pause();
 exit(0);	
}
