#!/bin/bash

(gettextize --version) < /dev/null > /dev/null 2>&1 || {
	echo;
	echo "You must have gettext installed to compile Gaim";
	echo;
	exit;
}

(libtoolize --version) < /dev/null > /dev/null 2>&1 || {
	echo;
	echo "You must have libtool installed to compile Gaim";
	echo;
	exit;
}

(automake --version) < /dev/null > /dev/null 2>&1 || {
	echo;
	echo "You must have automake installed to compile Gaim";
	echo;
	exit;
}

(autoconf --version) < /dev/null > /dev/null 2>&1 || {
	echo;
	echo "You must have autoconf installed to compile Gaim";
	echo;
	exit;
}

# Thanks decklin
if $(! test -f configure.in) && autoconf --version | grep '2\.[01]' > /dev/null 2>&1 ; then
	ln -sf configure.ac configure.in
fi

echo "Generating configuration files for Gaim, please wait...."
echo;

echo "Running gettextize, please ignore non-fatal messages...."
echo n | gettextize --copy --force;
echo "Running libtoolize, please ignore non-fatal messages...."
echo n | libtoolize --copy --force;

# we do this because autoconf defaults to installing to /usr/local
# and aclocal defaults to only looking in /usr/share/aclocal
if test -d /usr/local/share/aclocal ; then
	export ACLOCAL_FLAGS="$ACLOCAL_FLAGS -I /usr/local/share/aclocal"
fi

aclocal -I m4 $ACLOCAL_FLAGS;
autoheader;
automake --add-missing --copy;
autoconf;
./configure $@

