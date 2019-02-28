rock_library(platform_driver
    SOURCES CanPeakSysUSB.cpp CanDriveWhistle.cpp StrUtil.cpp TimeStamp.cpp Platform_Driver.cpp
    HEADERS CanMsg.h CanItf.h CanPeakSysUSB.h CanDriveItf.h CanDriveWhistle.h DriveParam.h Mutex.h StrUtil.h TimeStamp.h Platform_Driver.h
    DEPS_PKGCONFIG base-types base-lib)

include_directories($ENV{CMAKE_PREFIX_PATH}/include)
