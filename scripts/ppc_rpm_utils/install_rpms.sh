#!/bin/sh
#set -x

if [ "x$2" != "xquery" ] && [ "x$2" != "xpost" ]  && [ "$#" -ne 3 ]; then
	echo "Usage: $0 rootfs cmd rpm_pkg_list(absolute file path)"
	exit;
fi


root_dir=$1
cmd=$2
pkg_list=$3
target=powerpc

#RPM=$(dirname $0)/usr/bin/rpm
RPM=`which rpm`

echo $RPM
 
#ppc_e500mc

if [ "x$cmd" = "xinstall" ]; then 
	${RPM} --target=${target} --macros="%{_usrlibrpm}/macros:%{_usrlibrpm}/poky/macros:%{_usrlibrpm}/poky/%{_target}/macros:%{_etcrpm}/macros.*:%{_etcrpm}/macros:%{_etcrpm}/%{_target}/macros:~/.oerpmmacros" --root=${root_dir} --dbpath /var/lib/rpm --define="_openall_before_chroot 1" --noscripts --nolinktos --noparentdirs --ignoresize --replacepkgs --nodeps -Uvh --excludedocs  ${pkg_list}
elif [ "x$cmd" = "xquery" ]; then 
	${RPM} --target=${target} --macros="%{_usrlibrpm}/macros:%{_usrlibrpm}/poky/macros:%{_usrlibrpm}/poky/%{_target}/macros:%{_etcrpm}/macros.*:%{_etcrpm}/macros:%{_etcrpm}/%{_target}/macros:~/.oerpmmacros" --root=${root_dir} --dbpath /var/lib/rpm --define="_openall_before_chroot 1" -qa
elif [ "x$cmd" = "xremove" ]; then  
	${RPM} --target=${target} --macros="%{_usrlibrpm}/macros:%{_usrlibrpm}/poky/macros:%{_usrlibrpm}/poky/%{_target}/macros:%{_etcrpm}/macros.*:%{_etcrpm}/macros:%{_etcrpm}/%{_target}/macros:~/.oerpmmacros" --root=${root_dir} --dbpath /var/lib/rpm --define="_openall_before_chroot 1"  -e  ${pkg_list}
elif [ "x$cmd" = "xpost" ]; then
	${RPM} --target=${target} --macros="%{_usrlibrpm}/macros:%{_usrlibrpm}/poky/macros:%{_usrlibrpm}/poky/%{_target}/macros:%{_etcrpm}/macros.*:%{_etcrpm}/macros:%{_etcrpm}/%{_target}/macros:~/.oerpmmacros" --root=${root_dir} --dbpath /var/lib/rpm --define="_openall_before_chroot 1" -qa --qf 'Name: %{NAME}\n%|POSTIN?{postinstall scriptlet%|POSTINPROG?{ (using %{POSTINPROG})}|:\n%{POSTIN}\n}:{%|POSTINPROG?{postinstall program: %{POSTINPROG}\n}|}|'
fi

