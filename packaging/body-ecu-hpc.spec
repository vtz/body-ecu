%global cmake_build_dir %{_builddir}/%{name}-%{version}/build

Name:           body-ecu-hpc
Version:        0.1.0
Release:        1%{?dist}
Summary:        Body ECU HPC — MPU-side SOME/IP bridge and cloud gateway
License:        Apache-2.0
URL:            https://github.com/vtz/body-ecu

Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake >= 3.14
BuildRequires:  gcc-c++ >= 10
BuildRequires:  git-core
BuildRequires:  systemd-rpm-macros

%description
MPU-side Body ECU services for Red Hat In-Vehicle OS (RHIVOS / AutoSD).

Runs a SOME/IP client that bridges MCU body services (door lock, lighting,
vehicle mode) to the vehicle signal bus and cloud gateway. Currently uses
in-process stubs for the signal bus and cloud transport; these will be
replaced by Kuksa gRPC and NATS adapters as the project matures.

%prep
%autosetup -n %{name}-%{version}

%build
%cmake -S platforms/posix-mpu
%cmake_build

%install
install -Dpm 755 %{cmake_build_dir}/body_ecu_posix_mpu \
    %{buildroot}%{_bindir}/body-ecu-hpc

install -Dpm 644 packaging/body-ecu-hpc.service \
    %{buildroot}%{_unitdir}/body-ecu-hpc.service

install -Dpm 644 packaging/body-ecu-hpc.env \
    %{buildroot}%{_sysconfdir}/body-ecu/body-ecu-hpc.env

install -Dpm 644 config/services.yaml \
    %{buildroot}%{_sysconfdir}/body-ecu/services.yaml

%post
%systemd_post body-ecu-hpc.service

%preun
%systemd_preun body-ecu-hpc.service

%postun
%systemd_postun_with_restart body-ecu-hpc.service

%files
%license LICENSE
%{_bindir}/body-ecu-hpc
%{_unitdir}/body-ecu-hpc.service
%dir %{_sysconfdir}/body-ecu
%config(noreplace) %{_sysconfdir}/body-ecu/body-ecu-hpc.env
%config(noreplace) %{_sysconfdir}/body-ecu/services.yaml

%changelog
* Mon Apr 14 2025 Body ECU Contributors <body-ecu@example.com> - 0.1.0-1
- Initial RPM: SOME/IP bridge (posix-mpu) with stub signal bus and cloud transport
