#ifndef MIME_H
#define MIME_H

/*
 * Gets a mime type given a file name. Simply passing an extension is also
 * sufficient..
 */
const char *get_mime_type(const char *file);

#endif
