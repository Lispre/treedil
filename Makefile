SRCS=$(wildcard src/*.cpp)
OBJS=$(SRCS:.cpp=.o)
TARGET=libtreedil.a

INC_FLAGS=-I./inc
WARNING_FLAGS=-Wall -Wshadow -Wnon-virtual-dtor -Woverloaded-virtual -Wsign-promo -Winit-self -Wmissing-include-dirs -Wswitch-default \
              -Wswitch-enum -Wunknown-pragmas -Wundef -Wpointer-arith -Wcast-qual -Wcast-align -Wconversion -Wsign-conversion -Wlogical-op \
              -Winvalid-pch -Wvla
CXX_FLAGS=-pipe -ggdb3 -O2 -fno-strict-aliasing -fPIC -pthread -std=gnu++0x $(WARNING_FLAGS) $(INC_FLAGS)

.PHONY: all clean

all: $(TARGET)

clean:
	rm -rf $(TARGET) $(OBJS)

$(TARGET): $(OBJS)
	ar qcs $(TARGET) $(OBJS)

.cpp.o:
	g++ $(CXX_FLAGS) -o $@ -c $<
