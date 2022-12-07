
target := mconf
ncursesw-obj := mconf.o \
	lxdialog/checklist.o \
	lxdialog/inputbox.o \
	lxdialog/menubox.o \
	lxdialog/textbox.o \
	lxdialog/util.o \
	lxdialog/yesno.o
normal-obj := confdata.o \
	expr.o \
	parser.tab.o \
	lexer.lex.o \
	preprocess.o \
	symbol.o \
	util.o

CFLAGS := -Wall -Wmissing-prototypes -Wstrict-prototypes
CFLAGS += -O2 -fomit-frame-pointer -std=gnu89

NCURSESW_CFLAGS := -D_GNU_SOURCE

EXIST := $(shell if [ -d /usr/include/ncursesw ]; then echo "exist"; else echo "not exist"; fi )
ifeq ("$(EXIST)","exist")
NCURSESW_CFLAGS += -I/usr/include/ncursesw
LDLIBS := -lncursesw
else
LDLIBS := -lncurses
endif

LDFLAGS :=

.DEFAULT_GOAL := build

pre:
	$(Q)flex -olexer.lex.c -L lexer.l
	$(Q)bison -o parser.tab.c --defines=parser.tab.h -t -l parser.y

lexer.lex.c:
	$(Q)flex -olexer.lex.c -L lexer.l

parser.tab.c:
	$(Q)bison -o parser.tab.c --defines=parser.tab.h -t -l parser.y

$(ncursesw-obj):
%.o: %.c
	$(Q)$(CC) $(CFLAGS) $(NCURSESW_CFLAGS) -c -o $@ $<

$(normal-obj):
%.o: %.c
	$(Q)$(CC) $(CFLAGS) -c -o $@ $<

$(target): $(ncursesw-obj) $(normal-obj)
	$(Q)$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

build: $(target)

clean:
	$(Q)rm -f $(ncursesw-obj) $(normal-obj)

distclean: clean
	$(Q)rm -f $(target)
	$(Q)rm -f lexer.lex.c parser.tab.c parser.tab.h

.PHONY: build clean distclean

