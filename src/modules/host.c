#include "fastfetch.h"

void ffPrintHost(FFinstance* instance)
{
    if(ffPrintCachedValue(instance, &instance->config.hostKey, "Host"))
        return;

    FF_STRBUF_CREATE(family);
    ffGetFileContent("/sys/devices/virtual/dmi/id/product_family", &family);

    FF_STRBUF_CREATE(name);
    ffGetFileContent("/sys/devices/virtual/dmi/id/product_name", &name);

    FF_STRBUF_CREATE(version);
    ffGetFileContent("/sys/devices/virtual/dmi/id/product_version", &version);

    FF_STRBUF_CREATE(host);

    if(instance->config.hostFormat.length == 0)
    {
        if(family.length == 0 && name.length == 0)
        {
            ffPrintError(instance, &instance->config.hostKey, "Host", "neither family nor name could be determined");
            return;
        }
        else if(name.length == 0)
        {
            ffStrbufAppend(&host, &family);
        }
        else if(family.length == 0)
        {
            ffStrbufAppend(&host, &name);
        }
        else
        {
            ffStrbufAppend(&host, &family);
            ffStrbufAppendC(&host, ' ');
            ffStrbufAppend(&host, &name);
        }

        if(
            version.length > 0 &&
            ffStrbufIgnCaseCompS(&version, "None") != 0 &&
            ffStrbufIgnCaseCompS(&version, "To be filled by O.E.M.") != 0
        ) {
            ffStrbufAppendC(&host, ' ');
            ffStrbufAppend(&host, &version);
        }
    }
    else
    {
        ffParseFormatString(&host, &instance->config.hostFormat, 3,
            (FFformatarg){FF_FORMAT_ARG_TYPE_STRBUF, &family},
            (FFformatarg){FF_FORMAT_ARG_TYPE_STRBUF, &name},
            (FFformatarg){FF_FORMAT_ARG_TYPE_STRBUF, &version}
        );
    }

    ffPrintAndSaveCachedValue(instance, &instance->config.hostKey, "Host", &host);
    ffStrbufDestroy(&host);

    ffStrbufDestroy(&family);
    ffStrbufDestroy(&name);
    ffStrbufDestroy(&version);
}
