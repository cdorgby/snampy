#!/bin/sh

set -x

show_help()
{
    echo "${0##*/}: [-k] <path to directory> <file_list> <version string> <package name> <unpack prefix> <output file name>"
    echo "  -k -- keep working directory after exit"
}

echo_error()
{
    echo "${0##*/}: $*"
}

_keep=0
while test "$#" ; do
    args="$1"
    case $args in
        -k) _keep=1; shift 1;;
        -*) show_help; exit 1;;
        *) break;;
    esac
done

if test $# -lt 4; then
    show_help
    exit
fi

_dir=$1
_file_list=$2
_version_string=$3
_package_name_string=$4
_unpack_prefix=$5
_outfile=$6

if test ! -d $_dir; then
    echo_error "$_dir is not a directory"
    exit 1
fi

if test ! -e $_file_list; then
    echo_error "$_file_list doesn't exist"
    exit 1
fi

if test -e $_outfile; then
    echo_error "$_outfile already exists"
    exit 1
fi

_md5sum_util=`which md5sum`
if test $? -ne 0; then
    echo_error "Couldn't located md5sum"
    exit 1
fi

_tmp_dir=`mktemp -d`

if test $? -ne 0; then
    echo_error "Could not create temporary work directory"
    exit 1
fi

if test $_keep -eq 0; then
    _cleanup="rm -fr $_tmp_dir"
fi

_arch_name=`basename $_outfile`
_tmp_arch="$_tmp_dir/${_arch_name%.*}.tar.gz"
# from this point on no more calling of exit directly, now it should be
# eval $_cleanup; exit 1
_res_code=0
for _f in `cat $_file_list`; do
    _u="root"
    _g="root"
    # don't  change default permissions
    _p=
    if [ -f "$_dir/$_f.__tubs_user" ]; then
        _u=`cat "$_dir/$_f.__tubs_user"`
    fi
    if [ -f "$_dir/$_f.__tubs_group" ]; then
        _g=`cat "$_dir/$_f.__tubs_group"`
    fi
    if [ -f "$_dir/$_f.__tubs_mode" ]; then
        _p=`cat "$_dir/$_f.__tubs_mode"`
    fi

    if [ -n "$_u" ]; then 
        chown $_u $_dir/$_f
    fi
    if [ -n "$_g" ]; then 
        chgrp $_g $_dir/$_f
    fi
    if [ -n "$_p" ]; then 
        chmod $_p $_dir/$_f
    fi
done

(cd $_dir; tar c -T $_file_list | gzip > $_tmp_arch)

if test $? -ne 0; then
   echo_error "Couldn't create initial tar-ball"
   eval $_cleanup; exit 1
fi

_md5sum=`$_md5sum_util $_tmp_arch | cut -b-32`
_arch_size=`cat $_tmp_arch | wc -c`


. `dirname \`realpath $0\``/sfx_header.sh
cat $_tmp_arch >> $_outfile
eval $_cleanup; exit $_res_code
