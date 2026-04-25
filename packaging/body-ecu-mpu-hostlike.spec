%global cmake_build_dir %{__cmake_builddir}

Name:           body-ecu-mpu-hostlike
Version:        0.1.0
Release:        1%{?dist}
Summary:        Body ECU MPU host-like executable with CLI and SOME/IP client
License:        Apache-2.0
URL:            https://github.com/vtz/body-ecu

Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake >= 3.14
BuildRequires:  gcc-c++ >= 10
BuildRequires:  git-core

%description
Host-like MPU-side Body ECU executable for Fedora / AutoSD / RHIVOS.

Builds the POSIX MPU target with the same runtime model used for local
development: SOME/IP client bridge, stub cloud transport, in-process signal
bus, and interactive CLI.

%prep
%autosetup -n %{name}-%{version}

%build
%cmake -S platforms/posix-mpu
%cmake_build

%install
install -Dpm 755 %{cmake_build_dir}/body_ecu_posix_mpu \
    %{buildroot}%{_bindir}/body-ecu-mpu-hostlike

%files
%license LICENSE
%{_bindir}/body-ecu-mpu-hostlike

%changelog
* Sat Apr 25 2026 Body ECU Contributors <body-ecu@example.com> - 0.1.0-1
- Initial package for host-like MPU workflow (CLI + SOME/IP)
