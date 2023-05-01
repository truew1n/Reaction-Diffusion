#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <float.h>

#include <X11/Xlib.h>

#define HEIGHT 1000
#define WIDTH 1000

int8_t in_bounds(int32_t, int32_t, int64_t, int64_t);
void gc_put_pixel(void *, int32_t, int32_t, uint32_t);
void update(Display *, GC *, Window *, XImage *);

uint32_t decodeRGB(uint8_t r, uint8_t g, uint8_t b)
{
    return (r << 16) + (g << 8) + b;
}

double grid[HEIGHT][WIDTH][2] = {0};
double next[HEIGHT][WIDTH][2] = {0};

void fill() {
    for(int j = 0; j < HEIGHT; ++j) {
        for(int i = 0; i < WIDTH; ++i) {
            grid[j][i][0] = 1;
            grid[j][i][1] = 0;
            next[j][i][0] = 1;
            next[j][i][1] = 0;
        }
    }

    for(int j = 475; j < 525; ++j) {
        for(int i = 475; i < 525; ++i) {
            grid[j][i][1] = 1;
        }
    }
}


double lA(int x, int y)
{
    double sum = 0;
    sum += grid[ y ][ x ][0] * -1;
    sum += grid[ y ][x-1][0] * .2;
    sum += grid[ y ][x+1][0] * .2;
    sum += grid[y+1][ x ][0] * .2;
    sum += grid[y-1][ x ][0] * .2;
    sum += grid[y-1][x-1][0] * .05;
    sum += grid[y-1][x+1][0] * .05;
    sum += grid[y+1][x+1][0] * .05;
    sum += grid[y+1][x-1][0] * .05;
    return sum;
}

double lB(int x, int y)
{
    double sum = 0;
    sum += grid[ y ][ x ][1] * -1;
    sum += grid[ y ][x-1][1] * .2;
    sum += grid[ y ][x+1][1] * .2;
    sum += grid[y+1][ x ][1] * .2;
    sum += grid[y-1][ x ][1] * .2;
    sum += grid[y-1][x-1][1] * .05;
    sum += grid[y-1][x+1][1] * .05;
    sum += grid[y+1][x+1][1] * .05;
    sum += grid[y+1][x-1][1] * .05;
    return sum;
}

double dA = 1;
double dB = 0.5;
double f = 0.055;
double k = 0.062;

void draw(void *memory)
{
    for(int j = 1; j < HEIGHT-1; ++j) {
        for(int i = 1; i < WIDTH-1; ++i) {
            double a = grid[j][i][0];
            double b = grid[j][i][1];

            next[j][i][0] = a + ((dA * lA(i, j)) - (a*b*b) + (f*(1-a)));
            next[j][i][1] = b + ((dB * lB(i, j)) + (a*b*b) - (b*(k+f)));
        }
    }

    for(int j = 0; j < HEIGHT; ++j) {
        for(int i = 0; i < WIDTH; ++i) {
            double a = next[j][i][0];
            double b = next[j][i][1];
            int c = (int)((a-b) * 255);
            if(c > 255) c = 255;
            else if(c < 0) c = 0;
            gc_put_pixel(memory, i, j, decodeRGB(c, c, c));
        }
    }

    for(int j = 0; j < HEIGHT; ++j) {
        for(int i = 0; i < WIDTH; ++i) {
            double ta = grid[j][i][0];
            double tb = grid[j][i][1];
            grid[j][i][0] = next[j][i][0];
            grid[j][i][1] = next[j][i][1];
            next[j][i][0] = ta;
            next[j][i][1] = tb;
        }
    }
}


int8_t exitloop = 0;
int8_t auto_update = 0;

int main(void)
{
    Display *display = XOpenDisplay(NULL);

    int screen = DefaultScreen(display);

    Window window = XCreateSimpleWindow(
        display, RootWindow(display, screen),
        0, 0,
        WIDTH, HEIGHT,
        0, 0,
        0
    );

    char *memory = (char *) malloc(sizeof(uint32_t)*HEIGHT*WIDTH);

    XWindowAttributes winattr = {0};
    XGetWindowAttributes(display, window, &winattr);

    XImage *image = XCreateImage(
        display, winattr.visual, winattr.depth,
        ZPixmap, 0, memory,
        WIDTH, HEIGHT,
        32, WIDTH*4
    );

    GC graphics = XCreateGC(display, window, 0, NULL);

    Atom delete_window = XInternAtom(display, "WM_DELETE_WINDOW", 0);
    XSetWMProtocols(display, window, &delete_window, 1);

    Mask key = KeyPressMask | KeyReleaseMask;
    XSelectInput(display, window, ExposureMask | key);

    XMapWindow(display, window);
    XSync(display, False);

    fill();
    draw(memory);
    update(display, &graphics, &window, image);
    
    XEvent event;
    while(!exitloop) {
        while(XPending(display) > 0) {
            XNextEvent(display, &event);
            switch(event.type) {
                case Expose: {
                    update(display, &graphics, &window, image);
                    break;
                }
                case ClientMessage: {
                    if((Atom) event.xclient.data.l[0] == delete_window) {
                        exitloop = 1;   
                    }
                    break;
                }
                case KeyPress: {
                    if(event.xkey.keycode == 0x24) {
                        draw(memory);
                        update(display, &graphics, &window, image);
                    }
                    if(event.xkey.keycode == 0x41) {
                        auto_update = !auto_update;
                    }
                    break;
                }
            }
        }

        if(auto_update) {
            draw(memory);
            update(display, &graphics, &window, image);
        }
    }


    XCloseDisplay(display);

    free(memory);

    return 0;
}

int8_t in_bounds(int32_t x, int32_t y, int64_t w, int64_t h)
{
    return (x >= 0 && x < w && y >= 0 && y < h);
}

void gc_put_pixel(void *memory, int32_t x, int32_t y, uint32_t color)
{
    if(in_bounds(x, y, WIDTH, HEIGHT))
        *((uint32_t *) memory + y * WIDTH + x) = color;
}

void update(Display *display, GC *graphics, Window *window, XImage *image)
{
    XPutImage(
        display,
        *window,
        *graphics,
        image,
        0, 0,
        0, 0,
        WIDTH, HEIGHT
    );

    XSync(display, False);
}