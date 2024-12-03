RC=rc
RCI=rci

ifeq ($(DEBUG), 1)
	include Make.Debug
else
	include Make.$(shell uname)
endif

ifdef HISTORY
	CFLAGS += -DHISTORY=\"$(HISTORY)\"
endif

OFILES=\
	code.$O\
	exec.$O\
	getflags.$O\
	glob.$O\
	here.$O\
	io.$O\
	lex.$O\
	pcmd.$O\
	pfnc.$O\
	simple.$O\
	subr.$O\
	trap.$O\
	tree.$O\
	var.$O\
	y.tab.$O\
	unix.$O\
	havefork.$O\

HFILES=\
	rc.h\
	x.tab.h\
	io.h\
	exec.h\
	fns.h\

YFILES=syn.y

all: $(RC) $(RCI)

$(RC): $(OFILES) prompt-null.$O
	$(CC) $(ARCHS) -o $(RC) $(OFILES) prompt-null.$O

$(RCI): $(OFILES) prompt-$(PROMPT).$O
	$(CC) $(ARCHS) -o $(RCI) $(OFILES) prompt-$(PROMPT).$O $(LFLAGS)

%.$O: %.c $(HFILES)
	$(CC) $(ARCHS) $(CFLAGS) -c $*.c

y.tab.h y.tab.c: $(YFILES)
	$(YACC) $(YFLAGS) $(YFILES)

x.tab.h: y.tab.h
	cmp -s x.tab.h y.tab.h || cp y.tab.h x.tab.h

install: all rcmain.unix rc.1
	mkdir -p $(DESTDIR)/$(PREFIX)/bin
	mkdir -p $(DESTDIR)/$(PREFIX)/share/rci
	mkdir -p $(DESTDIR)/$(PREFIX)/share/man/man1
	install -m 755 $(RC)  $(DESTDIR)/$(PREFIX)/bin
	install -m 755 $(RCI) $(DESTDIR)/$(PREFIX)/bin
	install -m 644 rcmain.unix $(DESTDIR)/$(PREFIX)/share/rci/rcmain
	install -m 644 rc.1 $(DESTDIR)/$(PREFIX)/share/man/man1

%: %.c $(HFILES)
	$(CC) $(ARCHS) -o $@ $< $(LFLAGS)

clean:
	rm -f $(RC) $(RCI) *.$O $(CLEANFILES)

sloc:
	cloc --exclude-list-file=.clocignore --exclude-ext=md .
