######################################################################
# Automatically generated by qmake (3.0) Sat Sep 10 00:44:24 2016
######################################################################

TEMPLATE = app
TARGET = BiosemiAcquisitionTemp
INCLUDEPATH += .
INCLUDEPATH += /usr/local/include
QT+=widgets multimedia
QMAKE_LIBDIR_FLAGS += -L/usr/local/lib

# Input
HEADERS += biosemi_io.h \
           chunk_transform.h \
           hardware_interface.h \
           linking.h \
           Resampler.h
SOURCES += biosemi_io.cpp hardware_interface.cpp main.cpp
