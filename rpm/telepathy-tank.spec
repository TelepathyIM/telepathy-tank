Name:       telepathy-tank
Summary:    A Telegram connection manager
Version:    0.1.0
Release:    1
Group:      Applications/Communications
License:    GPLv2+
URL:        https://github.com/TelepathyIM/telepathy-tank
Source0:    https://github.com/TelepathyIM/telepathy-tank/archive/%{name}-%{version}.tar.bz2
Requires:   telepathy-mission-control

BuildRequires: pkgconfig(Qt5Core)
BuildRequires: pkgconfig(Qt5Network)
BuildRequires: pkgconfig(Qt5Gui)
BuildRequires: pkgconfig(Qt5DBus)
BuildRequires: pkgconfig(Qt5Xml)
BuildRequires: pkgconfig(TelepathyQt5) >= 0.9.7
BuildRequires: pkgconfig(TelepathyQt5Service) >= 0.9.7
# We don't use Farstream directly and it seems to be needed because of invalid TelepathyQt dependencies
BuildRequires: pkgconfig(TelepathyQt5Farstream) >= 0.9.7
BuildRequires: cmake >= 3.2
BuildRequires: libqmatrixclient-qt5-devel >= 0.5

BuildRequires: opt-gcc6

%description
Tank is a Qt-based matrix connection operator for the Telepathy framework.

%prep
%setup -q -n %{name}-%{version}

%build
cmake . \
    -DCMAKE_CXX_COMPILER=/opt/gcc6/bin/g++ \
    -DCMAKE_SHARED_LINKER_FLAGS="-L/opt/gcc6/lib -static-libstdc++" \
    -DCMAKE_INSTALL_PREFIX=%{_prefix} \
    -DCMAKE_INSTALL_LIBEXECDIR=%{_libexecdir} \
    -DCMAKE_INSTALL_DATADIR=%{_datadir}

make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install

%files
%defattr(-,root,root,-)

%{_libexecdir}/%{name}
%{_datadir}/dbus-1/services/*.service
%{_datadir}/telepathy/managers/*.manager
%{_datadir}/icons/hicolor/*/apps/%{name}.png
