SRCS=treedil_test.cpp
OBJS=$(SRCS:.cpp=.o)
LIBS=../libtreedil.a
TARGET=treedil_test

INC_FLAGS=-I../inc
WARNING_FLAGS=-Wall -Wshadow -Wnon-virtual-dtor -Woverloaded-virtual -Wsign-promo -Winit-self -Wmissing-include-dirs -Wswitch-default \
              -Wswitch-enum -Wunknown-pragmas -Wundef -Wpointer-arith -Wcast-qual -Wcast-align -Wconversion -Wsign-conversion -Wlogical-op \
              -Winvalid-pch -Wvla
CXX_FLAGS=-pipe -ggdb3 -O2 -fno-strict-aliasing -fPIC -pthread -std=gnu++0x $(WARNING_FLAGS) $(INC_FLAGS)
LD_FLAGS=-pthread -lrt -rdynamic

.PHONY: all clean

all: $(TARGET)

clean:
	make -C ../ clean
	rm -rf $(TARGET) $(OBJS)

$(TARGET): $(OBJS) $(LIBS)
	g++ $(LD_FLAGS) -o $@ $(OBJS) $(LIBS)

$(LIBS):
	make -C ../ all

.cpp.o:
	g++ $(CXX_FLAGS) -o $@ -c $<
