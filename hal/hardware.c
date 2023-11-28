#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <errno.h>
#include <limits.h>
#include <dlfcn.h>

#include <hardware.h>

#define HAL_LIBRARY_PATH1 "./libcamera.so"

static int load(const struct hw_module_t **pHmi)
{
    int status = 0;
    void *handle;
    struct hw_module_t *hmi;
    const char *sym = HAL_MODULE_INFO_SYM_AS_STR;
    // 여기에 dlopen / dlsym을 사용해서 ./libcamera.so 파일을 로딩하세요. 
    // 심볼은 아래 처럼 HAL_MODULE_INFO_SYM_AS_STR을 가지고 찾으세요.
    
    handle = dlopen(HAL_LIBRARY_PATH1, RTLD_LAZY);
    if (handle == NULL) {
        perror("dlopen");
        return -1;
    }
    hmi = (struct hw_module_t *)dlsym(handle, sym);
    if (hmi == NULL) {
        perror("dlsym");
        return -1;
    }
    *pHmi = hmi;
    return status;
}

int hw_get_camera_module(const struct hw_module_t **module)
{
    return load(module);
}
