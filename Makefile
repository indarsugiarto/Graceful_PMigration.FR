#DIRS = supv pmagent apps
DIRS = supv pmagent apps

RM := \rm -rf
LS := \ls -l

all: $(DIRS)
	@for d in $(DIRS); do (cd $$d; "$(MAKE)") || exit $$?; done
	@$(LS) binaries/*

clean: $(DIRS)
	@for d in $(DIRS); do (cd $$d; "$(MAKE)" clean) || exit $$?; done
	@$(RM) binaries/*
