Name:           receiver
Branch:         dev_rtp
Version:        3.2.150618       
Release:        1%{?dist}
Summary:        Interlive stream %{name}

Group:          Youku
License:        Commerical        
URL:            http://www.youku.com            
Source0:        %{name}-%{branch}-%{version}.tar.gz
Prefix:     /opt/interlive
BuildRoot:      /root/rpmbuild/buildroot

%description
The %{name} program is used in youku interlive project to %{name} live stream

%prep
%setup -q


%build
make cleanall
make receiver release=%{version} 


%install
rm -rf %{buildroot}
make install-receiver DESTDIR=%{buildroot}%{prefix}
mkdir -p /opt/logs/interlive/%{name}/


%clean
rm -rf %{buildroot}


%files
%defattr(-,root,root,-)
%{prefix}/receiver/%{name}
%{prefix}/%{name}/%{name}.xml
%{prefix}/%{name}/crossdomain.xml
%{prefix}/%{name}/scripts

%post
chmod +x %{prefix}/%{name}/scripts/*
cp -f %{prefix}/%{name}/scripts/%{name} /etc/init.d
service %{name} start
sh %{prefix}/%{name}/scripts/add_cron.sh %{name}

%preun
chmod +x %{prefix}/%{name}/scripts/remove_cron.sh
sh %{prefix}/%{name}/scripts/remove_cron.sh %{name}
service %{name} stop
rm -f /etc/init.d/%{name}


%changelog
