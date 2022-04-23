TARGET	= megadl
LIBTGT	= lib$(TARGET).a

CXX     = g++
AR		= ar
RM		= rm

SRCDIR	= src
SRCSRC	= $(wildcard $(SRCDIR)/*.cpp)
SRCOBJ	= $(SRCSRC:.cpp=.o)

LIBDIR	= lib
LIBINC	= $(LIBDIR)/include
LIBSRC	= $(wildcard $(LIBDIR)/*.cpp)
LIBOBJ	= $(LIBSRC:.cpp=.o)

CFLAGS   = -Wall -O2

$(TARGET): $(LIBTGT) $(SRCOBJ)
	@echo "Linking $@..."
	@$(CXX) $^ -o $@ -L . -l$(TARGET) -lcpr -lcryptopp -lboost_program_options -lcurl

lib: $(LIBTGT)

$(LIBTGT): $(LIBOBJ)
	@echo "Archiving $@..."
	@$(AR) rcs $@ $^

%.o: %.cpp
	@echo "Compiling $<"
	@$(CXX) -c $< -o $@ $(CFLAGS) -I $(LIBINC)

clean:
	@$(RM) -f $(LIBDIR)/*.o
	@$(RM) -f $(SRCDIR)/*.o
	@$(RM) -f $(TARGET)
	@$(RM) -f $(LIBTGT)

.PHONY: clean lib
