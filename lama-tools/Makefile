EXECUTABLE = src/lamac
MKDIR ?= mkdir

.PHONY: build

build:
	$(MAKE) -C runtime
	$(MAKE) -C tools

regression-all: regression regression-expressions

regression-expressions: tools
	$(MAKE) clean check -j8 -C regression/expressions
	$(MAKE) clean check -j8 -C regression/deep-expressions

clean:
	$(MAKE) clean -C runtime
	$(MAKE) clean -C tools
	$(MAKE) clean -C regression

regression:
	$(MAKE) clean check -C regression

performance:
	$(MAKE) clean check -C performance
