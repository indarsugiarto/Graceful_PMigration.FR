DIRS = helloW pingpong

all: $(DIRS)
	@for d in $(DIRS); do (cd $$d; "$(MAKE)") || exit $$?; done

clean: $(DIRS)
	@for d in $(DIRS); do (cd $$d; "$(MAKE)" clean) || exit $$?; done
