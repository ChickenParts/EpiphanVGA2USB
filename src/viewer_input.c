/* SPDX-License-Identifier: ISC */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static int monitor_events(void *context,void *event) {
    (void)context;
    switch(*(const uint32_t *)event) {
        case 0x300: case 0x301: case 0x302: case 0x303:
        case 0x400: case 0x401: case 0x402: case 0x403:
            return 0;
        default: return 1;
    }
}

/* Runtime-only SDL2 integration: its public SDL_Init/SetEventFilter ABI avoids
 * adding SDL development packages to capture hosts. Scope this preload to
 * ffplay, never the USB producer. A live monitor must not pause, seek or step
 * because command keys reached its window. Window expose/close stays enabled. */
int SDL_Init(uint32_t flags) {
    int (*initialize)(uint32_t)=dlsym(RTLD_NEXT,"SDL_Init");
    void (*set_filter)(int (*)(void *,void *),void *)=dlsym(RTLD_NEXT,"SDL_SetEventFilter");
    if(!initialize || !set_filter) {
        fprintf(stderr,"Live-viewer SDL2 input policy could not resolve its ABI\n");
        exit(EXIT_FAILURE);
    }
    int result=initialize(flags);
    if(result==0) {
        set_filter(monitor_events,NULL);
    }
    return result;
}
