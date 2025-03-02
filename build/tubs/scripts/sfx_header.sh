cat << EOF >  "$_outfile"
#!/bin/sh

MD5SUM=$_md5sum
VERSION=$_version_string
PACKAGE=$_package_name_string
FILESIZE=$_arch_size
UNPACK_PREFIX=$_unpack_prefix

show_help()
{
echo "\${0##*/}: <--verify|--version> | [--exec <command>] <path to exctract to>"
echo "   --verify  -- Verify the archive integrity"
echo "   --version -- Print package version"
echo "   --name    -- Print package name"
echo "   --list    -- List contents of package"
echo "   --exec    -- Execute <command> after unpacking add quotes (\") for arguments "
}

if test \$# -lt 1; then
   echo "\${0##*/}: Need at least one argument"
   show_help
   exit 1
fi

_verify=0
_version=0
_name=0
_list=0
_exec=
while test "\$#" ; do
    args="\$1"
    case \$args in
        --name) _name=1; shift 1;;
        --verify) _verify=1; shift 1;;
        --version) _version=1; shift 1;;
        --list) _list=1; shift;;
        --exec) shift 1; _exec=\$1; shift 1;;
        -*) show_help; exit 1;;
        *) break;;
    esac
done
_path=\$args

read_archive()
{
    blocks=\`expr \$FILESIZE / 4096\`
    bytes=\`expr \$FILESIZE % 4096\`
    skip=\`expr \\\`cat \$0 | wc -c | tr -d " "\\\` - \$FILESIZE\`
    dd if="\$0" ibs=\$skip skip=1 obs=4096 conv=sync 2> /dev/null | \\
    { test \$blocks -gt 0 && dd ibs=4096 obs=4096 count=\$blocks ; \\
      test \$bytes  -gt 0 && dd ibs=1 obs=4096 count=\$bytes ; } 2> /dev/null
}

_calc_md5sum=\`read_archive \$0 \$FILESIZE | md5sum | cut -b-32\`

if test \$_version -eq 1; then
    echo "\$VERSION"
    exit 0
fi

if test \$_name -eq 1; then
    echo "\$PACKAGE"
    exit 0
fi

if  test x"\$_calc_md5sum" != x"\$MD5SUM"; then
    exit 1
fi

if test \$_verify -eq 1; then
    exit 100
fi

if test -z "\$_path" -a ! -z "\$_exec" ; then
   echo "\${0##*/}: Need a directory to unpack to"
   exit 1
fi

if test ! -d "\$_path" -a \$_list -eq 0; then
   echo "\${0##*/}: \$_path is not a directory"
   exit 1
fi

_tar_args="-C \$_path\$UNPACK_PREFIX -xf"
if test \$_list -eq 1; then
    _tar_args="-tvf"
fi

( read_archive | gzip -d | tar \$_tar_args - )

if test ! -z "\$_exec"; then
    exec "\$_exec"
fi
exit
EOF
