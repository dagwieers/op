# Upstream: Corey Henderson <corman@cormander.com>

%define _with_pam 1
%define _with_shadow 0
%define _with_xauth 1

Summary: Controlled privilege escalation (a flexible alternative to sudo)
Name: op
Version: 1.32
Release: 1%{?dist}
License: GPL
Group: System Environment/Base
URL: https://github.com/dagwieers/op/
Packager: Alec Thomas <alec@swapoff.org>
#Source: https://github.com/dagwieers/op/archive/%{version}.tar.gz
Source: op-%{version}.tar.gz
NoSource: 0

BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root

%description
Op is a tool for allowing users to run root privileged commands
without the root password.

# ----------------------------------------------------------------------
%prep
%setup

# ----------------------------------------------------------------------
%build
%configure \
%{?with_pam:--with-pam} \
%{?with_shadow:--with-shadow} \
%{?with_xauth:--enable-xauth}
%{__make} %{?_smp_mflags}

# ----------------------------------------------------------------------
%install
%{__rm} -rf %{buildroot} # RHEL5
%{__make} install DESTDIR=%{?buildroot}
%{__install} -p -d -m 700 %{buildroot}%{_sysconfdir}/op.d/
%{__install} -p -d %{buildroot}%{_sysconfdir}/pam.d/
%{__install} -p -m 600 op.conf %{buildroot}%{_sysconfdir}
cat << EOF > %{buildroot}%{_sysconfdir}/pam.d/op
#%PAM-1.0
#<su>
#auth      sufficient   pam_rootok.so
# Uncomment the following line to implicitly trust users in the "wheel" group.
#auth      sufficient   pam_wheel.so trust use_uid
# Uncomment the following line to require a user to be in the "wheel" group.
#auth      required     pam_wheel.so use_uid
#</su>
auth       include      system-auth
#<su>
account    sufficient   pam_succeed_if.so uid = 0 use_uid quiet
#</su>
#<sudo>
account    include      system-auth
password   include      system-auth
#session    required     pam_limits.so
#</sudo>
#<su>
#session    optional     pam_xauth.so
#</su>
EOF

%clean
%{__rm} -rf %{buildroot}

#%pre
#%pre_control op
#%pre_control op.conf

#%post
#%post_control -s wheelonly op
#%post_control -s strict op.conf

# ----------------------------------------------------------------------
%files
%defattr(-, root, root, -)
# %caps(cap_linux_immutable)
%attr(644,root,root) %config(noreplace) %{_sysconfdir}/pam.d/op
%attr(600,root,root) %config(noreplace) %{_sysconfdir}/op.conf
%attr(4511, root, root) %{_bindir}/op
%{_mandir}/man1/op.1*
%doc AUTHORS ChangeLog COPYING INSTALL README
%doc op.conf.complex
%attr(700, root, root) %dir %{_sysconfdir}/op.d

%changelog
* Fri Jun  5 2012 Alec Thomas <alec@swapoff.org> - 1.32
- Initial package.
