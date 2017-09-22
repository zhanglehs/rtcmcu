Name:           xm_ants
Version:        1.0.3.10049
Release:        1%{?dist}
Summary:        Interlive stream %{name}

Group:          Youku
License:        Commerical
URL:            http://www.youku.com
Source0:        %{name}-%{version}.tar.gz
Prefix:         /opt/interlive
BuildRoot:      /root/rpmbuild/buildroot

%description
The %{name} program is used in youku interlive project to %{name} live stream

%prep
%setup -q


%build
#we can not use multi-thread building here because we did not define dependency in makefile now
#make %{?_smp_mflags}
make release
#make


%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}%{prefix}

%clean
rm -rf %{buildroot}


%files
%defattr(-,root,root,-)
%{prefix}/%{name}/%{name}
%{prefix}/%{name}/%{name}.sh
%{prefix}/%{name}/%{name}.conf
%{prefix}/%{name}/scripts
%{prefix}/%{name}/bin
%{prefix}/%{name}/config

%post
chmod +x %{prefix}/%{name}/scripts/%{name}
mkdir -p /opt/logs/interlive/%{name}
chmod +w /opt/logs/interlive/%{name}
cp -f %{prefix}/%{name}/scripts/%{name} /etc/init.d
ulimit -c unlimited
service %{name} start
chmod +x %{prefix}/%{name}/scripts/add_cron.sh
chmod +x %{prefix}/%{name}/scripts/check_service.sh
chmod +x %{prefix}/%{name}/scripts/remove_cron.sh
sh %{prefix}/%{name}/scripts/add_cron.sh

%preun
sh %{prefix}/%{name}/scripts/remove_cron.sh
service %{name} stop
rm -f /etc/init.d/%{name}
rm -rf %{prefix}/%{name}

%changelog
