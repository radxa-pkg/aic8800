#define LOG_TAG "libbt_aw"

#include <utils/Log.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>
#include <cutils/properties.h>
#include <stdlib.h>

#include "module_info.h"

static char *wifi_module_table[] = {
    "ap6210",
    "ap6330",
    "ap6335",
    "rtl8723bs", /* rtl8703as */
    "rtl8723bu",
    //"ap6212",
	"ap6236",
    "ap6356s",
    "ap6255",
    "rtl8723ds",
    "qca6174a",
    "rtl88x2bs",
    "unknown"
};

module_info_t module_info;
static int id_cached = 11;
static int cached = 0;

extern const char *get_wifi_vendor_name();
extern const char *get_wifi_module_name();

// This function should implement in system api.
static int aw_get_wifi_module_id(void)
{
    const char *p;

    if (cached == 0)
    {
        p = get_wifi_module_name();
        if (strcmp(p, "ap6210") == 0)
            id_cached = 0;
        else if (strcmp(p, "ap6330") == 0)
            id_cached = 1;
        else if (strcmp(p, "ap6335") == 0)
            id_cached = 2;
        else if (strcmp(p, "rtl8723bs") == 0)
            id_cached = 3;
        else if (strcmp(p, "rtl8723bu") == 0)
            id_cached = 4;
        //else if (strcmp(p, "ap6212") == 0)
        else if (strcmp(p, "ap6236") == 0)
            id_cached = 5;
        else if (strcmp(p, "ap6356s") == 0)
            id_cached = 6;
        else if (strcmp(p, "ap6255") == 0)
            id_cached = 7;
	else if (strcmp(p, "rtl8723ds") == 0)
	    id_cached = 8;
        else if (strcmp(p, "qca6174a") == 0)
            id_cached = 9;
	else if (strcmp(p, "rtl88x2bs") == 0)                                    
	    id_cached = 10;
	else
	    id_cached = 11;

        cached = 1;
    }

    return id_cached;
}

void aw_get_wifi_module_info(void)
{
    int id;

    id = aw_get_wifi_module_id();
    strcpy(module_info.mod_name, wifi_module_table[id]);
    if (((module_info.mod_name[0] == 'a') && (module_info.mod_name[1] == 'p'))
	|| ((module_info.mod_name[0] == 'q') && (module_info.mod_name[1] == 'c') && (module_info.mod_name[2] == 'a')))
        module_info.vendor_id = 0;
    else if ((module_info.mod_name[0] == 'r') && (module_info.mod_name[1] == 't') && (module_info.mod_name[2] == 'l'))
        module_info.vendor_id = 1;
    else
        module_info.vendor_id = 2;
    ALOGI("vendor id = %d, module name = %s", module_info.vendor_id, module_info.mod_name);
}
