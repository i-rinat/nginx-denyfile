nginx-denyfile
==============


nginx-denyfile (internal name is "ngx_http_access_denyfile_module") is a module
for Nginx web server, that denies access based on presence of a file with
specified name either in the same directory, or in any directory above up to
the document root.

Example configuration
---------------------

.. code::

    location / {
        root html;
        denyfile noaccess.txt;
    }


Directives
----------

======== ========================
Syntax:  **denyfile** *filename*;
Default: ""
Context: http, server, location
======== ========================

Sets name of the file. For example, if configuration have ``denyfile abcd.txt;``
and there is a file ``${document_root}/subdirectory/abcd.txt``, any requests
to ``/subdirectory/otherfile.html`` will return "403 Forbidden". Requests to
``/someotherfile.html`` will proceed as usual, though, if there is no file
named ``abcd.txt`` at that directory level too.

Empty string disables the module.

======== ================================
Syntax:  **denyfile_recursive** on | off;
Default: on
Context: http, server, location
======== ================================

This option enabled, makes module to look up not only in the currently requested
file's directory, but in all its parent directories, up to the document root.

Installation
------------

Download source of module somewhere. Download and unpack Nginx source. In Nginx
source directory, run:

.. code::

    ./configure --add-module=path/to/nginx-denyfile-module
    make install

License
-------

The MIT License. See LICENSE_ for details.


.. _LICENSE: LICENSE

