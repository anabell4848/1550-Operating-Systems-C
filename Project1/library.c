//Hanyu Xiong
//hax12
//cs1550 project 1

// gcc -c library.c
// gcc -c square.c
// gcc -o compiled library.c square.c
// ./compiled

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <termios.h>
#include <linux/fb.h>


int framebuffer;
struct fb_var_screeninfo var;
struct fb_fix_screeninfo fix;
struct termios ter;
void *filecontent; 
typedef unsigned color_t;
char c;


void init_graphics(){	//open, ioctl, mmap

	// open the graphics device
	framebuffer = open("/dev/fb0", O_RDWR);
	
	//get the screen size and bits per pixels
	ioctl(framebuffer, FBIOGET_VSCREENINFO, &var);
	ioctl(framebuffer, FBIOGET_FSCREENINFO, &fix);
	
	//ask the OS to map a file into our address space
	filecontent=mmap(0, var.yres_virtual*fix.line_length, PROT_READ|PROT_WRITE, MAP_SHARED, framebuffer, 0);
	
	//use the ioctl system call to disable keypress echo and buffering the keypresses
	ioctl(framebuffer, TCGETS, &ter);
	
	ter.c_lflag |= ICANON;
	ter.c_lflag |= ECHO;
	
	ioctl(framebuffer, TCSETS, &ter);
}

void exit_graphics(){	//ioctl
	ioctl(framebuffer, TCGETS, &ter);
	
	ter.c_lflag &=~ECHO;			//echo off
	ter.c_lflag &=~ICANON;			//1 char at a time
	
	ioctl(framebuffer, TCSETS, &ter);
}

void clear_screen(){	//write
	char *escape = "\033[2J";
	write(1,escape,7);
}

char getkey() {			//select, read
	fd_set set;
    struct timeval tv;
	tv.tv_sec=0;
    tv.tv_usec=0; 
    FD_ZERO(&set);
    FD_SET(0, &set);
	
	if (select(1, &set, NULL, NULL, &tv)>0){
		read(0, &c, 1);
	}
	return c;
}
void sleep_ms(long ms){	//nanosleep
	struct timespec tim;
	tim.tv_sec = 0;
	tim.tv_nsec = ms*1000000L;
	nanosleep(&tim, NULL);
}

/*Everything above this comment works fine I believe, however, I couldn't 
figure out how to draw_pixel(). The logic make sense though, and I wrote
the code for draw_line() and draw_square() too. I'm sorry it doesn't work :'( */

void draw_pixel(int x, int y, color_t color){
	unsigned r = (color >> 16) & 0xff;
	unsigned g = (color >> 8) & 0xff;
	unsigned b = (color >> 0) & 0xff;
	unsigned short t;
	unsigned short *p;
	
	r = r * 32 / 256;
	g = g * 64 / 256;
	b = b * 32 / 256;
	
	t = (r << 11) | (g << 5) | (b << 0);
	filecontent += fix.line_length * y;
	p = filecontent;
	p +=x;
	*p = t;
	
}

void draw_line(int x, int y, color_t color){
	unsigned count;
	
	for (count = x; count <= y; count++){
		draw_pixel(x, y, color);
	}
}
void draw_rect(int x1, int y1, int width, int height, color_t c){
	unsigned counter;
	
	for (counter = x1; counter <= x1+width; counter++){
		draw_line(counter, y1, c);
		draw_line(counter, y1+height, c);
	}
	
	for (counter = y1; counter <= y1+height; counter++){
		draw_line(x1, counter, c);
		draw_line(x1+width, counter, c);
	}
}





