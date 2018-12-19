FCGI Soup
=========
A simple webserver made for bloggers with embedded devices such as the old Raspberry Pi.


Features
--------
 * Static file serving.
 * Blog posts/articles.
 * Basic templating.
 

Static file serving
-------------------
Basically anything that doesn't have `blog/` as subpath is a static file.
If the URI refers to a folder, `index.html` is loaded.
If the file couldn't be read, 404 is returned.


Articles
--------
On startup, the file `blog.list` is loaded. This file contains entries for each blog post.
Each entry has the following format: `"<title>" "<author>" "<year>[-<month>[-<day>[ hour[:minute]]]]" "<file>" "<uri>"`.


Basic templating
----------------
It is easy to customize webpages with the Cinja templating library. The following templates
are used by FCGI soup:

- `main.html`
  All HTML pages are wrapped in this template. There is only one string variable, `BODY`, which
  represents the page being wrapped.
- `article\_list.html`: This page is used to list all articles. It has a single variable, `ARTICLES`,
   which is a list. Each item of the list is a dictionary with `URI`, `TITLE` and `DATE` as variables.
   To iterate over the list, you must use a `for` loop.
- `article.html`
  This is the wrapper for all blog posts. There are a number of interesting variables:
  - `PREV_URI` and `NEXT_URI`: these are strings that link to the previous and next article.
  - `PREV_TITLE` and `NEXT_TITLE`: the titles of the previous and next article, respectively.
  - `TITLE`: the title of the current article`
  - `AUTHOR`: the name of the author
  - `DATE`: the date the article was created (or well, what is listed in the blog list, anyways).
    It is currently formatted as "%Y-%M-%D %h:%m".
  - `BODY`: the body of the article (unparsed).
  - `COMMENTS`: a list of comments on the article. These only include the top-level comments.
  - `comment`: a function (or rather, a template) that takes a single comment as parameter.
- `comment.html`: A template for a single comment.
  - `AUTHOR`: the name of the author of the comment.
  - `DATE`: the date when the comment was posted.
  - `BODY`: the contents of the comment.
  - `ID`: the ID of the comment.
  - `REPLIES`: a list containing replies to this comment.
  - `comment`: a function to parse the replies.
- `error.html`: A template that can be used in case something not nice occured.
  - `STATUS`: the status code of the response.
  - `MESSAGE`: a message describing the error.

For examples, see the `www/` directory.

To post a comment, a form with the following parameters must be posted:
- `author`
- `body`
- (optionally) `reply-to`


Using FCGI Soup
===============
You will need a proxy of some sort that supports (F)CGI. e.g. Apache has `mod_fcgi`.
