# PowerVR Texture Compression decompressor from the PowerVR Native SDK.
# Copyright (c) Imagination Technologies Ltd.
# Licensed under the MIT License.
# References:
# - PowerVR commit: c1605c99281797e5cd4c8439e1bc679706bbb311
# - https://github.com/powervr-graphics/Native_SDK
PROJECT(PowerVR LANGUAGES CXX)

# Sources.
SET(libpvrtc_SRCS PVRTDecompress.cpp)
# Headers.
SET(libpvrtc_H PVRTDecompress.h)

######################
# Build the library. #
######################

ADD_LIBRARY(pvrtc STATIC ${libpvrtc_SRCS} ${libpvrtc_H})
TARGET_INCLUDE_DIRECTORIES(pvrtc
	INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}
	PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
	)

# Unix: Add -fpic/-fPIC in order to use this static library in plugins.
IF(UNIX AND NOT APPLE)
	SET(CMAKE_C_FLAGS	"${CMAKE_C_FLAGS} -fpic -fPIC")
	SET(CMAKE_CXX_FLAGS	"${CMAKE_CXX_FLAGS} -fpic -fPIC")
ENDIF(UNIX AND NOT APPLE)
