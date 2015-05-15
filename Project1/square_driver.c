#include <stdio.h>
#include <stdlib.h>
typedef unsigned short color_t;

void clear_screen();
void exit_graphics();
void init_graphics();
char getkey();
void sleep_ms(long ms);

void draw_pixel(int x, int y, color_t color);
void draw_line(int x, int y, color_t color);
void draw_rect(int x1, int y1, int width, int height, color_t c);

int main(int argc, char** argv)
{
	init_graphics();

	char key;
	int x = (640-20)/2;
	int y = (480-20)/2;

	do
	{
		clear_screen();
		key = getkey();
		if(key == 'w') y-=10;
		else if(key == 's') y+=10;
		else if(key == 'a') x-=10;
		else if(key == 'd') x+=10;
		//printf("\n  x=%d y=%d", x, y);
		//draw a blue rectangle
		draw_rect(x, y, 20, 20, 15);
		sleep_ms(20);
	} while(key != 'q');

	exit_graphics();

	return 0;

}
