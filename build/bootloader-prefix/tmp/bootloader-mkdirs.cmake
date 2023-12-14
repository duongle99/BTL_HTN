# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "E:/HK231/Espressif/esp-idf/components/bootloader/subproject"
  "E:/HK231/HE_THONG_NHUNG/BTL_HTN/sample_project/build/bootloader"
  "E:/HK231/HE_THONG_NHUNG/BTL_HTN/sample_project/build/bootloader-prefix"
  "E:/HK231/HE_THONG_NHUNG/BTL_HTN/sample_project/build/bootloader-prefix/tmp"
  "E:/HK231/HE_THONG_NHUNG/BTL_HTN/sample_project/build/bootloader-prefix/src/bootloader-stamp"
  "E:/HK231/HE_THONG_NHUNG/BTL_HTN/sample_project/build/bootloader-prefix/src"
  "E:/HK231/HE_THONG_NHUNG/BTL_HTN/sample_project/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "E:/HK231/HE_THONG_NHUNG/BTL_HTN/sample_project/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "E:/HK231/HE_THONG_NHUNG/BTL_HTN/sample_project/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
