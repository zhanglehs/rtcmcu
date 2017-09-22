Name:           forward
Version:        3.2.160219.1       
Release:        1%{?dist}
Summary:        Interlive stream forward

Group:          Youku
License:        Commerical        
URL:            http://www.youku.com            
Source0:        %{name}-%{version}.tar.gz
Prefix:     /opt/interlive
BuildRoot:      /root/rpmbuild/buildroot

%description
The forward program is used in youku interlive project to forward live stream

%prep
%setup -q


%build
make cleanall
make forward-nfp release=%{version} 


%install
rm -rf %{buildroot}
make install-forward-nfp DESTDIR=%{buildroot}%{prefix}


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
%{prefix}/forward/forward
%defattr(0766,interlive,interlive,-)
%{prefix}/forward/scripts
%defattr(0777,interlive,interlive,-)
%{prefix}/forward/scripts/*
%defattr(0666,interlive,interlive,-)
%{prefix}/forward/forward.xml
%{prefix}/forward/crossdomain.xml
%{prefix}/lib64/*

%post
echo "%post"
echo "/opt/interlive/lib64" > /etc/ld.so.conf.d/interlive.conf
ldconfig
cp -f %{prefix}/forward/scripts/forward /etc/init.d
mkdir -p /opt/logs/interlive/%{name}
chmod o+w /opt/logs/interlive/%{name}
service forward start 
sh %{prefix}/forward/scripts/add_cron.sh forward

%preun
chmod +x %{prefix}/forward/scripts/remove_cron.sh
sh %{prefix}/forward/scripts/remove_cron.sh forward
service forward stop
rm -f /etc/init.d/forward


%changelog
