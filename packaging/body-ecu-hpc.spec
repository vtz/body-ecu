%global cmake_build_dir %{_builddir}/%{name}-%{version}/build

Name:           body-ecu-hpc
Version:        0.2.0
Release:        1%{?dist}
Summary:        Body ECU HPC — MPU-side SOME/IP bridge, Kuksa broker client, and cloud gateway
License:        Apache-2.0
URL:            https://github.com/vtz/body-ecu

Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake >= 3.14
BuildRequires:  gcc-c++ >= 10
BuildRequires:  git-core
BuildRequires:  systemd-rpm-macros
BuildRequires:  grpc-devel
BuildRequires:  grpc-plugins
BuildRequires:  protobuf-devel
BuildRequires:  openssl-devel
BuildRequires:  systemd-devel

Recommends:     kuksa-databroker

%description
MPU-side Body ECU services for Red Hat In-Vehicle OS (RHIVOS / AutoSD).

Bridges MCU body services (door lock, lighting, vehicle mode) to the Eclipse
Kuksa Databroker via gRPC and to cloud services via NATS. Runs as a systemd
service that connects to the MCU over SOME/IP and exposes vehicle signals
through VSS-compliant paths.

%prep
%autosetup -n %{name}-%{version}

%build
%cmake -S platforms/autosd -DBODY_ECU_REAL_ADAPTERS=ON
%cmake_build

%install
install -Dpm 755 %{cmake_build_dir}/body_ecu_autosd \
    %{buildroot}%{_bindir}/body-ecu-hpc

install -Dpm 644 packaging/body-ecu-hpc.service \
    %{buildroot}%{_unitdir}/body-ecu-hpc.service

install -Dpm 644 packaging/body-ecu-hpc.env \
    %{buildroot}%{_sysconfdir}/body-ecu/body-ecu-hpc.env

install -Dpm 644 config/services.yaml \
    %{buildroot}%{_sysconfdir}/body-ecu/services.yaml

install -Dpm 644 config/vss_overlay.yaml \
    %{buildroot}%{_sysconfdir}/body-ecu/vss_overlay.yaml

install -Dpm 644 config/signal_bridge.yaml \
    %{buildroot}%{_sysconfdir}/body-ecu/signal_bridge.yaml

install -Dpm 644 config/deployment.yaml \
    %{buildroot}%{_sysconfdir}/body-ecu/deployment.yaml

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
%config(noreplace) %{_sysconfdir}/body-ecu/vss_overlay.yaml
%config(noreplace) %{_sysconfdir}/body-ecu/signal_bridge.yaml
%config(noreplace) %{_sysconfdir}/body-ecu/deployment.yaml

%changelog
* Fri Apr 18 2026 Body ECU Contributors <body-ecu@example.com> - 0.2.0-1
- Build AutoSD target with real Kuksa gRPC and NATS adapters
- Add SystemdLifecycleAdapter (Type=notify)
- Package VSS overlay, signal bridge, and deployment configs

* Mon Apr 14 2025 Body ECU Contributors <body-ecu@example.com> - 0.1.0-1
- Initial RPM: SOME/IP bridge (posix-mpu) with stub signal bus and cloud transport
