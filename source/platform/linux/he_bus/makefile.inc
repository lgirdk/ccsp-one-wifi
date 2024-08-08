OS = $(shell uname)
PLATFORM = $(shell uname -a)

BASE = $(shell pwd)
BUS_HOME = $(BASE)
#BUS_UTILS = $(BASE)/one_wifi_program/one-wifi/generic/source/utils
BUS_LIB = $(BASE)/lib
INSTALLDIR = $(BUS_HOME)/install

OBJDIR = obj
CXX = g++
CC = gcc
AR= ar cr
RM = -rm -rf
