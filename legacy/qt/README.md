# Historical Qt implementation

This directory preserves the Qt implementation and Qt-era project material as
historical reference only. It is not built, tested, packaged, or eligible for
new feature work. It is also excluded from source archives and source RPMs.

The maintained GTK application is in the repository-root `src/`, `data/`, and
`tests/` directories, and the root `CMakeLists.txt` builds it.

The Qt tree here is the last shipped Qt version, 2.1.3. Its privileged helper
protocol, allow-list, and install plan are the reference for behaviour parity
while the GTK port is completed.
