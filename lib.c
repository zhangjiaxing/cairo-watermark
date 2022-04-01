#define _GNU_SOURCE
#include <dlfcn.h>
#include <ctype.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <X11/Xlib.h>
#include <cairo/cairo-xlib.h>

#include "list.h"

struct func_node {
    char *name;
    void *func;
    struct list_head list;
};

struct func_node func_head;

static void* load_sym(const char *symname, void *proxyfunc) {
	void *funcptr = dlsym(RTLD_NEXT, symname);
	if(!funcptr) {
		fprintf(stderr, "Cannot load symbol '%s' %s\n", symname, dlerror());
		exit(1);
	} else {
		fprintf(stderr, "loaded symbol '%s'" " real addr %p  wrapped addr %p\n", symname, funcptr, proxyfunc);
	}
	if(funcptr == proxyfunc) {
		fprintf(stderr, "circular reference detected, aborting!\n");
		abort();
	}
	return funcptr;
}

/*
#define SETUP_SYM(X) do { true_ ## X = load_sym( # X, X ); } while(0)
typedef size_t (*fread_t)(void*, size_t, size_t, FILE*);
fread_t true_fread = NULL;
*/

#define DEBUG(format, ...) do{ fprintf(stderr, "%s: " format, __func__, ##__VA_ARGS__); } while(0)
#define ORIGIN_SYM(X) ((typeof(&X)) get_sym(#X))
#define ORIGIN_CALL(X, ...) (ORIGIN_SYM(X)(__VA_ARGS__))
#define SETUP_SYM(X) do { setup_sym( #X, X); } while(0)

static void *get_sym(const char *name){
    struct func_node *cur;
    list_for_each_entry(cur, &func_head.list, list){
        if(strcmp(name, cur->name) == 0){
            return cur->func;
        }
    }
    fprintf(stderr, "get_sym error %s\n", name);
    return NULL;
}


static void setup_sym(const char *name, void *proxyfunc){
    struct func_node *cur;
    list_for_each_entry(cur, &func_head.list, list){
        if(strcmp(name, cur->name) == 0){
            fprintf(stderr, "setup_sym error %s\n", name);
            exit(0);
        }
    }

    struct func_node *node = malloc(sizeof(*node));
    if(node == NULL){
        fprintf(stdout, "malloc error\n");
    }
    node->name = strdup(name);
    node->func = load_sym(name, proxyfunc);
    INIT_LIST_HEAD(&node->list);
    list_add_tail(&node->list, &func_head.list);
}

struct wid_node{
    Window wid;
    struct list_head list;
};

struct wid_node wid_head;


static void do_init(){
    static atomic_flag init_flag = ATOMIC_FLAG_INIT;
    if(! atomic_flag_test_and_set(&init_flag)){

        INIT_LIST_HEAD(&func_head.list);
        INIT_LIST_HEAD(&wid_head.list);

		SETUP_SYM(XCopyArea);
		SETUP_SYM(cairo_xlib_surface_create);
		SETUP_SYM(cairo_paint);
		SETUP_SYM(XMapWindow);
		SETUP_SYM(XUnmapWindow);
	}
}


cairo_surface_t *
cairo_xlib_surface_create (Display *dpy, Drawable drawable, Visual *visual, int width, int height){
	do_init();
	
	cairo_surface_t *surface = ORIGIN_CALL(cairo_xlib_surface_create, dpy, drawable, visual, width, height);
	
	Drawable win = cairo_xlib_surface_get_drawable (surface);
    width = cairo_xlib_surface_get_width(surface);
    height = cairo_xlib_surface_get_height(surface);

	DEBUG("Drawable surface=%p wid=%d width=%d height=%d\n", surface, win, width, height);

	return surface;
}


static bool cairo_surface_is_xlib_and_show(cairo_surface_t *surface){
    cairo_device_t *dev = cairo_surface_get_device(surface);

    DEBUG("%d\n", cairo_device_get_type(dev));

    if(dev == NULL || cairo_device_get_type(dev) != CAIRO_DEVICE_TYPE_XLIB){
        return false;
    }

    struct wid_node *cur;
    Drawable win = cairo_xlib_surface_get_drawable (surface);
    list_for_each_entry(cur, &wid_head.list, list){
        if(cur->wid == win){
            return true;
        }
    }
    return false;
}


void
cairo_paint (cairo_t *cr){
    do_init();

    ORIGIN_CALL(cairo_paint, cr);

    DEBUG("cairo paint\n");

    cairo_surface_t *surface = cairo_get_target (cr);
    if(cairo_surface_is_xlib_and_show(surface)){
#if 0
        static int i = 0;
        char filename[128];
        sprintf(filename, "xlib-%04d-%p.png", i++, surface);
        cairo_surface_write_to_png(surface, filename);
#endif

        cairo_save(cr);
        int width = cairo_xlib_surface_get_width(surface);
        int height = cairo_xlib_surface_get_height(surface);
        if(width > 128 && height > 128){
            //cairo_set_operator (cr, CAIRO_OPERATOR_MULTIPLY);
            cairo_set_operator (cr, CAIRO_OPERATOR_ATOP);
			cairo_move_to(cr, 0, 0); 
			cairo_set_source_rgba(cr, 0.5, 0.5, 0.5, 0.1);
			cairo_select_font_face (cr, "monospace",  CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD); 
			const char *font_family = cairo_toy_font_face_get_family(cairo_get_font_face(cr));
			cairo_set_font_size (cr, 40);

			const char *text = "知了 writebug@qq.com";
			cairo_rotate(cr, M_PI/6);
            // cairo_rotate(cr, M_PI);

			cairo_text_extents_t extents;
			cairo_text_extents (cr, text, &extents);

			int x = -extents.x_bearing, y=-extents.y_bearing; //x, y为要画文字的坐标
			int w = extents.width, h=extents.height;
			int x_padding = 120, y_padding = 80; 
			int offset = -(extents.width + x_padding) / 2;

			int hypotenuse = (int) (sqrt(width*width + height*height) + 0.5); //窗口对角线长度 四舍五入
			width = hypotenuse*2, height = hypotenuse*2; //因为水印可以旋转180度， 所以需要两倍大小的画布
			//x-= hypotenuse, y-=hypotenuse;
            int mhy = (int) (sqrt(1920*1920 + 1080*1080) + 0.5); //显示器大小
            x-= mhy, y-=mhy;

			while(y+h <= height){
				while(x+w <= width){
					cairo_move_to(cr, x, y);
					cairo_show_text(cr, text);
					x += w+x_padding;
				}
				y += h+y_padding;
                // x = -extents.x_bearing - hypotenuse + fmax(offset, 0);
				x = -extents.x_bearing - mhy + fmax(offset, 0);
				offset = -offset;
			}
		}

		cairo_restore(cr);
    }
}

int XCopyArea(Display *dpy, Drawable src, Drawable dest, GC  gc, int src_x, int src_y, unsigned int width, unsigned int height, int dest_x, int dest_y){
    do_init();

    DEBUG("src=%d dest=%d\n", src, dest);

	int ret = ORIGIN_CALL(XCopyArea, dpy, src, dest, gc, src_x, src_y, width, height, dest_x, dest_y);
    return ret;
}


int XMapWindow(Display* dpy, Window window){
    do_init();

    XWindowAttributes winattr;
    XGetWindowAttributes(dpy, window, &winattr);
    DEBUG("Drawable dest = %d size=%d*%d broder_width=%d\n", window, winattr.width, winattr.height, winattr.border_width);

    struct wid_node *node = malloc(sizeof(*node));
    if(node != NULL){
        INIT_LIST_HEAD(&node->list);
        node->wid = window;
        list_add_tail(&node->list, &wid_head.list);
    }
    return ORIGIN_CALL(XMapWindow, dpy, window);
}

int XUnmapWindow(Display* dpy, Window window){
    do_init();
    struct wid_node *cur, *tmp;
    list_for_each_entry_safe(cur, tmp, &wid_head.list, list){
        if(cur->wid == window){
            list_del_init(&cur->list);
            free(cur);
        }
    }
   return ORIGIN_CALL(XUnmapWindow, dpy, window);
}

static bool is_mapped_window(Window window){
    struct wid_node *cur;
    list_for_each_entry(cur, &wid_head.list, list){
        if(cur->wid == window){
            return true;
        }
    }
    return false;
}

