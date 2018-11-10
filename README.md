FCGI Soup
=========
A simple webserver made for bloggers with embedded devices such as the old Raspberry Pi.

Features
--------
 * Static file serving.
 * Article 'database' with author, date, URI, title and file.
 * Basic templating.
 
Static file serving
-------------------
Basically anything that doesn't have `blog/` as subpath is a static file.
If the URI refers to a folder, `index.html` is loaded.
If the file couldn't be read, 404 is returned.

Article 'database'
------------------
All articles are stored in a 'database'. Each entry has a field with the author, the date
of addition, the URI, the title and the path to the file.
When a URI is requested with `blog/` as subpath, the remainder of the URI is looked up
in the database. If the URI is found, the data is put inside an article object and processed
with a template.

Basic templating
----------------
There is limited templating support in the form of substitutions and basic conditionals.

### Substitution
A string can be inserted by including `{{ VAR }}` in a template.

### Conditionals
Currently, only `{% ifdef VAR %}` is supported. It allows excluding a section if a variable
is not defined. An `if` block is closed with `{% endif %}`.


Using FCGI Soup
===============
You will need a proxy of some sort that supports (F)CGI. e.g. Apache has `mod_fcgi`.
