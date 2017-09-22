Name:           receiver
Version:        3.2.160219.1
Release:        1%{?dist}
Summary:        Interlive stream %{name}

Group:          Youku
License:        Commerical        
URL:            http://www.youku.com            
Source0:        %{name}-%{version}.tar.gz
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


%clean
rm -rf %{buildroot}

%pre
echo "%pre"
default_user=interlive
user=$default_user
password=$user
#add an user for run interlive service
if id -u $user >/dev/null 2>&1; then
        echo "user $user exists"
else
        echo "user $user does not exist, add user"
        /usr/sbin/useradd $user 
        echo $user:$password | /usr/sbin/chpasswd
fi


%files
%defattr(04755,root,root,-)
%{prefix}/%{name}/%{name}
%defattr(0766,interlive,interlive,-)
%{prefix}/%{name}/scripts
%defattr(0777,interlive,interlive,-)
%{prefix}/%{name}/scripts/*
%defattr(0666,interlive,interlive,-)
%{prefix}/%{name}/%{name}.xml
%{prefix}/%{name}/crossdomain.xml
%{prefix}/lib64/*

%post
echo "%post"
echo "/opt/interlive/lib64" > /etc/ld.so.conf.d/interlive.conf
ldconfig
cp -f %{prefix}/%{name}/scripts/%{name} /etc/init.d
mkdir -p /opt/logs/interlive/%{name}
chmod o+w /opt/logs/interlive/%{name}
service %{name} start
sh %{prefix}/%{name}/scripts/add_cron.sh %{name}

%preun
chmod +x %{prefix}/%{name}/scripts/remove_cron.sh
sh %{prefix}/%{name}/scripts/remove_cron.sh %{name}
service %{name} stop
rm -f /etc/init.d/%{name}


%changelog
