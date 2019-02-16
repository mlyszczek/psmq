#!/bin/sh

scp_server="pkgs@kurwik"
project="psmq"
retval=1

atexit()
{
    set +e

    # removed installed deps
    apt-get purge -y libembedlog0
    apt-get purge -y libembedlog-dev

    # remove testing program
    apt-get purge -y "${project}"
    apt-get purge -y "lib${project}${abi_version}"
    apt-get purge -y "lib${project}-dev"

    exit ${retval}
}

if [ ${#} -ne 3 ]
then
    echo "usage: ${0} <version> <arch> <host_os>"
    echo ""
    echo "where:"
    echo "    <version>         git branch, tag or commit to build"
    echo "    <arch>            target architecture"
    echo "    <host_os>         target os (debian9, debian8 etc)"
    echo ""
    echo "example"
    echo "      ${0} 1.0.0 i386 debian9"
    exit 1
fi

version="${1}"
arch="${2}"
host_os="${3}"

trap atexit EXIT
set -e

###
# preparing
#

rm -rf "/tmp/${project}-${version}"
mkdir "/tmp/${project}-${version}"

cd "/tmp/${project}-${version}"
git clone "https://git.kurwinet.pl/${project}"
cd "${project}"

git checkout "${version}"

if [ ! -d "pkg/deb" ]
then
    echo "pkg/deb does not exist, cannot create debian pkg"
    exit 1
fi

version="$(grep "AC_INIT(" "configure.ac" | cut -f3 -d\[ | cut -f1 -d\])"
echo "version ${version}"
abi_version="$(echo ${version} | cut -f1 -d.)"
echo "abi version ${abi_version}"

###
# building package
#

###
# install build dependencies
#

apt-get install -y libembedlog0 libembedlog-dev
codename="$(lsb_release -c | awk '{print $2}')"

cp -r "pkg/deb/" "debian"
sed -i "s/@{DATE}/$(date -R)/" "debian/changelog.template"
sed -i "s/@{VERSION}/${version}/" "debian/changelog.template"
sed -i "s/@{CODENAME}/${codename}/" "debian/changelog.template"
sed -i "s/@{ABI_VERSION}/${abi_version}/" "debian/control.template"

mv "debian/changelog.template" "debian/changelog"
mv "debian/control.template" "debian/control"

export CFLAGS="-g"
#export LDFLAGS="-L/usr/bofc/lib"
debuild -us -uc

###
# unsed, so it these don't pollute gcc, when we built test program
#

unset CFLAGS
unset LDFLAGS

###
# verifying
#

cd ..

###
# debuild doesn't fail when lintial finds an error, so we need
# to check it manually, it doesn't take much time, so whatever
#

for d in *.deb
do
    echo "Running lintian on ${d}"
    lintian ${d}
done

dpkg -i "lib${project}${abi_version}_${version}_${arch}.deb"
dpkg -i "${project}_${version}_${arch}.deb"
dpkg -i "lib${project}-dev_${version}_${arch}.deb"

gcc ${project}/pkg/test.c -o testprog -lpsmq

if ldd ./testprog | grep "\/usr\/bofc"
then
    # sanity check to make sure test program uses system libraries
    # and not locally installed ones (which are used as build
    # dependencies for other programs

    echo "test prog uses libs from manually installed /usr/bofc \
        instead of system path!"
    exit 1
fi

./testprog

psmqd -v
psmq-sub -v
psmq-pub -v

dpkg -r "lib${project}-dev"
dpkg -r "${project}"
dpkg -r "lib${project}${abi_version}"

# run test prog again, but now fail if there is no error, testprog
# should fail as there is no library in te system any more
set +e
failed=0
./testprog && failed=1
psmqd -v && failed=1
psmq-sub -v && failed=1
psmq-pub -v && failed=1

if [ ${failed} -eq 1 ]
then
    exit 1
fi
set -e

if [ -n "${scp_server}" ]
then
    lib_dbgsym_pkg="lib${project}${abi_version}-dbgsym_${version}_${arch}.deb"
    bin_dbgsym_pkg="${project}-dbgsym_${version}_${arch}.deb"

    if [ ! -f "${lib_dbgsym_pkg}" ]
    then
        # on some systems packages with debug symbols are created with
        # ddeb extension and not deb
        lib_dbgsym_pkg="lib${project}${abi_version}-dbgsym_${version}_${arch}.ddeb"
    fi

    if [ ! -f "${bin_dbgsym_pkg}" ]
    then
        # on some systems packages with debug symbols are created with
        # ddeb extension and not deb
        bin_dbgsym_pkg="${project}-dbgsym_${version}_${arch}.ddeb"
    fi

    echo "copying data to ${scp_server}:${project}/${host_os}/${arch}"
    scp "${lib_dbgsym_pkg}" \
        "${bin_dbgsym_pkg}" \
        "lib${project}-dev_${version}_${arch}.deb" \
        "lib${project}${abi_version}_${version}_${arch}.deb" \
        "${project}_${version}_${arch}.deb" \
        "${project}_${version}.dsc" \
        "${project}_${version}.tar.xz" \
        "${project}_${version}_${arch}.build" \
        "${project}_${version}_${arch}.buildinfo" \
        "${project}_${version}_${arch}.changes" \
        "${scp_server}:${project}/${host_os}/${arch}" || exit 1
fi

retval=0
