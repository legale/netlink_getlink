# libnetlink_getlink
netlink socket based net devices names and low level (mac) addresses getter

## Howto build
```
git clone <repo_url>
cd <repo_dir>
make
```
Binary is in the build directory

## Howto use
### static
`build/getlink` or `build/getlink_static`
### shared
`LD_LIBRARY_PATH=build build/getlink_shared`
