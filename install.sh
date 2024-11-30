#!/bin/bash

if [ "$#" -ne 1 ]; then
  echo "Usage: $0 libspng | <fuzzer> | <sanitizer>"
  echo " - Supported fuzzers: radamsa, zzuf, afl++, certfuzz, honggfuzz"
  echo " - Supported sanitizers: clang, valgrind"
  exit 1
else
  case "$1" in
    libspng)
      echo "Installing libspng..."
      cd libspng
      git submodule update --init
      cmake -B build -S .
      cd build
      make
      cd ..
      ;;
    radamsa)
      echo "Installing Radamsa..."
      git clone https://gitlab.com/akihe/radamsa.git
      cd radamsa
      make
      sudo make install
      radamsa --help
      cd ..
      echo """
        Radamsa installed successfully!

        For more information, visit:
        https://gitlab.com/akihe/radamsa
      """
      ;;
    zzuf)
      echo "Installing zzuf..."
      wget https://github.com/samhocevar/zzuf/releases/download/v0.15/zzuf-0.15.tar.gz
      tar -xzvf zzuf-0.15.tar.gz 
      rm zzuf-0.15.tar.gz
      cd zzuf-0.15
      ./configure
      make
      sudo cp src/zzuf /usr/local/bin
      sudo mkdir /usr/local/lib/zzuf || true
      sudo cp src/.libs/libzzuf.so /usr/local/lib/zzuf
      echo """
        zzuf installed successfully!

        For more information, visit:
        https://github.com/samhocevar/zzuf
        http://caca.zoy.org/wiki/zzuf
      """
      ;;
    afl++)
      echo "Installing AFL++..."
      git clone https://github.com/AFLplusplus/AFLplusplus
      cd AFLplusplus
      make distrib
      sudo make install
      echo """
        AFL++ installed successfully!

        For more information, visit:
        https://aflplus.plus/
        https://github.com/AFLplusplus/AFLplusplus/blob/stable/README.md
      """
      cd ..
      ;;
    certfuzz)
      echo "Installing CERT BFF..."
      mkdir ~/bff
      wget https://resources.sei.cmu.edu/tools/downloads/vulnerability-analysis/bff/assets/BFF28/BFF-2.8.zip
      unzip BFF-2.8.zip -d ~/bff
      rm BFF-2.8.zip
      echo """
        BFF uses zzuf as fuzzer, so you need to install it as well by running: ./install.sh zzuf

        By default, BFF will use the following filesystem locations:
        For the location of the scripts (including bff.py):
        ~/bff

        For the results:
        ~/bff/results

        The default fuzzing target of ImageMagick:
        ~/convert

        All of these locations can be symlinks:
        ln -s /path/to/convert ~/convert

        To start fuzzing, run: 
        ~/bff/batch.sh

        For more information, visit:
        https://github.com/CERTCC/certfuzz/tree/main/src/linux
      """
      cd ..
      ;;
    honggfuzz)
      echo "Installing Honggfuzz..."
      sudo apt-get install binutils-dev libunwind-dev libblocksruntime-dev clang
      git clone https://github.com/google/honggfuzz.git
      cd honggfuzz
      make
      sudo make install
      cd ..
      echo """
        Honggfuzz installed successfully!

        For more information, visit:
        https://github.com/google/honggfuzz
      """
      ;;
    clang)
      echo "Installing clang..."
      sudo apt-get install clang
      echo """
        clang installed successfully!

        Now you can use ASan and MSan sanitizers.

        For more information, visit:
        https://github.com/google/sanitizers/wiki/AddressSanitizer
        https://github.com/google/sanitizers/wiki/MemorySanitizer
      """
      ;;
    valgrind)
      echo "Installing Valgrind..."
      sudo apt-get install valgrind
      echo """
        Valgrind installed successfully!

        Now you can use Memcheck sanitizer.

        For more information, visit:
        http://valgrind.org/docs/manual/quick-start.html
      """
      ;;
    *)
      echo "Unknown fuzzer: $1"
      ;;
  esac
fi