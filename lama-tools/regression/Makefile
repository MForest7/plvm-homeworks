TESTS=$(sort $(basename $(wildcard test*.lama)))

LAMAC=lamac
BIN=../tools/build/bin
INTERPRETER=$(BIN)/interpreter
BCDUMP=$(BIN)/bcdump

.PHONY: check $(TESTS)

check: $(TESTS)

$(TESTS): %: %.lama
	@echo "regression/$@"
	@LAMA=../runtime $(LAMAC) -b $< && cat $@.input | $(BCDUMP) $@.bc > $@.bcd
	@LAMA=../runtime $(LAMAC) -b $< && cat $@.input | $(INTERPRETER) $@.bc > $@.log && diff $@.log orig/$@.log

clean:
	$(RM) test*.log *.s *.sm *~ $(TESTS) *.i *.bc *.bcd
	$(MAKE) clean -C expressions
	$(MAKE) clean -C deep-expressions
