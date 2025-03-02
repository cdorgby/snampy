#!/bin/bash
# Generate the software update bundle using the files that are produced by the
# build system in the output.<build target>/images directory.
# It's assumed that the images directory will contain, at least, SPL, u-boot.img and boot.fit.
# These SPL and u-boot.img will be used to construct the bootloader image and boot.fit is the
# kernel and minimal root-fs to initialize the crypto and load & unpack software packages.
#
# Optionally, the images directory can contain software packages (files with *.pkg extension).
# These will also be included into the software bundle, assuming they are valid (have --name and --version options)
# if the 7th (file list) argument is provided it is assumed to be a path to a file container one path per line
# of files to include as packages into the bundle
#
# If the optional 8th argument is provided, it will be treated as a path to file containg paths to scripts to include
# into the swubundle
#
# For each files included into the update bundle this script will also generate a SHA256 hash and
# use OpenSSL wirth certificate to sign the bundle.

if [ ! $# -le 8 ]; then
    echo "No enough argumentsi, $#"
    echo "$0: <path to image dir> <key type> <package version> <buildroot/kernel version> <build board> <project> <output file> [file list [script list]]"
    echo "key type should be 'test'"
    exit 255
fi

set -x

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
IMAGE_DIR=$1
UPDATE_RELEASE_VERSION=$2
KEY_TYPE=$2
KEY_FILE="${SCRIPT_DIR}/${KEY_TYPE}.key.pem"
CERT_FILE="${SCRIPT_DIR}/${KEY_TYPE}.cert.pem"
SCRIPTS="${SCRIPT_DIR}/fix-ver-info.sh ${SCRIPT_DIR}/remount-alt.sh ${SCRIPT_DIR}/activate-alt.sh"
PACKAGE_VERSION=$3
BUILDROOT_VERSION=$4
BUILD_BOARD=$5
PROJECT_NAME=$6
UPDATE_FILE=$7
FILE_LIST=$8
INSTALL_PATH="/mnt/alt"

# add existing mendatory scripts into the bundle
_script_file_list="${SCRIPTS}"

if [ "$BUILD_BOARD" == "cna3000" ]; then
    KERNEL_IMAGE="boot.fit"
    KERNEL_PATH="/mnt/alt"
else
    KERNEL_IMAGE="bzImage"
    KERNEL_PATH="/mnt/alt/boot"
fi

tmp_dir=$(mktemp -d -t ci-XXXXXXXXXX)

sha256()
{
    _sum=`sha256sum $1 | cut -d " " -f 1`
    if [ $? -ne 0 ]; then
        echo "failed to sign"
        exit 96
    fi
    echo $_sum
}

sign()
{
    openssl cms -sign -in  $1 -out $1.sig -signer ${CERT_FILE} \
        -inkey ${KEY_FILE} -outform DER -nosmimecap -binary
    if [ $? -ne 0 ]; then
        echo "failed to sign"
        exit 97
    fi
}

if [ ! -r $KEY_FILE ]; then
    echo "${KEY_FILE} not found"
    exit 98
fi

if [ ! -r $CERT_FILE ]; then
    echo "${CERT_FILE} not found"
    exit 99
fi

FILES="${IMAGE_DIR}/${KERNEL_IMAGE} ${_script_file_list}"
for i in $FILES; do
    if [ ! -r $i ]; then
        echo "$i is not a file or can't be read"
        exit 100
    fi
done

# generate a single file containing both SPL and u-boot.
_kernel_hash=`sha256 ${IMAGE_DIR}/${KERNEL_IMAGE}`
_remount_active_hash=`sha256 ${SCRIPT_DIR}/remount-active.sh`
_remount_alt_hash=`sha256 ${SCRIPT_DIR}/remount-alt.sh`
_activate_alt_hash=`sha256 ${SCRIPT_DIR}/activate-alt.sh`

# generate config blocks for found packages
_pkg_config=
_update_pkg_config=
_script_config=
_pkg_file_list=
if [ -f "$FILE_LIST" ]; then
_pkg_file_list=`cat $FILE_LIST`
else
_pkg_file_list=${IMAGE_DIR}/*.pkg
fi

# create entries for .pkg files
for i in $_pkg_file_list; do
    if [ -f $i ]; then
        if [ ! -r $i ]; then
            ehco "$i can't be read"
            exit 101
        fi
        FILES="$FILES $i"
        _hash=`sha256 $i`
        _version=`sh $i --version`
        _name=`sh $i --name`
        _pkg_config="${_pkg_config}$(cat <<EOF
,
                {
                    filename = "`basename $i`";
                    path = "${INSTALL_PATH}/`basename $i`";
                    sha256 = "${_hash}";
                    name = "${_name}";
                    version = "${_version}";
                }
EOF
        )"
        _updated_pkg_config="${_updated_pkg_config}$(cat <<EOF
{
            "filename": "`basename $i`",
            "path": "${INSTALL_PATH}/`basename $i`",
            "sha256": "${_hash}",
            "name": "${_name}",
            "version":"${_version}"
        },
EOF
        )"
    fi
done

# create entries for shell scripts
for i in $_script_file_list; do
    if [ -f $i ]; then
        if [ ! -r $i ]; then
            ehco "$i can't be read"
            exit 101
        fi
        FILES="$FILES $i"
        _hash=`sha256 $i`
        _script_config="${_script_config}$(cat <<EOF
,
                {
                    filename = "`basename $i`";
                    type = "postinstall";
                    data = "rw";
                    sha256 = "${_hash}";
                }
EOF
        )"
    fi
done

# copy everything into the temp directory
for i in $FILES;do
    cp $i $tmp_dir;
done

cat > "${tmp_dir}/sw-description" << EOF
software =
{
    version = "${PACKAGE_VERSION}";
    main: {
        fs:
        {
            files: (
                {
                    filename = "${KERNEL_IMAGE}";
                    path = "${KERNEL_PATH}/${KERNEL_IMAGE}";
                    sha256 = "${_kernel_hash}";
                    name = "kernel";
                    version = "${BUILDROOT_VERSION}";
                }$_pkg_config
            );

            scripts: ( 
                {
                    filename = "remount-alt.sh";
                    type = "preinstall";
                    data = "rw";
                    sha256 = "${_remount_alt_hash}";
                }$_script_config
            );
        };
    };
}
EOF

cat > "${tmp_dir}/sw-description.json" << EOF
{
    "project":"${PROJECT_NAME}",
    "platform":"${BUILD_BOARD}",
    "version":"${PACKAGE_VERSION}",
    "files":[
        ${_updated_pkg_config}{
            "filename": "${KERNEL_IMAGE}",
            "path": "${KERNEL_PATH}/${KERNEL_IMAGE}",
            "sha256": "${_kernel_hash}",
            "name": "kernel",
            "version": "${BUILDROOT_VERSION}"
        }
    ]
}
EOF

cd ${tmp_dir}
sign "sw-description"
sign "sw-description.json"

cp sw-description /tmp
cp sw-description.json /tmp
cp sw-description.json.sig /tmp

# generate list of files to add to the CPIO archive, sw-description and sw-description must the first two files
# in the archive for swupdate to work
images="sw-description sw-description.sig sw-description.json sw-description.json.sig `find . -maxdepth 1 ! -name 'sw-description*' -a -type f`"
rm -f ${UPDATE_FILE}
for i in $images;do
    echo $i;

done | cpio -ov -H crc --no-absolute-filenames -D ${tmp_dir} >> ${UPDATE_FILE}

ls -l ${tmp_dir}
rm -rf $tmp_dir
echo "Update bundle created: ${UPDATE_FILE}"
