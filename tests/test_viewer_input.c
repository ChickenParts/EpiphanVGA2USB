/* SPDX-License-Identifier: ISC */
#include <assert.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>

/* Stable SDL2 ABI, sufficient for this runtime-only integration test. */
union event { uint32_t type; uint8_t bytes[56]; uint64_t alignment; };
int main(void) {
    void *library=dlopen("libSDL2-2.0.so.0",RTLD_NOW|RTLD_GLOBAL);
    assert(library);
    int (*init)(uint32_t)=dlsym(RTLD_DEFAULT,"SDL_Init");
    uint8_t (*state)(uint32_t,int)=dlsym(library,"SDL_EventState");
    int (*push)(void *)=dlsym(library,"SDL_PushEvent");
    int (*peep)(void *,int,int,uint32_t,uint32_t)=dlsym(library,"SDL_PeepEvents");
    int (*get_filter)(int (**)(void *,void *),void **)=dlsym(library,"SDL_GetEventFilter");
    void (*quit)(void)=dlsym(library,"SDL_Quit");
    assert(init && state && push && peep && get_filter && quit);
    assert(init(0x20)==0); /* SDL_INIT_VIDEO, using the dummy video driver. */
    const uint32_t blocked[]={0x300,0x301,0x400,0x401,0x402,0x403};
    for(unsigned i=0;i<sizeof blocked/sizeof *blocked;i++) {
        /* Inject through SDL_PushEvent, which exercises the same event filter. */
        union event event={.type=blocked[i]};
        assert(push(&event)==0);
    }
    /* SDL2-compat requires a real text-input window for PushEvent conversion.
     * Query its installed callback to check text types without invalid payloads. */
    int (*filter)(void *,void *)=NULL;
    void *context=NULL;
    assert(get_filter(&filter,&context) && filter);
    union event edit={.type=0x302},text={.type=0x303};
    assert(filter(context,&edit)==0 && filter(context,&text)==0);
    assert(state(0x200,-1)==1); /* Window resize/focus/expose events stay live. */
    union event window={.type=0x200}; window.bytes[12]=3; /* EXPOSED */
    assert(push(&window)==1);
    union event stop={.type=0x100};
    assert(push(&stop)==1);
    union event output={0};
    assert(peep(&output,1,2,0x200,0x200)==1 && output.type==0x200);
    assert(peep(&output,1,2,0x100,0x100)==1 && output.type==0x100);
    quit(); dlclose(library);
    puts("PASS real SDL: playback input ignored; expose and close events preserved");
    return 0;
}
