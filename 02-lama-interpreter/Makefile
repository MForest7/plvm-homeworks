EXECUTABLE = src/lamac
INSTALL ?= install -v
MKDIR ?= mkdir

.PHONY: regression-all

regression-all: regression regression-expressions

regression-expressions: interpreter
	$(MAKE) clean check -j8 -C regression/expressions
	$(MAKE) clean check -j8 -C regression/deep-expressions

clean:
	$(MAKE) clean -C runtime
	$(MAKE) clean -C interpreter
	$(MAKE) clean -C regression

regression: interpreter
	$(MAKE) -C runtime
	$(MAKE) -C interpreter
	$(MAKE) clean check -C regression
