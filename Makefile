CC := gcc
LDFLAGS := 
CFLAGS := -Wall -Iinclude -O2 -g -Wno-unused-function -Wno-unused-but-set-variable
SRC := 
LIBS := 

TARGET := hawkbeans 

include src/modules.mk

OBJ := $(patsubst %.c, %.o, \
	 $(filter %.c, $(SRC)))

DEPENDS := $(OBJ:.o=.d)

$(TARGET): $(OBJ)
	@echo "Linking...[$(TARGET) <- $<]"
	@$(CC) $(LDFLAGS) -o $@ $(OBJ) $(LIBS)

all: $(TARGET)

clean:
	@rm -f $(OBJ) $(TARGET) $(DEPENDS)

jlibs: 
	@ant -f jbuild.xml
	@jar xvf classes.jar
	@rm -rf classes.jar META-INF build	
	

include $(OBJ:.o=.d)

%.o: %.c
	@echo "$@ <- $<"
	@$(CC) $(CFLAGS) -c -o $@ $<
	
%.d: %.c
	@./depend.sh `dirname $*.c` \
	$(CFLAGS) $*.c > $@

buildtest: $(TARGET)
	@make -C test buildruntest
	@test/buildtest.sh

.PHONY: all clean buildtest
