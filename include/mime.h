#ifndef MIME_H
#define MIME_H

#include "dict.h"

/*
 * Gets a mime type given a file name. Simply passing an extension is also
 * sufficient..
 */
const string get_mime_type(const string file);

#endif
