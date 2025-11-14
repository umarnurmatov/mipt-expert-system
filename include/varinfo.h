#pragma once

typedef struct varinfo_t
{
    int line;
    const char* filename;
    const char* funcname;
    const char* varname;
} varinfo_t;

