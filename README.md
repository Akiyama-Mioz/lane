Need certain IDF version to build the project.

In your IDF directory.

```bash
cd $IDF_PATH
# I assume your origin is 
# https://github.com/espressif/esp-idf.git
git fetch origin v4.4.5
git checkout v4.4.5
git submodule update --init --recursive
./install.sh
source export.sh
```

```
-DIDF_MAINTAINER=1
```
